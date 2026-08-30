# PlatformIO: IDE často pošle prázdný --upload-port a přepíše IP z platformio.ini.
# Tento skript pro env m5stack-tab5-ota vždy nastaví UPLOAD_PORT z ini (nebo fallback).
Import("env")  # type: ignore  # noqa: F821

DEFAULT_IP = "192.168.50.249"

ini_port = ""
try:
    ini_port = (env.GetProjectOption("upload_port") or "").strip()  # type: ignore
except Exception:
    ini_port = ""

current = ""
try:
    current = str(env.get("UPLOAD_PORT") or "").strip()  # type: ignore
except Exception:
    current = ""

# Prázdný / jen whitespace / žádný → IP z ini nebo default
use = ini_port if ini_port else DEFAULT_IP
if (not current) or (current.lower() in ("none", "null", "-")):
    env.Replace(UPLOAD_PORT=use)  # type: ignore
    print(f"[OTA] UPLOAD_PORT forced → {use}")
else:
    # Serial COMx by u espota neměl být — když to nevypadá jako IP/hostname, přepiš
    is_serial = current.upper().startswith("COM") or current.startswith("/dev/")
    if is_serial:
        env.Replace(UPLOAD_PORT=use)  # type: ignore
        print(f"[OTA] UPLOAD_PORT was serial '{current}', forced → {use}")
    else:
        print(f"[OTA] UPLOAD_PORT = {current}")
