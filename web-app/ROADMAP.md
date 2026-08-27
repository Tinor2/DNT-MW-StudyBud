# StudyBud Web App Roadmap

## Overview
Barebones button-based UI for ESP32 communication with console feedback and event logging.

## Architecture
- **Event Log**: JSON-based log stored in memory (can persist to localStorage)
- **WebSocket**: Bidirectional sync for all screen data
- **Console Feedback**: Log all messages to console + UI log panel

## WebSocket Message Protocol

### Queries (Web → ESP32)
- `ping` - Connection check
- `full_sync` - Get all state
- `get_screen` - Current screen info
- `get_todos` - All todo items
- `get_timer` - Timer state
- `get_presets` - All presets
- `get_breathing` - Breathing exercises + active state
- `get_water` - Water tracking
- `get_settings` - Device settings

### Commands (Web → ESP32)
- `todo_add` - Add new todo
- `todo_update` - Update todo (id, text, done, priority)
- `todo_delete` - Delete todo by id
- `preset_add` - Add new preset
- `preset_update` - Update preset
- `preset_delete` - Delete preset
- `preset_select` - Set active preset
- `timer_command` - Control timer (start/pause/reset/skip)
- `water_log` - Add/remove water glass
- `water_goal` - Set water goal
- `breathing_start` - Start breathing exercise
- `breathing_stop` - Stop breathing exercise
- `settings_update` - Update settings

### Responses (ESP32 → Web)
- `pong` - Ping response with uptime
- `screen_info` - Current screen data
- `todos_info` - All todos
- `timer_info` - Timer state
- `presets_info` - All presets
- `breathing_info` - Breathing exercises + active state
- `water_info` - Water tracking data
- `settings_info` - Device settings
- `full_sync` - Complete state dump
- `todo_toggled` - Broadcast when todo completed
- `*_sync` - Broadcast updates for each type

## UI Sections

### 1. Connection Panel
- Connect/Disconnect buttons
- Status indicator (connected/disconnected/connecting)

### 2. Timer Control
- Start/Pause/Reset/Skip buttons
- Preset selection dropdown
- Timer display (remaining time)

### 3. Todos
- Add todo button → prompts for text
- Todo list with toggle/delete buttons
- Priority selector (1-5)

### 4. Breathing
- Start/Stop buttons
- Exercise selector (1-5)
- Current phase display

### 5. Water Tracker
- Add/Remove glass buttons
- Glass count display
- Goal setting

### 6. Presets
- Add/Update/Delete buttons
- Preset list with select button
- Name/focus/break duration inputs

### 7. Settings
- Brightness slider
- Volume slider
- Idle timeout input

### 8. Event Log
- Timestamped log of all messages
- Filter by type (in/out)
- Clear button
- Auto-scroll

## Implementation Phases

### Phase 1: Core Communication
- [x] WebSocket connection/disconnection
- [x] Query buttons (get_*)
- [ ] Command buttons (todo_add, timer_command, etc.)
- [ ] Response parsing and display

### Phase 2: UI Components
- [ ] Timer control panel
- [ ] Todo list with add/delete/toggle
- [ ] Breathing exercise controls
- [ ] Water tracker
- [ ] Preset manager
- [ ] Settings controls

### Phase 3: Event Logging
- [ ] In-memory event log
- [ ] localStorage persistence
- [ ] Log filtering
- [ ] Export functionality

### Phase 4: Polish
- [ ] Responsive design
- [ ] Keyboard shortcuts
- [ ] Error handling
- [ ] Offline support (localStorage cache)

## Data Flow
1. User clicks button → WebSocket message sent
2. ESP32 processes message → State updated
3. ESP32 broadcasts sync → Web receives update
4. Web updates UI + logs event
5. Console shows message details

## Console Feedback
- All messages logged to console.log
- Color-coded: green (sent), blue (received)
- Timestamps included
- JSON pretty-printed

## Event Log Schema
```json
{
  "timestamp": "ISO8601",
  "direction": "in|out",
  "type": "message_type",
  "data": { ... },
  "source": "user|esp32|system"
}
```