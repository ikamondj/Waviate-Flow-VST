# gcp_handler.py
import functions_framework
from dispatcher import handle
import json

@functions_framework.http
def http_entrypoint(request):
    headers = {k.lower(): v for k, v in request.headers.items()}
    ck = {k: v for k, v in request.cookies.items()}
    body = request.get_data(as_text=True) or "{}"
    result = handle(body, headers=headers, cookies=ck)
    status = result.pop("status", 200) if isinstance(result, dict) else 200
    return (json.dumps(result), status, {"content-type": "application/json"})
