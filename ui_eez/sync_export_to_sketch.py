#!/usr/bin/env python3
"""Copy ui_eez/export -> sketch/src as ui_eez_* and patch includes."""
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / "src"
EXPORT = Path(__file__).resolve().parent / "export"

MAP = {
    "screens.c": "ui_eez_screens.cpp",
    "screens.h": "ui_eez_screens.h",
    "ui.c": "ui_eez_ui.cpp",
    "ui.h": "ui_eez_ui.h",
    "styles.c": "ui_eez_styles.cpp",
    "styles.h": "ui_eez_styles.h",
    "images.c": "ui_eez_images.cpp",
    "images.h": "ui_eez_images.h",
    "fonts.h": "ui_eez_fonts.h",
    "vars.h": "ui_eez_vars.h",
    "actions.h": "ui_eez_actions.h",
    "structs.h": "ui_eez_structs.h",
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


def patch_lvgl_include(text: str) -> str:
    return text.replace('#include <lvgl/lvgl.h>', '#include "lg_lvgl.h"')


def patch_includes(text: str) -> str:
    for old, new in INCLUDE_MAP.items():
        text = text.replace(f'#include "{old}"', f'#include "{new}"')
    text = patch_lvgl_include(text)
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


def resolve_export_file(name: str) -> Path | None:
    """Prefer .cee (not compiled by Arduino); fall back to .c from fresh EEZ export."""
    cee = EXPORT / name.replace(".c", ".cee") if name.endswith(".c") else EXPORT / name
    if cee.exists():
        return cee
    src = EXPORT / name
    if src.exists():
        return src
    alt = EXPORT / name.replace(".cee", ".c")
    if alt.exists():
        return alt
    return None


def patch_source_name(src_name: str) -> str:
    """Normalize .cee/.c for patch hooks keyed on original EEZ names."""
    return src_name.replace(".cee", ".c")


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
    text = text.replace(
        "lv_obj_set_style_text_align(obj, LV_ALIGN_CENTER",
        "lv_obj_set_style_align(obj, LV_ALIGN_CENTER",
    )
    text = text.replace("&lv_font_montserrat_40", "&lv_font_montserrat_48")
    text = patch_remove_flag_cast(text)
    text = patch_led_color_ticks(text)
    return text


def patch_styles_c(text: str) -> str:
    text = text.replace(
        "lv_style_set_align(style, LV_ALIGN_LEFT",
        "lv_style_set_text_align(style, LV_TEXT_ALIGN_LEFT",
    )
    text = text.replace("&lv_font_montserrat_40", "&lv_font_montserrat_48")
    return text


def patch_button_clickable(text: str) -> str:
    """EEZ export sundava CLICKABLE i u tlacitek — bez toho LVGL neposle EVENT_CLICKED."""
    text = text.replace(
        "(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM)",
        "(LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM)",
    )
    text = text.replace(
        "lv_obj_add_event_cb(obj, action_",
        "lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);\n            lv_obj_add_event_cb(obj, action_",
    )
    return text


def patch_label_not_clickable(text: str) -> str:
    """Labely jsou ve vychozim stavu CLICKABLE a blokuji tlacitka pod sebou."""
    needle = (
        "lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE|"
        "LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|"
        "LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM));"
    )
    replacement = (
        needle + "\n            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);"
    )
    if needle not in text:
        return text
    return text.replace(needle, replacement)


def patch_btn_tichy_style(text: str) -> str:
    return text.replace(
        "objects.btn_tichy = obj;\n"
        "            lv_obj_set_pos(obj, 1040, 12);\n"
        "            lv_obj_set_size(obj, 210, 36);",
        "objects.btn_tichy = obj;\n"
        "            lv_obj_set_pos(obj, 1040, 12);\n"
        "            lv_obj_set_size(obj, 210, 36);\n"
        "            add_style_btn_tichy(obj);",
    )


def patch_screens_c(text: str) -> str:
    text = patch_lvgl_compat(text)
    text = patch_button_clickable(text)
    text = patch_label_not_clickable(text)
    text = patch_btn_tichy_style(text)
    text = text.replace(
        'lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_label_set_text_static(obj, "°C");',
        'lv_obj_set_style_text_font(obj, &ui_font_font_cs_24, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_label_set_text_static(obj, "°C");',
    )
    text = text.replace(
        'lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_color(obj, lv_color_hex(0xaeaeae), LV_PART_MAIN | LV_STATE_DEFAULT);',
        'lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '            lv_obj_set_style_text_color(obj, lv_color_hex(0xaeaeae), LV_PART_MAIN | LV_STATE_DEFAULT);',
    )
    text = text.replace(
        'lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '                    lv_label_set_text_static(obj, "Tichy rezim");',
        'lv_obj_set_style_text_font(obj, &ui_font_font_cs_16, LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '                    lv_obj_set_style_text_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);\n'
        '                    lv_label_set_text_static(obj, "Tichý režim");',
    )
    return text


def patch_ui_c(text: str) -> str:
    return text.replace(
        "lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);",
        "lv_scr_load(screen);",
    )


def wrap_extern_c(text: str) -> str:
    if 'extern "C"' in text[:300]:
        return text
    return (
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n\n"
        + text
        + "\n#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
    )


def rename_export_c_to_cee() -> None:
    """EEZ writes .c — rename so Arduino Tab5 (RISC-V) does not assemble them."""
    for path in EXPORT.glob("*.c"):
        dest = path.with_suffix(".cee")
        if dest.exists():
            path.unlink()
        else:
            path.rename(dest)
        print("renamed export", path.name, "->", dest.name)


def main():
    if not EXPORT.is_dir():
        raise SystemExit(f"Missing export folder: {EXPORT}")
    DEST.mkdir(exist_ok=True)
    for src_name, dst_name in MAP.items():
        src = resolve_export_file(src_name)
        if src is None:
            print("skip missing", src_name)
            continue
        text = src.read_text(encoding="utf-8")
        text = patch_includes(text)
        key = patch_source_name(src_name)
        if key == "vars.h":
            text = patch_vars_h(text)
        if key == "screens.c":
            text = patch_screens_c(text)
        if key == "styles.c":
            text = patch_styles_c(text)
        if key == "ui.c":
            text = patch_ui_c(text)
        if dst_name.endswith(".cpp"):
            text = wrap_extern_c(text)
        (DEST / dst_name).write_text(text, encoding="utf-8")
        print("wrote", f"src/{dst_name}", "from", src.name)

    rename_export_c_to_cee()


if __name__ == "__main__":
    main()
