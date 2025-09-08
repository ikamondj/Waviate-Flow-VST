from typing import Callable, Any, Dict, Optional
from urllib.parse import urlencode
import requests

class OAuthProvider:
    def __init__(
        self,
        authorize_url: str,
        token_url: str,
        client_id: str,
        client_secret: str,
        redirect_uri: str,
        get_authorization_params: Optional[Callable[[Any], Dict[str, Any]]] = None,
        fetch_token: Optional[Callable[[str], Dict[str, Any]]] = None,
        get_user_info: Optional[Callable[[str], Dict[str, Any]]] = None,
    ):
        self.authorize_url = authorize_url
        self.token_url = token_url
        self.client_id = client_id
        self.client_secret = client_secret
        self.redirect_uri = redirect_uri
        self.get_authorization_params = get_authorization_params or self.default_authorization_params
        self.fetch_token = fetch_token or self.default_fetch_token
        self.get_user_info = get_user_info or self.default_get_user_info

    def default_authorization_params(self, state: Any) -> Dict[str, Any]:
        return {
            "client_id": self.client_id,
            "redirect_uri": self.redirect_uri,
            "response_type": "code",
            "state": state,
        }

    def default_fetch_token(self, code: str) -> Dict[str, Any]:
        # Placeholder for token fetching logic
        return {}

    def default_get_user_info(self, access_token: str) -> Dict[str, Any]:
        # Placeholder for user info fetching logic
        return {}

    def get_authorization_url(self, state: Any) -> str:
        params = self.get_authorization_params(state)
        return f"{self.authorize_url}?{urlencode(params)}"

    def exchange_code_for_token(self, code: str) -> Dict[str, Any]:
        return self.fetch_token(code)

    def get_user(self, access_token: str) -> Dict[str, Any]:
        return self.get_user_info(access_token)
    
    @staticmethod
    def create_google(client_id: str, client_secret: str, redirect_uri: str) -> "OAuthProvider":
        def get_authorization_params(state):
            return {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "response_type": "code",
                "scope": "openid email profile",
                "access_type": "offline",
                "state": state,
                "prompt": "consent"
            }

        def fetch_token(code):
            data = {
                "code": code,
                "client_id": client_id,
                "client_secret": client_secret,
                "redirect_uri": redirect_uri,
                "grant_type": "authorization_code"
            }
            resp = requests.post("https://oauth2.googleapis.com/token", data=data)
            resp.raise_for_status()
            return resp.json()

        def get_user_info(access_token):
            headers = {"Authorization": f"Bearer {access_token}"}
            resp = requests.get("https://www.googleapis.com/oauth2/v2/userinfo", headers=headers)
            resp.raise_for_status()
            return resp.json()

        return OAuthProvider(
            authorize_url="https://accounts.google.com/o/oauth2/v2/auth",
            token_url="https://oauth2.googleapis.com/token",
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri,
            get_authorization_params=get_authorization_params,
            fetch_token=fetch_token,
            get_user_info=get_user_info,
        )

    @staticmethod
    def create_discord(client_id: str, client_secret: str, redirect_uri: str) -> "OAuthProvider":
        def get_authorization_params(state):
            return {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "response_type": "code",
                "scope": "identify email",
                "state": state,
                "prompt": "consent"
            }

        def fetch_token(code):
            data = {
                "client_id": client_id,
                "client_secret": client_secret,
                "grant_type": "authorization_code",
                "code": code,
                "redirect_uri": redirect_uri,
                "scope": "identify email"
            }
            headers = {"Content-Type": "application/x-www-form-urlencoded"}
            resp = requests.post("https://discord.com/api/oauth2/token", data=data, headers=headers)
            resp.raise_for_status()
            return resp.json()

        def get_user_info(access_token):
            headers = {"Authorization": f"Bearer {access_token}"}
            resp = requests.get("https://discord.com/api/users/@me", headers=headers)
            resp.raise_for_status()
            return resp.json()

        return OAuthProvider(
            authorize_url="https://discord.com/api/oauth2/authorize",
            token_url="https://discord.com/api/oauth2/token",
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri,
            get_authorization_params=get_authorization_params,
            fetch_token=fetch_token,
            get_user_info=get_user_info,
        )

    @staticmethod
    def create_github(client_id: str, client_secret: str, redirect_uri: str) -> "OAuthProvider":
        def get_authorization_params(state):
            return {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "scope": "read:user user:email",
                "state": state,
                "allow_signup": "true"
            }

        def fetch_token(code):
            data = {
                "client_id": client_id,
                "client_secret": client_secret,
                "code": code,
                "redirect_uri": redirect_uri,
                "state": ""
            }
            headers = {"Accept": "application/json"}
            resp = requests.post("https://github.com/login/oauth/access_token", data=data, headers=headers)
            resp.raise_for_status()
            return resp.json()

        def get_user_info(access_token):
            headers = {"Authorization": f"Bearer {access_token}", "Accept": "application/json"}
            resp = requests.get("https://api.github.com/user", headers=headers)
            resp.raise_for_status()
            user = resp.json()
            # Get email
            resp_email = requests.get("https://api.github.com/user/emails", headers=headers)
            resp_email.raise_for_status()
            emails = resp_email.json()
            user["email"] = next((e["email"] for e in emails if e.get("primary")), None)
            return user

        return OAuthProvider(
            authorize_url="https://github.com/login/oauth/authorize",
            token_url="https://github.com/login/oauth/access_token",
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri,
            get_authorization_params=get_authorization_params,
            fetch_token=fetch_token,
            get_user_info=get_user_info,
        )

    @staticmethod
    def create_facebook(client_id: str, client_secret: str, redirect_uri: str) -> "OAuthProvider":
        def get_authorization_params(state):
            return {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "state": state,
                "response_type": "code",
                "scope": "email,public_profile"
            }

        def fetch_token(code):
            params = {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "client_secret": client_secret,
                "code": code
            }
            resp = requests.get("https://graph.facebook.com/v12.0/oauth/access_token", params=params)
            resp.raise_for_status()
            return resp.json()

        def get_user_info(access_token):
            params = {
                "fields": "id,name,email,picture",
                "access_token": access_token
            }
            resp = requests.get("https://graph.facebook.com/me", params=params)
            resp.raise_for_status()
            return resp.json()

        return OAuthProvider(
            authorize_url="https://www.facebook.com/v12.0/dialog/oauth",
            token_url="https://graph.facebook.com/v12.0/oauth/access_token",
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri,
            get_authorization_params=get_authorization_params,
            fetch_token=fetch_token,
            get_user_info=get_user_info,
        )
    
    @staticmethod
    def create_apple(client_id: str, client_secret: str, redirect_uri: str) -> "OAuthProvider":
        def get_authorization_params(state):
            return {
                "client_id": client_id,
                "redirect_uri": redirect_uri,
                "response_type": "code",
                "scope": "name email",
                "state": state,
                "response_mode": "form_post"
            }

        def fetch_token(code):
            data = {
                "client_id": client_id,
                "client_secret": client_secret,
                "code": code,
                "grant_type": "authorization_code",
                "redirect_uri": redirect_uri,
            }
            headers = {"Content-Type": "application/x-www-form-urlencoded"}
            resp = requests.post("https://appleid.apple.com/auth/token", data=data, headers=headers)
            resp.raise_for_status()
            return resp.json()

        def get_user_info(access_token):
            # Apple does not provide a userinfo endpoint.
            # The user's info (email, name) is only available in the initial authorization response.
            # Here, we just return the access_token.
            return {"access_token": access_token}

        return OAuthProvider(
            authorize_url="https://appleid.apple.com/auth/authorize",
            token_url="https://appleid.apple.com/auth/token",
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri,
            get_authorization_params=get_authorization_params,
            fetch_token=fetch_token,
            get_user_info=get_user_info,
        )