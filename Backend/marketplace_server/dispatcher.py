# dispatcher.py
import json
import inspect
from typing import Any, Dict, Optional

import accounts
import admin
import content
import rating
import publishing
import cart

# --- Registry ---------------------------------------------------------------

FUNCTIONS = {
    # Accounts
    "registerUser": accounts.register_user,
    "getUserProfile": accounts.get_user_profile,
    "updateUserProfile": accounts.update_user_profile,
    "deleteUserAccount": accounts.delete_user_account,
    "startSubscription": accounts.start_subscription,
    "cancelSubscription": accounts.cancel_subscription,
    "getSubscriptionStatus": accounts.get_subscription_status,

    # Cart (or your selections API)
    "addToCart": cart.add_to_cart,
    "removeFromCart": cart.remove_from_cart,
    "viewCart": cart.view_cart,
    "downloadCart": cart.download_cart,

    # Publishing
    "createEntry": publishing.create_entry,
    "updateEntry": publishing.update_entry,
    "deleteEntry": publishing.delete_entry,
    "getCreatorDashboard": publishing.get_creator_dashboard,

    # Content
    "listEntries": content.list_entries,
    "searchEntries": content.search_entries,
    "getEntry": content.get_entry,
    "listEntriesByCreator": content.list_entries_by_creator,
    "listCategories": content.list_categories,
    "listNewEntries": content.list_new_entries,
    "listPopularEntries": content.list_popular_entries,

    # Rating
    "createReview": rating.create_review,
    "updateReview": rating.update_review,
    "deleteReview": rating.delete_review,
    "listReviewsByEntry": rating.list_reviews_by_entry,

    # Admin
    "adminListUsers": admin.admin_list_users,
    "adminListEntries": admin.admin_list_entries,
    "adminRemoveEntry": admin.admin_remove_entry,
    "adminBanUser": admin.admin_ban_user,
    "adminStats": admin.admin_stats,
}

# --- Central policy (coarse-grained) ---------------------------------------

REQUIRES_AUTH = {
    # Accounts
    "logoutUser", "getUserProfile", "updateUserProfile", "deleteUserAccount",
    "startSubscription", "cancelSubscription", "getSubscriptionStatus",
    # Cart / selections
    "addToCart", "removeFromCart", "viewCart", "downloadCart",
    # Publishing
    "createEntry", "updateEntry", "deleteEntry", "getCreatorDashboard",
    # Rating
    "createReview", "updateReview", "deleteReview",
    # Admin (also needs admin role)
    "adminListUsers", "adminListEntries", "adminRemoveEntry", "adminBanUser", "adminStats",
}

REQUIRES_ADMIN = {
    "adminListUsers", "adminListEntries", "adminRemoveEntry", "adminBanUser", "adminStats",
}

# Publishing and downloads should require an active/trialing subscription
REQUIRES_ACTIVE_SUB = {
    "createEntry", "updateEntry", "deleteEntry", "getCreatorDashboard", "downloadCart"
}

# --- Auth helpers -----------------------------------------------------------

def _unauthorized(msg="unauthorized", code="UNAUTHORIZED", status=401):
    return {"error": msg, "code": code, "status": status}

def _forbidden(msg="forbidden", code="FORBIDDEN", status=403):
    return {"error": msg, "code": code, "status": status}

def _bad_request(msg="bad request", code="BAD_REQUEST", status=400):
    return {"error": msg, "code": code, "status": status}

