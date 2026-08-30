# PlatformIO pre-script: Tab5 (RISC-V) — vypnout ARM Helium/NEON asm v LVGL 9.2
Import("env")  # noqa: F821

from pathlib import Path

libdeps = Path(env["PROJECT_DIR"]) / ".pio" / "libdeps" / env["PIOENV"]
if not libdeps.is_dir():
    print("patch_lvgl_tab5: libdeps not yet present — skip (first lib install)")
else:
    asm_files = [
        "src/draw/sw/blend/helium/lv_blend_helium.S",
        "src/draw/sw/blend/neon/lv_blend_neon.S",
    ]
    for lv_root in libdeps.glob("lvgl*"):
        if not lv_root.is_dir():
            continue
        for rel in asm_files:
            src = lv_root / rel
            off = src.with_suffix(src.suffix + ".off")
            if src.is_file():
                if off.is_file():
                    off.unlink()
                src.rename(off)
                print(f"patch_lvgl_tab5: disabled {rel}")
            elif off.is_file():
                print(f"patch_lvgl_tab5: already disabled {rel}")
