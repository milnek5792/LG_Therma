# PlatformIO pre-script: zapíše APP_FW_VERSION = YYMMDD-HHMM při každém buildu.
Import("env")  # type: ignore  # noqa: F821 — poskytuje PlatformIO

from datetime import datetime
from pathlib import Path

stamp = datetime.now().strftime("%y%m%d-%H%M")
out = Path(env["PROJECT_DIR"]) / "include" / "app_build_stamp.h"  # type: ignore  # noqa: F821
out.write_text(
    "#ifndef APP_BUILD_STAMP_H\n"
    "#define APP_BUILD_STAMP_H\n"
    f'#define APP_FW_VERSION "{stamp}"\n'
    "#endif\n",
    encoding="utf-8",
)
print(f"APP_FW_VERSION = {stamp}")
