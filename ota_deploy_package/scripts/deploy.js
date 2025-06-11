const { buildFirmware } = require('./buildPlatformIO');
const { signFirmware } = require('./signer');
const { uploadFirmware } = require('./s3Uploader');
const { checkVersionExists, saveMetadata, getLatestVersion } = require('./db');
const { triggerLambda } = require('./lambda');
const logger = require('../services/logger');
const fs = require('fs');
const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });


async function deployPipeline({ firmwareVersion, changelog, deployedBy }) {
  try {
    logger.info('Starting OTA update deployment...');

    // Version check..
    if (!firmwareVersion) {
      firmwareVersion = await getLatestVersion();
      logger.info(`Auto-assigned firmware version: ${firmwareVersion}`);
    }

    const exists = await checkVersionExists(firmwareVersion);
    if (exists) {
      logger.error(`Version ${firmwareVersion} already exists. Choose a new version.`);
      return;
    }

    // Build firmware
    await buildFirmware();

    // Sign firmware
    const { signature, checksum } = await signFirmware();

    // Upload firmware
    const firmwarePath = path.resolve(process.env.FIRMWARE_PATH);
    const firmwareKey = `firmwares/${firmwareVersion}.bin`;
    const firmwareUrl = await uploadFirmware(firmwarePath, firmwareKey);

    const signaturePath = path.resolve(process.env.SIGNATURE_PATH);
    const sigKey = `signatures/${firmwareVersion}.sig`;
    const sigUrl = await uploadFirmware(signaturePath, sigKey);

    // Save metadata to DB
    await saveMetadata({
      version: firmwareVersion,
      changelog,
      deployed_by: deployedBy,
      firmware_url: firmwareUrl,
      signature_url: sigUrl,
      checksum
    });

    // API URL..
    const apiGatewayFirmwareDownloadUrl = `${process.env.API_GATEWAY_BASE_URL}/firmware/${firmwareVersion}`;

    // Trigger Lambda
    await triggerLambda({
      version: firmwareVersion,
      firmwareUrl: apiGatewayFirmwareDownloadUrl,
      signatureUrl: sigUrl,
      checksum: checksum
    });

    logger.success(`The firmware has been deployed successfully. Here are the details:
      - Version     : ${firmwareVersion}
      - Deployed By : ${deployedBy}
      - Changelog   : ${changelog}
      - Firmware URL: ${apiGatewayFirmwareDownloadUrl}
      - Signature URL: ${sigUrl}`);

    // logger.success('OTA update deployed successfully!');
  } catch (err) {
    logger.error(`Deployment failed: ${err.message}`);
  } finally {
    try {
      logger.info('Cleaning temporary files..');
      fs.rmSync('firmware', { recursive: true, force: true });
      logger.success('Removed temporary files...');
    } catch (e) {
      logger.error(`Failed to remove temporary files: ${e.message}`);
    }
  }
}

module.exports = { deployPipeline };
