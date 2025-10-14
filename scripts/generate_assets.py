#!/usr/bin/env python3
"""Generate assets for F256 Switcharoo game following video_assets.md specification.

This script generates:
- 320x240 board bitmap with 4x8 checkerboard (28x28 cells each)
- 24x24 piece sprites (normal and swapped variants)
- 16x16 icon sprites
- Proper CLUT slot assignments per specification
- PNG files with modern color theme for visualization
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Tuple

try:
    from PIL import Image
except ImportError:
    print("PIL (Python Imaging Library) is required for PNG generation.")
    print("Install with: pip install pillow")
    exit(1)


# CLUT slot assignments per video_assets.md
CLUT_TRANSPARENT = 0
CLUT_BOARD_CELLS = list(range(1, 33))  # Slots 1-32 for board cells
CLUT_BOARD_BORDER = 33
CLUT_FUTURE_USE = list(range(34, 65))  # Slots 34-64 reserved
# Piece sprite CLUT slots 65-76
CLUT_PLAYER_A_EDGE_1 = 65
CLUT_PLAYER_A_EDGE_2 = 66
CLUT_PLAYER_A_FILL_1 = 67
CLUT_PLAYER_A_FILL_2 = 68
CLUT_PLAYER_B_EDGE_1 = 69
CLUT_PLAYER_B_EDGE_2 = 70
CLUT_PLAYER_B_FILL_1 = 71
CLUT_PLAYER_B_FILL_2 = 72
CLUT_PLAYER_A_SWAPPED_1 = 73
CLUT_PLAYER_A_SWAPPED_2 = 74
CLUT_PLAYER_B_SWAPPED_1 = 75
CLUT_PLAYER_B_SWAPPED_2 = 76
# Icon sprite CLUT slots 77-84
CLUT_ICON_EDGE_1 = 77
CLUT_ICON_EDGE_2 = 78
CLUT_ICON_FILL_1 = 79
CLUT_ICON_FILL_2 = 80
CLUT_ICON_SYMBOL_1 = 81
CLUT_ICON_SYMBOL_2 = 82
CLUT_ICON_SYMBOL_3 = 83
CLUT_ICON_SYMBOL_4 = 84
# Focus color slot
CLUT_FOCUS = 89

# Modern color palette (RGB tuples) - maps CLUT indices to actual colors
MODERN_COLOR_PALETTE: list[Tuple[int, int, int]] = [
    # 0: Transparent
    (0, 0, 0, 0),  # RGBA with alpha
    
    # 1-32: Board cells - modern gradient from dark blue to light blue
    (26, 26, 46), (31, 31, 51), (36, 36, 56), (41, 41, 61),
    (46, 46, 66), (51, 51, 71), (56, 56, 76), (61, 61, 81),
    (66, 66, 86), (71, 71, 91), (76, 76, 96), (81, 81, 101),
    (86, 86, 106), (91, 91, 111), (96, 96, 116), (101, 101, 121),
    (106, 106, 126), (111, 111, 131), (116, 116, 136), (121, 121, 141),
    (126, 126, 146), (131, 131, 151), (136, 136, 156), (141, 141, 161),
    (146, 146, 166), (151, 151, 171), (156, 156, 176), (161, 161, 181),
    (166, 166, 186), (171, 171, 191), (176, 176, 196), (181, 181, 201),
    
    # 33: Board border - light gray
    (204, 204, 204),
    
    # 34-64: Future use - various grays
    *[(i*4, i*4, i*4) for i in range(31)],
    
    # 65-68: Player A piece colors - blue theme
    (74, 144, 226), (33, 113, 181), (52, 152, 219), (41, 128, 185),
    
    # 69-72: Player B piece colors - purple theme  
    (155, 89, 182), (142, 68, 173), (125, 60, 152), (108, 52, 131),
    
    # 73-76: Swapped symbol colors - white and light variants
    (255, 255, 255), (240, 240, 240), (255, 255, 255), (240, 240, 240),
    
    # 77-84: Icon colors - orange theme
    (230, 126, 34), (211, 84, 0), (243, 156, 18), (230, 126, 34),
    (241, 196, 15), (243, 156, 18), (230, 126, 34), (211, 84, 0),
]

# Ensure palette is large enough to include focus color index (89)
if len(MODERN_COLOR_PALETTE) <= CLUT_FOCUS:
    # Pad with white for focus by default
    while len(MODERN_COLOR_PALETTE) <= CLUT_FOCUS:
        MODERN_COLOR_PALETTE.append((255, 255, 255))

# Board configuration
BOARD_COLUMNS = 4
BOARD_ROWS = 8
CELL_SIZE = 26
BOARD_WIDTH = BOARD_COLUMNS * CELL_SIZE + 8 + 3
BOARD_HEIGHT = BOARD_ROWS * CELL_SIZE + 8 + 7

# Screen configuration
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240

# Asset definitions
PIECE_SIZE = 24
ICON_SIZE = 16

# Gradient types for background patterns
GRADIENT_TYPES = [
    ("linear_horizontal", lambda x, y, w, h: int((x / w) * 30)),
    ("linear_vertical", lambda x, y, w, h: int((y / h) * 30)),
    ("radial", lambda x, y, w, h: int(math.sqrt((x - w/2)**2 + (y - h/2)**2) / math.sqrt((w/2)**2 + (h/2)**2) * 30)),
    ("conic", lambda x, y, w, h: int(((math.atan2(y - h/2, x - w/2) + math.pi) / (2 * math.pi)) * 30)),
    ("procedural", lambda x, y, w, h: int((math.sin(x / 20) + math.cos(y / 20)) * 15 + 15)),
    ("multi_stop", lambda x, y, w, h: int((x / w + y / h) / 2 * 30)),
    ("function_mapping", lambda x, y, w, h: int((math.sin(x / 10) * math.cos(y / 10) + 1) * 15)),
]


def ensure_directory(path: Path) -> None:
    """Create directory if it doesn't exist."""
    path.mkdir(parents=True, exist_ok=True)


