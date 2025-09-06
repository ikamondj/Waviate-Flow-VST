import json
from dispatcher import handle

def lambda_handler(event, context):
    # For API Gateway, body comes in as a JSON string
    body = event.get("body", "{}")
    result = handle(body)
    return {
        "statusCode": 200,
        "headers": {"Content-Type": "application/json"},
        "body": json.dumps(result)
    }
