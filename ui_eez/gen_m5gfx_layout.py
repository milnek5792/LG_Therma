#!/usr/bin/env python3
"""Layout constants for M5GFX HMI — same source as gen_hmi_eez_project.py (HTML mockup).

Renderer: src/ui_displej_hmi.h
EEZ generator: ui_eez/gen_hmi_eez_project.py
"""

COLORS = {
    "BG": 0x121214,
    "PANEL": 0x1A1A1F,
    "BORDER": 0x24242B,
    "GREEN": 0x30D158,
    "ORANGE": 0xFF9F0A,
    "BLUE": 0x0A84FF,
    "CYAN": 0x64D2FF,
}

BUTTONS = {
    "tichy": (1040, 12, 210, 36),
    "minus": (107, 151, 100, 100),
    "plus": (488, 152, 100, 100),
    "run": (865, 90, 195, 60),
    "stop": (1070, 90, 195, 60),
    "menu": (1024, 600, 256, 120),
}

if __name__ == "__main__":
    print("M5GFX layout from HTML mockup (1280x720)")
    print("Colors:", COLORS)
    print("Buttons:", BUTTONS)
