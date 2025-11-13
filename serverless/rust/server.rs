use axum::{routing::post, Json, Router};
use serde_json::{json, Value};
use std::net::SocketAddr;
use crate::dispatch;

async fn handle_request(Json(payload): Json<Value>) -> Json<Value> {
    let function_name = payload.get("function_name").and_then(|v| v.as_str()).unwrap_or("unknown");
    let input = payload.get("input").cloned().unwrap_or(Value::Null);

    let (result, error) = dispatch::dispatch(function_name, input);

    Json(json!({
        "result": result,
        "error": error
    }))
}

#[tokio::main]
async fn main() {
    let app = Router::new().route("/invoke", post(handle_request));

    let addr = SocketAddr::from(([127, 0, 0, 1], 3000));
    println!("Server running on http://{}", addr);

    axum::Server::bind(&addr)
        .serve(app.into_make_service())
        .await
        .unwrap();
}