def write_binary(path: Path, data: bytes) -> None:
    """Write binary data to file."""
    ensure_directory(path.parent)
    path.write_bytes(data)


def generate_board_bitmap_with_gradient(gradient_func) -> bytes:
    """Generate 320x240 board bitmap with centered 4x8 checkerboard and gradient background."""
    # Calculate board position (centered)
    board_x = (SCREEN_WIDTH - BOARD_WIDTH) // 2
    board_y = (SCREEN_HEIGHT - BOARD_HEIGHT) // 2
    
    # 4-pixel border around board
    board_margin = 4
    
    buffer = bytearray(SCREEN_WIDTH * SCREEN_HEIGHT)
    
    for y in range(SCREEN_HEIGHT):
        for x in range(SCREEN_WIDTH):
            idx = y * SCREEN_WIDTH + x
            
            # Check if we're in the board area (including border)
            in_board_area = (
                x >= board_x  and 
                x < board_x + BOARD_WIDTH  and
                y >= board_y  and 
                y < board_y + BOARD_HEIGHT 
            )
            
            if not in_board_area:
                # Outside board area - use gradient pattern
                gradient_index = min(30, max(0, gradient_func(x, y, SCREEN_WIDTH, SCREEN_HEIGHT)))
                clut_index = 34 + gradient_index
                buffer[idx] = clut_index
                continue
            # in Board Area
            # Check if we're in the border area
            in_border = (
                x < board_x + 4 or x >= board_x + BOARD_WIDTH - 4 or
                y < board_y + 4 or y >= board_y + BOARD_HEIGHT - 4
            )
            
            if in_border:
                buffer[idx] = CLUT_BOARD_BORDER
                continue
            
            # We're inside the board - calculate cell
            cell_x = (x - board_x - 4) // (CELL_SIZE + 1)
            cell_y = (y - board_y - 4) // (CELL_SIZE + 1)

            # Check for single-pixel borders between cells
            local_x = (x - board_x - 4) % (CELL_SIZE+1)
            local_y = (y - board_y - 4) % (CELL_SIZE+1)

            if local_x == CELL_SIZE  or local_y == CELL_SIZE :
                buffer[idx] = CLUT_BOARD_BORDER
            else:
                # Calculate which board cell (0-31)
                cell_index = cell_y * BOARD_COLUMNS + cell_x
                buffer[idx] = CLUT_BOARD_CELLS[cell_index]
    
    return bytes(buffer)



