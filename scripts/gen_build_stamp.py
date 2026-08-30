# PlatformIO pre-script: APP_FW_VERSION = YYMMDD-HHMM
Import("env")  # noqa: F821

from datetime import datetime
from pathlib import Path

stamp = datetime.now().strftime("%y%m%d-%H%M")
out = Path(env["PROJECT_DIR"]) / "include" / "app_build_stamp.h"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(
    "#ifndef APP_BUILD_STAMP_H\n"
    "#define APP_BUILD_STAMP_H\n"
    f'#define APP_FW_VERSION "{stamp}"\n'
    "#endif\n",
    encoding="utf-8",
)
print(f"APP_FW_VERSION = {stamp}")
