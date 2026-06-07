"""
Desktop Circular Window Popup Simulator
Simulates the CircuitPython popup using pygame

Install pygame:
pip install pygame
"""

import pygame
import math

# Initialize pygame
pygame.init()


class CircularPopup:
    """Desktop simulation of the CircuitPython CircularPopup"""

    def __init__(
        self,
        x,
        y,
        radius,
        background_color=(50, 50, 255),
        border_color=(255, 255, 255),
        border_width=3,
    ):

        self.x = x
        self.y = y
        self.radius = radius

        self.background_color = background_color
        self.border_color = border_color
        self.border_width = border_width

        self.visible = True

        self.text = ""
        self.text_color = (255, 255, 255)
        self.text_scale = 1

        self.font = pygame.font.SysFont(None, 36)

    def add_text(self, text, text_color=(255, 255, 255), scale=1):
        """Add text to popup"""

        self.text = text
        self.text_color = text_color
        self.text_scale = scale

        font_size = int(36 * scale)
        self.font = pygame.font.SysFont(None, font_size)

    def show(self):
        self.visible = True

    def hide(self):
        self.visible = False

    def set_position(self, x, y):
        self.x = x
        self.y = y

    def draw(self, screen):
        """Draw popup onto pygame screen"""

        if not self.visible:
            return

        # Border
        pygame.draw.circle(
            screen,
            self.border_color,
            (self.x, self.y),
            self.radius + self.border_width,
        )

        # Background
        pygame.draw.circle(
            screen,
            self.background_color,
            (self.x, self.y),
            self.radius,
        )

        # Text
        if self.text:
            text_surface = self.font.render(
                self.text,
                True,
                self.text_color,
            )

            text_rect = text_surface.get_rect(center=(self.x, self.y))
            screen.blit(text_surface, text_rect)


def create_example_popup(display_width=480, display_height=480):

    center_x = display_width // 2
    center_y = display_height // 2

    radius = min(display_width, display_height) // 3

    popup = CircularPopup(
        x=center_x,
        y=center_y,
        radius=radius,
        background_color=(50, 50, 255),
        border_color=(255, 255, 255),
        border_width=4,
    )

    popup.add_text("Hello!", scale=2)

    return popup


# ---------------- MAIN PROGRAM ---------------- #

WIDTH = 480
HEIGHT = 480

screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Circular Popup Simulator")

clock = pygame.time.Clock()

popup = create_example_popup()

running = True

while running:

    for event in pygame.event.get():

        if event.type == pygame.QUIT:
            running = False

        # Toggle visibility with SPACE
        if event.type == pygame.KEYDOWN:

            if event.key == pygame.K_SPACE:
                popup.visible = not popup.visible

    # Background
    screen.fill((20, 20, 20))

    # Draw popup
    popup.draw(screen)

    pygame.display.flip()
    clock.tick(60)

pygame.quit()