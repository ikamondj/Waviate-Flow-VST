import base64

# paste your base64 string here (everything between <value> ... </value>)
b64 = """PASTE_LONG_BASE64_HERE"""

data = base64.b64decode(b64)

with open("output.gif", "wb") as f:
    f.write(data)

print("Saved as output.gif")
