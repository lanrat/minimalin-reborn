
VERSION=$(shell cat package.json | grep version | grep -o "[0-9][0-9]*\.[0-9][0-9]*")
NAME=$(shell cat package.json | grep '"name":' | head -1 | sed 's/,//g' |sed 's/"//g' | awk '{ print $2 }')

default: build

init_overlays:
	mkdir -p resources/data
	touch resources/data/OVL_aplite.bin
	touch resources/data/OVL_basalt.bin
	touch resources/data/OVL_chalk.bin
	touch resources/data/OVL_diorite.bin

build: build-config-release weather-api init_overlays
	pebble build

debug: build-config-debug weather-api init_overlays
	pebble build

build-config-release:
	@echo "window.DEBUG = false;" > src/pkjs/build_config.js

build-config-debug:
	@echo "window.DEBUG = true;" > src/pkjs/build_config.js

weather-api:
	@echo "window.API_ID = '$(OPENWEATHERMAP_API_KEY)';" > src/pkjs/weather_id.js

config:
	pebble emu-app-config --emulator $(PEBBLE_EMULATOR)

log:
	pebble logs --emulator $(PEBBLE_EMULATOR)

install:
	pebble install --emulator $(PEBBLE_EMULATOR)

clean:
	pebble clean
	rm -f src/pkjs/weather_id.js src/pkjs/build_config.js

size:
	pebble analyze-size

logs:
	pebble logs --emulator $(PEBBLE_EMULATOR)

phone-logs:
	pebble logs --phone ${PEBBLE_PHONE}

screenshot:
	pebble screenshot --phone ${PEBBLE_PHONE}

deploy:
	pebble install --phone ${PEBBLE_PHONE}

timeline-on:
	pebble emu-set-timeline-quick-view on

timeline-off:
	pebble emu-set-timeline-quick-view off

wipe:
	pebble wipe

.PHONY: all build debug config log install clean size logs screenshot deploy timeline-on timeline-off wipe phone-logs weather-api build-config-release build-config-debug
