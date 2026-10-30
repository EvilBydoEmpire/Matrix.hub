# App

The app is plain HTML, CSS and scripts in `web/`, loaded as globals in the order `index.html` lists them. `tools/build_web.py` inlines each page, gzips it into `src/generated/WebAssets.h` and generates `web/palettes.js` from `src/patterns/Palettes.h` and `web/gallery.js` from `src/patterns/PatternCatalog.h`, so the library follows the firmware's gallery order. PlatformIO runs it before every build, `--check` reports stale output, and the compressed budgets are 32 KB for the app and 4 KB for the setup page. The firmware serves `/` and `/setup`, which also answers captive-portal probes, gzipped with an ETag, so a reload costs a 304. The pages load nothing from other hosts, fonts included, so the app works on a network without internet.

## Views

Phones show one view at a time behind the Playing, Library and Device tabs. From 900 px wide, Library and Playing sit side by side and Device opens from the header. The URL hash keeps the view, and light and dark follow the system.

- Playing: device brightness, Cycle patterns with its interval, the pattern's own controls, Colour with the palettes and Saturation, and Reset to defaults.
- Library: every pattern in one grid, in gallery order, without favourites, search, groups or hiding; a tile plays on tap.
- Device: schedule, Wi-Fi with the signal strength, display tuning with the white balance and test chart, and About with the panel, its address and the firmware's commit. On the setup network, where `.local` names don't resolve, the address is the IP alone.

## Look

The look is low-fi to match the panels: the system's monospace fonts, square corners, 2 px ink borders, hard offset shadows, dashed dividers and pixel-square bullets, with every section open. Palettes show as their four stops in hard blocks, and the playing preview casts a shadow in its palette's highlight. The app shows no internal machinery and no credits, which [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) carries. Keep new controls in this vocabulary and check them at phone and desktop widths in light and dark.

## Behaviour

Edits are live. Each pattern write carries the engine and variant its control was drawn for, and the firmware answers 409 once another pattern plays, so an edit never lands on the wrong pattern. Writes run one at a time with pending slider edits first, and the page follows the state revision in `/diagnostics`, so changes from another client or a restart appear without a reload. Redraws keep keyboard focus and leave a field alone while it is being edited.

Picking a pattern stops cycling and is remembered for the next boot. Saving a network never switches to it. Connect, Forget all networks and Reset to defaults ask on the button itself and act on a second press within 4 s, asking again if the network or the pattern changed in between. Wi-Fi changes report in a line that stays until the next action, naming the network to rejoin. The setup page asks for the scan list every half second while the matrix scans and keeps playing.

Library posters are each pattern's brightest frame. Only the playing preview and a hovered or focused tile animate, reduced motion keeps the posters, and the gallery is one tab stop with arrow keys, Home and End between tiles.

## USB bridge

`.venv/bin/python tools/open_usb_app.py`, or a double click on `tools/open_usb_app.command`, serves the same page at `http://127.0.0.1:48615`, or the port in `MATRIX_APP_PORT`, and tunnels its requests over serial, with this checkout's thumbnails for the panel's size. It opens the one USB serial port it finds; with several connected, `MATRIX_SERIAL_PORT` names the matrix's. Opening the port restarts the board once into normal boot: DTR stays low to keep GPIO0 high while RTS pulses EN. The bridge waits up to 10 s for "HTTP server started", holds one connection and lowers both lines before closing.

## Preview

`.venv/bin/python tools/preview_app.py --host VERIFIED_HOST` reads the device once and serves `web/` at `http://127.0.0.1:49386`, rebuilt on every load, with this checkout's thumbnails for the device's panel size. Writes go to a mock that edits, recolours and dims the captured pattern and keeps cycling and schedule edits; switching patterns and resetting are refused, because one snapshot holds no other pattern's settings.
