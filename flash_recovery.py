#!/usr/bin/env python3
"""Try to flash ESP32-S3 using tiny flash blocks to work around CH343P corruption."""
import struct
import sys
import os
import time

from esptool import detect_chip, FatalError

PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem59090523481'

print(f"Connecting to {PORT}...")
esp = detect_chip(port=PORT, baud=115200, connect_mode="default-reset")
print(f"Chip: {esp.CHIP_NAME}")
esp.IS_STUB = False

BUILD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build')

files = [
    (0x0000, 'bootloader/bootloader.bin'),
    (0x8000, 'partition_table/partition-table.bin'),
    (0x10000, 'studybud.bin'),
]

BLOCK_SIZE = 64  # bytes per flash block

# Init flash SPI
print("Attaching SPI flash...")
esp.flash_spi_attach(0)

# Get actual flash size
try:
    flash_size = esp.flash_size_bytes()
except:
    flash_size = 16 * 1024 * 1024  # 16 MB default
print(f"Flash size: {flash_size} ({flash_size / 1024 / 1024:.0f} MB)")

print("Setting flash parameters...")
esp.flash_set_parameters(flash_size)

for offset, filepath in files:
    fpath = os.path.join(BUILD_DIR, filepath)
    if not os.path.exists(fpath):
        print(f"ERROR: {fpath} not found!")
        continue

    with open(fpath, 'rb') as f:
        data = f.read()

    size = len(data)
    num_blocks = (size + BLOCK_SIZE - 1) // BLOCK_SIZE

    print(f"\n{filepath} ({size} bytes, {num_blocks}×{BLOCK_SIZE}B @ 0x{offset:06x})")

    # Full chip erase first (only needed once)
    if offset == 0x0000:
        print("  Full chip erase...")
        try:
            esp.erase_flash()
            print("  Full erase OK")
        except FatalError as e:
            print(f"  Full erase failed: {e}")

    # flash_begin with erase_size=0 (skip erase, already done)
    params = struct.pack("<IIII", 0, num_blocks, BLOCK_SIZE, offset)
    if esp.CHIP_NAME not in ("ESP32", "ESP8266"):
        params += struct.pack("<I", 0)  # encrypted = 0
    try:
        esp.check_command("flash begin", esp.ESP_CMDS["FLASH_BEGIN"], params, timeout=120000)
        print(f"  FLASH_BEGIN OK")
    except FatalError as e:
        print(f"  FLASH_BEGIN failed: {e}")
        esp.hard_reset()
        esp._port.close()
        sys.exit(1)

    for seq in range(num_blocks):
        chunk = data[seq * BLOCK_SIZE : (seq + 1) * BLOCK_SIZE]
        if len(chunk) < BLOCK_SIZE:
            chunk = chunk + b'\xff' * (BLOCK_SIZE - len(chunk))

        ok = False
        for attempt in range(5):
            try:
                esp.check_command(
                    f"write block {seq}",
                    esp.ESP_CMDS["FLASH_DATA"],
                    struct.pack("<IIII", len(chunk), seq, 0, 0) + chunk,
                    esp.checksum(chunk),
                    timeout=30000,
                )
                ok = True
                break
            except FatalError as e:
                if attempt < 4:
                    print(f"\n  Block {seq} failed ({attempt+1}/5): {str(e)[:60]}")
                    time.sleep(0.02)
                else:
                    print(f"\n  Block {seq} FAILED after 5 attempts")
                    pass

        if not ok:
            print(f"  Aborting at block {seq}")
            break

        if seq % 50 == 0 or seq == num_blocks - 1:
            sys.stdout.write(f"\r  {seq+1}/{num_blocks}")
            sys.stdout.flush()
    print()

    # flash_finish (no reboot, stay in flash mode)
    esp.check_command("flash finish", esp.ESP_CMDS["FLASH_END"], struct.pack("<I", 1), timeout=10000)
    print(f"  Done: {filepath}")

print("\nAll done!")
esp.hard_reset()
esp._port.close()
