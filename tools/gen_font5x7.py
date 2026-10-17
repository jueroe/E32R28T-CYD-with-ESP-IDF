#!/usr/bin/env python3
"""Generate main/font5x7.c and main/font5x7.h for the CYD test project.

The font is authored here as ASCII art (7 rows x 5 columns per glyph) so the
glyphs can be reviewed and extended easily.  The generated C table stores one
byte per column, bit 0 = top row:

    .data[col] bit row  ->  pixel at (x + col, y + row)

Run from the project root:

    python tools/gen_font5x7.py
"""

import os
import sys

WIDTH = 5
HEIGHT = 7
FIRST_CHAR = 0x20
LAST_CHAR = 0x5F

# char -> 7 rows of 5 characters ('#' = pixel on, ' ' = pixel off)
GLYPHS = {
    " ": ("     ", "     ", "     ", "     ", "     ", "     ", "     "),
    "!": ("  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "     ", "  #  "),
    '"': (" # # ", " # # ", "     ", "     ", "     ", "     ", "     "),
    "#": (" # # ", " # # ", "#####", " # # ", "#####", " # # ", " # # "),
    "$": ("  #  ", " ####", "# #  ", " ### ", "  # #", "#### ", "  #  "),
    "%": ("#   #", "#  # ", "   # ", "  #  ", " #   ", "#  # ", "#   #"),
    "&": (" #   ", "# #  ", "# #  ", " #   ", "# # #", "#  # ", " ## #"),
    "'": ("  #  ", "  #  ", "     ", "     ", "     ", "     ", "     "),
    "(": ("  #  ", " #   ", "#    ", "#    ", "#    ", " #   ", "  #  "),
    ")": ("#    ", " #   ", "  #  ", "  #  ", "  #  ", " #   ", "#    "),
    "*": ("     ", "# # #", " ### ", "#####", " ### ", "# # #", "     "),
    "+": ("     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "),
    ",": ("     ", "     ", "     ", "     ", "  #  ", "  #  ", " #   "),
    "-": ("     ", "     ", "     ", "#####", "     ", "     ", "     "),
    ".": ("     ", "     ", "     ", "     ", "     ", "  #  ", "  #  "),
    "/": ("    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "),
    "0": (" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "),
    "1": ("  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "),
    "2": (" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"),
    "3": ("#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### "),
    "4": ("   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "),
    "5": ("#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "),
    "6": ("  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### "),
    "7": ("#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "),
    "8": (" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "),
    "9": (" ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  "),
    ":": ("     ", "  #  ", "  #  ", "     ", "     ", "  #  ", "  #  "),
    ";": ("     ", "  #  ", "  #  ", "     ", "     ", "  #  ", " #   "),
    "<": ("   # ", "  #  ", " #   ", "#    ", " #   ", "  #  ", "   # "),
    "=": ("     ", "     ", "#####", "     ", "#####", "     ", "     "),
    ">": ("#    ", " #   ", "  #  ", "   # ", "  #  ", " #   ", "#    "),
    "?": (" ### ", "#   #", "    #", "   # ", "  #  ", "     ", "  #  "),
    "@": (" ### ", "#   #", "# ###", "# # #", "# ###", "#    ", " ### "),
    "A": ("  #  ", " # # ", "#   #", "#   #", "#####", "#   #", "#   #"),
    "B": ("#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "),
    "C": (" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "),
    "D": ("###  ", "#  # ", "#   #", "#   #", "#   #", "#  # ", "###  "),
    "E": ("#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"),
    "F": ("#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "),
    "G": (" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####"),
    "H": ("#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"),
    "I": (" ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "),
    "J": ("    #", "    #", "    #", "    #", "#   #", "#   #", " ### "),
    "K": ("#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"),
    "L": ("#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"),
    "M": ("#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #"),
    "N": ("#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #"),
    "O": (" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "),
    "P": ("#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "),
    "Q": (" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"),
    "R": ("#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"),
    "S": (" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "),
    "T": ("#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "),
    "U": ("#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "),
    "V": ("#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "),
    "W": ("#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #"),
    "X": ("#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"),
    "Y": ("#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "),
    "Z": ("#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"),
    "[": (" ### ", " #   ", " #   ", " #   ", " #   ", " #   ", " ### "),
    "\\": ("#    ", "#    ", " #   ", "  #  ", "   # ", "    #", "    #"),
    "]": (" ### ", "   # ", "   # ", "   # ", "   # ", "   # ", " ### "),
    "^": ("  #  ", " # # ", "#   #", "     ", "     ", "     ", "     "),
    "_": ("     ", "     ", "     ", "     ", "     ", "     ", "#####"),
}

HEADER = "font5x7.h"
SOURCE = "font5x7.c"


def validate() -> None:
    missing = [chr(c) for c in range(FIRST_CHAR, LAST_CHAR + 1) if chr(c) not in GLYPHS]
    if missing:
        sys.exit(f"missing glyphs: {missing}")
    for char, rows in GLYPHS.items():
        if len(rows) != HEIGHT:
            sys.exit(f"glyph {char!r} has {len(rows)} rows, expected {HEIGHT}")
        for index, row in enumerate(rows):
            if len(row) != WIDTH:
                sys.exit(f"glyph {char!r} row {index} is {len(row)} wide, expected {WIDTH}")


def to_columns(rows) -> list:
    columns = []
    for col in range(WIDTH):
        value = 0
        for row in range(HEIGHT):
            if rows[row][col] == "#":
                value |= 1 << row
        columns.append(value)
    return columns


def render(table, text: str) -> str:
    """Render `text` using the generated table (for visual verification)."""
    out = [""] * HEIGHT
    for char in text:
        columns = table[ord(char) - FIRST_CHAR]
        for row in range(HEIGHT):
            line = "".join("#" if columns[col] & (1 << row) else "." for col in range(WIDTH))
            out[row] += line + "."
    return "\n".join(out)


def main() -> None:
    validate()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "main")
    os.makedirs(out_dir, exist_ok=True)

    table = [to_columns(GLYPHS[chr(c)]) for c in range(FIRST_CHAR, LAST_CHAR + 1)]

    with open(os.path.join(out_dir, HEADER), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"""/*
 * Generated by tools/gen_font5x7.py - do not edit manually.
 *
 * 5x7 pixel font, characters 0x{FIRST_CHAR:02X}..0x{LAST_CHAR:02X} (space to underscore).
 * Every glyph is {WIDTH} columns wide and {HEIGHT} rows high.  One byte per column,
 * bit 0 is the top row.  gfx_text() renders lower case letters as upper case.
 */

#pragma once

#include <stdint.h>

#define FONT5X7_FIRST_CHAR 0x{FIRST_CHAR:02X}
#define FONT5X7_LAST_CHAR  0x{LAST_CHAR:02X}
#define FONT5X7_COUNT      {LAST_CHAR - FIRST_CHAR + 1}
#define FONT5X7_WIDTH      {WIDTH}
#define FONT5X7_HEIGHT     {HEIGHT}
#define FONT5X7_ADVANCE    {WIDTH + 1} /* glyph width plus one blank column */

extern const uint8_t font5x7[FONT5X7_COUNT][FONT5X7_WIDTH];
""")

    with open(os.path.join(out_dir, SOURCE), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("""/*
 * Generated by tools/gen_font5x7.py - do not edit manually.
 */

#include "font5x7.h"

const uint8_t font5x7[FONT5X7_COUNT][FONT5X7_WIDTH] = {
""")
        for char, columns in zip((chr(c) for c in range(FIRST_CHAR, LAST_CHAR + 1)), table):
            values = ", ".join(f"0x{value:02X}" for value in columns)
            handle.write(f"    /* 0x{ord(char):02X} */ {{{values}}},  /* '{char}' */\n")
        handle.write("};\n")

    print(f"wrote main/{HEADER} and main/{SOURCE} ({len(table)} glyphs)")
    print()
    print(render(table, "E32R28T CYD 240X320 !?"))
    print()
    print(render(table, "0123456789"))


if __name__ == "__main__":
    main()
