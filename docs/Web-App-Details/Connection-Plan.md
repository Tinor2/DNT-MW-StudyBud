# StudyBud — WiFi Communication Architecture

## Overview

StudyBud uses a **WiFi-only** communication model between the ESP32-S3 desk device and two UIs:

1. **Web App** — Svelte SPA (Progressive Web App) served from TF card, cached by browser for offline access
2. **Display UI** — LVGL/C running natively on the ESP32's 480×480 round LCD

The ESP32 runs a WebSocket server on the local network. Both UIs connect to the same ESP32 backend — the ESP32 is the single source of truth.

No BLE, no cloud, no MQTT — one protocol, one stack.

---

## Why WiFi-Only

| Reason | Detail |
|---|---|
| Simplicity | Single communication stack. No BLE coexistence issues, no dual-stack complexity |
| Speed | High bandwidth, low latency on local network (<50ms round-trip) |
| Rich data | Can send large payloads — UI assets, history data, config sync, firmware OTA |
| Desktop native | Works directly with Electron, Tauri, or web dashboard — no BLE dongle hacks |
| Web dashboard | ESP32 can serve a web UI directly (captive portal style), no app install required |
| Mature libraries | ESP-IDF has solid WebSocket server support (`esp_http_server` + WS) |
| Bidirectional | Both sides can push messages at any time |
| Testable | Standard protocols — test with browser DevTools, `wscat`, Postman |
| OTA updates | Firmware updates served over the same WiFi connection |

---

## Architecture

```
┌─────────────────┐         WiFi (LAN)        ┌─────────────────────────┐
│   StudyBud       │◄─────────────────────────►│   Web App (Svelte SPA)  │
│   ESP32-S3       │      WebSocket (TCP)       │                         │
│                  │                            │  First load: from TF    │
│  - WebSocket Srv │                            │  card via ESP32 HTTP    │
│  - HTTP Server   │                            │                         │
│  - mDNS          │                            │  Subsequent loads:      │
│  - App State     │                            │  from browser cache     │
│  - LVGL Display  │                            │  (PWA service worker)   │
│    (480×480 LCD)  │                            │                         │
│                  │                            │  - WS Client            │
│                  │    Encoder Input           │  - REST Client          │
│                  │◄───────────────────┐       │  - mDNS Discovery       │
└─────────────────┘                    │       └─────────────────────────┘
       ▲                               │
       │         ┌─────────────────────┘
       │         │  Rotary Encoder
       │         │  (scroll/select)
       │         │
       │    ┌────┴────┐
       │    │ Encoder  │
       │    └─────────┘
       │
       │  Internal
       │  (LVGL reads
       │   same state)
       ▼
  ┌─────────────┐
  │ LVGL Display │
  │ (on-device)  │
  └─────────────┘
```

**Key:** The ESP32 holds all application state. Both the Web App (via WebSocket) and the LVGL Display (via direct state access) reflect the same data. Neither UI talks directly to the other — all sync goes through the ESP32.

### Web App Accessibility

The web app is served from the ESP32's TF card and cached by the browser as a PWA (Progressive Web App):

| Scenario | Web App | ESP32 Communication |
|---|---|---|
| ESP32 on, phone on same WiFi | Full functionality | WebSocket connected |
| ESP32 off, app previously loaded | Opens from browser cache | Shows "Disconnected" |
| ESP32 off, never loaded before | Can't load (first load needs ESP32) | N/A |
| New phone/browser, never loaded | Can't load (first load needs ESP32) | N/A |

The service worker caches all web app assets (HTML, JS, CSS, fonts) on first load. After that, the app opens from cache even without the ESP32. The ESP32 is only required for the initial load and for real-time WebSocket communication.

---

## Communication Protocol

### WebSocket (Primary — Real-Time)

The ESP32 runs a WebSocket server on port `80` (or configurable). All real-time communication flows through this channel.

**Message Format:** JSON

```json
{
  "type": "timer_update",
  "data": {
    "remaining": 1200,
    "phase": "focus",
    "session": 3
  },
  "timestamp": 1720000000
}
```

**Direction:** Bidirectional — both device and app can push messages at any time.

**Message Categories:**

