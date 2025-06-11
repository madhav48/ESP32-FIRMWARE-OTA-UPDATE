const { S3Client, GetObjectCommand } = require("@aws-sdk/client-s3");
const s3 = new S3Client({ region: process.env.AWS_REGION });

exports.handler = async (event) => {
    // Extract details from API Gateway event...
    const firmwareVersion = event.pathParameters.version; 

    const bucketName = process.env.FIRMWARE_S3_BUCKET;
    const objectKey = `firmwares/${firmwareVersion}.bin`;
    const rangeHeader = event.headers ? (event.headers.range || event.headers.Range) : null; // Handle potential missing headers object

    let s3Params = {
        Bucket: bucketName,
        Key: objectKey,
    };

    if (rangeHeader) {
        s3Params.Range = rangeHeader;
    }

    try {
        const command = new GetObjectCommand(s3Params);
        const s3Response = await s3.send(command);

        const bodyBuffer = await s3Response.Body.transformToByteArray();
        const base64Body = Buffer.from(bodyBuffer).toString('base64');

        let headers = {
            'Content-Type': 'application/octet-stream',
            'Accept-Ranges': 'bytes',
        };

        let statusCode = 200;
        if (rangeHeader && s3Response.ContentRange) {
            statusCode = 206; // Partial Content
            headers['Content-Range'] = s3Response.ContentRange;
            headers['Content-Length'] = s3Response.ContentLength; // Content-Length for the chunk
        } else {
            headers['Content-Length'] = s3Response.ContentLength;
        }

        return {
            statusCode: statusCode,
            headers: headers,
            body: base64Body,
            isBase64Encoded: true,
        };

    } catch (error) {
        console.error("Error fetching firmware from S3:", error);
        if (error.name === 'NoSuchKey') {
            return {
                statusCode: 404,
                body: JSON.stringify({ message: 'Firmware not found' }),
                headers: { 'Content-Type': 'application/json' }
            };
        }
        return {
            statusCode: 500,
            body: JSON.stringify({ error_message: 'Internal Server Error' }),
            headers: { 'Content-Type': 'application/json' }
        };
    }
};