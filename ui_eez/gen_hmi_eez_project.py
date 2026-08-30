#!/usr/bin/env python3
"""Generate LG_Therma_HMI.eez-project from HTML mockup layout."""
import json
import uuid
from copy import deepcopy

OUT = r"C:\Users\mnekv\Documents\Arduino\LG_Therma\ui_eez\LG_Therma_HMI.eez-project"

_counter = 0

def uid():
    global _counter
    _counter += 1
    return str(uuid.uuid5(uuid.NAMESPACE_DNS, f"lg-therma-hmi-{_counter}"))

def style_ref(name="default"):
    return {
        "objID": uid(),
        "useStyle": name,
        "conditionalStyles": [],
        "childStyles": [],
    }

def local_style(**main_default):
    return {
        "objID": uid(),
        "definition": {
            "MAIN": {
                "DEFAULT": main_default,
            }
        },
    }

def base_widget(wtype, left, top, width, height, name="", children=None, **extra):
    w = {
        "objID": uid(),
        "type": wtype,
        "left": left,
        "top": top,
        "width": width,
        "height": height,
        "customInputs": [],
        "customOutputs": [],
        "style": style_ref(),
        "timeline": [],
        "eventHandlers": [],
        "leftUnit": "px",
        "topUnit": "px",
        "widthUnit": "px",
        "heightUnit": "px",
        "children": children or [],
        "flags": "PRESS_LOCK|CLICK_FOCUSABLE|GESTURE_BUBBLE|SNAPPABLE|SCROLL_CHAIN",
        "hiddenFlagType": "literal",
        "clickableFlagType": "literal",
        "scrollbarMode": "auto",
        "scrollDirection": "all",
        "checkedStateType": "literal",
        "disabledStateType": "literal",
        "states": "",
        "localStyles": {"objID": uid()},
        "group": "",
        "groupIndex": 0,
    }
    if name:
        w["identifier"] = name
    w.update(extra)
    return w

def label(left, top, text, name="", text_type="literal", preview="", **style):
    ls = local_style(**style) if style else {"objID": uid()}
    return base_widget(
        "LVGLLabelWidget",
        left,
        top,
        200,
        32,
        name=name,
        widthUnit="content",
        heightUnit="content",
        localStyles=ls,
        text=text,
        textType=text_type,
        previewValue=preview,
        longMode="WRAP",
        recolor=False,
    )

def button(left, top, width, height, name, action, child_text, use_style="default", **btn_style):
    child = label(
        0,
        0,
        child_text,
        text_type="literal",
        align="CENTER",
        text_font=btn_style.pop("text_font", "MONTSERRAT_18"),
        text_color=btn_style.pop("text_color", "#ffffff"),
    )
    w = base_widget(
        "LVGLButtonWidget",
        left,
        top,
        width,
        height,
        name=name,
        style=style_ref(use_style),
        localStyles=local_style(**btn_style) if btn_style else {"objID": uid()},
        clickable=True,
        clickableFlagType="literal",
        children=[child],
    )
    if action:
        w["eventHandlers"] = [
            {
                "objID": uid(),
                "eventName": "CLICKED",
                "handlerType": "action",
                "action": action,
            }
        ]
    return w

def container(left, top, width, height, name="", children=None, **style):
    w = base_widget(
        "LVGLContainerWidget",
        left,
        top,
        width,
        height,
        name=name,
        children=children or [],
        clickable=False,
        clickableFlagType="literal",
    )
    if style:
        w["localStyles"] = local_style(**style)
    return w

def led(left, top, size, name, sig, color_on=0x30D158):
    return base_widget(
        "LVGLLedWidget",
        left,
        top,
        size,
        size,
        name=name,
        widthUnit="px",
        heightUnit="px",
        color=f"{sig} ? {color_on} : 0x2c2c2e",
        colorType="expression",
        brightness=f"{sig} ? 255 : 50",
        brightnessType="expression",
        clickable=False,
        clickableFlagType="literal",
    )

