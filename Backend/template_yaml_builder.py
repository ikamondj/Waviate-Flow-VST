# generate_template.py
import ast
from pathlib import Path

def extract_functions_from_dispatcher(path: str):
    """Return sorted list of string keys from the FUNCTIONS dict in dispatcher.py."""
    src = Path(path).read_text(encoding="utf-8")
    tree = ast.parse(src, filename=path)

    functions = []
    for node in ast.walk(tree):
        # Look for: FUNCTIONS = { "key": something, ... }
        if isinstance(node, ast.Assign):
            # Handle single target like: FUNCTIONS = ...
            targets = node.targets
            if len(targets) != 1:
                continue
            target = targets[0]
            if isinstance(target, ast.Name) and target.id == "FUNCTIONS":
                if isinstance(node.value, ast.Dict):
                    for key_node in node.value.keys:
                        if isinstance(key_node, ast.Constant) and isinstance(key_node.value, str):
                            functions.append(key_node.value)
                # If the dict is built elsewhere (e.g., with dict(...) or updates), skip.
    return sorted(set(functions))

def generate_template(functions, out_path="marketplace_server/template.yaml"):
    template = f"""AWSTemplateFormatVersion: '2010-09-09'
Transform: AWS::Serverless-2016-10-31
Description: Local Marketplace Server (auto-generated)

Metadata:
  Note: |
    Functions discovered from dispatcher: {len(functions)}
    {"".join(f"- {name}\n    " for name in functions)}\
Resources:
  MarketplaceFunction:
    Type: AWS::Serverless::Function
    Properties:
      FunctionName: MarketplaceFunction
      Runtime: python3.11
      Handler: lambda.lambda_handler
      CodeUri: ./
      Timeout: 30
      MemorySize: 512
      Events:
        Api:
          Type: Api
          Properties:
            Path: /{{proxy+}}
            Method: ANY
"""
    Path(out_path).write_text(template, encoding="utf-8")
    print(f"Wrote {out_path} with {len(functions)} functions.")

if __name__ == "__main__":
    funcs = extract_functions_from_dispatcher("marketplace_server/dispatcher.py")
    generate_template(funcs)
