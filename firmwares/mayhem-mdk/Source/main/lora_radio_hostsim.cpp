// Host-side simulation stub for SX1262 driver command sequences (for unit testing outside IDF).
// Compile with a host C++ and mock spi; not linked in firmware.
// Provides minimal surface to replay register sequences from lora_radio.cpp for CI.

#include <cstdint>
#include <cstddef>
#include <vector>
#include <map>

struct MockSpi {
    std::vector<uint8_t> last_tx;
    void write(const uint8_t* d, size_t n) { last_tx.assign(d, d+n); }
    // extend for test asserts
};

// Example: a test could instantiate LoraRadio with mock and assert command bytes for configureFor etc.
// This file is intentionally not registered in CMake for firmware; used by external test harness.