def indicator_row(y, sig, text, name_prefix, color_on=0x30D158):
    x = 870
    return [
        led(x, y, 14, f"led_{name_prefix}", sig, color_on),
        label(
            x + 22,
            y - 2,
            text,
            name=f"lbl_{name_prefix}",
            text_font="MONTSERRAT_16",
            text_color="#e0e0e6",
            align="LEFT",
        ),
    ]

def telemetry_cell(x, label_text, var_expr, name, value_color="#ffffff", preview="--"):
    w = 256
    items = [
        label(
            x + 25,
            620,
            label_text,
            name=f"lbl_{name}_title",
            text_type="literal",
            text_font="MONTSERRAT_14",
            text_color="#8e8e93",
            align="LEFT",
        ),
        label(
            x + 25,
            645,
            var_expr,
            name=f"lbl_{name}_value",
            text_type="expression",
            preview=preview,
            text_font="MONTSERRAT_32",
            text_color=value_color,
            align="LEFT",
        ),
    ]
    return items

def build_variables():
    defs = [
        ("teplota_vody_set", "float", "40", "Pozadovana teplota vody"),
        ("teplota_vody_vstup", "float", "0", "Teplota vody vstup"),
        ("teplota_vody_vystup", "float", "0", "Teplota vody vystup"),
        ("teplota_vnitrni", "float", "-1000", "Pokojova teplota BLE"),
        ("teplota_venkovni", "float", "-1000", "Venkovni teplota"),
        ("teplota_spad", "float", "0", "Delta T"),
        ("rezim", "integer", "1", "0=Auto 1=Vystupni teplota"),
        ("stav_tc", "integer", "0", "0=Vyp 1=Cekam 2=Prestart 3=Beh"),
        ("sig_chod", "boolean", "0", "T/C provoz"),
        ("sig_cerpadlo", "boolean", "0", "Cerpadlo"),
        ("sig_kompresor", "boolean", "0", "Kompresor"),
        ("sig_el_topeni", "boolean", "0", "El. patrona"),
        ("sig_odmrazovani", "boolean", "0", "Odmrazovani"),
        ("sig_wifi", "boolean", "0", "WiFi"),
        ("sig_mqtt", "boolean", "0", "MQTT"),
        ("sig_ble", "boolean", "0", "BLE"),
        ("sig_utlum", "boolean", "0", "Utlum"),
        ("sig_alarm", "boolean", "0", "Alarm"),
        ("cas_text", "string", '"--:--"', "Cas HH:MM"),
        ("datum_text", "string", '"--.--.----"', "Datum"),
        ("cas_platny", "boolean", "0", "NTP OK"),
        ("akce_tlacitko", "integer", "0", "Akce tlacitka"),
        ("plan_text", "string", '"Pracovni dny (Po-Pa) • Pristi zmena ve 22:00"', "Text planovace"),
    ]
    gvars = []
    for i, (name, typ, default, desc) in enumerate(defs, 1):
        gvars.append(
            {
                "objID": uid(),
                "id": i,
                "name": name,
                "description": desc,
                "type": typ,
                "defaultValue": default,
                "native": True,
            }
        )
    return gvars

def build_actions():
    names = [
        "akce_teplota_plus",
        "akce_teplota_minus",
        "akce_start_stop",
        "akce_tichy_rezim",
        "akce_menu",
    ]
    return [
        {
            "objID": uid(),
            "components": [],
            "connectionLines": [],
            "localVariables": [],
            "name": n,
        }
        for n in names
    ]

