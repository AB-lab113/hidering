# HIDERING Block Explorer

Flask single-file explorer for the HRG mainnet, served as `ab113hrg/hrg-explorer:latest`.
Reads the daemon JSON-RPC (`HRG_DAEMON_RPC`, **required** — the app exits at startup without it) and renders the last 20 blocks on port 8081.
Each refresh costs 2 RPC calls (`get_info` + `get_block_headers_range`) and is cached for 5 seconds.

Build & run:

```bash
docker build -t ab113hrg/hrg-explorer:latest .
docker run -d --name hrgexplorer --restart unless-stopped \
  -p 127.0.0.1:8080:8081 \
  --add-host=host.docker.internal:host-gateway \
  -e HRG_DAEMON_RPC=http://host.docker.internal:19741/json_rpc \
  ab113hrg/hrg-explorer:latest
```

Deployed on OVH (`135.125.243.137`) behind nginx at https://explorer.hidering.org (`proxy_pass http://127.0.0.1:8080`).
Publish the port on `127.0.0.1` only: Docker's DNAT bypasses ufw, so `ufw deny 8080` does not protect a port published on `0.0.0.0`.
