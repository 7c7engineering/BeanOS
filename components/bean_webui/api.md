# API Specification: Skybean Flight Logger
**Version:** 1.0.0
**Protocol:** HTTP/1.1 & WebSocket (RFC 6455)
**Base HTTP URL:** `http://<skybean-ip-or-hostname>` (SoftAP default: `192.168.4.1`, mDNS: `skybean.local`)
**WebSocket URL:** `ws://<skybean-ip-or-hostname>/ws`

---

## 1. Architecture Overview

Communication is split into two pathways:

* **Control channel (inbound):** REST-like HTTP requests. Responses confirm the command immediately.
* **Telemetry channel (outbound):** a single persistent WebSocket endpoint broadcasting live sensor data to all connected clients.

The web UI is a **single self-contained `index.html`** (no external assets). It runs in two modes:

* **Served from the device** at `http://<host>/` — API host is taken from `location.host`.
* **Opened locally** from `file://` during development — the user enters the device's IP/hostname in the connect bar (persisted in `localStorage`). All `/api` responses carry `Access-Control-Allow-Origin: *` and `OPTIONS /api/*` answers CORS preflights, so a locally-opened page can talk to the device.

### Sensor hardware

| Sensor | Bus | Role |
|--------|-----|------|
| **BMP390** | I²C | Barometer — pressure (Pa) and temperature (°C) |
| **BMI088** | I²C | IMU — acceleration (m/s²) and angular rate (rad/s), 3 axes |
| VBAT ADC | — | Battery voltage (mV), sampled by the battery monitor task |

Sensors are polled by a dedicated sampler task at `bean_webui.sensor_update_hz` (config, default 5 Hz, clamped 1–10).

---

## 2. HTTP Control API

### 2.1 Get device status
* **Endpoint:** `GET /api/status`
* **Success Response (200 OK):**
  ```json
  {
    "armed": false,
    "flight_state": "pre_launch",
    "battery_mv": 4120,
    "usb_powered": true,
    "charging": false,
    "uptime_ms": 123456,
    "fw_version": "6686859",
    "idf_version": "v6.0.1",
    "free_heap": 148000
  }
  ```
  * `armed` (bool): logger arm flag.
  * `flight_state` (string): currently `"pre_launch"` or `"armed"`. Future firmware adds `"ascending"`, `"drogue_deployed"`, `"main_deployed"`, `"landed"` — **clients must tolerate unknown values**.
  * `battery_mv` (int): battery voltage in millivolts; `-1` until the first measurement (~5 s after boot).
  * `uptime_ms` (int): milliseconds since boot.
  * `free_heap` (int): free heap bytes (monitor for leaks).

### 2.2 Arm / disarm the logger
* **Endpoints:** `POST /api/arm`, `POST /api/disarm`
* **Body:** none.
* **Success Response (200 OK):** the full status object (§2.1) reflecting the new state.
* Note: arming is currently a **stub flag** — take-off detection and logger gating are not implemented yet. The API shape will not change when they are.

### 2.3 List flight logs
* **Endpoint:** `GET /api/logs`
* **Success Response (200 OK):**
  ```json
  {"files":[
    {"name":"log_d001.csv","size":18432,"active":false},
    {"name":"log_d002.csv","size":512,"active":true}
  ]}
  ```
  * Only `log_*.csv` files are listed (`log_dNNN` = data, `log_eNNN` = events).
  * `active` (bool): file is currently open for writing by the logger. Active files can be downloaded (content up to the last 1 s flush) but not deleted.

### 2.4 Download a log
* **Endpoint:** `GET /api/logs/download?name=<file>`
* **Success Response (200 OK):** chunked `text/csv` stream with `Content-Disposition: attachment`. No `Content-Length` (an active log grows while streaming).
* **Errors:** `400` invalid name (only `[A-Za-z0-9._-]`, no leading dot, < 32 chars), `404` no such file.

### 2.5 Delete a log
* **Endpoint:** `DELETE /api/logs?name=<file>`
* **Success Response (200 OK):** `{"deleted":true}`
* **Errors:** `400` invalid name, `404` no such file, `409 Conflict` `{"error":"log file is active"}` when the logger holds the file open.

