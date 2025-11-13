use lambda_runtime::{handler_fn, Context, Error};
use serde_json::{json, Value};
use crate::dispatch;

async fn lambda_handler(event: Value, _: Context) -> Result<Value, Error> {
    let function_name = event.get("function_name").and_then(|v| v.as_str()).unwrap_or("unknown");
    let input = event.get("input").cloned().unwrap_or(Value::Null);

    let (result, error) = dispatch::dispatch(function_name, input);

    Ok(json!({
        "result": result,
        "error": error
    }))
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    let handler = handler_fn(lambda_handler);
    lambda_runtime::run(handler).await?;
    Ok(())
}
