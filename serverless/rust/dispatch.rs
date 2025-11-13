use serde_json::Value; // Lightweight JSON type object from the serde_json crate
use std::collections::HashMap;

pub mod dispatch;

// Ensure the dispatcher module is accessible for both lambda.rs and server.rs

pub fn dispatch_map() -> HashMap<&str, fn(Value) -> (Option<String>, Option<String>)> {
    let mut functions: HashMap<&str, fn(Value) -> (Option<String>, Option<String>)> = HashMap::new();

    functions.insert("register_user", crate::functions::accounts::register_user);
    functions.insert("get_user_profile", crate::functions::accounts::get_user_profile);
    functions.insert("update_user_profile", crate::functions::accounts::update_user_profile);
    functions.insert("delete_user_profile", crate::functions::accounts::delete_user_profile);
    functions.insert("start_subscription", crate::functions::accounts::start_subscription);
    functions.insert("cancel_subscription", crate::functions::accounts::cancel_subscription);
    functions.insert("get_subscription_status", crate::functions::accounts::get_subscription_status);
    functions.insert("login", crate::functions::accounts::login);

    functions.insert("add_to_cart", crate::functions::cart::add_to_cart);
    functions.insert("remove_from_cart", crate::functions::cart::remove_from_cart);
    functions.insert("view_cart", crate::functions::cart::view_cart);
    functions.insert("download_cart", crate::functions::cart::download_cart);   

    functions.insert("create_entry", crate::functions::publishing::create_entry);
    functions.insert("update_entry", crate::functions::publishing::update_entry);
    functions.insert("delete_entry", crate::functions::publishing::delete_entry);
    functions.insert("get_creator_dashboard", crate::functions::publishing::get_creator_dashboard);

    functions.insert("create_review", crate::functions::rating::create_review);
    functions.insert("update_review", crate::functions::rating::update_review);
    functions.insert("delete_review", crate::functions::rating::delete_review);
    functions.insert("list_reviews_by_entry", crate::functions::rating::list_reviews_by_entry);

    functions.insert("admin_list_users", crate::functions::admin::admin_list_users);
    functions.insert("admin_list_entries", crate::functions::admin::admin_list_entries);
    functions.insert("admin_remove_entry", crate::functions::admin::admin_remove_entry);
    functions.insert("admin_ban_user", crate::functions::admin::admin_ban_user);
    functions.insert("admin_stats", crate::functions::admin::admin_stats);
    functions.insert("admin_setup_daily_random", crate::functions::admin::admin_setup_daily_random);
    functions.insert("admin_setup_daily_hottest", crate::functions::admin::admin_setup_daily_hottest);

    functions.insert("list_entries", crate::functions::content::list_entries);
    functions.insert("search_entries", crate::functions::content::search_entries);
    functions.insert("get_entry", crate::functions::content::get_entry);
    functions.insert("list_entries_by_creator", crate::functions::content::list_entries_by_creator);
    functions.insert("list_categories", crate::functions::content::list_categories);
    functions.insert("list_new_entries", crate::functions::content::list_new_entries);
    functions.insert("list_popular_entries", crate::functions::content::list_popular_entries);
    functions.insert("list_best_rated_entries", crate::functions::content::list_best_rated_entries);
    functions.insert("list_random_entries", crate::functions::content::list_random_entries);

    functions.insert("stripe_webhook", crate::functions::money::stripe_webhook);
    return functions;
}


/// Dispatches a function based on the provided function name and input JSON value.
/// Returns a tuple containing an optional JSON string result and an optional error message.
pub fn dispatch(function_name: &str, input: Value) -> (Option<String>, Option<String>) {
    // Function body to be implemented
    let functions: HashMap<&str, fn(Value) -> (Option<String>, Option<String>)> = dispatch_map();

    if let Some(function) = functions.get(function_name) {
        match function(input) {
            Ok(result) => (Some(result), None),
            Err(err) => (None, Some(err)),
        }
    } else {
        (None, Some(format!("Function '{}' not found", function_name)))
    }

}