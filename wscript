
#
# This file is the default set of rules to compile a Pebble project.
#
# Feel free to customize this to your needs.
#

import os.path
import os

try:
    from sh import CommandNotFound, jshint, cat, ErrorReturnCode_2
    hint = jshint
except (ImportError, CommandNotFound):
    hint = None

top = '.'
out = 'build'

def fetch_conf(ctx, name):
    if os.environ.has_key(name):
        ctx.define(name, os.environ[name], quote=False)

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    """
    This method is used to configure your build. ctx.load(`pebble_sdk`) automatically configures
    a build for each valid platform in `targetPlatforms`. Platform-specific configuration: add your
    change after calling ctx.load('pebble_sdk') and make sure to set the correct environment first.
    Universal configuration: add your change prior to calling ctx.load('pebble_sdk').
    """
    fetch_conf(ctx, 'SCREENSHOT')
    fetch_conf(ctx, 'NO_BT')
    fetch_conf(ctx, 'CONFIG_BLUETOOTH_ICON')
    fetch_conf(ctx, 'CONFIG_DATE_DISPLAYED')
    fetch_conf(ctx, 'CONFIG_RAINBOW_MODE')
    fetch_conf(ctx, 'CONFIG_WEATHER_ENABLED')
    fetch_conf(ctx, 'CONFIG_TEMPERATURE_UNIT')
    fetch_conf(ctx, 'CONFIG_MILITARY_TIME')
    ctx.load('pebble_sdk')
    ctx.env.BUNDLE_NAME = "minimalin-reborn.pbw"

def build(ctx):
    if False and hint is not None:
        try:
            hint([node.abspath() for node in ctx.path.ant_glob("src/**/*.js")], _tty_out=False) # no tty because there are none in the cloudpebble sandbox.
        except ErrorReturnCode_2 as e:
            ctx.fatal("\nJavaScript linting failed (you can disable this in Project Settings):\n" + e.stdout)

    # Concatenate all our JS files (but not recursively), and only if any JS exists in the first place.
    ctx.path.make_node('src/js/').mkdir()
    js_paths = ctx.path.ant_glob(['src/*.js', 'src/**/*.js'])
    if js_paths:
        ctx(rule='cat ${SRC} > ${TGT}', source=js_paths, target='pebble-js-app.js')
        has_js = True
    else:
        has_js = False

    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf='{}/pebble-app.elf'.format(p)
        ctx.pbl_program(source=ctx.path.ant_glob('src/**/*.c'),
        target=app_elf)

        if build_worker:
            worker_elf='{}/pebble-worker.elf'.format(p)
            binaries.append({'platform': p, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_worker(source=ctx.path.ant_glob('worker_src/**/*.c'),
            target=worker_elf)
        else:
            binaries.append({'platform': p, 'app_elf': app_elf})

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries, js='pebble-js-app.js' if has_js else [])
    