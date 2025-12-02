# minimalin reborn

[![banner](design/store/marketing-banner.png)](https://apps.rebble.io/en_US/application/63792649c6c24a000a815e70)

Updated fork based on the original minimalin watchface from [GringerApps](https://github.com/GringerApps/minimalin).

Minimalin is a fully customizable watchface that blends analog and digital for a modern and elegant look.

Minimalin uses Nupe, a custom font with numbers and icons, optimized with bitmap mapping to perfectly fit the pixel grid of the Pebble watch.

Some key features of Minimalin:

* Fully configurable colors
* Weather conditions and temperature
* Date display
* Steps from Pebble Health
* Low battery icon
* Bluetooth disconnected icon (pick your favorite)
* Rainbow hand :rainbow:

Go and grab it from [Rebble](https://apps.rebble.io/en_US/application/63792649c6c24a000a815e70) or [RePebble](https://apps.repebble.com/en_US/application/63792649c6c24a000a815e70).

[Test releases here](https://github.com/lanrat/minimalin-reborn/releases)

![Preview](design/minimalin_preview.png)

## License

[MIT](LICENSE.md) for the code.
[OFL](design/font/LICENSE.md) for the Nupe font.

## Dependencies

* [Ractive](https://github.com/ractivejs/ractive/blob/dev/LICENSE.md)
* [Slate](https://github.com/pebble/slate/blob/master/LICENSE)

## Contributing

If you would like a new feature, please [open an issue here](https://github.com/lanrat/minimalin-reborn/issues) and we'll see what we can do.

If you would like to contribute, that's awesome! But we would like you to follow these rules:

Check if an issue is already created for the issue/feature that you'd like to work on. If you'd like to work on a feature, please create an issue first and describe what you'd like to do. If your changes impact the design of the watchface or the configuration page, please provide designs and/or screenshots of your idea.
This way we'll be able to discuss the idea and see if it matches our vision before working on a PR.

## Development

### Creating a Release

To create a new tagged GitHub release:

**Important**: Pebble apps require versions in `X.Y` format (patch must be 0). Use minor version bumps only.

```bash
# 1. Update version in package.json (e.g., 2.1 → 2.2)
# Make sure patch version is always .0

# 2. Build to verify
make build

# 3. Commit the version bump
git add .
git commit -m "Bump version to X.Y"

# 4. Create and push the tag
git tag -a vX.Y
git push --tags

# The GitHub Action will automatically build and create the release with the .pbw file
```

## Credits

Thanks to [OpenWeatherMap](http://openweathermap.org/) for providing us with a free API key.
