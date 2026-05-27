#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import os.path
import os
import re
import json
import base64

top = '.'
out = 'build'


def build_config_page(ctx):
    """Generate src/pkjs/config_page.js: a self-contained data: URI of the
    settings page, built by inlining all assets referenced from docs/index.html.
    This removes the runtime dependency on the GitHub Pages site."""
    repo_root = ctx.path.abspath()
    docs = os.path.join(repo_root, 'docs')

    def read_text(p):
        with open(p, 'r', encoding='utf-8') as f:
            return f.read()

    def read_bytes(p):
        with open(p, 'rb') as f:
            return f.read()

    index_html = read_text(os.path.join(docs, 'index.html'))
    ractive_js = read_text(os.path.join(docs, 'js', 'ractive.min.js'))
    transitions_js = read_text(os.path.join(docs, 'js', 'ractive-transitions-slide.js'))
    slate_css = read_text(os.path.join(docs, 'css', 'slate.min.css'))
    font_b64 = base64.b64encode(read_bytes(os.path.join(docs, 'fonts', 'Nupe.ttf'))).decode('ascii')
    font_data_uri = 'data:font/ttf;base64,' + font_b64

    template_names = ['ConfigView', 'ItemColor', 'ToggleWithItemColor', 'Tabs', 'Input', 'Toggle']
    templates = {}
    for name in template_names:
        t = read_text(os.path.join(docs, 'template', name + '.html'))
        if name == 'ConfigView':
            t = t.replace('./fonts/Nupe.ttf', font_data_uri)
        templates[name] = t

    # Escape </ to avoid breaking the surrounding <script> tag when embedded.
    templates_json = json.dumps(templates).replace('</', '<\\/')

    # Mini loader that replaces ractive-load for the inlined case. Parses each
    # template's <link rel="ractive">, <style>, and <script> blocks, then builds
    # a Ractive component constructor from what remains.
    loader = (
        "(function(){"
        "var TEMPLATES=" + templates_json + ";"
        "var subNames=['Toggle','Input','Tabs','ItemColor','ToggleWithItemColor'];"
        "function compile(html){"
        "html=html.replace(/<link[^>]*rel=[\"']ractive[\"'][^>]*>\\s*/g,'');"
        "var css='';"
        "html=html.replace(/<style>([\\s\\S]*?)<\\/style>/g,function(_,c){css+=c;return '';});"
        "var opts={};"
        "html=html.replace(/<script>([\\s\\S]*?)<\\/script>/g,function(_,c){"
        "var component={exports:{}};(new Function('component',c))(component);"
        "for(var k in component.exports)opts[k]=component.exports[k];return '';});"
        "opts.template=html.trim();opts.css=css;return Ractive.extend(opts);"
        "}"
        "subNames.forEach(function(n){Ractive.components[n]=compile(TEMPLATES[n]);});"
        "var ConfigView=compile(TEMPLATES.ConfigView);"
        "new ConfigView({el:'body',data:data});"
        "})();"
    )

    html = index_html
    html = html.replace(
        "<script type='text/javascript' src='js/ractive.min.js'></script>",
        "<script>" + ractive_js + "</script>")
    html = html.replace(
        "<script type='text/javascript' src='js/ractive-transitions-slide.js'></script>",
        "<script>" + transitions_js + "</script>")
    html = html.replace(
        "<script type='text/javascript' src='//cdn.jsdelivr.net/ractive.load/latest/ractive-load.min.js'></script>",
        "")
    html = html.replace(
        "<link rel='stylesheet' type='text/css' href='css/slate.min.css'>",
        "<style>" + slate_css + "</style>")
    html = re.sub(r'<link rel="prefetch"[^>]*>\s*', '', html)

    # Swap the Ractive.load bootstrap (everything from `Ractive.load(...)` up
    # to but not including the closing </script>) with our inline loader.
    new_html, n = re.subn(
        r"Ractive\.load\('template/ConfigView\.html'\)[\s\S]*?(?=\s*</script>)",
        lambda m: loader,
        html, count=1)
    if n != 1:
        ctx.fatal('build_config_page: failed to locate Ractive.load bootstrap in docs/index.html')
    html = new_html

    data_uri = 'data:text/html;base64,' + base64.b64encode(html.encode('utf-8')).decode('ascii')
    out_path = os.path.join(repo_root, 'src', 'pkjs', 'config_page.js')
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('module.exports = ' + json.dumps(data_uri) + ';\n')

def fetch_conf(ctx, name):
    """Fetch environment variable and define it for build"""
    if name in os.environ:
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
    # Fetch custom configuration from environment variables
    fetch_conf(ctx, 'SCREENSHOT')
    fetch_conf(ctx, 'NO_BT')
    fetch_conf(ctx, 'CONFIG_BLUETOOTH_ICON')
    fetch_conf(ctx, 'CONFIG_DATE_DISPLAYED')
    fetch_conf(ctx, 'CONFIG_RAINBOW_MODE')
    fetch_conf(ctx, 'CONFIG_WEATHER_ENABLED')
    fetch_conf(ctx, 'CONFIG_TEMPERATURE_UNIT')
    fetch_conf(ctx, 'CONFIG_MILITARY_TIME')

    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')

    build_config_page(ctx)

    build_worker = os.path.exists('worker_src')
    binaries = []

    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[platform])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                          target=worker_elf,
                          bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
