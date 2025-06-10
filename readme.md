
# ESP32 Firmware OTA Update 

This repository contains the complete infrastructure for the Over-the-Air (OTA) firmware update system for ESP32 device. It is divided into two primary sections: the `ota_deploy_package` (backend for managing and deploying firmware) and the `esp_firmware_projects` (the device-side firmware, including a bootloader and the main application).

## Directory Tree
```
.
├── ota_deploy_package/
│   ├── .env
│   ├── cli.js                # Entry point (CLI handler)
│   ├── package.json
│   ├── readme.md
│   ├── certs/
│   │   └── rds-global-bundle.pem
│   └── scripts/
│       ├── buildPlatformIO.js
│       ├── db.js
│       ├── deploy.js         # Main deployment pipeline
│       ├── lambda.js
│       ├── s3Uploader.js
│       └── signer.js
└── esp_firmware_projects/
    ├── main_app/             # Main application firmware
    │   ├── components/
    │   │   ├── Common/
    │   │   └── OTAUpdateChecker/
    │   ├── src/
    │   └── partitions.csv
    └── ota_loader/           # Dedicated OTA bootloader firmware
        ├── components/
        │   ├── Common/
        │   └── OTAUpdateManager/
        ├── src/
        └── partitions.csv
```

## I. ota_deploy_package (Backend Deployment)

This Node.js package provides a complete backend pipeline to support Over-the-Air (OTA) firmware updates for ESP32 devices. It handles building, signing, uploading to AWS S3, storing metadata in PostgreSQL, and notifying clients through AWS IoT Core.

### Key Features

- **CLI-based Deployment**: Single command deployment.
- **Firmware Building**: Builds ESP-IDF firmware using PlatformIO.
- **Secure Signing**: Automatically signs and verifies firmware using OpenSSL (RSA).
- **Cloud Storage**: Uploads firmware binaries and signatures to AWS S3.
- **Metadata Management**: Stores versioned firmware metadata (version, changelog, URL, checksum) in PostgreSQL.
- **Device Notification**: Sends firmware update notifications to ESP32 devices via AWS IoT Core (MQTT broker), triggered by an AWS Lambda function.
- **Version Control**: Supports secure updates with automatic version management and conflict checks.

### Developer Setup (Backend)

#### Prerequisites:

- PlatformIO
- OpenSSL (ensure PATH is configured)
- Node.js (v18 or above)

#### Key Generation:

One-time setup to generate RSA private and public keys.

```bash
set KEY_DIR=%USERPROFILE%\.firmware_keys
if not exist "%KEY_DIR%" mkdir "%KEY_DIR%"
openssl genpkey -algorithm RSA -out "%KEY_DIR%\private.pem" -pkeyopt rsa_keygen_bits:2048
openssl rsa -pubout -in "%KEY_DIR%\private.pem" -out "%KEY_DIR%\public.pem"
type %USERPROFILE%\.firmware_keys\public.pem
```

> Note: The public key printed here is required for signature verification on the ESP32.

#### Node.js Setup:

- Clone the `ota_deploy_package`.
- Configure the `.env` file with your paths, AWS, and DB credentials.
- Install dependencies:

```bash
npm install dotenv node-fetch pg aws-sdk child_process crypto fs-extra
```

### Deployment Usage (Backend)

To deploy a new firmware version:

```bash
cd ota_deploy_package
node cli.js deploy --version="1.0.0" --changelog="Initial Release"
```

(If `--version` is omitted, a patch version is auto-generated based on the latest in the database.)

### AWS Setup (Backend)

- **S3 Bucket**: Create an S3 bucket with ACLs enabled and an IAM user with programmatic access.
- **PostgreSQL**: Set up a PostgreSQL database and create a table for firmware metadata.
- **AWS Lambda**: Create a Lambda function with permissions to publish to AWS IoT Core MQTT topics.

---

## II. esp_firmware_projects (ESP32 Device Firmware)

This section contains the firmware designed for the ESP32 devices, implementing a robust and secure OTA update mechanism using a dual-project approach.

### Architecture Overview

The system is structured into two separate projects for modularity and security:

- **ota_loader**: A minimal, secure bootloader that runs first. It's responsible for checking for and installing updates.
- **main_app**: The actual application firmware that handles the device's primary logic and triggers the `ota_loader` when an update is available.

### main_app

- Runs the device's application logic.
- Subscribes to `/firmware_update` MQTT topic.
- Parses firmware metadata (version, URL, signature) from MQTT JSON payload.
- Uses the `OTAUpdateChecker` component to:
  - Compare current vs. target firmware versions.
  - Switch the boot partition to trigger `ota_loader`.
  - Initiate a system reboot.

#### Custom Library: OTAUpdateChecker

- Purpose: Performs lightweight checks to determine if an update is needed.
- Responsibilities: Version comparison, triggering reboot, and switching boot partition to `ota_loader`.

### ota_loader

- Dedicated bootloader responsible for downloading, verifying, and flashing new firmware.

#### Key Features:

- Executes from the factory partition.
- Downloads firmware and signature from a remote server (e.g., AWS S3).
- Performs SHA256 hash and RSA digital signature validation.
- Writes new firmware to the OTA partition using `esp_ota_ops`.
- Updates the boot partition and stores version info using NVS.
- Reboots into the new main application.

#### Custom Library: OTAUpdateManager

- Purpose: Core logic for downloading, verifying, and flashing firmware updates.
- Responsibilities:
  - NVSStorageHandler: Manages persistent version tracking.
  - HTTPDownloader: Downloads binaries and signatures.
  - SignatureVerifier: Validates firmware integrity (SHA256, RSA).
  - FirmwareFlasher: Writes firmware to OTA partition.
  - OTAUpdateManager: Orchestrates the entire OTA workflow.

### OTA Workflow

1. **Bootloader Startup** (factory partition):
    - Checks for new firmware.
    - If available, downloads, verifies, and flashes it to `ota_0` partition.

2. **Main Application Execution** (`ota_0`):
    - Connects to Wi-Fi/MQTT.
    - Subscribes to update topic.
    - Compares versions.
    - Triggers bootloader if update required.

3. **Bootloader Flashing**:
    - Downloads new OTA image.
    - Validates and flashes it.
    - Sets boot partition and reboots device.

### Partition Table

The common partition table ensures proper memory allocation:

```csv
# Name,     Type, SubType, Offset,     Size
nvs,        data, nvs,     0x9000,     0x5000
otadata,    data, ota,     0xE000,     0x2000
factory,    app,  factory, 0x10000,    0x150000
ota_0,      app,  ota_0,   0x160000,   0xD0000
spiffs,     data, spiffs,  0x230000,   0x1D0000
```

---
