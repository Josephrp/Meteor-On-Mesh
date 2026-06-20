#!/usr/bin/env python3
"""Validate lora_decode vectors (nonce + AES-CTR + protobuf gates) against LWD reference."""
import struct
import sys
sys.path.insert(0, "firmwares/mayhem-mdk/lora-wideband-decoder/src")
from decoder import aes_ctr_decrypt, parse_protobuf, KNOWN_PORTNUMS, MESH_AES_KEY

def test_nonce_and_decrypt():
    nonce = bytearray(16)
    nonce[0:4] = struct.pack("<I", 0xAABBCCDD)
    nonce[8:12] = struct.pack("<I", 0x12345678)
    enc = bytes.fromhex("8942c5b56ae2")
    dec = aes_ctr_decrypt(MESH_AES_KEY, bytes(nonce), enc)
    assert dec == bytes.fromhex("080112026869"), dec.hex()
    pn = -1
    for fn, wt, val in parse_protobuf(dec):
        if fn == 1 and wt == 0:
            pn = int(val)
    assert pn in KNOWN_PORTNUMS
    print("lora_decode python reference vectors: PASS")

if __name__ == "__main__":
    test_nonce_and_decrypt()
