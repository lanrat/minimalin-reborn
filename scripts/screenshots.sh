#!/usr/bin/env bash

function screenshots(){
  mkdir -p "screenshots/${2}"
  pebble install --emulator "$2" -v || exit 1
  i=0
  for hour in $(seq 0 4 23)
  do
    for minute in $(seq 0 10 50)
    do
      # pebble-tool v5 dropped PEBBLE_QEMU_TIME; emu-set-time sets the RTC live
      pebble emu-set-time --emulator "$2" "$hour:$minute:00" || exit 1
      pebble screenshot --emulator "$2" --no-correction "screenshots/${2}/${1}${i}.png" || exit 1
      i=$((i+1))
    done
  done
  pebble kill --force
  killall qemu-pebble
  tmpdir="$(dirname "$(mktemp tmp.XXXXXXXXXX -ut)")"
  rm "$tmpdir/pb-emulator.json"
}

if [[ $1 ]]; then
  prefix="${1}_"
else
  prefix="NO_CONFIG_"
fi

#screenshots "$prefix" "aplite"
#screenshots "$prefix" "basalt"
#screenshots "$prefix" "chalk"
#screenshots "$prefix" "diorite"
screenshots "$prefix" "emery"
#screenshots "$prefix" "flint"
screenshots "$prefix" "gabbro"

wait
