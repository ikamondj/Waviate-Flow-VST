use serde_json::Value;
use stripe::{Event, Webhook, WebhookEvent};
use std::collections::HashMap;
use std::env;

pub fn stripe_webhook(input: Value) -> (Option<String>, Option<String>) {
    let payload = match input.get("payload").and_then(|v| v.as_str()) {
        Some(p) => p,
        None => return (None, Some("Missing payload".to_string())),
    };

    let sig_header = match input.get("signature_header").and_then(|v| v.as_str()) {
        Some(s) => s,
        None => return (None, Some("Missing signature header".to_string())),
    };

    let endpoint_secret = env::var("STRIPE_WEBHOOK_SECRET").unwrap_or_else(|_| "your-webhook-signing-secret".to_string());

    let event = match Webhook::construct_event(payload, sig_header, &endpoint_secret) {
        Ok(e) => e,
        Err(err) => {
            let error_message = format!("Webhook error: {}", err);
            return (None, Some(error_message));
        }
    };

    match event.type_().as_str() {
        "invoice.payment_succeeded" => {
            // Handle successful payment
            // Add your logic here
            (Some("Payment succeeded".to_string()), None)
        }
        "customer.subscription.deleted" => {
            // Handle subscription cancellation
            // Add your logic here
            (Some("Subscription deleted".to_string()), None)
        }
        _ => {
            // Unhandled event type
            (Some("Unhandled event type".to_string()), None)
        }
    }
}