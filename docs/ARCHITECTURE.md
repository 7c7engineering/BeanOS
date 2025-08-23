# Architectural Design

# Memory layout
## internal FLASH

The internal flash is partitioned as follows (see `partitions.csv`):

| Name      | Type  | SubType | Offset   | Size (hex) | Size (KB/MB)   | Description                |
|-----------|-------|---------|----------|------------|----------------|----------------------------|
| nvs       | data  | nvs     | 0x9000   | 0x5000     | 20 KB          | Non-volatile storage       |
| otadata   | data  | ota     | 0xe000   | 0x2000     | 8 KB           | OTA data partition         |
| app0      | app   | ota_0   | 0x10000  | 0x1e0000   | 1920 KB        | Main application           |
| spiffs    | data  | spiffs  | 0x1f0000 | 0x200000   | 2 MB           | SPIFFS filesystem          |
| coredump  | data  | coredump| 0x3f0000 | 0x10000    | 64 KB          | Core dump storage          |

Each partition serves a specific purpose:

- **nvs**: Used for non-volatile storage (key-value pairs, settings, etc.).
- **otadata**: Stores OTA (Over-The-Air) update metadata. (not used)
- **app0**: The main application firmware image (OTA slot 0).
- **spiffs**: SPIFFS filesystem for persistent file storage. (settings, logs, etc. no flightlogs!)
- **coredump**: Reserved for storing core dumps after crashes for debugging.