def create_png_from_clut_data(data: bytes, width: int, height: int, filename: Path) -> None:
    """Create a PNG image from CLUT-indexed data using the modern color palette."""
    # Create RGB image
    img = Image.new('RGB', (width, height), (0, 0, 0))
    pixels = img.load()
    
    for y in range(height):
        for x in range(width):
            clut_index = data[y * width + x]
            if clut_index < len(MODERN_COLOR_PALETTE):
                color = MODERN_COLOR_PALETTE[clut_index]
                # Handle RGBA tuples (for transparency)
                if len(color) == 4:
                    # For transparent pixels, use a dark background color
                    pixels[x, y] = (26, 26, 46) if color[3] == 0 else color[:3]
                else:
                    pixels[x, y] = color
            else:
                # Fallback for out-of-range indices
                pixels[x, y] = (255, 0, 255)  # Magenta for errors
    
    img.save(filename)


def generate_png_assets(output_dir: Path, binary_assets: dict[str, bytes]) -> None:
    """Generate PNG versions of all binary assets."""
    png_dir = output_dir / "png"
    ensure_directory(png_dir)
    
    # Generate PNGs for board bitmaps
    for filename, data in binary_assets.items():
        if filename.startswith("board_bitmap_") and filename.endswith(".bin"):
            png_name = filename.replace(".bin", ".png")
            create_png_from_clut_data(data, SCREEN_WIDTH, SCREEN_HEIGHT, 
                                    png_dir / png_name)
    

def generate_assets(output_dir: Path) -> dict[str, bytes]:
    """Generate all game assets."""
    assets = {}
    
    # Generate board bitmaps for each gradient type
    for gradient_name, gradient_func in GRADIENT_TYPES:
        assets[f"board_bitmap_{gradient_name}.bin"] = generate_board_bitmap_with_gradient(gradient_func)
    
       
    # Write all binary assets to files
    for filename, data in assets.items():
        write_binary(output_dir / filename, data)
    
    # Generate PNG versions
    generate_png_assets(output_dir, assets)
    
    return assets





def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Generate F256 Switcharoo game assets")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/generated"),
        help="Output directory for generated assets",
    )
    return parser.parse_args()


def main() -> None:
    """Main entry point."""
    args = parse_args()
    output_dir = args.output.resolve()
    
    project_root = Path(__file__).resolve().parents[1]

    
    print(f"Generating assets to {output_dir}")
    assets = generate_assets(output_dir)

    # Write gradient details to a file
    gradient_details_path = output_dir / "gradient_details.txt"
    with open(gradient_details_path, 'w') as f:
        f.write("Gradient Background Patterns for Board Bitmaps\n")
        f.write("=" * 50 + "\n\n")
        for name, _ in GRADIENT_TYPES:
            f.write(f"- {name}: board_bitmap_{name}.bin / board_bitmap_{name}.png\n")
        f.write("\nDescriptions:\n")
        f.write("- linear_horizontal: Horizontal gradient from left to right\n")
        f.write("- linear_vertical: Vertical gradient from top to bottom\n")
        f.write("- radial: Radial gradient from center outward\n")
        f.write("- conic: Conic gradient based on angle from center\n")
        f.write("- procedural: Sine and cosine wave pattern\n")
        f.write("- multi_stop: Average of horizontal and vertical gradients\n")
        f.write("- function_mapping: Sine product function mapping\n")
    
    print(f"Generated {len(assets)} binary assets:")
    for filename in sorted(assets.keys()):
        print(f"  {filename} ({len(assets[filename])} bytes)")
    
    png_dir = output_dir / "png"
    png_files = list(png_dir.glob("*.png")) if png_dir.exists() else []
    print(f"Generated {len(png_files)} PNG assets:")
    for png_file in sorted(png_files):
        print(f"  {png_file.name}")
    
    print(f"Gradient details written to {gradient_details_path}")


if __name__ == "__main__":
    main()