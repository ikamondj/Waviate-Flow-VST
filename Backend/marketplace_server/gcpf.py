import functions_framework
import json
from dispatcher import handle

@functions_framework.http
def dispatcher_http(request):
    try:
        req_json = request.get_json(silent=True) or {}
        body = json.dumps(req_json)
        result = handle(body)
        return (json.dumps(result), 200, {"Content-Type": "application/json"})
    except Exception as e:
        return (json.dumps({"error": str(e)}), 400, {"Content-Type": "application/json"})
