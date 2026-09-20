#!/usr/bin/env bash
#
# Capture a time sweep of the watchface on every target platform.
#
#   scripts/screenshots.sh [prefix] [platform ...]
#
# The prefix labels the run (use it when capturing a config variant, e.g.
# CONFIG_MILITARY_TIME=true); with no platforms given, every platform in
# package.template.json is captured. Each platform gets one PNG per time plus a
# looping GIF of all of them, a second a frame. A default run then moves the set
# into design/store/<platform>/ as the numbered appstore assets; a variant run
# (prefix given) leaves it in screenshots/<platform>/.
#
# The build is a SCREENSHOT build: mock weather, steps, distance and
# battery/BT/quiet-time glyphs so every info block appears, and on emery and
# gabbro the dense second lines (forecast low/high, weekday, walked distance)
# are forced on regardless of the emulator's persisted config.

set -uo pipefail

ALL_PLATFORMS=(aplite basalt chalk diorite emery flint gabbro)

# Five shots per platform, each earning its place. The watchface keys its
# screenshot mocks to these exact times (see the SCREENSHOT block in
# src/c/minimalin.c), so changing one here without changing it there loses
# whatever that shot was showing.
#
#   10:09  classic pose, all four info blocks clear, day icon, stock hands
#   08:20  low battery glyph, day icon, stock hands
#   16:35  hands low on the dial, day icon, blue hour hand
#   21:25  quiet time glyph, night icon, rainbow minute hand
#   01:05  merged "1:05" display (both hands share a spoke), amber hour hand
#
# The hand colors land on the color platforms only; aplite, diorite and flint
# render all five white on black.
TIMES=(10:09 08:20 16:35 21:25 01:05)

# Emulators only render the second info lines on these; the rest are captured
# with the single-line blocks they compile to.
DENSE_PLATFORMS=(emery gabbro)

# Store folders carry the hardware the codename means, since nobody browsing
# design/store/ remembers which rock is which watch.
declare -A PLATFORM_NAMES=(
  [aplite]="Pebble Classic"
  [basalt]="Pebble Time"
  [chalk]="Pebble Time Round"
  [diorite]="Pebble 2"
  [emery]="Pebble Time 2"
  [flint]="Pebble 2 Duo"
  [gabbro]="Pebble Round 2"
)

# ImageMagick 7 renamed convert to magick; either one builds the GIFs, and
# without either the PNGs are still captured.
IMAGEMAGICK="$(command -v magick || command -v convert)"

function is_dense(){
  local platform="$1"
  for dense in "${DENSE_PLATFORMS[@]}"; do
    [[ "$platform" == "$dense" ]] && return 0
  done
  return 1
}

function stop_emulator(){
  pebble kill --force >/dev/null 2>&1
  killall qemu-pebble >/dev/null 2>&1
  tmpdir="$(dirname "$(mktemp tmp.XXXXXXXXXX -ut)")"
  rm -f "$tmpdir/pb-emulator.json"
}

