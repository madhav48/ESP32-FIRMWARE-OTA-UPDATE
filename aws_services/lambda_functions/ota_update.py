import json
import boto3
import os

iot_data = boto3.client('iot-data', region_name=os.environ['AWS_REGION'])

def lambda_handler(event, context):
    # Extract data
    data = event.get('data', {})
    version = data.get('version', 'unknown-version')
    firmware_url = data.get('firmware_url', '')
    signature_url = data.get('signature_url', '')
    checksum = data.get('checksum', '')
    topic = data.get('topic', '')

    # Construct message
    message = {
        "version": version,
        "firmware_url": firmware_url,
        "signature_url": signature_url,
        "checksum": checksum
    }

    try:
        response = iot_data.publish(
            topic=topic,
            qos=1,
            payload=json.dumps(message),
            retain=True
        )
        return {
            'statusCode': 200,
            'body': json.dumps(f'OTA command published successfully to topic {topic}')
        }
    except Exception as e:
        print(f"Failed to publish MQTT message: {str(e)}")
        return {
            'statusCode': 500,
            'body': json.dumps('Failed to publish OTA command')
        }
