# aws_handler.py
from http import cookies as http_cookies
from dispatcher import handle
import json

def lambda_handler(event, context):
    headers = {k.lower(): v for k, v in (event.get("headers") or {}).items()}
    # Parse cookies
    raw_cookie = headers.get("cookie", "")
    jar = http_cookies.SimpleCookie()
    if raw_cookie:
        jar.load(raw_cookie)
    ck = {k: morsel.value for k, morsel in jar.items()}

    body = event.get("body") or "{}"
    # If body is base64-encoded (rare for JSON), handle event.get("isBase64Encoded")
    result = handle(body, headers=headers, cookies=ck)

    status = result.pop("status", 200) if isinstance(result, dict) else 200
    return {
        "statusCode": status,
        "headers": {"content-type": "application/json"},
        "body": json.dumps(result),
    }
