"""Writes a compile_commands.json for the game's C sources without configuring CMake (no SDL, vcpkg or libultraship
build needed), with the include directories, definitions and options of the stadium_game target in CMakeLists.txt.
For the CI check (.github/workflows/checks.yml): abi_calls.py collect reads it like a real build's database.

usage: ci_compdb.py <out.json>
Keep the flags in step with target_include_directories / target_compile_definitions / target_compile_options of
stadium_game in CMakeLists.txt.
"""
import glob, json, os, shlex, sys

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INCLUDES = ["src/port", "game", "src/port/sdk", "src/port/sdk/PR", "game/include", "game/src", "game/src/libnaudio",
            "game/include/audio", "game/include/libleo", "libultraship/include", "libultraship/include/libultraship",
            "game/lib/ultralib/include", "game/lib/ultralib"]
DEFINES = ["LANGUAGE_C", "_LANGUAGE_C", "F3DEX_GBI_2", "NDEBUG", "_FINALROM", "N_MICRO", "NON_MATCHING", "AVOID_UB",
           "BUILD_VERSION=VERSION_I_P", "VERSION_JP=1", "PORT_STATIC_FRAGMENTS"]
OPTIONS = ["-std=gnu11", "-fno-builtin", "-funsigned-char", "-include", os.path.join(root, "src/port/sdk/port_compat.h")]

files = sorted(f for f in glob.glob(os.path.join(root, "game/src/**/*.c"), recursive=True) if "/game/src/libultra/" not in f)
flags = ["-D" + d for d in DEFINES] + ["-I" + os.path.join(root, i) for i in INCLUDES] + OPTIONS
db = [dict(directory=root, file=f, command=shlex.join(["clang"] + flags + ["-c", f])) for f in files]
json.dump(db, open(sys.argv[1], "w"), indent=1)
print("%d sources" % len(db))
