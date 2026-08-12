# ROM Loader Architecture

## Partition Layout

```
0x01000   Bootloader           ~30KB
0x09000   Partition Table
0x0A000   NVS                  20KB
0x0F000   PHY                  4KB
0x10000   Factory (Loader)     696KB     ← persistent, never overwritten
0xBE000   OTA Data             8KB       ← boot selection
0xC0000   OTA_0 (ROM slot)     3,328KB   ← loader writes ROM here
0x400000  ───── 4MB end ──────
```

App partitions must be 0x10000 (64KB) aligned.

## Boot Flow

```
Power/Reset
    │
    ▼
Bootloader reads OTA Data partition
    │
    ├── OTA Data → factory ──→ Run LOADER
    │                              │
    │                    User selects ROM
    │                              │
    │                    Extract app from merged bin
    │                    Write to ota_0 via esp_ota_write()
    │                    esp_ota_set_boot_partition(ota_0)
    │                    esp_restart()
    │                              │
    └── OTA Data → ota_0 ──→ Run ROM
                                   │
                          app_main() first line:
                          return_to_loader_setup()
                          → sets OTA Data back to factory
                          → ROM continues running normally
                                   │
                          Any reset/restart
                                   │
                                   ▼
                          Bootloader → factory → LOADER
```

The ROM's first action redirects the next boot to the Loader. This means
**any** reboot (reset button, power-cycle, crash, `esp_restart()`) returns
to the Loader.

## Return-to-Loader Header

```c
#include "return_to_loader.h"

void app_main(void) {
    return_to_loader_setup();  // must be first line
    // ... normal init ...
}
```

The function checks if the running partition is ota_0. If so, it writes
`factory` to the OTA Data partition via `esp_ota_set_boot_partition()`.
If running from factory (e.g. direct esptool flash), it's a no-op.

## ROM File Format

ROM files on the TF card are **ESP32 merged flash images** (full flash dump
from address 0x0). All known ROMs share the same structure:

```
0x00000   0xFF padding (4KB)
0x01000   Bootloader image (magic 0xE9)     ← skipped by loader
0x08000   Partition table (magic 0xAA50)    ← skipped by loader
0x10000   App image (magic 0xE9)            ← extracted and written to ota_0
...       Data partitions (if any)          ← skipped
```

The Loader detects the format by checking magic byte:
- Offset 0 = 0xE9 → app-only image, start from 0
- Offset 0x10000 = 0xE9 → merged image, start from 0x10000

### App Image Size Calculation

The Loader parses the ESP-IDF image header to calculate exact size:

```
Header (24 bytes):
  magic(1) segment_count(1) ... hash_appended(1 at offset 23)

For each segment:
  load_addr(4) + data_len(4) + data(data_len)

After last segment:
  pad to 16-byte boundary
  checksum (1 byte)
  pad to 16-byte boundary
  SHA-256 hash (32 bytes, if hash_appended)
```

This ensures only the app image is written — trailing data partitions in the
merged bin are excluded.

## Skip-Write Optimization

The Loader tracks which ROM is currently in ota_0 using NVS:

- **After successful flash**: saves ROM name + file_size to NVS namespace `loader`
- **On boot**: reads NVS + verifies ota_0 first byte is 0xE9
- **When user selects ROM**: if name + file_size match NVS record → skip write,
  just set boot partition and restart

This means re-selecting the same ROM causes **zero flash wear**.

The ROM list shows `>` prefix for the currently-loaded ROM.

## Burning Interface

Always available regardless of flash state:

1. **ESP32 ROM Bootloader**: In mask ROM, runs before any flash code
2. **GD32 USB-UART Bridge**: Independent MCU firmware, not affected by ESP32 flash
3. **esptool**: Communicates with ROM bootloader via UART, before app code runs

The Loader only writes to ota_0 — bootloader and partition table are never
touched. Even if ota_0 write fails mid-way, the Loader in factory remains intact.

## OTA Write API (for reference)

```c
const esp_partition_t *part = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);

esp_ota_handle_t handle;
esp_ota_begin(part, image_size, &handle);

// Write in chunks
while (remaining > 0) {
    esp_ota_write(handle, buf, chunk_size);
}

esp_ota_end(handle);                // validates image hash
esp_ota_set_boot_partition(part);   // set next boot to ota_0
esp_restart();
```

## Existing ROM Compatibility

| ROM | App Size | No extra partitions? | Fits ota_0? |
|-----|----------|---------------------|-------------|
| pico-8.bin | 1.3MB | Yes | **Fully compatible** |
| firmware.bin (MicroPython) | 1.7MB | Needs FAT (vfs/cfg) | Partial |
| retro-go.bin | 1.1MB | Needs ota_1 (retro-core) | Needs dual-slot |
| sketch_jul4a | 0.3MB | Needs SPIFFS | Add spiffs partition |

ROMs that need data partitions not in the Loader's partition table will fail
when trying to mount them. Add the needed partitions to `partitions.csv` and
adjust ota_0 size accordingly.
