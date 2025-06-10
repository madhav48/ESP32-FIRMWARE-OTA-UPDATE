const AWS = require('aws-sdk');
const logger = require('../services/logger');
const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });

const lambda = new AWS.Lambda({
  region: process.env.AWS_REGION 
});

async function triggerLambda(metadata) {
  try {
    const payload = {
      action: "ota_update",
      data: {
        version: metadata.version,
        firmware_url: metadata.firmwareUrl,
        signature_url: metadata.signatureUrl,
        checksum: metadata.checksum
      }
    };

    const result = await lambda.invoke({
      FunctionName: process.env.LAMBDA_FUNCTION_NAME, 
      Payload: JSON.stringify(payload),
    }).promise();

    logger.success('Lambda triggered successfully!');
    // logger.info(`Lambda response: ${result.StatusCode} - ${result.Payload}`);

    const recv_payload = JSON.parse(result.Payload);

    if (result.StatusCode === 200) {
      const message = JSON.parse(recv_payload.body);
      logger.success(`Lambda Success: ${message}`);
    } else {
      logger.error(`Lambda Error: StatusCode ${result.StatusCode} - ${recv_payload}`);
    }

  } catch (err) {
    logger.error(`Failed to trigger Lambda: ${err.message}`);
  }
}

module.exports = { triggerLambda };
