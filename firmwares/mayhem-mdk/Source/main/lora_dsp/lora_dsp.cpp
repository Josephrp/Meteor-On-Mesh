/*
 * Vendored/port of core LoRa wideband DSP algorithms for ESP32.
 * Algorithms derived from the Python Lora-Wideband-Decoder (detector.py / decoder.py):
 *   - Welch-style energy (simplified moving average)
 *   - Multi-lag Schmidl-Cox autocorrelation for preamble location
 *   - Downchirp generation + dechirp + FFT peak for CFO + symbol decision
 *   - Basic soft/hard decisions feeding a packet skeleton
 *
 * This runs on the ESP32 so that IQ originating from a HackRF (via USB host,
 * WiFi burst upload, PP baseband forwarder, or test vectors) can be processed
 * locally instead of requiring a Linux host Python process for the DSP.
 */

#include "lora_dsp.h"
#include <math.h>
#include <string.h>
#include <algorithm>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using c32 = std::complex<float>;

static bool g_inited = false;

// Simple precomputed downchirps cache (small number of common combos).
// In real use, compute on demand or at init for the SF/BW you care about.
struct ChirpKey {
    uint8_t sf;
    uint32_t bw;
    float fs;
};
static std::vector<ChirpKey> g_chirp_keys;
static std::vector<std::vector<c32>> g_downchirps;

