# preflight.ps1 — pred Compile/Upload (Tab5)
# 1) vypne ARM asm v LVGL  2) sync EEZ export  3) kontrola ze ve sketchi neni .c
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root

& (Join-Path $PSScriptRoot "patch_lvgl_tab5.ps1")
python (Join-Path $PSScriptRoot "sync_export_to_sketch.py")

$bad = Get-ChildItem -Path $Root -Recurse -Filter "*.c" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\\.git\\' }
if ($bad) {
    Write-Host "CHYBA: .c soubory Arduino na Tab5 rozbiji — premenuj na .cee:" -ForegroundColor Red
    $bad | ForEach-Object { Write-Host "  $($_.FullName)" }
    exit 1
}
Write-Host "OK: preflight hotov, muzes Verify/Upload"
