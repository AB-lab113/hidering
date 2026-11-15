from flask import Flask, render_template_string, jsonify, send_from_directory
import os, sys, time, threading, requests

app = Flask(__name__, static_folder=None)  # icons go through the fixed routes below only
DAEMON = os.environ.get("HRG_DAEMON_RPC")
if not DAEMON:
    sys.exit("HRG_DAEMON_RPC is required, e.g. http://host.docker.internal:19741/json_rpc")

RECENT_BLOCKS = 20
CACHE_TTL = 5  # seconds; every page view inside the window reuses one snapshot
_cache = {"at": 0.0, "data": None}
_cache_lock = threading.Lock()

def rpc(method, params=None):
    try:
        r = requests.post(DAEMON, json={"jsonrpc":"2.0","id":"0","method":method,"params":params or {}}, timeout=10)
        return r.json().get("result", {})
    except Exception:
        return {}

def snapshot():
    # 2 RPC calls per refresh (get_info + one header range), at most once per CACHE_TTL.
    # The lock also coalesces concurrent requests onto a single refresh.
    with _cache_lock:
        now = time.monotonic()
        if _cache["data"] is not None and now - _cache["at"] < CACHE_TTL:
            return _cache["data"]
        info = rpc("get_info")
        height = info.get("height", 0)
        blocks = []
        if height > 0:
            r = rpc("get_block_headers_range", {"start_height": max(0, height-RECENT_BLOCKS), "end_height": height-1})
            for hdr in r.get("headers", []):
                blocks.append({
                    "height": hdr.get("height", 0),
                    "hash": hdr.get("hash", ""),
                    "num_txes": hdr.get("num_txes", 0),
                    "block_size": hdr.get("block_size", 0),
                    "reward": hdr.get("reward"),
                })
        blocks.reverse()
        _cache["data"] = (info, blocks)
        _cache["at"] = now
        return _cache["data"]

def fmt_hrg(atomic):
    # Atomic units (1 HRG = 10^12) to a decimal string, integer arithmetic only.
    if not isinstance(atomic, int) or isinstance(atomic, bool) or atomic < 0:
        return None
    whole, frac = divmod(atomic, 10**12)
    frac_s = f"{frac:012d}".rstrip("0")
    return f"{whole}.{frac_s}" if frac_s else str(whole)

HTML = """<!DOCTYPE html>
<html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>HIDERING Explorer — HRG block explorer</title>
<meta name="description" content="HIDERING Explorer — HRG block explorer. Latest blocks of the HIDERING (HRG) mainnet, read live from an HRG node.">
<link rel="icon" href="/favicon.ico?v=2" sizes="any">
<link rel="icon" type="image/png" sizes="16x16" href="/favicon-16.png?v=2">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon-32.png?v=2">
<link rel="icon" type="image/png" sizes="48x48" href="/favicon-48.png?v=2">
<link rel="icon" type="image/png" sizes="192x192" href="/favicon-192.png?v=2">
<link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png?v=2">
<link rel="manifest" href="/site.webmanifest?v=2">
<meta name="theme-color" content="#000000">
<meta property="og:image" content="https://explorer.hidering.org/og-image.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:image" content="https://explorer.hidering.org/og-image.png">
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#080808;color:#D4CFC0;font-family:Inter,sans-serif;padding:2rem}
h1{font-family:Georgia,serif;color:#C9A84C;font-size:2rem;margin-bottom:0.5rem;display:flex;align-items:center;gap:0.6rem}
h1 img{width:32px;height:32px}
.sub{color:#7A7060;font-size:0.85rem;margin-bottom:2rem}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;margin-bottom:2rem}
.card{background:#161616;border:1px solid rgba(201,168,76,0.18);padding:1.5rem;border-radius:4px;display:flex;flex-direction:column-reverse;justify-content:flex-end}
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
<h1><img src="/favicon-192.png?v=2" alt="">HIDERING</h1>
<div class="sub">HRG Block Explorer · Mainnet</div>

<div class="cards">
  <!-- label before value in the source (search engines read it in this order); the CSS shows the value on top -->
  <div class="card">
    <div class="card-label">Block Height</div>
    <div class="card-val gold">{{ info.get("height", "—") }}</div>
  </div>
  <div class="card">
    <div class="card-label">Difficulty</div>
    <div class="card-val">{{ info.get("difficulty", "—") }}</div>
  </div>
  <div class="card">
    <div class="card-label">Last Block Reward (HRG, incl. fees)</div>
    <div class="card-val gold">{{ last_reward or "—" }}</div>
  </div>
  <div class="card">
    <div class="card-label">Mempool TXs</div>
    <div class="card-val">{{ info.get("tx_pool_size", "—") }}</div>
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
    info, blocks = snapshot()
    last_reward = fmt_hrg(blocks[0]["reward"]) if blocks else None
    return render_template_string(HTML, info=info, blocks=blocks, last_reward=last_reward)

# Icons, served from ./static (copied by the Dockerfile). Fixed list, no user path.
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")
STATIC_FILES = {
    "favicon.ico": "image/vnd.microsoft.icon",
    "favicon-16.png": "image/png",
    "favicon-32.png": "image/png",
    "favicon-48.png": "image/png",
    "favicon-192.png": "image/png",
    "favicon-512.png": "image/png",
    "apple-touch-icon.png": "image/png",
    "og-image.png": "image/png",
    "site.webmanifest": "application/manifest+json",
}

def _static(name):
    resp = send_from_directory(STATIC_DIR, name, mimetype=STATIC_FILES[name], max_age=86400)
    resp.headers["Cache-Control"] = "public, max-age=86400"
    return resp

for _name in STATIC_FILES:
    app.add_url_rule("/" + _name, "static_" + _name, lambda _n=_name: _static(_n))

@app.route("/api/info")
def api_info():
    return jsonify(snapshot()[0])

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8081)
