# Hardware Profiles

One source tree builds a separate image per device; the table in [README.md](../README.md#hardware) maps devices to environments. Each environment has an `_ota` twin for uploads over Wi-Fi. Every panel keeps its own white balance, set by eye in Device → Display tuning against a test chart of grey, primary and ramp patches.

## WS2812B

Both WROOM profiles drive GPIO 22 into the panel's DIN with GRB order at 800 kHz, a top-left origin, horizontal serpentine rows and rotation 0. The panel takes regulated 5 V and shares ground with the controller. Before rewiring, remove power and confirm connector polarity, the controller's power input, the supply's capacity and any level shifter on the assembly itself. 5 V on an ESP32 GPIO or 3V3 pin destroys it, and the panel supply must not feed back into a computer over USB.

Patterns render full-scale RGB565. `src/platform/LedEncoder.h` decodes every channel with the same sRGB curve, applies the white balance, brightness and fades as linear gains, and writes exact RGB888 straight into NeoPixel's buffer. The LED behind each pixel is learned from NeoMatrix at boot, so its layout and rotation remain the one description of the wiring. Brightness drives `(b/15)²` of full output, except that brightness 1 keeps three of the 255 PWM steps rather than one, so a dim colour keeps a hue between the primaries. Every LED is seen on its own, so a 4×4 ordered dither sets only the brightest channel's level and the other channels follow it in proportion: each LED keeps its colour's hue down to a single PWM step, and areas dimmer than one step light a fixed pattern of single LEDs that keeps their average. At equal drive the WS2812B's green and blue outshine sRGB's, so yellows read lime and whites cool; WS2812 panels therefore start with green at 69 % and blue at 94 %, the usual WS2812B correction, which Matrix.16 confirmed by eye, and Reset in Device → Display tuning returns there. NeoMatrix's per-channel `gamma5` and `gamma6` tables stay off the pattern path, and only the update notice uses them; NeoPixel brightness stays at 255.

At 24 bits per pixel and 800 kbit/s, sending a frame takes about 7.7 ms on 16×16 and 14.5 ms on 22×22, plus latch and software time, and NeoPixel's `show()` blocks for all of it: Matrix.16 measures 8.3 ms, so a pattern has about 8 ms to draw a 60 fps frame there and about 2 ms on 22×22. The reported WS2812 refresh counts frame writes per second. Nothing limits current in software, so brightness has to suit the actual supply.

`partitions-wroom.csv` is the flash layout every WROOM matrix already carries: two 1.75 MiB application slots and 448 KiB of LittleFS that nothing uses. OTA can't change partitions, so the table stays as it is.

## HUB75

The MatrixPortal S3 drives the panel through its HUB75 connector. The reference board has 8 MB of flash and keeps the board's default `partitions-8MB-tinyuf2.csv`: two 2 MiB application slots, the TinyUF2 bootloader and a FAT partition.

`src/platform/DmaMatrix.h` configures the pinned DMA driver for four bits with double buffering. Its clock, latch blanking, clock phase and output-enable duty were tuned together on the reference panel, so change them together and only while testing on hardware. Brightness and fades scale the colours ahead of the four bits.

Final output rotates 90° clockwise in `src/platform/PanelOrientation.h`. The white balance scales each channel of every frame. The driver scans continuously, so panel refresh differs from the animation frame rate; report driver calculations, row-counter measurements and optical measurements as separate figures.
