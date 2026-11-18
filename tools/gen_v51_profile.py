# SPDX-License-Identifier: MIT
"""Generate the dedicated FC04 profile from the canonical TOML."""
from pathlib import Path
import json
import math
import tomllib

Import("env")  # noqa: F821  SCons exports Import

root = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
profile = tomllib.loads((root / "profiles/afore/af5k_sl.toml").read_text(encoding="utf-8"))
blocks = profile["block"]
if profile.get("write") or any(block["space"] != "input" for block in blocks):
    raise ValueError("The SHADOW profile must contain only input register blocks")
if not all(1 <= block["count"] <= 125 for block in blocks):
    raise ValueError("Invalid FC04 quantity")
types = {"u16": "U16", "s16": "S16", "u32": "U32", "s32": "S32"}
fields = []
for register in profile["register"]:
    if register["space"] != "input":
        raise ValueError("The SHADOW profile must contain only input registers")
    address, dtype = register["address"], register["type"]
    words = 2 if dtype.endswith("32") else 1
    if not any(b["start"] <= address and address + words <= b["start"] + b["count"] for b in blocks):
        continue  # This row is outside the qualified polling blocks.
    scale = register.get("scale", 1)
    if not math.isfinite(scale):
        raise ValueError("Invalid scaling factor")
    if register.get("word_order", "msw_first") != "msw_first":
        raise ValueError("Only MSW-first words are supported in the prototype")
    ident = json.dumps(register["measurement"])
    fields.append(f"    {{{ident}, {address}, WordType::{types[dtype]}, {float(scale)}}},")
if not any(r["address"] == 2002 and r["measurement"] == "battery.soc" for r in profile["register"]):
    raise ValueError("Missing SOC probe")
rows = [f'    {{{b["start"]}, {b["count"]}}},' for b in blocks]
directory = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
directory.mkdir(parents=True, exist_ok=True)
header = (
    "// Generated from the board's single input-register profile.\n"
    "#pragma once\n"
    f"inline constexpr uint32_t kBaud = {profile['serial']['baud']};\n"
    "inline constexpr Block kBlocks[] = {\n"
    + "\n".join(rows)
    + "\n};\ninline constexpr Field kFields[] = {\n"
    + "\n".join(fields)
    + "\n};\n"
)
destination = directory / "v51_profile_generated.h"
if not destination.exists() or destination.read_text(encoding="utf-8") != header:
    destination.write_text(header, encoding="utf-8")
env.Append(CPPPATH=[str(directory)])  # noqa: F821
