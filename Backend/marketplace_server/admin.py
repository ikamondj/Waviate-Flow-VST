import os
import numpy as np
from db import fetch_all, execute_query, get_db
from entity.daily_random import DailyRandom

# Centralized Auth Helpers
def get_admin_token():
    """Get the expected admin token from env variable."""
    return os.getenv("ADMIN_SERVICE_TOKEN")

def check_access_token(access_token: str):
    """Raise PermissionError if the access token is invalid."""
    expected = get_admin_token()
    if not expected or access_token != expected:
        raise PermissionError("Unauthorized: Invalid access token.")

def admin_list_users(access_token: str):
    check_access_token(access_token)
    return {"message": "not implemented"}


def admin_list_entries(access_token: str):
    check_access_token(access_token)
    return {"message": "not implemented"}


def admin_remove_entry(access_token: str, entry_id: str = None):
    check_access_token(access_token)
    return {"message": "not implemented"}


def admin_ban_user(access_token: str, user_id: str = None):
    check_access_token(access_token)
    return {"message": "not implemented"}


def admin_stats(access_token: str):
    check_access_token(access_token)
    return {"message": "not implemented"}



def get_marketplace_entry_ids():
    # Fetch all (nodeId, userId) pairs
    rows = fetch_all("SELECT nodeId, userId FROM marketplace_content")
    # Structured array for fast shuffling
    dtype = np.dtype([('nodeId', np.uint64), ('userId', np.uint64)])
    return np.array(rows, dtype=dtype)

def compute_k(num_entries):
    if num_entries < 36000:
        return 25
    elif num_entries > 144000:
        return 100
    else:
        return round(num_entries / 1440)

def generate_random_daily_entries(entry_ids, k):
    num_minutes = 1440
    total_needed = num_minutes * k
    n = len(entry_ids)
    result = np.empty(total_needed, dtype=entry_ids.dtype)
    filled = 0
    while filled < total_needed:
        shuffled = np.copy(entry_ids)
        np.random.shuffle(shuffled)
        to_copy = min(n, total_needed - filled)
        result[filled:filled+to_copy] = shuffled[:to_copy]
        filled += to_copy
    return result.reshape((num_minutes, k))

def update_random_daily_table(random_entries):
    db = next(get_db())
    db.query(DailyRandom).delete()  # Truncate
    for minute, ids in enumerate(random_entries):
        # Store as comma-separated pairs: "nodeId1:userId1,nodeId2:userId2,..."
        entry_str = ','.join(f"{int(e['nodeId'])}:{int(e['userId'])}" for e in ids)
        db.add(DailyRandom(minute=minute, entry_ids=entry_str))
    db.commit()

def admin_setup_daily_random(access_token: str):
    check_access_token(access_token)
    entry_ids = get_marketplace_entry_ids()
    num_entries = len(entry_ids)
    if num_entries == 0:
        return
    k = compute_k(num_entries)
    random_entries = generate_random_daily_entries(entry_ids, k)
    update_random_daily_table(random_entries)

def admin_setup_daily_hottest(access_token: str):
    check_access_token(access_token)
    return {"message": "not implemented"}