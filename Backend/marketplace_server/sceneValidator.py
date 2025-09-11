import json_validator

def validate_scene_json(json_str: str) -> bool:
    """Validate a scene JSON string using the C++ validator."""
    return json_validator.validate_json(json_str)