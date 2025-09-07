Set-Location marketplace_client
npm install 
Start-Process "D:\Program Files\nodejs\npm.cmd" -ArgumentList "run dev" -NoNewWindow

Set-Location ../marketplace_server
Start-Process "sam" "local start-api --template template.yaml" -NoNewWindow

Set-Location ../

Start-Process "C:\Program Files\Cockroach\cockroach.exe" -ArgumentList "start-single-node --insecure --listen-addr=localhost:26257 --http-addr=localhost:8092 --store=cockroach-data" -NoNewWindow

Start-Process "C:\Program Files\Cockroach\cockroach.exe" 'sql --insecure --execute="schema.sql"' -NoNewWindow

Start-Process "C:\Windows\py.exe" -ArgumentList ".\populate_fake_data.py" -NoNewWindow




