use serde_json::Value;

pub fn create_review(input: Value) -> (Option<String>, Option<String>) {
    if input.get("user_id").is_none() || input.get("entry_user_id").is_none() {
        return (None, Some("Missing user or entry IDs".to_string()));
    }
    // Implement logic here
    (Some("Review created".to_string()), None)
}

pub fn update_review(input: Value) -> (Option<String>, Option<String>) {
    if input.get("review_id").is_none() {
        return (None, Some("Missing review_id".to_string()));
    }
    // Implement logic here
    (Some("Review updated".to_string()), None)
}

pub fn delete_review(input: Value) -> (Option<String>, Option<String>) {
    if input.get("review_id").is_none() {
        return (None, Some("Missing review_id".to_string()));
    }
    // Implement logic here
    (Some("Review deleted".to_string()), None)
}

pub fn list_reviews_by_entry(input: Value) -> (Option<String>, Option<String>) {
    unimplemented!()
}