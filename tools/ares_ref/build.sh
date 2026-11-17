#!/bin/bash
# Builds the ares reference driver: tools/ares_ref/build.sh <work dir>
# Clones ares into <work dir>/ares (pinned commit), adds the ares_ref target (this directory's ares_ref.cpp) to its
# CMake project and builds the N64 and Game Boy cores (the Game Boy one for the Transfer Pak / GB Player) with
# paraLLEl-RDP into <work dir>/build. paraLLEl-RDP needs Vulkan: on macOS MoltenVK is loaded at run time from
# /Applications/ares.app (brew install --cask ares-emulator); on a headless Linux box Mesa's software driver works
# (apt install mesa-vulkan-drivers libvulkan-dev: lavapipe). ARES_REF_CLONE_FROM=<ares checkout> clones
# from a local tree instead of GitHub. Safe to rerun. Then run scripts with ref.sh:
#   ARES_REF_BUILD=<work dir>/build ARES_REF_SRC=<work dir>/ares tools/ares_ref/ref.sh <script> [state]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="${1:?usage: tools/ares_ref/build.sh <work dir>}"
ARES_COMMIT=9408cb43d4948fc3ea6e152a307a34348df3fe04
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
if [ ! -d "$WORK/ares/.git" ]; then
    git clone ${ARES_REF_CLONE_FROM:+--shared} "${ARES_REF_CLONE_FROM:-https://github.com/ares-emulator/ares.git}" "$WORK/ares"
fi
git -C "$WORK/ares" fetch --depth 1 origin "$ARES_COMMIT" 2>/dev/null || true
git -C "$WORK/ares" checkout -q "$ARES_COMMIT"
# add the driver target before the top-level CMakeLists' final return()
if ! grep -q "ares_ref" "$WORK/ares/CMakeLists.txt"; then
    python3 -I - "$WORK/ares/CMakeLists.txt" "$HERE/ares_ref.cpp" <<'EOF'
import sys
path, src = sys.argv[1], sys.argv[2]
s = open(path).read()
block = f'''# pokestadium-port reference-frame driver (tools/ares_ref)
add_executable(ares_ref {src})
target_include_directories(ares_ref PRIVATE ${{CMAKE_SOURCE_DIR}} ${{CMAKE_SOURCE_DIR}}/ares)
target_link_libraries(ares_ref PRIVATE ares::ares ares::nall)
if(ARES_ENABLE_CHD)
  target_link_libraries(ares_ref PRIVATE chdr-static)
endif()

'''
i = s.rindex('return()')
open(path, 'w').write(s[:i] + block + s[i:])
EOF
fi
cmake -S "$WORK/ares" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DARES_CORES=n64;gb" \
    -DARES_BUILD_OPTIONAL_TARGETS=OFF -DARES_PRECOMPILE_HEADERS=OFF -DARES_UNITY_CORES=OFF > "$WORK/configure.log"
nice -n 15 ninja -C "$WORK/build" ${ARES_REF_JOBS:+-j $ARES_REF_JOBS} ares_ref
echo "built $WORK/build/ares_ref"