# Killing QEMU mid-write leaves the emulator's flash image corrupt, and the next
# boot lands on the sad watch crash screen (or dies outright, hanging the
# install) until the image is thrown away. Since every sweep kills QEMU, start
# each platform from a fresh image; the SDK recreates it on boot. It also means
# no persisted config from an earlier session carries into the screenshots.
function reset_emulator_state(){
  local platform="$1"
  rm -f "$HOME"/.pebble-sdk/*/"${platform}"/qemu_spi_flash.bin
}

# A crashed watchapp keeps rendering the same crash screen whatever the RTC
# says, so every frame comes out identical — which a working capture never does,
# because the hands move between shots.
function frames_differ(){
  local prefix="$1" platform="$2"
  local unique
  unique="$(md5sum "screenshots/${platform}/${prefix}"*.png 2>/dev/null | awk '{print $1}' | sort -u | wc -l)"
  [[ "$unique" -eq "${#TIMES[@]}" ]]
}

# One looping GIF per platform, a second on each frame, in the TIMES order
# above. Frames are listed explicitly rather than globbed so a run only picks up
# its own prefix and keeps the order the times are written in.
function animate(){
  local prefix="$1" platform="$2"
  local gif="screenshots/${platform}/${prefix}loop.gif"
  local frames=()
  for clock in "${TIMES[@]}"
  do
    frames+=("screenshots/${platform}/${prefix}${clock/:/}.png")
  done
  if [[ -z "$IMAGEMAGICK" ]]; then
    echo "   no ImageMagick on PATH, skipping $gif" >&2
    return 0
  fi
  "$IMAGEMAGICK" -delay 100 -loop 0 "${frames[@]}" "$gif" || return 1
  echo "   $gif"
}

# design/store/<platform>/ holds what gets uploaded to the appstores, numbered
# the way the listing orders them: the loop GIF as asset 1, then the stills in
# TIMES order. The whole set is replaced, so a shot dropped from TIMES doesn't
# linger as a stale number.
function publish_to_store(){
  local prefix="$1" platform="$2"
  local dest="design/store/${platform} - ${PLATFORM_NAMES[$platform]:-unknown}"
  local src="screenshots/${platform}"
  # Older runs wrote to a bare codename directory; drop it so the platform isn't
  # listed twice.
  [[ -d "design/store/${platform}" ]] && rm -rf "design/store/${platform:?}"
  mkdir -p "$dest"
  rm -f "$dest"/[0-9].png "$dest"/[0-9].gif
  [[ -f "${src}/${prefix}loop.gif" ]] && mv "${src}/${prefix}loop.gif" "${dest}/1.gif"
  local n=2
  for clock in "${TIMES[@]}"
  do
    mv "${src}/${prefix}${clock/:/}.png" "${dest}/${n}.png" || return 1
    n=$((n+1))
  done
  rmdir "$src" 2>/dev/null
  echo "   ${dest}/ (1.gif, 2.png-$((n-1)).png)"
}

function capture(){
  local prefix="$1" platform="$2"
  mkdir -p "screenshots/${platform}"
  rm -f "screenshots/${platform}/${prefix}"*.png
  # When the emulator's QEMU dies on startup, pebble install waits on it
  # forever; the timeout turns that into one failed platform instead of a stuck
  # sweep.
  timeout 180 pebble install --emulator "$platform" -v || return 1
  for clock in "${TIMES[@]}"
  do
    # pebble-tool v5 dropped PEBBLE_QEMU_TIME; emu-set-time sets the RTC live
    pebble emu-set-time --emulator "$platform" "${clock}:00" || return 1
    # The watchface repaints on the minute tick the new RTC triggers, and on the
    # bigger displays that lands after the screenshot does: without this wait,
    # emery and gabbro capture the previous shot's hand colors.
    sleep 2
    pebble screenshot --emulator "$platform" --no-correction "screenshots/${platform}/${prefix}${clock/:/}.png" || return 1
  done
  stop_emulator
  frames_differ "$prefix" "$platform" || return 1
}

function screenshots(){
  local prefix="$1" platform="$2"
  if is_dense "$platform"; then
    echo "== $platform (dense info lines)"
  else
    echo "== $platform"
  fi
  for attempt in 1 2
  do
    reset_emulator_state "$platform"
    if capture "$prefix" "$platform"; then
      animate "$prefix" "$platform"
      return 0
    fi
    stop_emulator
    echo "   $platform capture failed or came back identical frames (attempt ${attempt}/2)" >&2
  done
  return 1
}

cd "$(dirname "$0")/.." || exit 1

if [[ ${1:-} ]]; then
  prefix="${1}_"
  # A variant run captures something other than the stock watchface, so it stays
  # in screenshots/ rather than overwriting the store assets.
  publish=false
else
  prefix="NO_CONFIG_"
  publish=true
fi
shift || true

if [[ $# -gt 0 ]]; then
  platforms=("$@")
else
  platforms=("${ALL_PLATFORMS[@]}")
fi

SCREENSHOT=1 make build || exit 1

failed=()
captured=()
for platform in "${platforms[@]}"
do
  if screenshots "$prefix" "$platform"; then
    captured+=("$platform")
  else
    failed+=("$platform")
  fi
  stop_emulator
done

# Only once every platform is captured, so a run that dies halfway leaves the
# store assets as they were instead of half replaced.
if [[ "$publish" == true && ${#failed[@]} -eq 0 ]]; then
  echo "== publishing to design/store"
  for platform in "${captured[@]}"
  do
    publish_to_store "$prefix" "$platform" || failed+=("$platform")
  done
fi

if [[ ${#failed[@]} -gt 0 ]]; then
  echo "Failed platforms: ${failed[*]}" >&2
  exit 1
fi
if [[ "$publish" == true ]]; then
  echo "Store assets written to design/store/<platform>/"
else
  echo "Screenshots written to screenshots/<platform>/${prefix}*.png, animated as ${prefix}loop.gif"
fi
