# patch_lvgl_tab5.ps1 — Tab5 (RISC-V) nema ARM Helium/NEON; Arduino jinak pada na lv_blend_*.S
# Spustit po kazde instalaci/upgrade LVGL 9.2.x: .\ui_eez\patch_lvgl_tab5.ps1
$ErrorActionPreference = "Stop"
$lvRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "..\libraries\lvgl"
$lvRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\libraries\lvgl")).Path

$asmFiles = @(
    "src\draw\sw\blend\helium\lv_blend_helium.S",
    "src\draw\sw\blend\neon\lv_blend_neon.S"
)

foreach ($rel in $asmFiles) {
    $src = Join-Path $lvRoot $rel
    $off = "$src.off"
    if (Test-Path $src) {
        if (Test-Path $off) { Remove-Item $off -Force }
        Rename-Item $src $off
        Write-Host "disabled $rel"
    } elseif (Test-Path $off) {
        Write-Host "already disabled $rel"
    } else {
        Write-Warning "missing $rel"
    }
}

Write-Host "OK: LVGL ARM asm disabled for Tab5"