def build_auth_context(headers: Dict[str, str], cookies: Dict[str, str]) -> Optional[Dict[str, Any]]:
    """
    Return a verified auth context dict or None.
    Accepts either:
      - Bearer JWT in Authorization header (your own signed access token), or
      - Session cookie 'sid' -> lookup in your session store.
    IMPORTANT: Do NOT trust any client-provided 'auth' inside JSON body.
    """
    # 1) Try Bearer JWT
    authz = headers.get("authorization") or headers.get("Authorization")
    if authz and authz.lower().startswith("bearer "):
        token = authz.split(" ", 1)[1].strip()
        ctx = verify_internal_jwt(token)  # implement with your signing key
        if ctx:
            return ctx

    # 2) Try server session cookie
    sid = cookies.get("sid")
    if sid:
        ctx = lookup_session(sid)  # implement: DB/Redis look-up -> dict
        if ctx and not ctx.get("revoked") and not ctx_expired(ctx):
            return {
                "user_id": ctx["user_id"],          # 48-bit int
                "role": ctx.get("role", "user"),
                "is_banned": bool(ctx.get("is_banned", False)),
                "subscription": ctx.get("subscription", "none"),  # 'active','trialing','canceled',...
                "provider": ctx.get("provider"),
                "session_id": sid,
                "issued_at": ctx.get("issued_at"),
                "expires_at": ctx.get("expires_at"),
            }

    return None

def ctx_expired(ctx: Dict[str, Any]) -> bool:
    import time
    exp = ctx.get("expires_at")
    return bool(exp and time.time() > int(exp))

def verify_internal_jwt(token: str) -> Optional[Dict[str, Any]]:
    """
    Verify your own short-lived access token (HS256/RS256).
    Must check signature, exp, nbf, and optionally a session_version/jti.
    Return a dict like:
      {"user_id": 123, "role":"user","subscription":"active","is_banned":False,"session_id":"...", "issued_at":..., "expires_at":...}
    """
    # TODO: implement with your jwt lib (PyJWT, etc.)
    return None

def lookup_session(sid: str) -> Optional[Dict[str, Any]]:
    """
    Load session by sid from Cockroach/Redis.
    Return a dict with user_id, role, subscription, is_banned, issued_at, expires_at, revoked, provider.
    """
    # TODO: implement
    return None

def _function_accepts_param(fn, name: str) -> bool:
    params = inspect.signature(fn).parameters
    return name in params

def _enforce_policy(func_name: str, auth: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    """Return an error dict if policy fails, else None."""
    if func_name in REQUIRES_AUTH:
        if not auth:
            return _unauthorized()
        if auth.get("is_banned"):
            return _forbidden("account banned", code="BANNED")

    if func_name in REQUIRES_ADMIN:
        if not auth or auth.get("role") != "admin":
            return _forbidden("admin only", code="ADMIN_ONLY")

    if func_name in REQUIRES_ACTIVE_SUB:
        if not auth or auth.get("subscription") not in ("active", "trialing"):
            return _forbidden("subscription required", code="SUBSCRIPTION_REQUIRED")

    return None

# --- Core dispatcher --------------------------------------------------------

def handle(event_body: str, *, headers: Optional[Dict[str, str]] = None, cookies: Optional[Dict[str, str]] = None):
    """
    Dispatcher entrypoint.
    - event_body: JSON string: {"func":"...", "args": {...}}
    - headers/cookies: pass raw request headers/cookies from AWS/GCP (used for auth)
    Returns a dict result or error dict (with optional status).
    """
    headers = headers or {}
    cookies = cookies or {}

    try:
        req = json.loads(event_body)
    except Exception:
        return _bad_request("invalid JSON")

    func_name = req.get("func")
    args = req.get("args", {}) or {}
    if not func_name or func_name not in FUNCTIONS:
        return _bad_request(f"Unknown function: {func_name}")

    # Build verified auth context once
    auth_ctx = build_auth_context(headers, cookies)

    # Central policy gate
    policy_err = _enforce_policy(func_name, auth_ctx)
    if policy_err:
        return policy_err

    # Inject auth context if the target function accepts it
    fn = FUNCTIONS[func_name]
    call_args = dict(args)
    if _function_accepts_param(fn, "auth"):
        call_args["auth"] = auth_ctx  # dict or None

    # Optional: inject user_id only if function expects it (handy for older call sites)
    if auth_ctx and _function_accepts_param(fn, "user_id") and "user_id" not in call_args:
        call_args["user_id"] = auth_ctx["user_id"]

    try:
        return fn(**call_args)
    except Exception as e:
        # You can expand this to structured errors
        return {"error": str(e), "code": "INTERNAL"}
