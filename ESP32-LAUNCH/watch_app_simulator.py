"""
Watch App Simulator for DNT-MW-StudyBud
CircuitPython version using displayio and vectorio for ESP32-S3 with round display
"""

try:
    import displayio
    import vectorio
    from adafruit_display_shapes.circle import Circle
    from adafruit_display_text import label
    import terminalio
    import board
    import busio
    import time
    import rtc
    CIRCUITPYTHON_AVAILABLE = True
except ImportError:
    CIRCUITPYTHON_AVAILABLE = False
    print("Note: CircuitPython libraries not available. Running in desktop test mode.")


# App definitions
APPS = [
    {"name": "To-do", "sub": "3 tasks today", "color": 0x7C3AED, "emoji": "✓", "view": "todo"},
    {"name": "Timer", "sub": "Countdown", "color": 0x0891B2, "emoji": "⏱", "view": "timer"},
    {"name": "Counter", "sub": "Click counter", "color": 0x059669, "emoji": "＋", "view": "counter"},
    {"name": "Clock", "sub": "World time", "color": 0xD97706, "emoji": "🕐", "view": "clock2"},
    {"name": "Notes", "sub": "Quick notes", "color": 0xDC2626, "emoji": "✎", "view": "notes"},
]


class WatchAppSimulator:
    """Main watch app simulator class"""
    
    def __init__(self, display, width=480, height=480):
        """
        Initialize the watch app simulator
        
        Args:
            display: displayio.Display object
            width: Display width (default 480 for TL021WVC02)
            height: Display height (default 480 for TL021WVC02)
        """
        self.display = display
        self.width = width
        self.height = height
        self.center_x = width // 2
        self.center_y = height // 2
        
        # State
        self.current_view = "watch"  # "watch", "library", "app"
        self.selected_app = None
        self.lib_index = 0
        self.dial_value = 0
        self.prev_dial_value = 0
        self.timer_secs = 300
        self.timer_running = False
        self.counter_val = 0
        
        # Create main group
        self.main_group = displayio.Group()
        self.display.root_group = self.main_group
        
        # View groups
        self.watch_group = displayio.Group()
        self.library_group = displayio.Group()
        self.app_group = displayio.Group()
        
        self.main_group.append(self.watch_group)
        self.main_group.append(self.library_group)
        self.main_group.append(self.app_group)
        
        # Initialize views
        self._init_watch_face()
        self._init_library()
        
        # Show watch face initially
        self._show_view("watch")
        
        # Start clock update
        if CIRCUITPYTHON_AVAILABLE:
            self._update_clock()
    
    def _show_view(self, view_name):
        """Show the specified view"""
        self.watch_group.hidden = (view_name != "watch")
        self.library_group.hidden = (view_name != "library")
        self.app_group.hidden = (view_name != "app")
        self.current_view = view_name
    
    def _init_watch_face(self):
        """Initialize the watch face view"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        # Background
        bg = Circle(
            pixel_shader=displayio.Palette(2),
            x=self.center_x,
            y=self.center_y,
            r=self.width // 2 - 5
        )
        bg.pixel_shader[0] = 0x0A0A0A  # Dark background
        bg.pixel_shader[1] = 0x0A0A0A
        self.watch_group.append(bg)
        
        # Time label
        self.time_label = label.Label(
            terminalio.FONT,
            text="--:--",
            color=0xFFFFFF,
            scale=4
        )
        self.time_label.anchor_point = (0.5, 0.5)
        self.time_label.anchored_position = (self.center_x, self.center_y - 20)
        self.watch_group.append(self.time_label)
        
        # Date label
        self.date_label = label.Label(
            terminalio.FONT,
            text="---",
            color=0x737373,
            scale=1
        )
        self.date_label.anchor_point = (0.5, 0.5)
        self.date_label.anchored_position = (self.center_x, self.center_y + 30)
        self.watch_group.append(self.date_label)
        
        # Hint text
        hint = label.Label(
            terminalio.FONT,
            text="← dial to browse",
            color=0x4D4D4D,
            scale=1
        )
        hint.anchor_point = (0.5, 0.5)
        hint.anchored_position = (self.center_x, self.height - 80)
        self.watch_group.append(hint)
    
    def _update_clock(self):
        """Update the clock display"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        try:
            now = rtc.RTC().datetime
            hours = now.tm_hour
            minutes = now.tm_min
            days = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"]
            months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]
            
            self.time_label.text = f"{hours:02d}:{minutes:02d}"
            self.date_label.text = f"{days[now.tm_wday]} {now.tm_mday} {months[now.tm_mon - 1]}"
        except:
            pass
    
    def _init_library(self):
        """Initialize the app library view"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        # Background
        bg = Circle(
            pixel_shader=displayio.Palette(2),
            x=self.center_x,
            y=self.center_y,
            r=self.width // 2 - 5
        )
        bg.pixel_shader[0] = 0x111111  # Slightly lighter than watch
        bg.pixel_shader[1] = 0x111111
        self.library_group.append(bg)
        
        # Title
        title = label.Label(
            terminalio.FONT,
            text="APPS",
            color=0x666666,
            scale=1
        )
        title.anchor_point = (0.5, 0.5)
        title.anchored_position = (self.center_x, 50)
        self.library_group.append(title)
        
        # App list container
        self.app_list_group = displayio.Group()
        self.library_group.append(self.app_list_group)
        
        self._build_app_list()
    
    def _build_app_list(self):
        """Build the app list display"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        # Clear existing list
        while len(self.app_list_group) > 0:
            self.app_list_group.pop()
        
        # Add apps
        y_offset = 90
        for i, app in enumerate(APPS):
            # App item group
            item_group = displayio.Group()
            
            # Highlight if selected
            if i == self.lib_index:
                bg_rect = displayio.Bitmap(280, 40, 65535)
                bg_palette = displayio.Palette(1)
                bg_palette[0] = 0x1A1A1A
                tile_grid = displayio.TileGrid(bg_rect, pixel_shader=bg_palette, x=30, y=y_offset)
                item_group.append(tile_grid)
            
            # App icon (simplified as colored circle)
            icon = Circle(
                pixel_shader=displayio.Palette(2),
                x=50,
                y=y_offset + 20,
                r=15
            )
            icon.pixel_shader[0] = app["color"]
            icon.pixel_shader[1] = app["color"]
            item_group.append(icon)
            
            # App name
            name_label = label.Label(
                terminalio.FONT,
                text=app["name"],
                color=0xFFFFFF if i == self.lib_index else 0x999999,
                scale=1
            )
            name_label.anchor_point = (0, 0.5)
            name_label.anchored_position = (80, y_offset + 20)
            item_group.append(name_label)
            
            # Subtitle
            sub_label = label.Label(
                terminalio.FONT,
                text=app["sub"],
                color=0x4D4D4D,
                scale=0
            )
            sub_label.anchor_point = (0, 0.5)
            sub_label.anchored_position = (80, y_offset + 35)
            item_group.append(sub_label)
            
            self.app_list_group.append(item_group)
            y_offset += 50
    
    def _build_app_view(self, app):
        """Build the active app view"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        # Clear existing app view
        while len(self.app_group) > 0:
            self.app_group.pop()
        
        # Background
        bg = Circle(
            pixel_shader=displayio.Palette(2),
            x=self.center_x,
            y=self.center_y,
            r=self.width // 2 - 5
        )
        bg.pixel_shader[0] = 0x0A0A0A
        bg.pixel_shader[1] = 0x0A0A0A
        self.app_group.append(bg)
        
        # Large icon
        icon = Circle(
            pixel_shader=displayio.Palette(2),
            x=self.center_x,
            y=self.center_y - 80,
            r=35
        )
        icon.pixel_shader[0] = app["color"]
        icon.pixel_shader[1] = app["color"]
        self.app_group.append(icon)
        
        # App name
        name_label = label.Label(
            terminalio.FONT,
            text=app["name"],
            color=0xFFFFFF,
            scale=2
        )
        name_label.anchor_point = (0.5, 0.5)
        name_label.anchored_position = (self.center_x, self.center_y - 20)
        self.app_group.append(name_label)
        
        # Subtitle
        sub_label = label.Label(
            terminalio.FONT,
            text=app["sub"],
            color=0x595959,
            scale=1
        )
        sub_label.anchor_point = (0.5, 0.5)
        sub_label.anchored_position = (self.center_x, self.center_y + 10)
        self.app_group.append(sub_label)
        
        # App-specific content
        self._build_app_content(app)
    
    def _build_app_content(self, app):
        """Build app-specific content"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        if app["view"] == "timer":
            self.timer_secs = 300
            self.timer_running = False
            
            self.timer_label = label.Label(
                terminalio.FONT,
                text="05:00",
                color=0xFFFFFF,
                scale=3
            )
            self.timer_label.anchor_point = (0.5, 0.5)
            self.timer_label.anchored_position = (self.center_x, self.center_y + 80)
            self.app_group.append(self.timer_label)
            
            hint = label.Label(
                terminalio.FONT,
                text="press button to start",
                color=0x4D4D4D,
                scale=0
            )
            hint.anchor_point = (0.5, 0.5)
            hint.anchored_position = (self.center_x, self.height - 60)
            self.app_group.append(hint)
        
        elif app["view"] == "counter":
            self.counter_val = 0
            
            self.counter_label = label.Label(
                terminalio.FONT,
                text="0",
                color=0xFFFFFF,
                scale=5
            )
            self.counter_label.anchor_point = (0.5, 0.5)
            self.counter_label.anchored_position = (self.center_x, self.center_y + 80)
            self.app_group.append(self.counter_label)
            
            hint = label.Label(
                terminalio.FONT,
                text="press button to count",
                color=0x4D4D4D,
                scale=0
            )
            hint.anchor_point = (0.5, 0.5)
            hint.anchored_position = (self.center_x, self.height - 60)
            self.app_group.append(hint)
        
        elif app["view"] == "todo":
            # Simple todo list
            todos = [("Buy groceries", True), ("Call dentist", False), ("Finish project", False)]
            y_offset = self.center_y + 40
            for todo, done in todos:
                todo_label = label.Label(
                    terminalio.FONT,
                    text=todo,
                    color=0x4D4D4D if done else 0x8C8C8C,
                    scale=1
                )
                todo_label.anchor_point = (0.5, 0.5)
                todo_label.anchored_position = (self.center_x, y_offset)
                self.app_group.append(todo_label)
                y_offset += 25
        
        elif app["view"] == "clock2":
            # World clock
            cities = [("New York", -4), ("London", 1), ("Tokyo", 9)]
            y_offset = self.center_y + 40
            for city, offset in cities:
                city_label = label.Label(
                    terminalio.FONT,
                    text=f"{city}",
                    color=0x737373,
                    scale=1
                )
                city_label.anchor_point = (0.5, 0.5)
                city_label.anchored_position = (self.center_x, y_offset)
                self.app_group.append(city_label)
                y_offset += 25
        
        elif app["view"] == "notes":
            notes_label = label.Label(
                terminalio.FONT,
                text="No notes yet",
                color=0x4D4D4D,
                scale=1
            )
            notes_label.anchor_point = (0.5, 0.5)
            notes_label.anchored_position = (self.center_x, self.center_y + 80)
            self.app_group.append(notes_label)
    
    def handle_dial(self, value):
        """
        Handle rotary encoder input
        
        Args:
            value: Current dial value (0-100)
        """
        delta = value - self.prev_dial_value
        self.prev_dial_value = value
        self.dial_value = value
        
        DIAL_SWITCH = 30
        
        if self.current_view == "watch":
            if value >= DIAL_SWITCH:
                self.current_view = "library"
                self._show_view("library")
                self._build_app_list()
                self.dial_value = DIAL_SWITCH
                self.prev_dial_value = DIAL_VALUE = DIAL_SWITCH
        
        elif self.current_view == "app":
            if value <= DIAL_SWITCH - 5:
                self.current_view = "watch"
                self._show_view("watch")
                self._update_clock()
        
        elif self.current_view == "library":
            if value < DIAL_SWITCH - 5:
                self.current_view = "watch"
                self._show_view("watch")
                self._update_clock()
                self.dial_value = 0
                self.prev_dial_value = 0
            else:
                if delta > 0 and self.lib_index < len(APPS) - 1:
                    self.lib_index += 1
                    self._build_app_list()
                elif delta < 0 and self.lib_index > 0:
                    self.lib_index -= 1
                    self._build_app_list()
    
    def handle_button(self):
        """Handle button press"""
        if self.current_view == "library":
            self.selected_app = APPS[self.lib_index]
            self.current_view = "app"
            self._build_app_view(self.selected_app)
            self._show_view("app")
            self.dial_value = 0
            self.prev_dial_value = 0
        
        elif self.current_view == "app":
            if self.selected_app["view"] == "timer":
                self.timer_running = not self.timer_running
                if not self.timer_running and self.timer_secs == 0:
                    self.timer_secs = 300
            elif self.selected_app["view"] == "counter":
                self.counter_val += 1
                if hasattr(self, 'counter_label'):
                    self.counter_label.text = str(self.counter_val)
            else:
                # Go back to library
                self.current_view = "library"
                self._show_view("library")
                self._build_app_list()
                self.dial_value = 50
                self.prev_dial_value = 50
        
        elif self.current_view == "watch":
            self.current_view = "library"
            self._show_view("library")
            self._build_app_list()
            self.dial_value = 50
            self.prev_dial_value = 50
    
    def update(self):
        """Update timer and other periodic tasks"""
        if not CIRCUITPYTHON_AVAILABLE:
            return
            
        # Update timer
        if self.timer_running and hasattr(self, 'timer_label'):
            self.timer_secs = max(0, self.timer_secs - 1)
            mm = self.timer_secs // 60
            ss = self.timer_secs % 60
            self.timer_label.text = f"{mm:02d}:{ss:02d}"
            if self.timer_secs == 0:
                self.timer_running = False
        
        # Update clock periodically
        self._update_clock()


# Example usage
def main():
    """Example main function for testing"""
    if not CIRCUITPYTHON_AVAILABLE:
        print("WatchAppSimulator class created successfully!")
        print("This module is designed for CircuitPython with displayio.")
        print("On the ESP32-S3, initialize display and create WatchAppSimulator instance.")
        return
    
    # This would be the actual CircuitPython code:
    # from displayio import release_displays
    # release_displays()
    # 
    # import busio
    # import board
    # import dotclockframebuffer
    # from framebufferio import FramebufferDisplay
    # 
    # # Initialize display (see display_setup.md for your specific display)
    # # ... display initialization code ...
    # 
    # # Create simulator
    # simulator = WatchAppSimulator(display)
    # 
    # # Main loop
    # while True:
    #     simulator.update()
    #     time.sleep(0.1)


if __name__ == "__main__":
    main()
