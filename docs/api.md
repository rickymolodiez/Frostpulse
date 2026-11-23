# FrostPulse API contract

This file is the agreed contract between the firmware, the cloud, and the web pages.
If you change a payload, update it here in the same PR.

## Conventions

- Base URL is set per device (`API_BASE_URL` in `config.h`, e.g. `http://192.168.1.50:3000`).
- Request and response bodies are JSON (`Content-Type: application/json`).
- Auth: if the device has an `API_KEY` set, it sends `Authorization: Bearer <key>`.
  The cloud should reject a missing or wrong key with `401` once keys are turned on.
- Timestamps are UTC ISO 8601 (`2026-11-23T02:37:10Z`).
- Units: temperature in °C, relative humidity in %, coordinates in decimal degrees (WGS84),
  durations in milliseconds.

---

## Environment node (`firmware/env-node`)

The node (TTGO ESP32 + DHT11) takes one **observation** per upload interval and keeps
it in a RAM queue (30 by default) until the cloud accepts it. If the network is down,
observations pile up and are sent oldest-first once it comes back. When the queue is
full, the oldest observation is dropped and `dropped` goes up.

> The readings are context for road reports. They do **not** detect ice. The DHT11 is
> unreliable below 0 °C, and air humidity alone can't tell whether a road is slippery.

### `POST /api/readings`: one observation

```json
{
  "node_id": "env-01",
  "boot_id": "a1b2c3d4",
  "seq": 42,
  "observed_at": "2026-11-23T02:37:10Z",
  "age_ms": 1200,
  "status": "ok",
  "temp_c": 3.0,
  "humidity": 81.0,
  "dew_point_c": 0.0,
  "in_rated_range": true,
  "sensor_age_ms": 800,
  "sensor_failures": 0,
  "lat": 45.497000,
  "lon": -73.578900,
  "rssi": -61,
  "uptime_ms": 3600400,
  "config_version": 2,
  "queue_depth": 1,
  "dropped": 0,
  "config_error": null
}
```

| Field | Type | Notes |
|-------|------|-------|
| `node_id` | string | Unique per node (`NODE_ID`). |
| `boot_id` | string | 8 hex characters, random on every boot. |
| `seq` | integer | Observation number, starting at 1 on each boot. **A retry sends the same `seq`.** |
| `observed_at` | string \| null | When the observation was taken (device NTP time). `null` if the device never synced NTP. |
| `age_ms` | integer | How long ago it was taken, measured when it was sent. Use it if `observed_at` is `null`: `observed_at = received_at − age_ms`. |
| `status` | `"ok"` \| `"stale"` \| `"no_data"` | Sensor health when the observation was taken, see below. |
| `temp_c` | number \| null | `null` unless `status == "ok"`. |
| `humidity` | number \| null | 0–100. `null` unless `status == "ok"`. |
| `dew_point_c` | number \| null | Magnus formula. `null` unless `status == "ok"`. |
| `in_rated_range` | boolean \| null | `false` if outside the DHT11 rating (0–50 °C, 20–90 % RH): low confidence. `null` unless `ok`. |
| `sensor_age_ms` | integer \| null | Age of the last good sensor read when the observation was taken. `null` for `no_data`. |
| `sensor_failures` | integer | Failed or rejected sensor reads in a row. |
| `lat`, `lon` | number | Where the node is installed (set in config, not GPS). |
| `rssi` | integer | Wi-Fi signal in dBm when sent. |
| `uptime_ms` | integer | Time since boot when sent. |
| `config_version` | integer | Settings version in use (0 = built-in defaults). |
| `queue_depth` | integer | Observations waiting, including this one. Above 1 means it's catching up after an outage. |
| `dropped` | integer | Observations lost since boot because the queue overflowed. |
| `config_error` | string \| null | Why the last settings update was rejected, or `null`. |

#### Sensor status

| `status` | Meaning | How the cloud should treat it |
|----------|---------|-------------------------------|
| `ok` | Fresh reading (newer than `stale_after_ms`) | Store it and show it. |
| `stale` | Sensor has failed for longer than `stale_after_ms` | Don't show values as current. Mark the node "sensor stale". |
| `no_data` | No good reading since boot (sensor missing or broken) | Same as `stale`. |

The firmware sends `null` values for `stale` and `no_data` on purpose, so an old
temperature can never be mistaken for a current one. It throws out a reading when the
sensor doesn't respond, when the value is out of range (−20 to 60 °C), or when the
temperature jumps more than 10 °C from the last fresh reading.

#### Responses

| Code | Meaning | What the device does |
|------|---------|----------------------|
| `201` | Stored | Removes it from the queue and sends the next one |
| `200` | Duplicate, already stored | Same as `201` |
| `400` / `422` | Payload invalid | **Drops it** (retrying won't help) and logs it |
| `401`, `408`, `429`, `5xx`, network error | Temporary problem | Keeps it, retries after 5 s |

**Duplicates:** `(node_id, boot_id, seq)` identifies an observation. If the cloud gets
one it already stored (the response was lost, so the device sent it again), it should
reply `200` and not store it twice.

#### Is the data current? (cloud side)

- **Sensor stale:** the latest observation has `status` `stale` or `no_data`.
- **Node offline:** no request from the node for 3 × `upload_interval_ms` (90 s by default).
- **Old observation:** `age_ms` is larger than `upload_interval_ms`. It's a late upload
  after an outage, so store it at its `observed_at` time and don't treat it as the
  latest reading.

The dashboard should show these three states differently.

### `GET /api/settings/{node_id}`: remote configuration

The node asks for this every 60 s (`SETTINGS_POLL_MS`) while it's online, and right
after it connects.

```json
{
  "version": 3,
  "read_interval_ms": 2000,
  "upload_interval_ms": 30000,
  "stale_after_ms": 10000
}
```

| Field | Required | Allowed values |
|-------|----------|----------------|
| `version` | yes | Positive integer. **Bump it on every change.** The node only applies a version that differs from the one it has. |
| `read_interval_ms` | no | 1000–600000 (DHT11 needs at least 1 s between reads) |
| `upload_interval_ms` | no | 5000–3600000 |
| `stale_after_ms` | no | ≥ 2 × `read_interval_ms` and ≤ 3600000 |

- Leaving a field out keeps the node's current value.
- An invalid document is **rejected as a whole**. The node keeps its old settings and
  reports the reason in `config_error` on every reading until a valid version arrives.
- Accepted settings are saved in the node's flash and survive a reboot.
  `config_version` in each reading confirms which version is running.
- `404` means no settings are set for this node, so it keeps what it has.
- Respond `200` with the document. Other codes are logged and retried at the next poll.

---

## Other endpoints (to be defined)

These are planned in `cloud/src/routes/` and still need a contract from their owners:

- `POST /api/reports`: rider button reports (double press) from `rider-device`
- `GET /api/alerts`, `POST /api/acks`: alerts sent to riders and their acknowledgements
- `GET /api/hazards`: active hazard zones (used by the dashboard and `tools/seed_hazards.json`)
- `POST /api/locations`: phone GPS updates from `web/phone`
