import os
import requests
import jwt
import datetime
from db import get_db
from entity.user import User

# Example: Google OAuth config (replace with your provider as needed)
GOOGLE_CLIENT_ID = os.getenv("GOOGLE_CLIENT_ID")
GOOGLE_CLIENT_SECRET = os.getenv("GOOGLE_CLIENT_SECRET")
GOOGLE_TOKEN_URL = "https://oauth2.googleapis.com/token"
GOOGLE_USERINFO_URL = "https://openidconnect.googleapis.com/v1/userinfo"

# Add Microsoft and Apple OAuth config as needed
MICROSOFT_CLIENT_ID = os.getenv("MICROSOFT_CLIENT_ID")
MICROSOFT_CLIENT_SECRET = os.getenv("MICROSOFT_CLIENT_SECRET")
MICROSOFT_TOKEN_URL = "https://login.microsoftonline.com/common/oauth2/v2.0/token"
MICROSOFT_USERINFO_URL = "https://graph.microsoft.com/oidc/userinfo"

APPLE_CLIENT_ID = os.getenv("APPLE_CLIENT_ID")
APPLE_CLIENT_SECRET = os.getenv("APPLE_CLIENT_SECRET")
APPLE_TOKEN_URL = "https://appleid.apple.com/auth/token"
APPLE_USERINFO_URL = "https://appleid.apple.com/auth/userinfo"

JWT_SECRET = os.getenv("JWT_SECRET", "dev_secret")
JWT_ALGORITHM = "HS256"
JWT_EXPIRY_SECONDS = 60 * 60 * 24 * 7  # 1 week


def register_user(user_id: int, email: str, provider_type: int = 0, username: str = None, profile: dict = None, role: str = 'user'):
    """
    Register a new user. Returns error if email or id is not unique or id is invalid.
    """
    if not user_id or not email:
        return {"error": "Missing user_id or email", "code": 400}
    if user_id < 16:
        return {"error": "User ID must be >= 16", "code": 400}
    db = next(get_db())
    existing = db.query(User).filter((User.id == user_id) | (User.email == email)).first()
    if existing:
        return {"error": "User with this ID or email already exists", "code": 409}
    user = User(id=user_id, email=email, provider_type=provider_type, username=username, profile=profile, role=role)
    db.add(user)
    db.commit()
    return {"message": "User registered", "user_id": user.id, "email": user.email}


def get_user_profile(requesting_user_id: int, target_user_id: int, is_subscriber: bool = False):
    """
    Get a user's public profile. If requesting own profile, show all info. Otherwise, show public info and entries.
    If not a subscriber, hide non-public nodes unless viewing own profile.
    """
    db = next(get_db())
    user = db.query(User).filter_by(id=target_user_id).first()
    if not user:
        return {"error": "User not found", "code": 404}
    # Get all entries by this user
    from entity.entry import Entry
    entries = db.query(Entry).filter_by(creator_id=target_user_id).all()
    entries_list = []
    for entry in entries:
        # If not viewing own profile and not a subscriber, only show public/active entries
        if requesting_user_id != target_user_id and not is_subscriber:
            if not entry.is_active:
                continue
        entries_list.append({
            "id": str(entry.id),
            "title": entry.title,
            "description": entry.description,
            "is_active": entry.is_active,
            "created_at": str(entry.created_at)
        })
    # Public profile info
    profile = {
        "user_id": user.id,
        "username": user.username,
        "profile": user.profile,
        "entries": entries_list
    }
    # If viewing own profile, include private info
    if requesting_user_id == target_user_id:
        profile["email"] = user.email
        profile["role"] = user.role
        profile["is_banned"] = user.is_banned
        profile["last_login_at"] = str(user.last_login_at)
    return profile


def update_user_profile(user_id: str = None, data: dict = None):
    return {"message": "not implemented"}


def delete_user_profile(requesting_user_id: int, target_user_id: int):
    """
    Delete a user profile. Only the user themselves can delete their profile.
    """
    if requesting_user_id != target_user_id:
        return {"error": "Forbidden: can only delete your own profile", "code": 403}
    db = next(get_db())
    user = db.query(User).filter_by(id=target_user_id).first()
    if not user:
        return {"error": "User not found", "code": 404}
    db.delete(user)
    db.commit()
    return {"message": "User profile deleted", "user_id": target_user_id}


