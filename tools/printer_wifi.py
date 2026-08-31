"""Generate a build-local C++ header from ignored printer Wi-Fi settings.

The header is written inside the selected PlatformIO environment's build
directory. Credentials therefore never appear in tracked source or compiler
flags, but are still compiled into firmware for the leaderboard to use on
demand.
"""

from __future__ import annotations

import json
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
config_path = project_dir / ".printer-wifi.json"

try:
    document = json.loads(config_path.read_text(encoding="utf-8"))
except FileNotFoundError as error:
    raise RuntimeError(
        "Missing .printer-wifi.json; copy .printer-wifi.example.json and set "
        "the string fields 'ssid' and 'password'."
    ) from error
except json.JSONDecodeError as error:
    raise RuntimeError(f"Invalid JSON in {config_path.name}: {error}") from error

if not isinstance(document, dict):
    raise RuntimeError(".printer-wifi.json must contain a JSON object.")
if set(document) != {"ssid", "password"}:
    raise RuntimeError(
        ".printer-wifi.json must contain exactly the fields 'ssid' and 'password'."
    )

ssid = document["ssid"]
password = document["password"]
if not isinstance(ssid, str) or not ssid:
    raise RuntimeError(".printer-wifi.json field 'ssid' must be a non-empty string.")
if not isinstance(password, str):
    raise RuntimeError(".printer-wifi.json field 'password' must be a string.")

generated_dir = Path(env.subst("$BUILD_DIR")) / "generated"  # type: ignore[name-defined]
generated_dir.mkdir(parents=True, exist_ok=True)
header_path = generated_dir / "PrinterWifiCredentials.hpp"
header = f"""#pragma once

namespace scorebot {{
inline constexpr char kPrinterWifiSsid[] = {json.dumps(ssid)};
inline constexpr char kPrinterWifiPassword[] = {json.dumps(password)};
}}  // namespace scorebot
"""
if not header_path.exists() or header_path.read_text(encoding="utf-8") != header:
    header_path.write_text(header, encoding="utf-8")

env.Append(CPPPATH=[str(generated_dir)])  # type: ignore[name-defined]