def build_styles():
    def style(name, widget_type, **props):
        return {
            "objID": uid(),
            "name": name,
            "forWidgetType": widget_type,
            "childStyles": [],
            "definition": {
                "objID": uid(),
                "definition": {
                    "MAIN": {"DEFAULT": props},
                },
            },
        }

    return [
        style(
            "btn_run",
            "LVGLButtonWidget",
            bg_color="#30d158",
            text_color="#000000",
            radius=8,
            text_align="CENTER",
            text_font="MONTSERRAT_18",
            border_width=0,
        ),
        style(
            "btn_stop",
            "LVGLButtonWidget",
            bg_color="#24242b",
            text_color="#636366",
            radius=8,
            text_align="CENTER",
            text_font="MONTSERRAT_18",
            border_width=0,
        ),
        style(
            "btn_step",
            "LVGLButtonWidget",
            bg_color="#24242b",
            text_color="#ffffff",
            radius=50,
            text_align="CENTER",
            text_font="MONTSERRAT_40",
            border_width=2,
            border_color="#3a3a45",
        ),
        style(
            "btn_menu",
            "LVGLButtonWidget",
            bg_color="#24242b",
            text_color="#ffffff",
            radius=0,
            text_align="CENTER",
            text_font="MONTSERRAT_18",
            border_width=0,
        ),
        style(
            "btn_tichy",
            "LVGLButtonWidget",
            bg_color="#0a84ff",
            text_color="#ffffff",
            radius=6,
            text_align="CENTER",
            text_font="MONTSERRAT_14",
            border_width=0,
        ),
    ]

def build_screen_children():
    children = []

    # Screen background panels (absolute layout)
    children.append(
        container(
            0,
            0,
            1280,
            720,
            "panel_screen",
            bg_color="#121214",
            border_width=0,
        )
    )
    children.append(
        container(
            0,
            0,
            1280,
            60,
            "panel_top",
            bg_color="#1a1a1f",
            border_width=0,
            border_side="BOTTOM",
            border_color="#24242b",
        )
    )
    children.append(
        container(
            0,
            600,
            1280,
            120,
            "panel_bottom",
            bg_color="#1a1a1f",
            border_width=0,
            border_side="TOP",
            border_color="#24242b",
        )
    )
    children.append(
        container(
            850,
            60,
            430,
            540,
            "panel_tech",
            bg_color="#151518",
            border_width=0,
            border_side="LEFT",
            border_color="#24242b",
        )
    )
    children.append(
        container(
            850,
            60,
            2,
            540,
            "divider_main",
            bg_color="#24242b",
            border_width=0,
        )
    )

    # Top bar
    children.append(
        label(
            30,
            16,
            "cas_text",
            name="lbl_cas",
            text_type="expression",
            preview="14:32",
            text_font="MONTSERRAT_22",
            text_color="#ffffff",
            align="LEFT",
        )
    )
    children.append(
        label(
            500,
            20,
            'sig_wifi ? "Wi-Fi: OK" : "Wi-Fi: ---"',
            name="lbl_wifi",
            text_type="expression",
            preview="Wi-Fi: OK",
            text_font="MONTSERRAT_16",
            text_color="#30d158",
            align="LEFT",
        )
    )
    children.append(
        label(
            720,
            20,
            'sig_mqtt ? "MQTT: Pripojeno" : "MQTT: ---"',
            name="lbl_mqtt",
            text_type="expression",
            preview="MQTT: Pripojeno",
            text_font="MONTSERRAT_16",
            text_color="#30d158",
            align="LEFT",
        )
    )
    children.append(
        button(
            1040,
            12,
            210,
            36,
            "btn_tichy",
            "akce_tichy_rezim",
            "Tichy rezim",
            use_style="btn_tichy",
        )
    )

    # User zone - temp control
    children.append(
        label(
            490,
            130,
            "POZADOVANA TEPLOTA",
            name="lbl_setpoint_title",
            text_font="MONTSERRAT_14",
            text_color="#8e8e93",
            align="CENTER",
        )
    )
    children.append(
        button(
            300,
            175,
            100,
            100,
            "btn_minus",
            "akce_teplota_minus",
            "-",
            use_style="btn_step",
        )
    )
    children.append(
        label(
            455,
            165,
            "teplota_vody_set",
            name="lbl_setpoint",
            text_type="expression",
            preview="22.5",
            text_font="MONTSERRAT_48",
            text_color="#ffffff",
            align="CENTER",
        )
    )
    children.append(
        label(
            590,
            200,
            "°C",
            name="lbl_setpoint_unit",
            text_font="MONTSERRAT_28",
            text_color="#ffffff",
            align="LEFT",
        )
    )
    children.append(
        button(
            720,
            175,
            100,
            100,
            "btn_plus",
            "akce_teplota_plus",
            "+",
            use_style="btn_step",
        )
    )

    # Plan widget
    children.append(
        container(
            40,
            430,
            770,
            90,
            "panel_plan",
            bg_color="#1a1a1f",
            radius=8,
            border_width=1,
            border_color="#24242b",
        )
    )
    children.append(
        label(
            60,
            445,
            "TYDENNI PLAN AKTIVNI",
            name="lbl_plan_title",
            text_font="MONTSERRAT_16",
            text_color="#ff9f0a",
            align="LEFT",
        )
    )
    children.append(
        label(
            60,
            472,
            "plan_text",
            name="lbl_plan_text",
            text_type="expression",
            preview="Pracovni dny (Po-Pa) • Pristi zmena ve 22:00 (utlum na 18.0°C)",
            text_font="MONTSERRAT_16",
            text_color="#aeaeae",
            align="LEFT",
        )
    )

    # Tech zone - RUN/STOP
    children.append(
        button(
            865,
            90,
            195,
            60,
            "btn_run",
            "akce_start_stop",
            "RUN",
            use_style="btn_run",
        )
    )
    children.append(
        button(
            1070,
            90,
            195,
            60,
            "btn_stop",
            "akce_start_stop",
            "STOP",
            use_style="btn_stop",
        )
    )

    # Indicators
    rows = [
        ("sig_chod", "BEH SYSTEMU", "chod", 0x30D158),
        ("sig_cerpadlo", "OBEHOVE CERPADLO", "cerpadlo", 0x30D158),
        ("sig_kompresor", "KOMPRESOR", "kompresor", 0xFF9F0A),
        ("sig_odmrazovani", "ODMRAZOVANI", "odmrazovani", 0x30D158),
        ("sig_el_topeni", "PRIDAVNE EL. TOPENI", "el_topeni", 0xFF9F0A),
    ]
    y = 185
    for sig, text, name, color in rows:
        children.extend(indicator_row(y, sig, text, name, color))
        y += 42

    # Bottom telemetry
    children.extend(
        telemetry_cell(0, "Vnitrni teplota", "teplota_vnitrni", "vnitrni", "#ffffff", "21.2")
    )
    children.extend(
        telemetry_cell(256, "Venkovni teplota", "teplota_venkovni", "venkovni", "#64d2ff", "4.5")
    )
    children.extend(
        telemetry_cell(512, "Vstupni voda", "teplota_vody_vstup", "vstup", "#ffffff", "38.5")
    )
    children.extend(
        telemetry_cell(768, "Vystupni voda", "teplota_vody_vystup", "vystup", "#ff9f0a", "44.2")
    )
    children.append(
        button(
            1024,
            600,
            256,
            120,
            "btn_menu",
            "akce_menu",
            "MENU",
            use_style="btn_menu",
        )
    )

    return children

