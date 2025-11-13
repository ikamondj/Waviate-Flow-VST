use serde_json::Value;

pub fn add_to_cart(input: Value) -> (Option<String>, Option<String>) {
    if input.get("item_id").is_none() {
        return (None, Some("Missing item_id".to_string()));
    }
    // Implement logic here
    (Some("Item added to cart".to_string()), None)
}

pub fn remove_from_cart(input: Value) -> (Option<String>, Option<String>) {
    if input.get("item_id").is_none() {
        return (None, Some("Missing item_id".to_string()));
    }
    // Implement logic here
    (Some("Item removed from cart".to_string()), None)
}

pub fn view_cart(input: Value) -> (Option<String>, Option<String>) {
    if input.get("user_id").is_none() {
        return (None, Some("Missing user_id".to_string()));
    }
    // Implement logic here
    (Some("Cart viewed".to_string()), None)
}

pub fn download_cart(input: Value) -> (Option<String>, Option<String>) {
    unimplemented!()
}