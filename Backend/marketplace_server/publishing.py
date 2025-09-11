import sceneValidator
import db
def create_entry(data: dict = None):
    if not data:
        return {"error": "Invalid data", "code": 400}
    if "json" not in data.keys():
        return {"error": "Invalid JSON payload", "code": 400}
    if "oauth" not in data.keys():
        return {"error": "Missing OAuth token", "code": 401}
    if not sceneValidator.validate_scene_json(data["json"]):
        return {"error": "Invalid scene JSON", "code": 500}

    # db.execute_query("INSERT INTO marketplace_content (userId, json) VALUES (?, ?)", (data["oauth"]["userId"], data["json"]))

    return {"error": "not implemented", "code": 501}
    return {"message": "Successfully Created Entry"}

def update_entry( data: dict = None):
    if not data:
        return {"error": "Invalid data", "code": 400}
    if "json" not in data.keys():
        return {"error": "Invalid JSON payload", "code": 400}
    if "oauth" not in data.keys():
        return {"error": "Missing OAuth token", "code": 401}
    if "entryId" not in data.keys():
        return {"error": "Missing entryId", "code": 400}

    return {"message": "not implemented"}

def delete_entry(entry_id: str = None):
    if not entry_id:
        return {"error": "Missing entryId", "code": 400}
    return {"message": "not implemented"}

def get_creator_dashboard(user_id: str = None):
    if not user_id:
        return {"error": "Missing userId", "code": 400}
    return {"message": "not implemented"}