def build_project():
    screen = base_widget(
        "LVGLScreenWidget",
        0,
        0,
        1280,
        720,
        name="screen_main",
        children=build_screen_children(),
        clickable=True,
        clickableFlagType="literal",
        localStyles=local_style(bg_color="#121214"),
    )

    build_files = [
        ("screens.h", "#ifndef EEZ_LVGL_UI_SCREENS_H\r\n#define EEZ_LVGL_UI_SCREENS_H\r\n\r\n//${eez-studio LVGL_INCLUDE}\r\n\r\n#ifdef __cplusplus\r\nextern \"C\" {\r\n#endif\r\n\r\n//${eez-studio LVGL_SCREENS_DECL}\r\n//${eez-studio LVGL_SCREENS_DECL_EXT}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_SCREENS_H*/"),
        ("screens.c", '#include <string.h>\n\n#include "screens.h"\n#include "images.h"\n#include "fonts.h"\n#include "actions.h"\n#include "vars.h"\n#include "styles.h"\n#include "ui.h"\n\n//${eez-studio LVGL_SCREENS_DEF}\n//${eez-studio LVGL_SCREENS_DEF_EXT}'),
        ("actions.h", '#ifndef EEZ_LVGL_UI_EVENTS_H\r\n#define EEZ_LVGL_UI_EVENTS_H\r\n\r\n//${eez-studio LVGL_INCLUDE}\r\n\r\n#ifdef __cplusplus\r\nextern "C" {\r\n#endif\r\n\r\n//${eez-studio LVGL_ACTIONS_DECL}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_EVENTS_H*/'),
        ("vars.h", '#ifndef EEZ_LVGL_UI_VARS_H\r\n#define EEZ_LVGL_UI_VARS_H\r\n\r\n#include <stdint.h>\r\n#include <stdbool.h>\r\n\r\n#ifdef __cplusplus\r\nextern "C" {\r\n#endif\r\n\r\n//${eez-studio FLOW_ENUMS}\r\n//${eez-studio FLOW_GLOBAL_VARIABLES_ENUM}\r\n//${eez-studio LVGL_VARS_DECL}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_VARS_H*/'),
        ("structs.h", '#ifndef EEZ_LVGL_UI_STRUCTS_H\n#define EEZ_LVGL_UI_STRUCTS_H\n\n//${eez-studio EEZ_FOR_LVGL_CHECK}\n\n#if defined(EEZ_FOR_LVGL)\n\n#include <eez/flow/flow.h>\n#include <stdint.h>\n#include <stdbool.h>\n\n#include "vars.h"\n\nusing namespace eez;\n\n//${eez-studio FLOW_STRUCTS}\n\n//${eez-studio FLOW_STRUCT_VALUES}\n\n#endif\n\n#endif /*EEZ_LVGL_UI_STRUCTS_H*/\n'),
        ("images.h", '#ifndef EEZ_LVGL_UI_IMAGES_H\r\n#define EEZ_LVGL_UI_IMAGES_H\r\n\r\n//${eez-studio LVGL_INCLUDE}\r\n\r\n#ifdef __cplusplus\r\nextern "C" {\r\n#endif\r\n\r\n//${eez-studio LVGL_IMAGES_DECL}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_IMAGES_H*/'),
        ("images.c", '#include "images.h"\n\n//${eez-studio LVGL_IMAGES_DEF}'),
        ("fonts.h", '#ifndef EEZ_LVGL_UI_FONTS_H\r\n#define EEZ_LVGL_UI_FONTS_H\r\n\r\n//${eez-studio LVGL_INCLUDE}\r\n\r\n#ifdef __cplusplus\r\nextern "C" {\r\n#endif\r\n\r\n//${eez-studio LVGL_FONTS_DECL}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_FONTS_H*/'),
        ("styles.h", '#ifndef EEZ_LVGL_UI_STYLES_H\r\n#define EEZ_LVGL_UI_STYLES_H\r\n\r\n//${eez-studio LVGL_INCLUDE}\r\n\r\n#ifdef __cplusplus\r\nextern "C" {\r\n#endif\r\n\r\n//${eez-studio LVGL_STYLES_DECL}\r\n\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif /*EEZ_LVGL_UI_STYLES_H*/'),
        ("styles.c", '#include "styles.h"\n#include "images.h"\n#include "fonts.h"\n\n//${eez-studio LVGL_STYLES_DEF}\n'),
        ("ui.h", '#ifndef EEZ_LVGL_UI_GUI_H\n#define EEZ_LVGL_UI_GUI_H\n\n//${eez-studio LVGL_INCLUDE}\n\n//${eez-studio EEZ_FOR_LVGL_CHECK}\n\n#if defined(EEZ_FOR_LVGL)\n#include <eez/flow/lvgl_api.h>\n#endif\n\n#if !defined(EEZ_FOR_LVGL)\n#include "screens.h"\n#endif\n\n#ifdef __cplusplus\nextern "C" {\n#endif\n\n//${eez-studio GUI_ASSETS_DECL}\n\nvoid ui_init();\nvoid ui_tick();\n\n#if !defined(EEZ_FOR_LVGL)\nvoid loadScreen(enum ScreensEnum screenId);\n#endif\n\n#ifdef __cplusplus\r\n}\r\n#endif\r\n\r\n#endif // EEZ_LVGL_UI_GUI_H'),
        ("ui.c", '#if defined(EEZ_FOR_LVGL)\n#include <eez/core/vars.h>\n#endif\n\n#include "ui.h"\n#include "screens.h"\n#include "images.h"\n#include "actions.h"\n#include "vars.h"\n\n//${eez-studio GUI_ASSETS_DEF}\n\n//${eez-studio LVGL_NATIVE_VARS_TABLE_DEF}\n\n//${eez-studio LVGL_ACTIONS_ARRAY_DEF}\n\n#if defined(EEZ_FOR_LVGL)\n\nvoid ui_init() {\n    eez_flow_init(assets, sizeof(assets), (lv_obj_t **)&objects, sizeof(objects), images, sizeof(images), actions);\n}\n\nvoid ui_tick() {\n    eez_flow_tick();\n    tick_screen(g_currentScreen);\n}\n\n#else\n\n#include <string.h>\n\nstatic int16_t currentScreen = -1;\n\nstatic lv_obj_t *getLvglObjectFromIndex(int32_t index) {\n    if (index == -1) {\n        return 0;\n    }\n    return ((lv_obj_t **)&objects)[index];\n}\n\nvoid loadScreen(enum ScreensEnum screenId) {\n    currentScreen = screenId - 1;\n    lv_obj_t *screen = getLvglObjectFromIndex(currentScreen);\n    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);\n}\n\nvoid ui_init() {\n    create_screens();\n    loadScreen(SCREEN_ID_MAIN);\n}\n\nvoid ui_tick() {\n    tick_screen(currentScreen);\n}\n\n#endif\n'),
    ]

    return {
        "themesVersion": 1,
        "objID": uid(),
        "settings": {
            "objID": uid(),
            "general": {
                "objID": uid(),
                "projectVersion": "v3",
                "projectType": "lvgl",
                "lvglVersion": "9.0",
                "extensions": [],
                "imports": [],
                "flowSupport": False,
                "displayWidth": 1280,
                "displayHeight": 720,
                "displayBorderRadius": 0,
                "darkTheme": True,
                "colorFormat": "BGR",
                "resourceFiles": [],
                "hiddenWidgetLines": "dimmed",
                "dimmedLinesOpacity": "20",
            },
            "build": {
                "objID": uid(),
                "configurations": [{"objID": uid(), "name": "Default"}],
                "files": [
                    {"objID": uid(), "fileName": fn, "template": tpl}
                    for fn, tpl in build_files
                ],
                "destinationFolder": "ui_eez\\export",
                "separateFolderForImagesAndFonts": False,
                "lvglInclude": "lvgl/lvgl.h",
                "generateSourceCodeForEezFramework": False,
                "compressFlowDefinition": False,
                "executionQueueSize": 1000,
                "expressionEvaluatorStackSize": 20,
            },
        },
        "variables": {
            "objID": uid(),
            "globalVariables": build_variables(),
            "structures": [],
            "enums": [],
        },
        "actions": build_actions(),
        "userPages": [
            {
                "objID": uid(),
                "components": [screen],
                "connectionLines": [],
                "localVariables": [],
                "userProperties": [],
                "name": "Main",
                "left": 0,
                "top": 0,
                "width": 1280,
                "height": 720,
            }
        ],
        "userWidgets": [],
        "lvglStyles": {
            "objID": uid(),
            "styles": build_styles(),
            "defaultStyles": {},
        },
        "lvglGroups": {"objID": uid(), "groups": []},
        "fonts": [],
        "bitmaps": [],
        "colors": [],
        "themes": [{"objID": uid(), "name": "Default", "colors": []}],
    }

if __name__ == "__main__":
    project = build_project()
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(project, f, indent=2)
    print("Wrote", OUT)
    print("variables", len(project["variables"]["globalVariables"]))
    print("widgets on screen", len(project["userPages"][0]["components"][0]["children"]))
