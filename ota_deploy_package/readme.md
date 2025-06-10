
# ESP32 OTA Backend Deployment

This Node.js package provides a complete backend pipeline to support **Over-the-Air (OTA)** firmware updates for ESP32 devices. It handles building the firmware, signing, uploading to AWS S3, storing metadata in PostgreSQL, and notifying clients through AWS IoT Core (MQTT broker). The tool is built to be used by developers with minimal setup and one-line deployment.

---

## Project Structure

```
ota_deploy_package
│   .env
│   .gitignore
│   cli.js                   # Entry point (CLI handler)
│   package-lock.json
│   package.json
│   readme.md
│
├───certs
│       rds-global-bundle.pem
│
├───scripts
│       buildPlatformIO.js   # Builds firmware using PlatformIO
│       db.js                # Handles PostgreSQL operations
│       deploy.js            # Main pipeline: Build → Sign → Upload → Store → Notify
│       lambda.js            # Lambda invocation utilities
│       s3Uploader.js        # Handles AWS S3 uploads
│       signer.js            # Signs firmware & generates checksums
│
└───services
        logger.js            # Logging utility
```

---

## Features

- CLI-based deployment pipeline
- Builds ESP-IDF firmware using PlatformIO
- Automatically signs and verifies firmware using OpenSSL
- Uploads firmware and signature to AWS S3
- Stores versioned firmware metadata in PostgreSQL
- Sends firmware update notification to ESP32 devices via AWS IoT Core
- Supports secure updates with version conflict checks

---

## Developer Setup

### Prerequisites

- [PlatformIO](https://platformio.org/install) (For building ESP-IDF firmware)
- [OpenSSL](https://slproweb.com/products/Win32OpenSSL.html) (Ensure PATH is configured)
- Node.js (v18 or above)


### ESP-IDF Project Setup

1. Create a new ESP-IDF project using PlatformIO.
2. Rename `main.c` to `main.cpp`.
3. Ensure to wrap `main()` and other C functions with:
   ```cpp
   extern "C" {
       // Your C code
   }
   ```
4. Create a simple test firmware.

---

### Key Generation (One-time setup)

```bash
set KEY_DIR=%USERPROFILE%\.firmware_keys
if not exist "%KEY_DIR%" mkdir "%KEY_DIR%"
openssl genpkey -algorithm RSA -out "%KEY_DIR%\private.pem" -pkeyopt rsa_keygen_bits:2048
openssl rsa -pubout -in "%KEY_DIR%\private.pem" -out "%KEY_DIR%\public.pem"
type %USERPROFILE%\.firmware_keys\public.pem
```

> **Note:** Copy the public key (printed in the terminal) as this will be required while verifying the signature on the ESP32.

---

### Node.js Setup

1. Clone this repository.
2. Configure the `.env` file with your paths, AWS, and DB credentials.
3. Install dependencies:

```bash
npm install dotenv node-fetch pg aws-sdk child_process crypto fs-extra
```

---

## Deployment Usage

```bash
node cli.js deploy --version="1.0.0" --changelog="Initial Release"
```

If the version is not provided, it will auto-generate one based on the latest in the database. The CLI tool (`cli.js`) internally calls `deploy.js`, which executes the following:

### Deployment Steps

1. **Version Management**:  
   - If a version is provided, it checks for clashes in the DB.
   - If not provided, it auto-generates a patch version.

2. **Build**:  
   - Uses PlatformIO to build the firmware.
   - Output: `.bin` firmware file.

3. **Sign and Checksum**:  
   - Signs the firmware using RSA (OpenSSL).
   - Generates a checksum.

4. **Upload**:  
   - Uploads firmware and signature to AWS S3.
   - Configurable using `.env` for S3 bucket name and credentials.

5. **Database Logging**:  
   - Inserts firmware metadata (version, changelog, URL, checksum) into a PostgreSQL table.

6. **Notify Devices**:  
   - Triggers an AWS Lambda function.
   - Lambda function sends `/firmware_update` MQTT message via AWS IoT Core to subscribed ESP32 devices.

---

##  AWS Setup

### S3 Bucket

- Create a new S3 bucket.
- Enable ACLs.
- Create an IAM user with programmatic access.
- Generate and store Access Key and Secret in `.env`.

### PostgreSQL

- Create a new table for firmware metadata.
- Make DB publicly accessible.
- Modify inbound rules to allow connections (e.g., 0.0.0.0/0).
- Store credentials in `.env`.

### AWS Lambda

- Create a Lambda function to trigger OTA update.
- Assign it a role with permissions for IoT publish.
- Ensure correct policy for MQTT topic publishing.
- Link with this tool via `lambda.js`.

---


## Conclusion

With a single CLI command, this package automates:

- Firmware build
- Version control
- Signature and checksum creation
- Secure S3 upload
- Metadata storage in DB
- MQTT notification to all ESP32 devices via AWS IoT

---
