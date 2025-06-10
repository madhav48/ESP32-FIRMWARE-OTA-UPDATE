const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });

const fs = require('fs');
const crypto = require('crypto');
const logger = require('../services/logger');

function signFirmware() {
  try {
    logger.info('Signing firmware and generating checksum...');

    // Resolve paths using environment variables
    const keysDir = process.env.KEYS_DIR.replace('%USERPROFILE%', process.env.USERPROFILE);
    const privateKeyPath = path.join(keysDir, process.env.PRIVATE_KEY);
    const firmwarePath = process.env.FIRMWARE_PATH;
    const signaturePath = process.env.SIGNATURE_PATH;

    if (!firmwarePath || !privateKeyPath || !signaturePath) {
      throw new Error('Missing required env vars: FIRMWARE_PATH, PRIVATE_KEY, or SIGNATURE_PATH');
    }

    if (!fs.existsSync(firmwarePath)) throw new Error(`Firmware not found: ${firmwarePath}`);
    if (!fs.existsSync(privateKeyPath)) throw new Error(`Private key not found: ${privateKeyPath}`);

    // Load firmware and private key
    const firmware = fs.readFileSync(firmwarePath);
    const privateKeyPem = fs.readFileSync(privateKeyPath, 'utf-8');

    // Sign the firmware using SHA256
    const signer = crypto.createSign('RSA-SHA256');
    signer.update(firmware);
    signer.end();
    const signatureBuffer = signer.sign(privateKeyPem);

    // Write signature as raw binary
    fs.writeFileSync(signaturePath, signatureBuffer);
    logger.success(`Signature (binary) written to ${signaturePath} (${signatureBuffer.length} bytes)`);

    // Compute and log checksum
    const checksumHex = crypto.createHash('sha256').update(firmware).digest('hex');
    logger.success(`Checksum (SHA-256): ${checksumHex}`);

    return {
      signature: signatureBuffer,
      checksum: checksumHex
    };
  } catch (err) {
    logger.error('Failed to sign firmware: ' + err.message);
    return null;
  }
}

module.exports = { signFirmware };
