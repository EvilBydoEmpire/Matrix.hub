"""Expose the existing S3 clock divider ONLY in the isolated timing build.

Post-build-configuration hook: dependency installation has already completed.
Pinned Protomatter 1.7.1 otherwise remains unchanged. Refuse unknown sources.
"""
from pathlib import Path
Import("env")
name=env.subst("$PIOENV")
if name not in ("matrixportal_s3_hub75_4bit_20mhz","matrixportal_s3_hub75_4bit_20mhz_usb"):
    raise RuntimeError("Timing hook must not modify a production environment")
header=Path(env.subst("$PROJECT_LIBDEPS_DIR"))/name/"Adafruit Protomatter"/"src"/"arch"/"esp32-s3.h"
source=header.read_text()
original="#define LCD_CLK_PRESCALE 9 // 8, 9, 10 allowed. Bit clock = 160 MHz / this."
patched="#ifndef LCD_CLK_PRESCALE\n"+original+"\n#endif // matrix isolated timing test"
if patched not in source:
    if source.count(original)!=1:raise RuntimeError("Unexpected Protomatter S3 source; review before patching")
    header.write_text(source.replace(original,patched))
print("Isolated Protomatter timing test: LCD divider 8 = 20 MHz; all timing safety margins retained")
