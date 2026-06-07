"""
Circular Window Popup for DNT-MW-StudyBud
Compatible with vectorio library for CircuitPython on ESP32-S3
"""

try:
    import displayio
    import vectorio
    from adafruit_display_shapes.circle import Circle
    from adafruit_display_text import label
    import terminalio
    CIRCUITPYTHON_AVAILABLE = True
except ImportError:
    # Desktop testing mode - CircuitPython libraries not available
    CIRCUITPYTHON_AVAILABLE = False
    print("Note: CircuitPython libraries not available. Running in desktop test mode.")


class CircularPopup:
    """A circular window popup widget for the round display"""
    
    def __init__(self, x, y, radius, background_color=0x000000, border_color=0xFFFFFF, border_width=2):
        """
        Initialize the circular popup
        
        Args:
            x: Center x position
            y: Center y position
            radius: Radius of the circle
            background_color: Fill color (RGB565 format)
            border_color: Border color (RGB565 format)
            border_width: Border thickness in pixels
        """
        if not CIRCUITPYTHON_AVAILABLE:
            print(f"Would create CircularPopup at ({x}, {y}) with radius {radius}")
            self.group = None
            self.x = x
            self.y = y
            self.radius = radius
            self.background_color = background_color
            self.border_color = border_color
            self.border_width = border_width
            return
            
        self.x = x
        self.y = y
        self.radius = radius
        self.background_color = background_color
        self.border_color = border_color
        self.border_width = border_width
        
        # Create the main group for this popup
        self.group = displayio.Group()
        
        # Create the background circle
        self.background_circle = Circle(
            pixel_shader=displayio.Palette(2),
            x=x,
            y=y,
            r=radius
        )
        self.background_circle.pixel_shader[0] = background_color
        self.background_circle.pixel_shader[1] = border_color
        self.group.append(self.background_circle)
        
        # Create border circle (slightly larger)
        if border_width > 0:
            self.border_circle = Circle(
                pixel_shader=displayio.Palette(2),
                x=x,
                y=y,
                r=radius + border_width
            )
            self.border_circle.pixel_shader[0] = 0x000000  # Transparent/black
            self.border_circle.pixel_shader[1] = border_color
            # Insert border behind background
            self.group.insert(0, self.border_circle)
    
    def add_text(self, text, font=None, text_color=0xFFFFFF, scale=1):
        """
        Add text to the center of the popup
        
        Args:
            text: Text string to display
            font: Font object (defaults to terminalio.FONT)
            text_color: Text color (RGB565 format)
            scale: Text scale factor
        """
        if not CIRCUITPYTHON_AVAILABLE:
            print(f"Would add text: '{text}' to popup")
            return None
            
        if font is None:
            font = terminalio.FONT
        
        text_area = label.Label(
            font,
            text=text,
            color=text_color,
            scale=scale
        )
        
        # Center the text
        text_area.anchor_point = (0.5, 0.5)
        text_area.anchored_position = (self.x, self.y)
        
        self.group.append(text_area)
        return text_area
    
    def show(self):
        """Show the popup by making it visible"""
        if not CIRCUITPYTHON_AVAILABLE:
            print("Would show popup")
            return
        self.group.hidden = False
    
    def hide(self):
        """Hide the popup"""
        if not CIRCUITPYTHON_AVAILABLE:
            print("Would hide popup")
            return
        self.group.hidden = True
    
    def set_position(self, x, y):
        """Move the popup to a new position"""
        if not CIRCUITPYTHON_AVAILABLE:
            print(f"Would move popup to ({x}, {y})")
            self.x = x
            self.y = y
            return
            
        dx = x - self.x
        dy = y - self.y
        self.x = x
        self.y = y
        
        # Move all elements in the group
        for item in self.group:
            item.x += dx
            item.y += dy


# Example usage for the 2.1" 480x480 round display
def create_example_popup(display_width=480, display_height=480):
    """
    Create an example circular popup centered on the display
    
    Args:
        display_width: Display width (default 480 for TL021WVC02)
        display_height: Display height (default 480 for TL021WVC02)
    
    Returns:
        CircularPopup instance
    """
    center_x = display_width // 2
    center_y = display_height // 2
    radius = min(display_width, display_height) // 3
    
    popup = CircularPopup(
        x=center_x,
        y=center_y,
        radius=radius,
        background_color=0x3333FF,  # Blue background
        border_color=0xFFFFFF,       # White border
        border_width=3
    )
    
    popup.add_text("Hello!", text_color=0xFFFFFF, scale=2)
    
    return popup


# For testing on desktop (not CircuitPython)
if __name__ == "__main__":
    print("CircularPopup class created successfully!")
    print("This module is designed for CircuitPython with vectorio.")
    print("On the ESP32-S3, import and use this class in your code.py")
