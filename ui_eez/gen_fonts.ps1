# gen_fonts.ps1 — ceske fonty pro Tab5 (lv_font_conv, bez Dockeru)
# Pouziti: .\ui_eez\gen_fonts.ps1
# Dulezite: LVGL 9.2 plain fonty podporuji bpp 1/2/4 (NE 8!) — jinak rozpadle pixely.
# Pozn.: vystup je src/*.cpp — .cee v generated_fonts Arduino NEkompiluje (Tab5 RISC-V bug s .c)$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root

$ttf = Join-Path $PSScriptRoot "Fonts\Roboto-Regular.ttf"
if (-not (Test-Path $ttf)) {
    python -c @"
import json, base64, pathlib
d = json.load(open('ui_eez/LG_Therma_HMI.eez-project', encoding='utf-8'))
p = pathlib.Path('ui_eez/Fonts')
p.mkdir(exist_ok=True)
(p / 'Roboto-Regular.ttf').write_bytes(base64.b64decode(d['fonts'][0]['embeddedFontFile']))
print('extracted TTF')
"@
}

$out = Join-Path $PSScriptRoot "generated_fonts"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$range = "0x20-0x7F,0xA0-0x17F"

foreach ($size in @(16, 24)) {
    $name = "ui_font_font_cs_$size"
    $tmp = Join-Path $out "$name.cee"
    npx --yes lv_font_conv `
        --font $ttf `
        -r $range `
        --size $size `
        --bpp 4 `
        --no-compress `
        --format lvgl `
        --lv-include "lg_lvgl.h" `
        --lv-font-name $name `
        -o $tmp
}

$dest = Join-Path $Root "src"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path $out "ui_font_font_cs_16.cee") (Join-Path $dest "ui_eez_font_cs_16.cpp") -Force
Copy-Item (Join-Path $out "ui_font_font_cs_24.cee") (Join-Path $dest "ui_eez_font_cs_24.cpp") -Force
foreach ($f in @("ui_eez_font_cs_16.cpp","ui_eez_font_cs_24.cpp")) {
    $p = Join-Path $dest $f
    $body = Get-Content $p -Raw
    $body = $body -replace '#include "lvgl.h"', '#include "lg_lvgl.h"'
    $body = $body -replace '(?m)^const lv_font_t (ui_font_font_cs_\d+) = \{', 'extern const lv_font_t $1 = {'
    $body = $body -replace '\.bitmap_format = 1,', '.bitmap_format = 0,'
    if ($body -notmatch 'extern "C"') {
        $wrapped = "#ifdef __cplusplus`nextern `"C`" {`n#endif`n`n" + $body + "`n#ifdef __cplusplus`n}`n#endif`n"
        Set-Content $p $wrapped -NoNewline
    }
}
Write-Host "OK: src/ui_eez_font_cs_16.cpp + src/ui_eez_font_cs_24.cpp"
