from flask import Flask, render_template_string, jsonify
import os, requests, json

app = Flask(__name__)
DAEMON = os.environ.get("HRG_DAEMON_RPC", "http://135.125.243.137:19741/json_rpc")

def rpc(method, params={}):
    try:
        r = requests.post(DAEMON, json={"jsonrpc":"2.0","id":"0","method":method,"params":params}, timeout=10)
        return r.json().get("result", {})
    except:
        return {}

HTML = """<!DOCTYPE html>
<html><head>
<title>HIDERING Explorer</title>
<meta charset="UTF-8">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#080808;color:#D4CFC0;font-family:Inter,sans-serif;padding:2rem}
h1{font-family:Georgia,serif;color:#C9A84C;font-size:2rem;margin-bottom:0.5rem}
.sub{color:#7A7060;font-size:0.85rem;margin-bottom:2rem}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;margin-bottom:2rem}
.card{background:#161616;border:1px solid rgba(201,168,76,0.18);padding:1.5rem;border-radius:4px}
.card-val{font-size:1.5rem;color:#C9A84C;font-family:Georgia,serif}
.card-label{font-size:0.75rem;color:#7A7060;text-transform:uppercase;letter-spacing:0.1em;margin-top:0.3rem}
table{width:100%;border-collapse:collapse;background:#161616;border:1px solid rgba(201,168,76,0.18)}
th{text-align:left;padding:0.75rem 1rem;font-size:0.72rem;letter-spacing:0.1em;text-transform:uppercase;color:#7A7060;border-bottom:1px solid rgba(201,168,76,0.18)}
td{padding:0.75rem 1rem;font-size:0.82rem;border-bottom:1px solid rgba(201,168,76,0.08);font-family:monospace}
tr:hover td{background:rgba(201,168,76,0.03)}
.gold{color:#C9A84C}
h2{font-family:Georgia,serif;color:#F0ECE0;margin-bottom:1rem;font-size:1.2rem}
</style>
</head><body>
<h1>HIDERING</h1>
<div class="sub">HRG Block Explorer · Mainnet · v2.0.0</div>

<div class="cards">
  <div class="card">
    <div class="card-val gold">{{ info.height }}</div>
    <div class="card-label">Block Height</div>
  </div>
  <div class="card">
    <div class="card-val">{{ info.difficulty }}</div>
    <div class="card-label">Difficulty</div>
  </div>
  <div class="card">
    <div class="card-val gold">42.86</div>
    <div class="card-label">Block Reward (HRG)</div>
  </div>
  <div class="card">
    <div class="card-val">{{ info.tx_pool_size }}</div>
    <div class="card-label">Mempool TXs</div>
  </div>
</div>

<h2>Recent Blocks</h2>
<table>
  <thead><tr><th>Height</th><th>Hash</th><th>Txs</th><th>Size</th></tr></thead>
  <tbody>
  {% for b in blocks %}
  <tr>
    <td class="gold">{{ b.height }}</td>
    <td>{{ b.hash }}</td>
    <td>{{ b.num_txes }}</td>
    <td>{{ b.block_size }} B</td>
  </tr>
  {% endfor %}
  </tbody>
</table>
</body></html>"""

@app.route("/")
def index():
    info = rpc("get_info")
    height = info.get("height", 0)
    blocks = []
    for h in range(max(0, height-20), height):
        b = rpc("get_block", {"height": h})
        if b:
            hdr = b.get("block_header", {})
            blocks.append({
                "height": hdr.get("height", h),
                "hash": hdr.get("hash", ""),
                "num_txes": hdr.get("num_txes", 0),
                "block_size": hdr.get("block_size", 0)
            })
    blocks.reverse()
    return render_template_string(HTML, info=info, blocks=blocks)

@app.route("/api/info")
def api_info():
    return jsonify(rpc("get_info"))

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8081)