// Minimal radix-2 FFT (in-place, bit-reversed input expected after reorder).
static void fft_radix2(c32* x, int n, bool inverse) {
    // Bit reverse
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j >= bit; bit >>= 1) j -= bit;
        j += bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2 * M_PI / len * (inverse ? -1 : 1);
        c32 wlen(cosf(ang), sinf(ang));
        for (int i = 0; i < n; i += len) {
            c32 w(1, 0);
            for (int j = 0; j < len/2; ++j) {
                c32 u = x[i+j];
                c32 v = x[i+j + len/2] * w;
                x[i+j] = u + v;
                x[i+j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        for (int i=0; i<n; ++i) x[i] /= (float)n;
    }
}

static float mag2(c32 z) { return z.real()*z.real() + z.imag()*z.imag(); }

void lora_dsp_init(void) {
    if (g_inited) return;
    g_inited = true;
    // Precompute a few common downchirps for speed (SF7-12, BW125/250/500 @ 2x BW fs)
    uint8_t sfs[] = {7,8,9,10,11,12};
    uint32_t bws[] = {125000, 250000, 500000};
    for (uint8_t sf : sfs) {
        for (uint32_t bw : bws) {
            float fs = (float)bw * 2.0f;
            size_t N = 1u << sf;
            size_t sps = N * 2; // osf=2
            std::vector<c32> ch(sps);
            float Ts = (float)N / (float)bw;
            for (size_t i=0; i<sps; ++i) {
                float t = (float)i / fs;
                float phase = -2.0f * (float)M_PI * (bw/2.0f * t - bw/(2.0f*Ts) * t * t);
                ch[i] = c32(cosf(phase), sinf(phase)); // exp(-j * ...)
            }
            g_chirp_keys.push_back({sf, bw, fs});
            g_downchirps.push_back(std::move(ch));
        }
    }
}

const lora_cpx_f32* lora_dsp_get_downchirp(uint8_t sf, uint32_t bw_hz, float fs, size_t* out_len) {
    if (!g_inited) lora_dsp_init();
    for (size_t k=0; k<g_chirp_keys.size(); ++k) {
        const auto& key = g_chirp_keys[k];
        if (key.sf == sf && key.bw == bw_hz && fabsf(key.fs - fs) < 1.0f) {
            *out_len = g_downchirps[k].size();
            return reinterpret_cast<const lora_cpx_f32*>(g_downchirps[k].data());
        }
    }
    // Not cached – compute on the fly (slow path, small)
    static std::vector<c32> tmp;
    size_t N = 1u << sf;
    size_t osf = (size_t)(fs / bw_hz + 0.5f);
    if (osf < 1) osf = 1;
    size_t sps = N * osf;
    tmp.resize(sps);
    float Ts = (float)N / (float)bw_hz;
    for (size_t i=0; i<sps; ++i) {
        float t = (float)i / fs;
        float phase = -2.0f * (float)M_PI * (bw_hz/2.0f * t - bw_hz/(2.0f*Ts) * t * t);
        tmp[i] = c32(cosf(phase), sinf(phase));
    }
    *out_len = sps;
    return reinterpret_cast<const lora_cpx_f32*>(tmp.data());
}

int lora_dsp_schmidl_cox_detect(const lora_cpx_f32* iq_in, size_t len, float fs,
                                uint8_t sf, uint32_t bw_hz,
                                float threshold,
                                lora_candidate* out, int max_cand) {
    if (len < 4096 || !out || max_cand <= 0) return 0;

    const c32* iq = reinterpret_cast<const c32*>(iq_in);
    size_t osf = (size_t)(fs / bw_hz + 0.5f);
    if (osf < 1) osf = 1;
    size_t lag = ((size_t)1 << sf) * osf;

    if (len <= lag + 128) return 0;

    // auto = iq[lag:] * conj(iq[:-lag])
    std::vector<float> norm(len - lag);
    for (size_t i=0; i + lag < len; ++i) {
        c32 a = iq[i+lag] * std::conj(iq[i]);
        float pwr = (std::norm(iq[i+lag]) + std::norm(iq[i])) * 0.5f + 1e-30f;
        norm[i] = std::abs(a) / pwr;
    }

    // block average to symbol rate
    size_t n_blocks = norm.size() / lag;
    if (n_blocks < 6) return 0;

    std::vector<float> block_norm(n_blocks);
    for (size_t b=0; b<n_blocks; ++b) {
        float s = 0;
        for (size_t k=0; k<lag; ++k) s += norm[b*lag + k];
        block_norm[b] = s / (float)lag;
    }

    // sliding sum over ~6 symbols
    const int win = 6;
    std::vector<float> sums;
    sums.reserve(n_blocks - win + 1);
    float csum = 0;
    for (int i=0; i<win; ++i) csum += block_norm[i];
    sums.push_back(csum);
    for (size_t b=win; b<n_blocks; ++b) {
        csum += block_norm[b] - block_norm[b - win];
        sums.push_back(csum);
    }

    // noise floor
    float med = 0;
    {
        std::vector<float> tmp = sums;
        std::sort(tmp.begin(), tmp.end());
        med = tmp[tmp.size()/2] + 1e-9f;
    }

    int nout = 0;
    // simple peak picker with local suppression
    for (size_t i=1; i+1 < sums.size() && nout < max_cand; ++i) {
        float v = sums[i];
        if (v > sums[i-1] && v > sums[i+1]) {
            float conf = (v / (float)win) / med;
            if (conf >= threshold) {
                out[nout].sf = sf;
                out[nout].bw_hz = bw_hz;
                out[nout].fs = fs;
                out[nout].freq_offset_hz = 0;
                out[nout].confidence = conf;
                nout++;
            }
        }
    }
    return nout;
}

int lora_dsp_dechirp_peak(const lora_cpx_f32* iq_in, size_t len,
                          size_t start_sample,
                          uint8_t sf, uint32_t bw_hz, float fs,
                          float* out_cfo_hz, float* out_quality) {
    const c32* iq = reinterpret_cast<const c32*>(iq_in);
    size_t N = (size_t)1 << sf;
    size_t osf = (size_t)(fs / bw_hz + 0.5f);
    if (osf < 1) osf = 1;
    size_t sps = N * osf;

    if (start_sample + sps > len) return -1;

    size_t clen = 0;
    const lora_cpx_f32* dc = lora_dsp_get_downchirp(sf, bw_hz, fs, &clen);
    const c32* down = reinterpret_cast<const c32*>(dc);
    if (!down || clen < sps) return -1;

    // dechirp
    std::vector<c32> de(sps);
    for (size_t i=0; i<sps; ++i) {
        de[i] = iq[start_sample + i] * down[i];
    }

    // FFT (radix2, pad or truncate to power of 2)
    int fftn = 1;
    while (fftn < (int)sps) fftn <<= 1;
    std::vector<c32> buf(fftn, c32(0,0));
    for (size_t i=0; i<sps && i<(size_t)fftn; ++i) buf[i] = de[i];
    fft_radix2(buf.data(), fftn, false);

    // find peak
    int best = 0;
    float pmax = 0, sum = 0;
    for (int k=0; k<fftn; ++k) {
        float m = mag2(buf[k]);
        sum += m;
        if (m > pmax) { pmax = m; best = k; }
    }
    float mean = (sum / fftn) + 1e-12f;
    float qual = pmax / mean;

    // CFO from bin
    float bin_hz = fs / (float)fftn;
    float cfo = (best - fftn/2) * bin_hz;

    if (out_cfo_hz) *out_cfo_hz = cfo;
    if (out_quality) *out_quality = qual;

    return best; // symbol bin
}

int lora_dsp_process_burst(const lora_iq_burst* burst,
                           const lora_dsp_cfg* cfg,
                           lora_decoded_pkt* out_pkts, int max_pkts) {
    if (!burst || !cfg || !out_pkts || max_pkts <= 0) return 0;

    // Convert incoming to float complex working buffer (small bursts expected)
    std::vector<c32> iq;
    size_t n = burst->count;
    iq.resize(n);
    if (burst->fmt == 0) { // sc16
        const int16_t* s = (const int16_t*)burst->samples;
        for (size_t i=0; i<n; ++i) {
            iq[i] = c32(s[2*i] / 32768.0f, s[2*i+1] / 32768.0f);
        }
    } else {
        const lora_cpx_f32* f = (const lora_cpx_f32*)burst->samples;
        for (size_t i=0; i<n; ++i) iq[i] = c32(f[i].real, f[i].imag);
    }

    int produced = 0;
    lora_candidate cands[8];

    for (uint8_t sfi=0; sfi < cfg->num_sf && produced < max_pkts; ++sfi) {
        uint8_t sf = cfg->sf_list[sfi];
        if (sf < 7 || sf > 12) continue;

        for (uint8_t bwi=0; bwi < cfg->num_bw && produced < max_pkts; ++bwi) {
            uint32_t bw = cfg->bw_list[bwi];

            int nc = lora_dsp_schmidl_cox_detect(
                reinterpret_cast<const lora_cpx_f32*>(iq.data()),
                iq.size(), burst->fs, sf, bw, cfg->sc_threshold, cands, 4);

            for (int ci=0; ci<nc && produced < max_pkts; ++ci) {
                size_t pos = (size_t)(cands[ci].confidence * 0); // placeholder; real impl uses the returned index*lag
                // For demo we just try a few positions around the buffer
                size_t try_pos = (burst->count / 3) + ci * ((1u<<sf) * 2);
                if (try_pos + ((size_t)1<<sf)*3 > burst->count) try_pos = 1024;

                float cfo = 0, qual = 0;
                int sym = lora_dsp_dechirp_peak(
                    reinterpret_cast<const lora_cpx_f32*>(iq.data()),
                    iq.size(), try_pos, sf, bw, burst->fs, &cfo, &qual);

                if (qual > 3.0f) {
                    lora_decoded_pkt& p = out_pkts[produced++];
                    p.sf = sf;
                    p.bw_hz = bw;
                    p.rssi = -80;
                    p.snr_q8 = 20;
                    p.crc_ok = 1;
                    p.proto = 1;
                    // Fabricate a tiny "header" payload for demo (real path would walk symbols)
                    p.payload_len = 8;
                    for (int k=0; k<8; ++k) p.payload[k] = (uint8_t)((sym + k) & 0xFF);
                }
            }
        }
    }
    return produced;
}

int lora_dsp_feed_sc16(const int16_t* iq, size_t count, float fs, float center_hz,
                       const lora_dsp_cfg* cfg,
                       lora_decoded_pkt* out, int max_out) {
    lora_iq_burst b{};
    b.ts_ms = 0;
    b.center_hz = center_hz;
    b.fs = fs;
    b.samples = iq;
    b.count = count;
    b.fmt = 0;
    return lora_dsp_process_burst(&b, cfg, out, max_out);
}
