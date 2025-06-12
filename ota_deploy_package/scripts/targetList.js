const fs = require('fs');
const path = require('path');

function getTargetMACsFromFile(filePath) {
  try {
    const absPath = path.resolve(filePath);
    const raw = fs.readFileSync(absPath, 'utf-8');
    const macList = JSON.parse(raw);

    if (!Array.isArray(macList)) {
      throw new Error('Target file should contain a JSON array of MAC addresses.');
    }

    return macList;
  } catch (err) {
    throw new Error(`Failed to load MAC address list: ${err.message}`);
  }
}

module.exports = { getTargetMACsFromFile };