| Direction | Type | Purpose |
|---|---|---|
| Device → App | `full_sync` | Complete state dump (on reconnect or first connect) |
| Device → App | `timer_update` | Timer state changes |
| Device → App | `status` | Device health, WiFi signal, uptime |
| Device → App | `notification` | Stress management reminders, break alerts |
| Device → App | `stats_update` | Session stats, daily progress |
| Device → App | `todo_sync` | Full todo list state (on connect or change) |
| Device → App | `presets_sync` | Full preset list (on connect or change) |
| Device → App | `water_update` | Water intake state |
| Device → App | `settings_sync` | Full settings state (on connect or change) |
| Device → App | `background_sync` | Available backgrounds + current selection |
| App → Device | `timer_command` | Start, pause, reset, skip |
| App → Device | `settings_update` | Change brightness, sounds, themes |
| App → Device | `mode_change` | Switch between study/break/idle |
| App → Device | `config_sync` | Sync app preferences to device |
| App → Device | `todo_add` | Add a new task |
| App → Device | `todo_update` | Update task (text, done, order) |
| App → Device | `todo_delete` | Remove a task |
| App → Device | `preset_add` | Create a new timer preset |
| App → Device | `preset_update` | Modify an existing preset |
| App → Device | `preset_delete` | Remove a preset |
| App → Device | `preset_select` | Switch active preset |
| App → Device | `water_log` | Log or remove a glass of water |
| App → Device | `water_goal` | Set daily water goal |
| App → Device | `background_select` | Change idle background |
| App → Device | `background_upload` | Upload image to TF card (binary chunks) |
| Encoder → App | `encoder_event` | Rotate (direction), press (short/long) |

### HTTP REST (Secondary — Configuration)

