#!/usr/bin/env python3
"""Rebuild design/store/marketing-banner.png from the current store screenshots.

The watch bodies are the emulator decorations the Pebble SDK ships, which are
straight-on renders with a transparent screen hole sized exactly like the
platform's display: paste a screenshot at the hole offset, lay the frame over
it, and the watch is done, no scaling of the screen art and no perspective work.

The "minimalin" wordmark is lifted from the original banner rather than
re-typeset (design/assets/banner-text.png), since the font it was set in is not
in this repo and guessing at it would change the look.

Usage: scripts/banner.py [--out FILE]
"""

import argparse
import glob
import os
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Appstore banner size. The watches are scaled to fit this height with their
# straps bleeding off the top and bottom edges, the way the original did.
SIZE = (720, 320)
BACKGROUND = (0, 0, 0)

TEXT_STRIP = "design/assets/banner-text.png"

# Which watch goes where: the decoration to use, the screenshot to put in it,
# and where the pair lands on the canvas once scaled. Colorways are gold round
# and silver Time 2, both on blue straps: the black bodies would disappear
# against the background. The two screenshots differ in time and hand color so
# the pair advertises the config rather than looking copy-pasted.
WATCHES = [
    {
        "frame": "pr2-gd14.png",
        "shot": "design/store/gabbro - Pebble Round 2/2.png",  # 10:09, stock red hands
        "scale": 0.70,
        "at": (272, -1),
    },
    {
        "frame": "pt2-sb.png",
        "shot": "design/store/emery - Pebble Time 2/4.png",  # 16:35, blue hour hand
        "scale": 0.70,
        "at": (478, 9),
    },
]


def decorations_dir():
    """Newest SDK's QEMU decorations, which is where the watch bodies live."""
    candidates = sorted(glob.glob(os.path.expanduser(
        "~/.pebble-sdk/SDKs/*/toolchain/share/qemu/pebble-decorations")))
    if not candidates:
        sys.exit("no pebble-decorations found; install a Pebble SDK first")
    return candidates[-1]


def screen_hole(frame):
    """Bounding box of the frame's transparent screen area.

    The hole is the only fully transparent region that the border cannot reach,
    so flood the outside first and whatever transparency is left is the screen.
    """
    width, height = frame.size
    alpha = frame.split()[3].load()
    outside = [[False] * width for _ in range(height)]
    stack = []
    for x in range(width):
        for y in (0, height - 1):
            if alpha[x, y] < 20:
                outside[y][x] = True
                stack.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if alpha[x, y] < 20 and not outside[y][x]:
                outside[y][x] = True
                stack.append((x, y))
    while stack:
        x, y = stack.pop()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height and not outside[ny][nx] and alpha[nx, ny] < 20:
                outside[ny][nx] = True
                stack.append((nx, ny))
    xs, ys = [], []
    for y in range(height):
        for x in range(width):
            if alpha[x, y] < 20 and not outside[y][x]:
                xs.append(x)
                ys.append(y)
    if not xs:
        sys.exit("frame has no screen hole")
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def build_watch(spec, decorations):
    frame = Image.open(os.path.join(decorations, spec["frame"])).convert("RGBA")
    shot = Image.open(os.path.join(REPO, spec["shot"])).convert("RGBA")
    left, top, right, bottom = screen_hole(frame)
    hole = (right - left, bottom - top)
    if shot.size != hole:
        sys.exit(f"{spec['shot']} is {shot.size}, but {spec['frame']} expects {hole}")
    watch = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    watch.paste(shot, (left, top))
    watch.alpha_composite(frame)
    scaled = (round(frame.width * spec["scale"]), round(frame.height * spec["scale"]))
    # LANCZOS over NEAREST: at 0.7 the display's 1px hands break up under a
    # nearest-neighbour shrink, and the banner is viewed as a picture of a
    # watch rather than as pixels.
    return watch.resize(scaled, Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="design/store/marketing-banner.png")
    args = parser.parse_args()

    decorations = decorations_dir()
    banner = Image.new("RGB", SIZE, BACKGROUND)
    text = Image.open(os.path.join(REPO, TEXT_STRIP)).convert("RGBA")
    banner.paste(text, (0, 0), text)

    for spec in WATCHES:
        watch = build_watch(spec, decorations)
        x, y = spec["at"]
        # Centre the scaled watch vertically on the offset given, so nudging a
        # watch is a matter of moving it rather than recomputing its height.
        banner.paste(watch, (x, y + (SIZE[1] - watch.height) // 2), watch)

    out = os.path.join(REPO, args.out)
    banner.save(out)
    print(f"wrote {args.out} ({banner.width}x{banner.height})")


if __name__ == "__main__":
    main()
