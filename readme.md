
# ESP32 Firmware OTA Update 

This repository contains the complete infrastructure for the Over-the-Air (OTA) firmware update system for ESP32 device. It is divided into two primary sections: the `ota_deploy_package` (backend for managing and deploying firmware) and the `esp_firmware_project` (the device-side firmware).

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
    └── esp32_project/
        ├── components/
        │   ├── Common/
        │   └── OTAUpdateManager/  # Implements complete OTA logic
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
- **API URL Generation and Delivery**: After uploading firmware to S3, a downloadable API URL is generated using a predefined AWS API Gateway endpoint.
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
node cli.js deploy --version="1.0.0" --changelog="Initial Release" --target="targetDevices.json"
```

> If `--version` is omitted, a patch version is auto-generated based on the latest in the database.
For targeted delivery, pass the json file with MAC adress to `--target`. If ommited, firmware will be broadcasted to the fleet 

### AWS Setup (Backend)

- **S3 Bucket**: Create an S3 bucket with ACLs enabled and an IAM user with programmatic access.
- **PostgreSQL**: Set up a PostgreSQL database and create a table for firmware metadata.
- **AWS Lambda**: Create a Lambda function with permissions to publish to AWS IoT Core MQTT topics.

---

## II. esp_firmware_projects (ESP32 Device Firmware)

This section contains the firmware designed for the ESP32 device, implementing a robust and secure OTA update mechanism.


### esp32_project (main_app)

- Runs the device's application logic.
- Subscribes to `/firmware_update` & `/firmware_update/<MAC-ID>` MQTT topic.
- Parses firmware metadata (version, URL, signature) from MQTT JSON payload.
- Uses `OTAUpdateManager` to:
  - Compare current vs. target firmware versions.
  - Handle secure firmware download via HTTPS.
  - Stream firmware and write to OTA partition.
  - Verify SHA256 checksum and RSA signature.
  - Finalize OTA write and set the new partition as boot.
  - Store new version in NVS and reboot.


#### Custom Library: OTAUpdateManager

- Purpose: Core logic for downloading, verifying, and flashing firmware updates.
- Responsibilities:
  - NVSStorageHandler: Manages persistent version tracking.
  - HTTPDownloader: Downloads binaries and signatures.
  - SignatureVerifier: Validates firmware integrity (SHA256, RSA).
  - OTAUpdateManager: Orchestrates the entire OTA workflow.


### Partition Table

The common partition table ensures proper memory allocation:

```csv
# Name,     Type, SubType, Offset,     Size
nvs,        data, nvs,     0x9000,     0x5000
otadata,    data, ota,     0xE000,     0x2000
ota_0,      app,  ota_0,   0x10000,    0x1E0000
ota_1,      app,  ota_1,   0x1F0000,   0x1E0000
```

---

## Firmware Delivery Process

Once a new firmware version is built and uploaded to the AWS S3 bucket, a downloadable URL is generated using a predefined API Gateway endpoint. This URL provides secure and authenticated access to the firmware using an API key.

### Delivery Flow:

1. **Upload to S3**: The signed firmware `.bin` and its corresponding signature are uploaded to a protected S3 bucket.
2. **API Gateway Integration**: A versioned download URL is generated dynamically using the AWS API Gateway. This gateway triggers a Lambda function that delivers the firmware **in small chunks**, which is particularly helpful in environments with **poor or unstable connectivity**, ensuring continuity.
3. **MQTT Notification**: This download URL, along with metadata like version, changelog, and SHA256 checksum, is sent to all relevant devices via MQTT through AWS IoT Core.
4. **Secure Download**:
    - The ESP32 uses the `HttpDownloader` module to initiate a secure HTTPS connection to the URL.
    - Custom HTTP headers like `x-api-key` and `Accept: application/octet-stream` are added to ensure proper authentication and MIME handling.
    - The ESP32 reads the file in **streamed (chunked)** fashion using `esp_http_client_read`, avoiding memory overflow.
5. **Validation**:
    - After download, the firmware's SHA256 hash is verified.
    - The RSA digital signature is validated using the previously flashed public key.
6. **Flashing and Reboot**:
    - On successful verification, the firmware is written to the OTA partition.
    - The boot partition is updated, and the device reboots into the new firmware version.

> This architecture ensures firmware is delivered in a **secure** and **memory-efficient**  way.

---
