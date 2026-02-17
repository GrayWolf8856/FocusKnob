#!/usr/bin/env python3
"""
Chunked flash writer for ESP32-S3 with flaky USB-Serial/JTAG.
Splits large binaries into chunks, flashing each with a fresh connection.
Uses the stub flasher (faster) but reconnects between chunks to avoid USB dropout.

Usage: python3 flash_chunked.py
"""

import subprocess
import sys
import time
import os
import glob
import tempfile
from pathlib import Path

# Auto-detect paths
HOME = Path.home()
ARDUINO15 = HOME / "Library" / "Arduino15"

# Find esptool (prefer latest version)
ESPTOOL_CANDIDATES = sorted(
    glob.glob(str(ARDUINO15 / "packages/esp32/tools/esptool_py/*/esptool")),
    reverse=True,
)
ESPTOOL = ESPTOOL_CANDIDATES[0] if ESPTOOL_CANDIDATES else "esptool"

# Find ESP32 core version (prefer latest)
CORE_CANDIDATES = sorted(
    glob.glob(str(ARDUINO15 / "packages/esp32/hardware/esp32/*")),
    reverse=True,
)
CORE_DIR = CORE_CANDIDATES[0] if CORE_CANDIDATES else ""

PORT_PATTERNS = ["/dev/cu.usbmodem*", "/dev/cu.usbserial*"]
CHIP = "esp32s3"

# Find compiled sketch binaries
SKETCH_CACHE = HOME / "Library" / "Caches" / "arduino" / "sketches"
SKETCH_DIR = None
for d in sorted(SKETCH_CACHE.iterdir(), key=lambda p: p.stat().st_mtime, reverse=True) if SKETCH_CACHE.exists() else []:
    if (d / "FocusKnob.ino.bin").exists():
        SKETCH_DIR = str(d)
        break

if not SKETCH_DIR:
    print("ERROR: Could not find compiled FocusKnob binaries.")
    print("Run: arduino-cli compile --fqbn 'esp32:esp32:waveshare_esp32_s3_touch_amoled_18:CDCOnBoot=cdc,PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB' ~/Desktop/FocusKnob")
    sys.exit(1)

BOOT_APP = os.path.join(CORE_DIR, "tools", "partitions", "boot_app0.bin")

FLASH_ITEMS = [
    (0x0,     os.path.join(SKETCH_DIR, "FocusKnob.ino.bootloader.bin")),
    (0x8000,  os.path.join(SKETCH_DIR, "FocusKnob.ino.partitions.bin")),
    (0xe000,  BOOT_APP),
    (0x10000, os.path.join(SKETCH_DIR, "FocusKnob.ino.bin")),
]

# 64KB chunks - small enough to complete before USB drops
CHUNK_SIZE = 0x10000  # 64KB
MAX_RETRIES = 5
RETRY_DELAY = 3


def find_port():
    """Find the ESP32-S3 USB port (exclude known non-ESP32 devices)."""
    ports = []
    for pattern in PORT_PATTERNS:
        ports.extend(glob.glob(pattern))
    for p in ports:
        # Skip Dell speakerphone and other known devices
        if "usbmodem01" in p:
            continue
        return p
    return None


def wait_for_port(timeout=20):
    """Wait for the ESP32-S3 port to appear."""
    start = time.time()
    while time.time() - start < timeout:
        port = find_port()
        if port:
            time.sleep(1)
            return port
        time.sleep(0.5)
    return None


def flash_file(port, address, filepath):
    """Flash a file using the stub flasher with default-reset."""
    cmd = [
        ESPTOOL,
        "--chip", CHIP,
        "--port", port,
        "--before", "default-reset",
        "--after", "no-reset",
        "write-flash",
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "16MB",
        f"0x{address:x}", filepath,
    ]

    result = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    output = result.stdout + result.stderr

    if result.returncode == 0 and "Hash of data verified" in output:
        return True, ""
    elif "stopped responding" in output:
        return False, "chip stopped responding"
    else:
        lines = output.strip().split("\n")
        return False, lines[-1] if lines else "unknown error"


def main():
    print("=" * 60)
    print("  FocusKnob — ESP32-S3 Chunked Flash Writer")
    print("  Handles flaky USB-Serial/JTAG by chunking large binaries")
    print("=" * 60)
    print(f"  esptool: {ESPTOOL}")
    print(f"  sketch:  {SKETCH_DIR}")
    print(f"  core:    {CORE_DIR}")

    # Verify all files exist
    for addr, path in FLASH_ITEMS:
        if not os.path.exists(path):
            print(f"\nERROR: Missing file: {path}")
            print("Compile first with arduino-cli compile")
            return 1

    for base_address, filepath in FLASH_ITEMS:
        file_size = os.path.getsize(filepath)
        filename = os.path.basename(filepath)

        if file_size <= CHUNK_SIZE:
            print(f"\n[{filename}] 0x{base_address:x} ({file_size} bytes) - single write")

            for attempt in range(MAX_RETRIES):
                port = wait_for_port()
                if not port:
                    print(f"  No port found! Plug in device or press RESET.")
                    time.sleep(RETRY_DELAY)
                    continue

                ok, err = flash_file(port, base_address, filepath)
                if ok:
                    print(f"  OK")
                    break
                else:
                    print(f"  FAIL ({err}) - retry {attempt+1}/{MAX_RETRIES}")
                    time.sleep(RETRY_DELAY)
            else:
                print(f"  FATAL: Could not flash {filename}")
                return 1
        else:
            num_chunks = (file_size + CHUNK_SIZE - 1) // CHUNK_SIZE
            print(f"\n[{filename}] 0x{base_address:x} ({file_size} bytes) - {num_chunks} chunks of {CHUNK_SIZE//1024}KB")

            with open(filepath, "rb") as f:
                full_data = f.read()

            offset = 0
            chunk_num = 0
            while offset < file_size:
                chunk_num += 1
                chunk_len = min(CHUNK_SIZE, file_size - offset)
                chunk_addr = base_address + offset
                pct = offset * 100 // file_size

                print(f"  Chunk {chunk_num}/{num_chunks}: 0x{chunk_addr:x} ({chunk_len} bytes, {pct}%)", end="", flush=True)

                tmp_path = tempfile.mktemp(suffix=".bin")
                with open(tmp_path, "wb") as tmp:
                    tmp.write(full_data[offset:offset + chunk_len])

                success = False
                for attempt in range(MAX_RETRIES):
                    port = wait_for_port()
                    if not port:
                        print(f" [no port]", end="", flush=True)
                        time.sleep(RETRY_DELAY)
                        continue

                    ok, err = flash_file(port, chunk_addr, tmp_path)
                    if ok:
                        print(f" OK")
                        success = True
                        break
                    else:
                        print(f" [fail:{err[:30]}]", end="", flush=True)
                        time.sleep(RETRY_DELAY)

                os.unlink(tmp_path)

                if not success:
                    print(f"\n  FATAL: Chunk at 0x{chunk_addr:x} failed after {MAX_RETRIES} retries")
                    return 1

                offset += chunk_len
                time.sleep(1)

    print(f"\n{'=' * 60}")
    print(f"  FLASH COMPLETE!")
    print(f"  Press RESET on the board to boot the new firmware.")
    print(f"{'=' * 60}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
