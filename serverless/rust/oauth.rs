use serde_json::Value;
use reqwest::Client;
use std::collections::HashMap;

pub async fn validate_token(provider: &str, token: &str) -> Result<Value, String> {
    let client = Client::new();
    let user_info_url = match provider {
        "google" => "https://www.googleapis.com/oauth2/v2/userinfo",
        "github" => "https://api.github.com/user",
        "microsoft" => "https://graph.microsoft.com/v1.0/me",
        _ => return Err("Unsupported provider".to_string()),
    };

    let response = client
        .get(user_info_url)
        .bearer_auth(token)
        .send()
        .await
        .map_err(|e| format!("Request error: {}", e))?;

    if response.status().is_success() {
        response
            .json::<Value>()
            .await
            .map_err(|e| format!("Failed to parse response: {}", e))
    } else {
        Err(format!("Token validation failed with status: {}", response.status()))
    }
}