### 2.6 Read / update device configuration
* **Endpoint:** `GET /api/config`
* **Success Response (200 OK):** the full live configuration tree (contents of `conf.json` merged over firmware defaults). Note: includes the WiFi credentials in plain text.

* **Endpoint:** `POST /api/config`
* **Request body:** a JSON object with any subset of the configuration tree, e.g.:
  ```json
  {"bean_webui":{"wifi":{"sta":{"enabled":true,"ssid":"MyNet","password":"secret"}}}}
  ```
  Values are deep-merged into the live config: unknown keys are ignored, type mismatches keep the current value. On change the result is persisted to `conf.json`. Body limit 4 KB. Most settings (WiFi, sample rate) take effect **after reboot** (see §2.7).
* **Success Response (200 OK):** `{"updated":3,"ignored":0,"type_mismatches":0}`
* **Errors:** `400` missing/oversized body or invalid JSON, `500` persist failure.

#### WiFi mode (`bean_webui.wifi.mode`)
| Value | Behavior |
|---|---|
| `"ap"` | Access point only (field default without infrastructure). |
| `"apsta"` | AP always on **and** join the configured `wifi.sta` network (default). |
| `"sta"` | Join the configured network; if it cannot be joined after ~5 attempts the device automatically starts its AP as a fallback so it stays reachable in the field (STA keeps retrying up to 10 attempts total). |

A mode requiring STA with an empty `sta.ssid` falls back to `"ap"` with a warning.

### 2.7 Reboot
* **Endpoint:** `POST /api/reboot`
* **Success Response (200 OK):** `{"rebooting":true}` — the device restarts ~0.5 s later. Used to apply saved WiFi settings in the field.

### 2.8 CORS preflight
* **Endpoint:** `OPTIONS /api/*`
* **Response (200 OK, no body)** with:
  ```
  Access-Control-Allow-Origin: *
  Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS
  Access-Control-Allow-Headers: Content-Type
  ```
  Every `/api` response (including errors) also carries `Access-Control-Allow-Origin: *`.

---

## 3. WebSocket Streaming Protocol (`/ws`)

Push-only telemetry; the device ignores inbound data frames (all commands go over HTTP). The number of concurrent clients is bounded by the HTTP server's socket pool (6 sockets shared with regular HTTP requests) — treat **2 concurrent telemetry clients** as the practical design limit.

One JSON text frame per sample tick (default 5 Hz):

```json
{"t":123456,"p":101325.20,"tc":24.51,
 "ax":0.12,"ay":-0.03,"az":9.81,
 "gx":0.001,"gy":0.002,"gz":0.000,
 "vbat":4120,"armed":false,"state":"pre_launch"}
```

* `t` (int): milliseconds since boot (same clock as log timestamps and `uptime_ms`).
* `p` (float): pressure in Pa. `tc` (float): temperature in °C.
* `ax..az` (float): acceleration in m/s². `gx..gz` (float): angular rate in rad/s.
* `vbat` (int): battery millivolts (`-1` until first sample).
* `armed` (bool) / `state` (string): same semantics as `/api/status`.
* On sensor read errors the previous (stale) values are re-sent; the device logs the condition.

---

## 4. Log file CSV format

Data logs (`log_dNNN.csv`): header `timestamp,measurement_type,value`, rows `<ms-since-boot>,<type>,<value>`.

| measurement_type | Meaning |
|---|---|
| 0 | temperature (°C) |
| 1 | pressure (Pa) |
| 2 | altitude (m) |
| 3 | acceleration |
| 4 | gyroscope |
| 5 | battery voltage (mV) |

Event logs (`log_eNNN.csv`): header `timestamp,event_id,event_data`.

---

## 5. Client Implementation Rules

* **Reconnection:** expect sudden drops (WiFi loss, device reset, flight). Reconnect the WebSocket with backoff and re-fetch `/api/status` after reconnecting.
* **Timestamps:** all device timestamps are milliseconds since boot, not wall-clock time.
* **State changes from other clients:** arm/disarm can happen from any client; the telemetry stream carries `armed`/`state`, so UIs should reconcile against it.
* **USB MSC exclusivity:** when the device boots into USB mass-storage mode the web UI (and this API) is not running.
