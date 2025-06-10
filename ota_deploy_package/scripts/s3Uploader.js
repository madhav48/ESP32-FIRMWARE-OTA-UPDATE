const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });
const logger = require('../services/logger');


const AWS = require('aws-sdk');
const fs = require('fs');

const s3 = new AWS.S3({ region: process.env.AWS_REGION });

async function uploadFirmware(filePath, key) {
  const file = fs.readFileSync(filePath);
  const res = await s3.upload({
    Bucket: process.env.S3_BUCKET,
    Key: key,
    Body: file,
    ACL: 'public-read'
  }).promise();
  logger.success(`Uploaded to S3: ${res.Location}`);
  return res.Location;
}

module.exports = { uploadFirmware };
