#!/usr/bin/env bash
# Run inside meshtonic-mdk-idf container. Builds ESP32PP and exports artifacts to /out.
set -euo pipefail

# When compose overrides entrypoint, ensure IDF tools are on PATH.
if [[ -f /opt/esp/idf/export.sh ]]; then
  # shellcheck disable=SC1091
  . /opt/esp/idf/export.sh
fi

cd /project

# sdkconfig already targets esp32s3; set-target wipes config and re-downloads toolchains/components.
if [[ ! -f sdkconfig ]] || ! grep -q 'CONFIG_IDF_TARGET="esp32s3"' sdkconfig; then
  echo "=== Setting target esp32s3 (first-time only) ==="
  idf.py set-target esp32s3
else
  echo "=== Target esp32s3 already configured; skipping set-target ==="
fi

echo "=== ESP-IDF build ==="
idf.py build

echo "=== Merging flash image ==="
mkdir -p /out
python -m esptool --chip esp32s3 merge_bin \
  -o /out/merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 build/bootloader/bootloader.bin \
  0x10000 build/ESP32PP.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin

cp -f build/ESP32PP.bin /out/
cp -f build/bootloader/bootloader.bin /out/
cp -f build/partition_table/partition-table.bin /out/
cp -f build/ota_data_initial.bin /out/ 2>/dev/null || true
cp -f build/ESP32PP.elf /out/ 2>/dev/null || true

{
  echo "target=esp32s3"
  echo "app=ESP32PP"
  echo "idf=v5.4.1"
  date -u +"%Y-%m-%dT%H:%M:%SZ" | sed 's/^/built=/'
} > /out/build-info.txt

echo "=== Artifacts in /out ==="
ls -la /out/
