# HIDERING DNS seed configuration — `hidering.org`

The HIDERING daemon discovers initial mainnet peers by resolving a fixed
list of hostnames (compiled into the binary) to A records, then connecting
to the returned IPs on the P2P port `19740`. No IP literal is hardcoded
in the binary, so seed re-scheduling on Flux only requires a DNS update,
never a release.

## Hostnames the daemon queries

Defined in `src/p2p/net_node.h` (`m_seed_nodes_list`):

| Host | Purpose |
| --- | --- |
| `seed1.hidering.org` | Primary Flux instance (currently `hideringseed1`) |
| `seed2.hidering.org` | Reserved for second instance |
| `seed3.hidering.org` | Reserved for third instance |
| `seed4.hidering.org` | Reserved for fourth instance |

The daemon resolves all four in parallel with a `CRYPTONOTE_DNS_TIMEOUT_MS`
deadline. Names that don't resolve are silently skipped. Until additional
seeds are deployed, only `seed1` needs to exist.

## Records to create at the registrar

For each hostname above:

| Type | Name | Value | TTL |
| --- | --- | --- | --- |
| `A` | `seed1` | live Flux IP of `hideringseed1` (see below) | **300** |
| `A` | `seed2` | (omit until seed2 is deployed) | 300 |
| `A` | `seed3` | (omit until seed3 is deployed) | 300 |
| `A` | `seed4` | (omit until seed4 is deployed) | 300 |

A single hostname may carry multiple A records (round-robin). The daemon
iterates every IP returned. Use this for redundancy if a single Flux app
runs multiple instances.

### Source of truth for the seed1 IP

Flux re-schedules an app onto a different node on every delete/recreate
(observed: 3 distinct IPs in one session). Always pull the current IP
from the Flux API before updating the A record:

```
curl -s https://api.runonflux.io/apps/location/hideringseed1 \
  | jq -r '.data[0].ip'
```

Today (2026-05-04) this returns `149.154.177.170`.

### TTL = 300 s

Five-minute TTL keeps DNS propagation aligned with how often Flux can
move the app. Lower TTL is wasteful (lookup latency), higher TTL leaves
new clients pointing at a dead IP for longer than needed.

### DNSSEC

Not required by the daemon (`get_ipv4` reads the result regardless of
`dnssec_valid`), but recommended because the rest of HIDERING already
goes through DNSSEC-validating resolvers (see `src/common/dns_utils.cpp`,
commit `aaebbec00`). Enable DNSSEC on `hidering.org` if the registrar
supports it; do not gate seed bootstrap on it.

## Operations runbook

### Flux re-scheduled `hideringseed1` — what do I update?

1. Re-fetch the new IP: `curl -s https://api.runonflux.io/apps/location/hideringseed1 | jq -r '.data[0].ip'`
2. Update the `seed1.hidering.org` A record at the registrar to the new IP.
3. Wait for TTL (≤ 5 min). Verify with `dig +short seed1.hidering.org`.

### Adding seed2 (or 3, or 4)

1. Deploy a new Flux app (use a distinct name, e.g. `hideringseed2`) with
   the same Docker image as `hideringseed1`.
2. Pull its IP from `https://api.runonflux.io/apps/location/hideringseed2`.
3. Create the A record `seed2.hidering.org → <ip>`, TTL 300.
4. No daemon change required — the binary already queries seed1..4.

### A user is behind a network that blocks DNS resolution

Tell them to start the daemon with explicit seed pinning:

```
hideringd --seed-node <ip>:19740
```

…using any current Flux IP (or a known-good community node).

### Verify what the daemon will see

```
dig +short seed1.hidering.org
dig +short seed2.hidering.org seed3.hidering.org seed4.hidering.org
```

Anything that resolves becomes a bootstrap peer. Anything that doesn't
is logged at `MWARNING` level and skipped — non-fatal.

## Why no IP fallback in the binary

`net_node.inl::get_ip_seed_nodes()` returns an empty set for mainnet on
purpose. The previous hardcoded `149.154.177.90:19740` aged out the
moment Flux moved the app, leaving fresh clients with a dead pointer
that ships in every release until the next rebuild. Driving bootstrap
through DNS means an IP rotation is a single registrar update, not a
release cycle.

If DNS resolution fails entirely *and* the user has no `--seed-node`
flag *and* the daemon has no cached white/gray peers from a prior run,
bootstrap fails — that is the explicit trade-off. Document the
`--seed-node` escape hatch in the user-facing README before mainnet.

## Automation hint (optional)

A small script can keep the A record in sync with Flux. Pseudocode for
Cloudflare's API:

```bash
# expects $CF_API_TOKEN, $CF_ZONE_ID, $CF_RECORD_ID for seed1.hidering.org
new_ip=$(curl -s https://api.runonflux.io/apps/location/hideringseed1 | jq -r '.data[0].ip')
curl -X PATCH \
  -H "Authorization: Bearer $CF_API_TOKEN" \
  -H "Content-Type: application/json" \
  "https://api.cloudflare.com/client/v4/zones/$CF_ZONE_ID/dns_records/$CF_RECORD_ID" \
  --data "{\"content\":\"$new_ip\",\"ttl\":300,\"type\":\"A\",\"name\":\"seed1.hidering.org\"}"
```

Run hourly via cron on a host with the API token. Adjust for the
registrar in use; most registrars expose an equivalent record-update
endpoint.
