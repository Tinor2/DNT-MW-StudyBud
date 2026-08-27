#!/usr/bin/env python3
"""
Convert PNG files to LVGL 8 C image assets.

Generates both .c and .h files.

Usage:
    python3 png_to_lvgl.py image.png [output_name]

Example:
    python3 png_to_lvgl.py water_drop.png water_icon

Creates:
    water_icon.c
    water_icon.h
"""

import sys
import os
from PIL import Image


def png_to_lvgl(png_path, output_name=None):
    """
    Convert a PNG file to an LVGL 8
    LV_IMG_CF_TRUE_COLOR_ALPHA image.

    The generated image uses:

        RGB565 + Alpha

    Each pixel consists of:

        Byte 0: RGB565
        Byte 1: RGB565
        Byte 2: Alpha

    Args:
        png_path:
            Path to PNG image.

        output_name:
            Name of the generated LVGL image.
            If None, uses the PNG filename.

    Returns:
        Tuple of:
            header_content
            source_content
    """

    # ---------------------------------------------------------
    # Validate PNG path
    # ---------------------------------------------------------

    if not os.path.isfile(png_path):
        raise FileNotFoundError(
            f"PNG file not found: {png_path}"
        )

    # ---------------------------------------------------------
    # Determine output name
    # ---------------------------------------------------------

    if output_name is None:
        output_name = os.path.splitext(
            os.path.basename(png_path)
        )[0]

    # Make output name safe for C
    output_name = output_name.replace("-", "_")
    output_name = output_name.replace(" ", "_")

    # ---------------------------------------------------------
    # Load PNG
    # ---------------------------------------------------------

    img = Image.open(png_path).convert("RGBA")

    width, height = img.size

    print(f"Input image : {png_path}")
    print(f"Dimensions  : {width} x {height}")
    print(f"Output name : {output_name}")

    # ---------------------------------------------------------
    # Convert image pixels
    # ---------------------------------------------------------

    buf = bytearray()

    for pixel in img.getdata():
        if not isinstance(pixel, tuple) or len(pixel) != 4:
            raise ValueError(
                f"Unexpected pixel format from image {png_path}: {pixel!r}"
            )

        r, g, b, a = pixel

        # Convert 8-bit RGB to RGB565
        r5 = r >> 3
        g6 = g >> 2
        b5 = b >> 3

        # LVGL RGB565 byte order
        #
        # Byte 0:
        #   GGG BBBBB
        #
        # Byte 1:
        #   RRRRR GGG

        byte0 = ((g6 & 0x07) << 5) | b5
        byte1 = (r5 << 3) | ((g6 >> 3) & 0x07)

        # RGB565
        buf.append(byte0)
        buf.append(byte1)

        # Alpha
        buf.append(a)

    # ---------------------------------------------------------
    # Generate C array
    # ---------------------------------------------------------

    lines = []

    bytes_per_line = 12

    for i in range(0, len(buf), bytes_per_line):

        chunk = buf[i:i + bytes_per_line]

        line = (
            "    "
            + ", ".join(
                f"0x{byte:02X}"
                for byte in chunk
            )
            + ","
        )

        lines.append(line)

    c_array = "\n".join(lines)

    # ---------------------------------------------------------
    # Generate header
    # ---------------------------------------------------------

    header_content = f"""#pragma once

#include "lvgl.h"

extern const lv_img_dsc_t {output_name};
"""

    # ---------------------------------------------------------
    # Generate LVGL 8 source
    # ---------------------------------------------------------

    source_content = f"""#include "{output_name}.h"

static const uint8_t {output_name}_map[] = {{
{c_array}
}};

const lv_img_dsc_t {output_name} = {{
    .header.always_zero = 0,
    .header.w = {width},
    .header.h = {height},
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .data_size = sizeof({output_name}_map),
    .data = {output_name}_map,
}};
"""

    return header_content, source_content


def main():

    # ---------------------------------------------------------
    # Check arguments
    # ---------------------------------------------------------

    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    png_path = sys.argv[1]

    if len(sys.argv) >= 3:
        output_name = sys.argv[2]
    else:
        output_name = None

    # ---------------------------------------------------------
    # Convert
    # ---------------------------------------------------------

    try:

        header_content, source_content = png_to_lvgl(
            png_path,
            output_name
        )

        # Determine output name
        if output_name is None:
            output_name = os.path.splitext(
                os.path.basename(png_path)
            )[0]

        output_name = output_name.replace("-", "_")
        output_name = output_name.replace(" ", "_")

        # -----------------------------------------------------
        # Output filenames
        # -----------------------------------------------------

        h_filename = f"{output_name}.h"
        c_filename = f"{output_name}.c"

        # -----------------------------------------------------
        # Write header
        # -----------------------------------------------------

        with open(
            h_filename,
            "w",
            encoding="utf-8"
        ) as f:

            f.write(header_content)

        # -----------------------------------------------------
        # Write source
        # -----------------------------------------------------

        with open(
            c_filename,
            "w",
            encoding="utf-8"
        ) as f:

            f.write(source_content)

        # -----------------------------------------------------
        # Success
        # -----------------------------------------------------

        print()
        print(f"✓ Created {h_filename}")
        print(f"✓ Created {c_filename}")

        print()
        print("Use with:")
        print()
        print(f'#include "{h_filename}"')
        print()
        print(
            f"lv_img_set_src(img_obj, &{output_name});"
        )

    except FileNotFoundError as e:

        print(
            f"Error: {e}",
            file=sys.stderr
        )

        sys.exit(1)

    except Exception as e:

        print(
            f"Error: {e}",
            file=sys.stderr
        )

        sys.exit(1)


if __name__ == "__main__":
    main()