# StudyBud Web App

Barebones button-based UI for ESP32 communication with event logging.

## Features

- **WebSocket Connection**: Connect to ESP32 via WebSocket
- **Timer Control**: Start/Pause/Reset/Skip timer
- **Todo Management**: Add/Complete/Delete todos
- **Water Tracking**: Track water intake with goals
- **Breathing Exercises**: Start/Stop breathing sessions
- **Preset Management**: Select/Add/Delete timer presets
- **Settings Control**: Adjust brightness, volume, idle timeout
- **Event Log**: Timestamped log of all messages (persists to localStorage)
- **Export**: Download event log as JSON

## Development

```bash
npm run dev
```

Vite will proxy WebSocket connections to `ws://localhost:80/ws` when running on localhost.

## Building

```bash
npm run build
```

## Usage

1. Open the app in a browser
2. Click "Connect" to establish WebSocket connection
3. Use the control buttons to interact with ESP32
4. View event log for message history
5. Export log for debugging

## Console Feedback

All messages are logged to browser console:
- **Green**: Messages sent to ESP32
- **Blue**: Messages received from ESP32
- **Timestamps**: ISO8601 format
- **JSON**: Pretty-printed for readability

## Event Log

- Stored in localStorage (persists across sessions)
- Maximum 500 events
- Export as JSON for analysis
- Filter by direction (in/out)