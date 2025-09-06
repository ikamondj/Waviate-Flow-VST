import json
import accounts
import admin
import content
import rating
import publishing
import cart


# Registry
FUNCTIONS = {
    # Accounts
    "registerUser": accounts.register_user,
    "loginUser": accounts.login_user,
    "logoutUser": accounts.logout_user,
    "getUserProfile": accounts.get_user_profile,
    "updateUserProfile": accounts.update_user_profile,
    "deleteUserAccount": accounts.delete_user_account,
    "startSubscription": accounts.start_subscription,
    "cancelSubscription": accounts.cancel_subscription,
    "getSubscriptionStatus": accounts.get_subscription_status,

    # Cart
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

def handle(event_body: str):
    """
    Dispatcher: event_body is a JSON string with shape:
      { "func": "registerUser", "args": { "name": "Thomas" } }
    """
    try:
        req = json.loads(event_body)
        func_name = req.get("func")
        args = req.get("args", {})
        if func_name not in FUNCTIONS:
            return {"error": f"Unknown function: {func_name}"}
        fn = FUNCTIONS[func_name]
        return fn(**args)
    except Exception as e:
        return {"error": str(e)}