The ESP32 also runs an HTTP server for non-real-time operations.

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/status` | GET | Full device status snapshot |
| `/api/settings` | GET/PUT | Read/write device settings |
| `/api/stats` | GET | Historical study statistics |
| `/api/todos` | GET | Get all todos (full state) |
| `/api/presets` | GET | Get all timer presets |
| `/api/water` | GET | Get water tracker state |
| `/api/backgrounds` | GET | List available backgrounds |
| `/api/backgrounds/upload` | POST | Upload background image (multipart) |
| `/api/firmware` | POST | OTA firmware upload |
| `/api/config` | GET/PUT | Device configuration |
| `/webapp/*` | GET | Serve Svelte app static files from TF card |
| `/` | GET | Redirect to `/webapp/` or captive portal |

---

## WiFi Provisioning (First-Time Setup)

The single biggest UX challenge is getting WiFi credentials onto the device. StudyBud uses a **captive portal** approach.

### Flow

1. On first boot (or after factory reset), ESP32 enters **AP mode**
2. Device broadcasts SSID: `StudyBud-Setup`
3. User connects phone/laptop to this network
4. Browser auto-opens captive portal page
5. User selects their home WiFi network and enters password
6. Credentials are stored in NVS (persistent across reboots)
7. ESP32 reboots into **STA mode**, connects to home WiFi
8. Device advertises itself via mDNS as `studybud.local`

### Network Modes

| Mode | When | Behavior |
|---|---|---|
| STA (Station) | Normal operation | Connects to home WiFi as a client |
| AP (Access Point) | First boot / provisioning | Creates its own network for setup |
| STA+AP | Fallback | Runs both if STA connection fails repeatedly |

---

## Device Discovery

### mDNS

Once connected to the home network, the device registers itself via mDNS:

- **Hostname:** `studybud.local`
- **Service:** `_studybud._tcp`
- **Port:** 80

Apps scan for this service to auto-discover the device without requiring manual IP entry.

### Fallback

If mDNS fails (some routers block multicast):

- App shows a list of discovered devices on the network
- Manual IP entry option
- QR code on device display showing connection info (future)

---

## WebSocket Authentication

The ESP32 WebSocket server uses **token-based authentication via query string** with ESP-IDF's pre-handshake callback.

### Flow

1. Client logs in via REST API: `POST /api/auth/login` with password
2. Server validates password against NVS, returns a session token
3. Client connects to WebSocket: `ws://studybud.local/ws?token=<session_token>`
4. ESP-IDF's `ws_pre_handshake_cb` extracts the token from the query string
5. Validates against stored token in NVS
6. Returns `ESP_OK` (handshake proceeds) or `ESP_FAIL` (connection rejected)

### Implementation

```c
// Enable in sdkconfig.defaults:
// CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT=y

static esp_err_t ws_pre_handshake_cb(httpd_req_t *req) {
    char token[64];
    if (httpd_query_key_value(req->uri, "token", token, sizeof(token)) != ESP_OK) {
        return ESP_FAIL;  // No token provided
    }
    return validate_session_token(token) ? ESP_OK : ESP_FAIL;
}

static const httpd_uri_t ws_uri = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .is_websocket = true,
    .ws_pre_handshake_cb = ws_pre_handshake_cb,
};
```

### Notes

- Token is a simple session string stored in NVS (single-user auth — one password for the device)
- Pre-handshake rejection means no unauthenticated WebSocket connections are ever established
- This approach is modular — can be swapped for JWT headers or cookies if the project expands beyond local-only use

---

## Reliability

### Reconnection Logic

| Scenario | Behavior |
|---|---|
| WiFi drop | ESP32 auto-reconnects to saved AP (~10-30s) |
| Router reboot | ESP32 retries connection with exponential backoff |
| WebSocket disconnect | Client auto-reconnects with backoff (1s → 2s → 4s → ... → 30s max) |
| Power outage | Device boots, reads NVS, reconnects to WiFi automatically |
| WiFi password change | Requires re-provisioning via captive portal |

### Connection Health

- **WebSocket ping/pong** every 30 seconds to detect dead connections
- **Heartbeat messages** every 5 seconds with device status (uptime, WiFi RSSI, free heap)
- **Dead connection detection** — if no pong received within 60 seconds, client disconnects and retries

### Persistence

- WiFi credentials stored in **NVS** (Non-Volatile Storage) — survive reboots
- Device settings stored in NVS
- Study statistics stored in NVS or SPIFFS/LittleFS
- Web app assets stored on TF card (FAT filesystem) and **cached by browser** (PWA service worker)
- Session token stored in NVS (single-user auth)

---

## Dual-UI Sync

StudyBud has two UIs that must stay synchronized:

### The Problem

When a user starts a timer on the web app, the LVGL display must immediately show it running. When the user scrolls through tasks on the display, the web app must reflect the current task. Both UIs read from and write to the same state.

### The Solution: ESP32 as Single Source of Truth

```
Web App ──WS──► ESP32 (app_state_t) ──► LVGL Display
     ◄──WS──              │
                          └──► NVS (persistent storage)
```

The ESP32 holds all application state in a single `app_state_t` struct in RAM. Both UIs:
1. **Read** from this struct to render their UI
2. **Write** commands via WebSocket (web app) or encoder events (display)
3. **Receive** state updates when any change occurs

### Sync Rules

| Rule | Detail |
|---|---|
| One source of truth | ESP32 `app_state_t` — never duplicated or cached elsewhere |
| Broadcast on change | Any state change is broadcast to all connected WebSocket clients |
| LVGL reads directly | The display reads `app_state_t` directly (no WebSocket round-trip) |
| Conflict resolution | Last-write-wins — ESP32 applies changes sequentially |
| Offline operation | LVGL display works without WiFi (reads/writes local state directly) |

### What Goes Where

| UI | Optimized For | Input Method |
|---|---|---|
| Web App | Complex interactions (typing, drag-drop, multi-select) | Touch, mouse, keyboard |
| LVGL Display | Quick glance + simple interactions (scroll, select) | Rotary encoder |

The display shows a **simplified view** of the same data — large font, fewer elements, encoder-friendly navigation. The web app shows the **full-featured view** — detailed lists, charts, editors.

---

## Known Limitations

| Limitation | Impact | Mitigation |
|---|---|---|
| Same network required | App can't connect from outside the house | VPN or future cloud relay |
| WiFi power draw | ~100-200mA active. Not battery-friendly | Acceptable for a desk device (always plugged in) |
| Router dependency | Connection breaks if router fails | ESP32 reconnects automatically; LVGL display continues operating standalone |
| No remote access | Can't check device from outside home | Out of scope for Phase 1 |
| Display offline | Web app can't connect if WiFi is down | LVGL display works independently — timer, todos, water tracker all function without WiFi |

---

## Technology Stack

| Layer | Technology |
|---|---|
| MCU | ESP32-S3 (ESP-IDF v6.0.1) |
| WiFi | ESP-IDF `esp_wifi` (STA + AP modes) |
| WebSocket Server | ESP-IDF `esp_http_server` with WebSocket upgrade |
| HTTP Server | ESP-IDF `esp_http_server` (serves TF card static files) |
| mDNS | ESP-IDF `mdns` component |
| Provisioning | Custom captive portal (HTTP server in AP mode) |
| Storage | NVS (credentials, settings, stats) + TF card (FAT, web assets, backgrounds) |
| Web App | Svelte SPA (Vite, PWA with service worker caching) |
| Display UI | LVGL v8.2.0 (C, runs natively on ESP32) |
| Web App | Svelte (compiled via Vite, served from TF card, PWA cached) |
| Input | Rotary encoder (display navigation), browser (web app) |

---

## Future Expansion (Out of Scope)

If the project grows beyond Phase 1:

- **Phase 2: BLE** — Add BLE for mobile app (works without WiFi), use WiFi only for desktop
- **Phase 3: Cloud** — MQTT broker for remote access, data logging, IoT integration
- **Phase 4: Multi-device** — Multiple StudyBud devices synced via MQTT or cloud
