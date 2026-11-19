# Building plethron

[Back to the README](../README.md)

## Building

You need your own dump of **Pocket Monsters Stadium** (Japan, N64), as a 16 MiB `.z64` with SHA-1
`8bc8d2ff7df25b26bd0c353d2adfe83e4e3a7a87`. The ROM is not part of the build: it is read when the game starts.
The port refuses other revisions and patched or translated ROMs.

All platforms need Git, CMake 3.20 or newer, Ninja, Python 3 and Clang. GCC and MSVC are not supported.

Clone the repository with its submodule (libultraship):

```bash
git clone --recursive https://github.com/team-hippodrome/plethron.git
cd plethron
git submodule update --init --recursive   # safe to rerun; same as tools/setup.sh
```

### Windows (PowerShell)

Install [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) (the `ucrt-x86_64` zip) and put its `bin`
folder on `PATH`. Also install [vcpkg](https://github.com/microsoft/vcpkg); `$env:VCPKG_ROOT` should point to it.

```powershell
$env:PATH = "C:\llvm-mingw\bin;$env:PATH"   # where you unpacked llvm-mingw
& "$env:VCPKG_ROOT\vcpkg" install "sdl3[core,vulkan]" glew libzip nlohmann-json tinyxml2 spdlog zlib bzip2 glslang `
    --triplet x64-mingw-static --host-triplet x64-mingw-static
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static `
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$PWD/cmake/toolchain-mingw-clang.cmake"
cmake --build build-win
```

The game is `build-win\plethron.exe`.

- **Keep `RelWithDebInfo`.** Without a build type, vcpkg links its debug SDL3, and that version stops at startup
  with an assertion dialog.
- **Windows on ARM:** set `$env:MINGW_TRIPLE = "aarch64-w64-mingw32"` and use the `arm64-mingw-static` triplet.

### Linux

On a distribution that packages SDL3 (Ubuntu 25.04 or newer, Debian 13, Fedora, Arch). Package names here are
Ubuntu's:

```bash
sudo apt install clang lld cmake ninja-build python3 libsdl3-dev libglew-dev libgl-dev libzip-dev libbz2-dev \
  zlib1g-dev nlohmann-json3-dev libtinyxml2-dev libspdlog-dev glslang-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

The game is `build/plethron`.

- If configure fails on glslang, add `-DLUS_ENABLE_VULKAN=OFF`. You then get the OpenGL renderer only.
- The release AppImage is built differently, so that it runs on other distributions too: SDL3 from source, the
  other libraries from vcpkg, all linked statically. See the `linux` job in `.github/workflows/build.yml`.

### Running

Start the executable without arguments. The launcher asks for the Stadium ROM, then optionally a save and a Game
Boy ROM and save for each Transfer Pak. It remembers your choices for next time (hold Shift while starting to see
it again).

You can also pass the ROM directly, which skips the launcher:

```bash
build/plethron "path/to/Pocket Monsters Stadium (J).z64"
```

The N64 save is `saves/stadium.sra` in the app folder next to the executable (32 KiB, N64 byte order), so it works in emulators too. Western nicknames are
kept next to it in `stadium.sra.western`.
