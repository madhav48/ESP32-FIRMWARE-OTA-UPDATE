
# ESP32 OTA Firmware Update 

A robust **Over-the-Air (OTA) firmware update system** for ESP32 devices, structured into two separate projects for modularity and security:

---

## ESP32 Firmware Projects

### 1. Bootloader: `ota_loader`

Acts as a minimal, secure loader similar to a BIOS. This runs **first** after power-up or reset, checks if an OTA update is triggered, verifies and flashes the firmware if available, and reboots into the updated main application.

> **Note:** This partition is **read-only** and remains untouched during updates.

### 2. Main Application: `main_app`

The actual working firmware, responsible for the device’s primary logic. It connects to Wi-Fi and MQTT, listens for update notifications, verifies version differences, and triggers the bootloader to install the new firmware when needed.

---

## Project Structure

### `main_app`

**Function**: Runs device logic, handles MQTT update trigger, and initiates the OTA update process.

**Key Features**:
- Subscribes to `firmware_update` topic via MQTT.
- Parses firmware metadata in JSON (`{ "version": "1.2.0", "url": "...", "sig": "..." }`).
- Uses `OTAUpdateChecker` to:
  - Compare current and target firmware versions.
  - Switch boot partition to OTA update.
  - Trigger system reboot.

**Directory Layout**:
```
main_app/
├── components/
│   ├── Common/
│   │   ├── include/Common/certificates.h
│   │   └── src/certificates.cpp
│   └── OTAUpdateChecker/
│       ├── include/OTAUpdateChecker/OTAUpdateChecker.h
│       └── src/OTAUpdateChecker.cpp
├── include/
├── lib/
├── src/
│   ├── main.cpp
│   └── CMakeLists.txt
├── partitions.csv
├── platformio.ini
```

---

### `ota_loader`

**Function**: Handles downloading, verifying, and flashing the new firmware.

**Key Features**:
- Executes from the `factory` partition at boot.
- Receives updates from a MQTT topic.
- Downloads firmware and signature from a remote server.
- Performs **SHA256 and RSA** signature validation.
- Writes firmware to OTA partition using `esp_ota_ops`.
- Updates boot partition and stores version info using **NVS**.
- Reboots into main application.

**Directory Layout**:
```
ota_loader/
├── components/
│   ├── Common/
│   │   ├── include/Common/certificates.h
│   │   └── src/certificates.cpp
│   └── OTAUpdateManager/
│       ├── include/OTAUpdateManager/
│       │   ├── FirmwareFlasher.h
│       │   ├── HTTPDownloader.h
│       │   ├── NVSStorageHandler.h
│       │   ├── OTAUpdateManager.h
│       │   └── SignatureVerifier.h
│       └── src/
│           ├── FirmwareFlasher.cpp
│           ├── HTTPDownloader.cpp
│           ├── NVSStorageHandler.cpp
│           ├── OTAUpdateManager.cpp
│           └── SignatureVerifier.cpp
├── src/
│   ├── main.cpp
│   └── CMakeLists.txt
├── partitions.csv
├── platformio.ini
```

---

## OTA Workflow

1. **Bootloader Startup (`factory` partition)**  
   - Checks for new firmware version.
   - If available, downloads and verifies it.
   - Flashes firmware to `ota_0` partition.

2. **Main Application Execution (`ota_0`)**  
   - Connects to Wi-Fi and MQTT.
   - Subscribes to the update topic.
   - Compares firmware version and triggers bootloader if necessary.

3. **Bootloader Flashing**  
   - Downloads new OTA image.
   - Validates and flashes it.
   - Sets boot partition and reboots.

---

## Partition Table

```
# Name,     Type, SubType, Offset,     Size
nvs,        data, nvs,     0x9000,     0x5000
otadata,    data, ota,     0xE000,     0x2000
factory,    app,  factory, 0x10000,    0x150000
ota_0,      app,  ota_0,   0x160000,   0xD0000  
spiffs,     data, spiffs,  0x230000,   0x1D0000   
```



## Custom Libraries

This architecture uses two custom-built libraries that encapsulate the OTA update process logic:

### 1. OTAUpdateManager (used in `ota_loader`)

Located in: `ota_loader/components/OTAUpdateManager/`

**Purpose**: Handles the core logic for downloading, verifying, and flashing firmware updates.

**Responsibilities**:
- **NVSStorageHandler**: Manages persistent version tracking and metadata using NVS storage.
- **HTTPDownloader**: Downloads firmware binaries and signatures from a remote server (e.g., AWS S3).
- **SignatureVerifier**: Validates the firmware integrity using SHA256 hash and RSA digital signature.
- **FirmwareFlasher**: Writes the new firmware binary to the OTA partition using ESP-IDF's `esp_ota_ops` API.
- **OTAUpdateManager**: Coordinates the above components and manages the complete OTA workflow.

---

### 2. OTAUpdateChecker (used in `main_app`)

Located in: `main_app/components/OTAUpdateChecker/`

**Purpose**: Performs lightweight checks within the main application to determine whether an update is required.

**Responsibilities**:
- Compares current firmware version with the received version from MQTT payload.
- Triggers a reboot and switches the boot partition to invoke the bootloader (`ota_loader`) if an update is necessary.
