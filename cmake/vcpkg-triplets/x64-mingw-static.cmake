# Overlay of vcpkg's community x64-mingw-static triplet that builds the dependencies with llvm-mingw's clang and
# libc++, like the game itself. The stock triplet takes whichever *-w64-mingw32-gcc comes first on PATH: a GCC MinGW
# (Strawberry Perl, MSYS2, w64devkit, ...) then builds spdlog/fmt against libstdc++ and the game fails to link
# (undefined std::__cxx11 / _Rb_tree / spdlog symbols). Use with: vcpkg install ... --overlay-triplets=cmake/vcpkg-triplets
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED PATH)
set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/mingw-clang.cmake")
