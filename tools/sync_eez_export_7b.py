#!/usr/bin/env python3
"""Sync EEZ export -> LG_Therma_7B/src/ui (keeps custom ui_eez_ui / settings / wifi)."""
from __future__ import annotations

import re
import shutil
from pathlib import Path

EEZ_EXPORT = Path(r"C:\Users\mnekv\Documents\Arduino\LG_Therma\ui_eez\export")
DEST = Path(r"C:\Users\mnekv\Documents\Arduino\LG_Therma_7B\src\ui")

MAP = {
    "screens.c": "ui_eez_screens.cpp",
    "screens.h": "ui_eez_screens.h",
    "styles.c": "ui_eez_styles.cpp",
    "styles.h": "ui_eez_styles.h",
    "images.c": "ui_eez_images.cpp",
    "images.h": "ui_eez_images.h",
    "fonts.h": "ui_eez_fonts.h",
    "vars.h": "ui_eez_vars.h",
    # actions.h — 7B má navíc settings/wifi akce, nepřepisovat
    "structs.h": "ui_eez_structs.h",
    # ui.c / ui.h — NEPŘEPISOVAT (7B má vlastní navigaci + settings)
}

INCLUDE_MAP = {
    "screens.h": "ui_eez_screens.h",
    "ui.h": "ui_eez_ui.h",
    "styles.h": "ui_eez_styles.h",
    "images.h": "ui_eez_images.h",
    "fonts.h": "ui_eez_fonts.h",
    "vars.h": "ui_eez_vars.h",
    "actions.h": "ui_eez_actions.h",
    "structs.h": "ui_eez_structs.h",
}


def patch_includes(text: str) -> str:
    for old, new in INCLUDE_MAP.items():
        text = text.replace(f'#include "{old}"', f'#include "{new}"')
    text = text.replace("#include <lvgl/lvgl.h>", '#include "lg_lvgl.h"')
    text = text.replace("#include <lvgl.h>", '#include "lg_lvgl.h"')
    return text


def patch_remove_flag_cast(text: str) -> str:
    lines = []
    for line in text.splitlines(keepends=True):
        if "lv_obj_remove_flag(" in line and "LV_OBJ_FLAG" in line:
            line = re.sub(
                r"lv_obj_remove_flag\((obj, )(.+)\);",
                r"lv_obj_remove_flag(\1(lv_obj_flag_t)(\2));",
                line,
            )
        lines.append(line)
    return "".join(lines)


def patch_lvgl_compat(text: str) -> str:
    text = text.replace(
        "lv_obj_set_style_align(obj, LV_ALIGN_LEFT",
        "lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT",
    )
    text = text.replace(
        "lv_style_set_align(style, LV_ALIGN_LEFT",
        "lv_style_set_text_align(style, LV_TEXT_ALIGN_LEFT",
    )
    text = text.replace("&lv_font_montserrat_40", "&lv_font_montserrat_48")
    return patch_remove_flag_cast(text)


def patch_button_clickable(text: str) -> str:
    text = text.replace(
        "(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM)",
        "(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM)",
    )
    text = text.replace(
        "lv_obj_add_event_cb(obj, action_",
        "lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);\n"
        "            lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);\n"
        "            lv_obj_add_event_cb(obj, action_",
    )
    return text


def patch_label_not_clickable(text: str) -> str:
    needle = (
        "lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|"
        "LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|"
        "LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));"
    )
    replacement = needle + "\n            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);"
    return text.replace(needle, replacement) if needle in text else text


def patch_led_color_ticks(text: str) -> str:
    leds = (
        ("get_var_sig_chod___3199320___0x2c2c2e()", "objects.led_chod"),
        ("get_var_sig_cerpadlo___3199320___0x2c2c2e()", "objects.led_cerpadlo"),
        ("get_var_sig_kompresor___16752394___0x2c2c2e()", "objects.led_kompresor"),
        ("get_var_sig_odmrazovani___3199320___0x2c2c2e()", "objects.led_odmrazovani"),
        ("get_var_sig_el_topeni___16752394___0x2c2c2e()", "objects.led_el_topeni"),
    )
    for getter, led_obj in leds:
        new_block = f"""    {{
        static uint32_t last_hex = 0xFFFFFFFF;
        uint32_t hex = {getter};
        if (hex != last_hex) {{
            last_hex = hex;
            tick_value_change_obj = {led_obj};
            lv_led_set_color({led_obj}, lv_color_hex(hex));
            tick_value_change_obj = NULL;
        }}
    }}"""
        for cur_read in (
            f"lv_color_to_u32(((lv_led_t *){led_obj})->color)",
            f"ui_eez_led_color_u32({led_obj})",
        ):
            old_block = f"""    {{
        uint32_t new_val = {getter};
        new_val = lv_color_to_u32(lv_color_hex(new_val));
        uint32_t cur_val = {cur_read};
        if (new_val != cur_val) {{
            tick_value_change_obj = {led_obj};
            lv_led_set_color({led_obj}, lv_color_hex(new_val));
            tick_value_change_obj = NULL;
        }}
    }}"""
            text = text.replace(old_block, new_block)
    return text


