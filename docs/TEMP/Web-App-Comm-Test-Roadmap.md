# Web App + ESP32 Communication Test — Roadmap

## Goal

Prove the ESP32 can run an HTTP + WebSocket server on WiFi, and a Svelte web app can connect and exchange messages. No UI polish — bare bones functional only.

---

## Architecture

```
┌──────────────────────┐         WiFi (LAN)        ┌──────────────────────┐
│   ESP32-S3           │◄─────────────────────────►│   Svelte Web App     │
│                      │      WebSocket (TCP)       │   (Vite dev server)  │
│  - WiFi STA mode     │                            │                      │
│  - HTTP Server :80   │   ┌──────────────────┐     │  - WS client store   │
│  - WebSocket /ws     │   │  /api/status     │     │  - REST fetch        │
│  - Heartbeat 5s      │◄──│  GET → JSON      │     │  - Auto-reconnect    │
│  - Echo handler      │   └──────────────────┘     │  - Message log       │
│                      │                            │                      │
│  LVGL Display        │                            │  No UI framework     │
│  (unchanged)         │                            │  Just test buttons   │
└──────────────────────┘                            └──────────────────────┘
```

---

## Phase 1: ESP32 Firmware — WiFi + Server

### 1.1 Update sdkconfig.defaults

Enable required ESP-IDF components:
- `CONFIG_ESP_WIFI=y` — WiFi support
- `CONFIG_ESP_HTTP_SERVER=y` — HTTP server
- `CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT=y` — WebSocket pre-handshake (for future auth)
- `CONFIG_NVS_ENCRYPTION=n` — simple NVS (no encryption)
- `CONFIG_MDNS=y` — mDNS for `studybud.local`

### 1.2 Create main/networking/wifi_manager.c + .h

WiFi Station mode manager:
- `wifi_manager_init(const char *ssid, const char *password)` — NVS flash, netif, WiFi STA init
- Event handlers: connected, disconnected (auto-reconnect with 5s delay), got IP
- Logs IP address to serial on connect
- Provides `wifi_manager_get_ip()` getter

### 1.3 Create main/networking/web_server.c + .h

HTTP + WebSocket server:
- `web_server_init()` — starts httpd on port 80
- WebSocket endpoint at `/ws`:
  - `ping` → responds `pong`
  - `echo` → echoes back the data field
  - Any unknown type → logs and responds with error
  - Heartbeat broadcast every 5 seconds: `{"type":"heartbeat","uptime_ms":...,"free_heap":...,"wifi_rssi":...}`
- REST endpoint at `/api/status`:
  - GET returns `{"status":"ok","uptime_ms":...,"free_heap":...,"wifi_rssi":...}`

### 1.4 Update main/CMakeLists.txt

- Register new source files: `wifi_manager.c`, `web_server.c`
- Add REQUIRES deps: `esp_wifi`, `esp_http_server`, `nvs_flash`, `esp_netif`

### 1.5 Update main/main.cpp

- Include `wifi_manager.h` and `web_server.h`
- Call `wifi_manager_init("SSID", "PASSWORD")` before display init
- Call `web_server_init()` after WiFi connects
- Display IP on serial

---

## Phase 2: Svelte Web App

### 2.1 Scaffold web-app/

Vite + Svelte project at `web-app/`:
```
web-app/
├── package.json
├── vite.config.js
├── index.html
└── src/
    ├── main.js
    ├── App.svelte
    └── lib/
        ├── stores/websocket.js
        └── api.js
```

### 2.2 App.svelte — Bare-Bones Test UI

Minimal HTML (no CSS framework):
- Connection status text (Connected / Disconnected / Reconnecting...)
- Log panel showing all received messages
- Input field + Send button to send arbitrary JSON
- Quick-test buttons: Ping, Echo "hello", GET /api/status

### 2.3 websocket.js Store

- Connects to `ws://{hostname}/ws`
- Auto-reconnect with exponential backoff (1s → 2s → 4s → 30s max)
- Parses incoming JSON, exposes as reactive store
- Send function for outbound messages

### 2.4 vite.config.js

- Dev proxy: `/ws` → `ws://{ESP32_IP}:80`
- `/api` → `http://{ESP32_IP}:80`
- Production: plain build to `dist/`

---

## Phase 3: Test & Verify

1. Flash firmware, connect ESP32 to WiFi, note IP from serial
2. Run `npm run dev` from `web-app/` on same network
3. Open browser — verify WebSocket connects, heartbeat arrives
4. Send messages — ESP32 echoes back
5. GET `/api/status` — JSON response

---

## Message Protocol (Subset for Test)

### Device → App
| Type | Payload | When |
|---|---|---|
| `heartbeat` | `{uptime_ms, free_heap, wifi_rssi}` | Every 5s |
| `pong` | `{}` | In response to ping |
| `echo` | `{data: "..."}` | Echo back |

### App → Device
| Type | Payload | When |
|---|---|---|
| `ping` | `{}` | User clicks Ping button |
| `echo` | `{data: "hello"}` | User clicks Echo button |

---

## Files Changed/Created

| File | Action |
|---|---|
| `sdkconfig.defaults` | Modified (add WiFi/HTTP/WS/NVS/mDNS) |
| `main/networking/wifi_manager.c` | Created |
| `main/networking/wifi_manager.h` | Created |
| `main/networking/web_server.c` | Created |
| `main/networking/web_server.h` | Created |
| `main/CMakeLists.txt` | Modified (add new sources + deps) |
| `main/main.cpp` | Modified (init WiFi + server) |
| `web-app/package.json` | Created |
| `web-app/vite.config.js` | Created |
| `web-app/index.html` | Created |
| `web-app/src/main.js` | Created |
| `web-app/src/App.svelte` | Created |
| `web-app/src/lib/stores/websocket.js` | Created |
| `web-app/src/lib/api.js` | Created |