def start_subscription(user_id: str = None):
    return {"message": "not implemented"}


def cancel_subscription(user_id: str = None):
    return {"message": "not implemented"}


def get_subscription_status(user_id: str = None):
    return {"message": "not implemented"}


def login(oauth_code: str, client_type: str, redirect_uri: str = None, provider: str = "google"):
    """
    Unified login endpoint for web and desktop clients, issues JWT with client_type claim.
    - oauth_code: the code/token from the OAuth provider
    - client_type: 'web' or 'desktop' (or other)
    - redirect_uri: required for web OAuth flows
    - provider: 'google', 'apple', or 'microsoft'
    """
    if not oauth_code or not client_type:
        return {"error": "Missing parameters", "code": 400}
    if provider == "google":
        # Exchange code for tokens (web) or verify ID token (desktop)
        if client_type == "web":
            if not redirect_uri:
                return {"error": "Missing redirect_uri for web login", "code": 400}
            data = {
                "code": oauth_code,
                "client_id": GOOGLE_CLIENT_ID,
                "client_secret": GOOGLE_CLIENT_SECRET,
                "redirect_uri": redirect_uri,
                "grant_type": "authorization_code"
            }
            resp = requests.post(GOOGLE_TOKEN_URL, data=data)
            if not resp.ok:
                return {"error": "OAuth token exchange failed", "code": 401}
            tokens = resp.json()
            id_token = tokens.get("id_token")
        else:
            id_token = oauth_code
        headers = {"Authorization": f"Bearer {id_token}"}
        userinfo_resp = requests.get(GOOGLE_USERINFO_URL, headers=headers)
        if not userinfo_resp.ok:
            return {"error": "Failed to fetch user info", "code": 401}
        userinfo = userinfo_resp.json()
        email = userinfo.get("email")
        sub = userinfo.get("sub")
    elif provider == "microsoft":
        if client_type == "web":
            if not redirect_uri:
                return {"error": "Missing redirect_uri for web login", "code": 400}
            data = {
                "client_id": MICROSOFT_CLIENT_ID,
                "client_secret": MICROSOFT_CLIENT_SECRET,
                "code": oauth_code,
                "redirect_uri": redirect_uri,
                "grant_type": "authorization_code",
                "scope": "openid email profile"
            }
            resp = requests.post(MICROSOFT_TOKEN_URL, data=data)
            if not resp.ok:
                return {"error": "OAuth token exchange failed", "code": 401}
            tokens = resp.json()
            id_token = tokens.get("id_token")
        else:
            id_token = oauth_code
        headers = {"Authorization": f"Bearer {id_token}"}
        userinfo_resp = requests.get(MICROSOFT_USERINFO_URL, headers=headers)
        if not userinfo_resp.ok:
            return {"error": "Failed to fetch user info", "code": 401}
        userinfo = userinfo_resp.json()
        email = userinfo.get("email")
        sub = userinfo.get("sub") or userinfo.get("oid")
    elif provider == "apple":
        # Apple flow is more complex; this is a placeholder
        # You may need to verify the ID token using a JWT library
        id_token = oauth_code
        # Apple does not have a userinfo endpoint; decode the ID token
        try:
            userinfo = jwt.decode(id_token, options={"verify_signature": False})
            email = userinfo.get("email")
            sub = userinfo.get("sub")
        except Exception:
            return {"error": "Failed to decode Apple ID token", "code": 401}
    else:
        return {"error": "Unsupported provider", "code": 400}
    if not email or not sub:
        return {"error": "Invalid user info from provider", "code": 401}
    db = next(get_db())
    user = db.query(User).filter_by(email=email).first()
    if not user:
        user = User(id=int(sub), email=email)
        db.add(user)
        db.commit()
    # Issue JWT session token with client_type claim
    payload = {
        "user_id": user.id,
        "email": user.email,
        "client_type": client_type,
        "exp": datetime.datetime.utcnow() + datetime.timedelta(seconds=JWT_EXPIRY_SECONDS)
    }
    session_token = jwt.encode(payload, JWT_SECRET, algorithm=JWT_ALGORITHM)
    return {"message": "Login successful", "user_id": user.id, "email": user.email, "session_token": session_token}