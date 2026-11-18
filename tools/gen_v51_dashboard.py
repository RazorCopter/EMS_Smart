# SPDX-License-Identifier: MIT
from pathlib import Path
import gzip

Import("env")  # noqa: F821
root = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
directory = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
directory.mkdir(parents=True, exist_ok=True)
binary = gzip.compress((root / "src/v51/dashboard.html").read_bytes(), mtime=0)
header = "inline constexpr unsigned char kDashboard[] = {\n"
header += "\n".join("    " + ", ".join(f"0x{byte:02x}" for byte in binary[i:i + 16]) + ","
                    for i in range(0, len(binary), 16))
header += "\n};\n"
file = directory / "dashboard_gzip.h"
if not file.exists() or file.read_text() != header:
    file.write_text(header)
env.Append(CPPPATH=[str(directory)])  # noqa: F821
