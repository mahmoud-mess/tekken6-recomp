# Linux port preparation

The local `tekken6-native-online-source-main/` directory is a source archive
(no top-level `.git` metadata), ignored by this repository. The existing working
native Linux port is this repository, on branch `linux-port`.
This online source snapshot has not yet been ported or run on Linux.

## Prepared

- Isolated ReXGlue checkout at this fork's required revision:
  `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`.
- All 23 nested dependency checkouts populated at their pinned revisions using
  local Git object stores, without modifying the parent port's SDK.
- Applied this fork's SDK and FFmpeg patches successfully.
- Native CMake configuration succeeded with Clang 19 and Vulkan.

The SDK/dependency checkouts use Git alternates pointing at the parent port's
object stores. They are suitable for local development; retain those object
stores, or make the clones independent before moving this source directory.

## First confirmed compilation blocker

Building target `ggpo` fails on `winsock2.h`. CMake unconditionally defines
`_WINDOWS` and links `ws2_32` / `winmm`. GGPO's `types.h` has a Linux include
branch, but the referenced `platform_linux.h` is not supplied in this snapshot.
UDP and polling code also directly use Winsock and Windows event APIs.

## Next implementation milestones

1. Add real POSIX platform, socket, and polling support to this GGPO fork.
   Preserve its project-specific protocol and rollback changes. Test local
   UDP traffic, timeout behavior, and error handling before integrating it.
2. Port the game's shared platform helpers: timing, safe file/string helpers,
   symbol lookup and input. Windows DLL-name lookup needs ELF equivalents.
3. Port snapshot memory queries, worker synchronization, deterministic audio,
   thread handling, and diagnostics. These need faithful semantics, not no-op
   replacements. The online fork has runtime exports absent in the parent SDK.
4. Generate a separate guest source tree using this fork's configuration and
   apply its pagemap/rawstore/sndcall/faststore patch tools. Keep the parent's
   generated code and SDK outputs separate.
5. Build and test offline Practice, input record/replay, and snapshot restore.
   Retain the online fork's floating-point settings: do not enable the parent's
   FMA defaults or apply its global frame cap to rollback resimulation blindly.
6. Port the Qt launcher and test LAN netplay, followed by cross-platform
   determinism. Public sources omit official lobby configuration/certificate
   assets and the lobby server implementation.

## Configuration used

```sh
cd tekken6-native-online-source-main
cmake -S . -B build/linux-amd64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_CXX_FLAGS=-mssse3 \
  -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
cmake --build build/linux-amd64 --target ggpo --parallel 2
```

Configure log: `/tmp/tekken6-online-linux-configure.log`.
Initial compile log: `/tmp/tekken6-online-linux-ggpo.log`.
