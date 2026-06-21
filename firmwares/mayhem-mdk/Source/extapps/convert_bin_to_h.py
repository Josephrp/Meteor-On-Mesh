#!/usr/bin/env python3
"""
Tiny helper to turn a built PortaPack module/standalone app .bin into a .h
that can be included by the ESP32PP firmware (like sattrack.h or meshtonic_lora.h).

Usage:
  python convert_bin_to_h.py myapp.bin meshtonic_lora_app > meshtonic_lora.h

The output .h will contain an array whose first bytes are the standalone_app_info header.
The ESP side does:
  PPHandler::add_app( (uint8_t*)the_array , sizeof(the_array) );
and the size must be a multiple of 32 and >= sizeof(standalone_app_info).

After producing the .h, replace the body in meshtonic_lora.h (or the whole file) and rebuild the MDK firmware.
"""
import sys
import os

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    bin_path = sys.argv[1]
    array_name = sys.argv[2]
    out_path = sys.argv[3] if len(sys.argv) > 3 else None
    with open(bin_path, "rb") as f:
        data = f.read()
    # ESP add_app() requires size multiple of 32.
    pad = (-len(data)) % 32
    if pad:
        data = data + bytes(pad)
    lines = []
    lines.append("// Auto-generated from %s by convert_bin_to_h.py" % os.path.basename(bin_path))
    lines.append("// Do not edit by hand. Re-run the converter when you rebuild the PP app.")
    lines.append("unsigned char %s[] = {" % array_name)
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        line = ", ".join("0x%02x" % b for b in chunk)
        lines.append("    " + line + ("," if i+16 < len(data) else ""))
    lines.append("};")
    lines.append("/* size = %d (0x%x) */" % (len(data), len(data)))
    text = "\n".join(lines) + "\n"
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        print(text)

if __name__ == "__main__":
    main()