from entity.daily_random import DailyRandom
from db import get_db
import datetime

def list_entries(page: int = 1, limit: int = 50):
    return {"message": "not implemented"}

def search_entries(query: str = "", page: int = 1, limit: int = 50):
    return {"message": "not implemented"}

def get_entry(entry_id: str = None):
    return {"message": "not implemented"}

def list_entries_by_creator(creator_id: str = None):
    return {"message": "not implemented"}

def list_categories():
    return {"message": "not implemented"}

def list_new_entries():
    return {"message": "not implemented"}

def list_popular_entries():
    return {"message": "not implemented"}

def list_best_rated_entries():
    return {"message": "not implemented"}

def list_random_entries(minute: int = None):
    """
    Return the random entry list for the given minute (default: current minute of day).
    """
    if minute is None:
        now = datetime.datetime.utcnow()
        minute = now.hour * 60 + now.minute
    db = next(get_db())
    row = db.query(DailyRandom).filter_by(minute=minute).first()
    if not row:
        return {"error": "No random entries for this minute", "code": 404}
    # Parse entry_ids into a list
    entries = row.entry_ids.split(",") if row.entry_ids else []
    return {"minute": minute, "entry_ids": entries}