def patch_screens_h(text: str) -> str:
    # EEZ export má jen MAIN — 7B potřebuje SETTINGS + WIFI_SETUP
    text = text.replace(
        """enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};""",
        """enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SETTINGS = 2,
    SCREEN_ID_WIFI_SETUP = 3,
    _SCREEN_ID_LAST = 3
};""",
    )
    return text


def patch_screens_c(text: str) -> str:
    text = patch_lvgl_compat(text)
    text = patch_button_clickable(text)
    text = patch_label_not_clickable(text)
    text = patch_led_color_ticks(text)

    # tick hooks for native settings / wifi form
    text = text.replace(
        """typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};""",
        """#include "ui_eez_settings.h"
#include "ui_eez_wifi_form.h"

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    uiSettingsTick,
    uiWifiFormTick,
};""",
    )
    text = text.replace(
        "if (screen_index >= 0 && screen_index < 1)",
        "if (screen_index >= 0 && screen_index < 3)",
    )
    return text


def patch_vars_h(text: str) -> str:
    for name in (
        "teplota_vody_set",
        "teplota_vody_vstup",
        "teplota_vody_vystup",
        "teplota_vnitrni",
        "teplota_venkovni",
    ):
        text = text.replace(
            f"extern float get_var_{name}();",
            f"extern const char *get_var_{name}();",
        )
    extra = """
extern const char *get_var_sig_wifi____wi_fi__ok_____wi_fi______();
extern const char *get_var_sig_mqtt____mqtt__pripojeno_____mqtt______();
extern uint32_t get_var_sig_chod___3199320___0x2c2c2e();
extern int32_t get_var_sig_chod___255___50();
extern uint32_t get_var_sig_cerpadlo___3199320___0x2c2c2e();
extern int32_t get_var_sig_cerpadlo___255___50();
extern uint32_t get_var_sig_kompresor___16752394___0x2c2c2e();
extern int32_t get_var_sig_kompresor___255___50();
extern uint32_t get_var_sig_odmrazovani___3199320___0x2c2c2e();
extern int32_t get_var_sig_odmrazovani___255___50();
extern uint32_t get_var_sig_el_topeni___16752394___0x2c2c2e();
extern int32_t get_var_sig_el_topeni___255___50();
"""
    return text.replace(
        "#ifdef __cplusplus\n}\n#endif\n\n#endif /*EEZ_LVGL_UI_VARS_H*/",
        extra + "\n#ifdef __cplusplus\n}\n#endif\n\n#endif /*EEZ_LVGL_UI_VARS_H*/",
    )


def wrap_extern_c(text: str) -> str:
    if 'extern "C"' in text[:400]:
        return text
    return (
        '#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
        + text
        + '\n#ifdef __cplusplus\n}\n#endif\n'
    )


def resolve_src(name: str) -> Path | None:
    p = EEZ_EXPORT / name
    if p.exists():
        return p
    if name.endswith(".c"):
        alt = EEZ_EXPORT / name.replace(".c", ".cee")
        if alt.exists():
            return alt
    return None


def maybe_copy_fonts() -> None:
    for src in EEZ_EXPORT.glob("ui_font_*.c"):
        dst = DEST / ("ui_eez_" + src.stem.replace("ui_font_", "font_") + ".cpp")
        # keep existing naming: ui_eez_font_cs_24.cpp
        name = src.name  # ui_font_font_cs_24.c
        m = re.match(r"ui_font_(.+)\.c", name)
        if not m:
            continue
        dst = DEST / f"ui_eez_{m.group(1)}.cpp"
        text = src.read_text(encoding="utf-8")
        text = patch_includes(text)
        text = wrap_extern_c(text)
        dst.write_text(text, encoding="utf-8")
        print("wrote", dst.name, "from", src.name)


def main() -> None:
    if not EEZ_EXPORT.is_dir():
        raise SystemExit(f"Missing export: {EEZ_EXPORT}")
    DEST.mkdir(parents=True, exist_ok=True)

    for src_name, dst_name in MAP.items():
        src = resolve_src(src_name)
        if src is None:
            print("skip missing", src_name)
            continue
        text = src.read_text(encoding="utf-8")
        text = patch_includes(text)
        if src_name == "screens.h":
            text = patch_screens_h(text)
        if src_name == "screens.c":
            text = patch_screens_c(text)
        if src_name == "vars.h":
            text = patch_vars_h(text)
        if src_name == "styles.c":
            text = patch_lvgl_compat(text)
            text = text.replace(
                "lv_style_set_align(style, LV_ALIGN_LEFT",
                "lv_style_set_text_align(style, LV_TEXT_ALIGN_LEFT",
            )
        if dst_name.endswith(".cpp"):
            text = wrap_extern_c(text)
        (DEST / dst_name).write_text(text, encoding="utf-8")
        print("wrote", dst_name, "from", src.name)

    maybe_copy_fonts()

    # rename fresh .c in export to .cee (Arduino safety, same as Tab5 flow)
    for path in EEZ_EXPORT.glob("*.c"):
        dest = path.with_suffix(".cee")
        if dest.exists():
            path.unlink()
        else:
            path.rename(dest)
        print("renamed export", path.name if path.exists() else dest.name)

    print("OK: synced to", DEST)
    print("NOTE: ui_eez_ui.cpp / settings / wifi / actions.cpp left untouched")


if __name__ == "__main__":
    main()
