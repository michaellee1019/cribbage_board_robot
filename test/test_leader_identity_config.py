"""Compile production authority checks in both modes and reject bad configs."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = shlex.split(os.environ.get("CXX", "c++"))
common = compiler + ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                     "-I" + str(ROOT / "lib/scorebot/src")]
node = "-DSCOREBOT_LEADER_NODE_ID=0x10203040u"
mac = "-DSCOREBOT_LEADER_BLUETOOTH_MAC=0x123456789abcULL"  # Synthetic, never a real Pi identity.
with tempfile.TemporaryDirectory() as temporary:
    for flags in ([], [node, mac]):
        binary = str(Path(temporary) / "authority")
        subprocess.run(common + flags + [str(ROOT / "test/native/test_configured_authority.cpp"),
                        str(ROOT / "src/BoardRole.cpp"), "-o", binary], check=True)
        subprocess.run([binary], check=True)
    for flags in ([node], [mac], [mac, "-DSCOREBOT_LEADER_NODE_ID=0"],
                  [mac, "-DSCOREBOT_LEADER_NODE_ID=0x100000000ULL"],
                  [node, "-DSCOREBOT_LEADER_BLUETOOTH_MAC=0"],
                  [node, "-DSCOREBOT_LEADER_BLUETOOTH_MAC=0x1000000000000ULL"],
                  [mac, "-DSCOREBOT_LEADER_NODE_ID=2934574912u"],
                  [mac, "-DSCOREBOT_LEADER_NODE_ID=2934577084u"],
                  [node, "-DSCOREBOT_LEADER_BLUETOOTH_MAC=0x983daeea0f41ULL"],
                  [node, "-DSCOREBOT_LEADER_BLUETOOTH_MAC=0x983daeea17bdULL"]):
        result = subprocess.run(common + flags + ["-fsyntax-only", str(ROOT / "src/BoardRole.cpp")],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert result.returncode != 0, flags
print("Invalid authority configuration checks passed")
