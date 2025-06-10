const { execSync } = require('child_process');
const fs = require('fs');
const logger = require('../services/logger');
const path = require('path');


function ensureFirmwareFolderExists() {
  const firmwareDir = path.resolve('firmware');
  if (!fs.existsSync(firmwareDir)) {
    fs.mkdirSync(firmwareDir, { recursive: true });
  }
}


async function buildFirmware() {
  logger.info('Building firmware...');
  execSync('pio run', { stdio: 'inherit' });
  ensureFirmwareFolderExists();
  fs.copyFileSync('.pio/build/esp32dev/firmware.bin', 'firmware/firmware.bin');
}

module.exports = { buildFirmware };
