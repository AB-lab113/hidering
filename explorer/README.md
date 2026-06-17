# HIDERING Block Explorer

Flask single-file explorer for the HRG mainnet, served as `ab113hrg/hrg-explorer:latest`.
Reads the daemon JSON-RPC (`HRG_DAEMON_RPC`, default `http://135.125.243.137:19741/json_rpc`) and renders recent blocks on port 8081.

Build & run:

```bash
docker build -t ab113hrg/hrg-explorer:latest .
docker run -d --name hrgexplorer --restart unless-stopped -p 8080:8081 ab113hrg/hrg-explorer:latest
```

Deployed on OVH (`135.125.243.137`) behind nginx at https://explorer.hidering.org.
