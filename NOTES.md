# pydsi implementation notes

melonDS is pinned to `906e9ebb27da8c6a715cd7abab4abfe8a8d29427` (master, 2026-08-24). All file:line
references below are into `externals/melonDS/src` at that commit unless stated otherwise.

## Phase 1: discovery summary

### Version choice

The latest tag is `1.1` (2025-11-18). Master is 75 commits ahead of it. I pinned master because several of those commits matter here:

- #2523 / `87e1145c` lets DSi mode direct-boot without BIOS, firmware or NAND dumps. It uses FreeBIOS-TWL and generated DSi firmware.
- `ce3dc375` and `60f2ced9` give correct `SCFG_EXT` values on DSi direct boot. Our DSi-mode test reads this register.
- `d7a3c917` contains SD/MMC controller fixes.
- `f5cc5439` makes "full BIOS boot" automatic.
- #2732 / `906e9ebb` fixes `MakeEmbed.cmake` lookup when melonDS is built as a CMake subproject.

### Construction

- `melonDS::NDS(NDSArgs&&, void* userdata)` is at NDS.h:561.
- `melonDS::DSi(DSiArgs&&, void* userdata)` is at DSi.h. `DSiArgs` derives from `NDSArgs` (Args.h).
- `NDSArgs` holds the ARM9/ARM7 BIOS images, a `Firmware` object, JIT args, the audio bit depth, interpolation, the output sample rate, GDB args and a `Renderer`.
  - The BIOS images default to FreeBIOS. The firmware defaults to generated firmware.
  - `Renderer` is `nullptr`, which selects the software renderer (GPU.cpp:334-339).
- `DSiArgs` adds the ARM9i/ARM7i BIOS (default `BrokenBIOS`, so always pass FreeBIOS-TWL or a dump), `std::optional<NANDImage>`, `std::optional<FATStorage>` for the SD card, and `DSPHLE`.
- The ROM and its save are not part of the args.
  - The save is passed to `NDSCart::ParseROM(std::unique_ptr<u8[]>&&, u32 len, void* userdata, std::optional<NDSCartArgs>&&)` through `NDSCartArgs{SDCard, SRAM, SRAMLength}`.
  - The resulting cart is then inserted with `NDS::SetNDSCart`.
- Boot sequence, as used by the Qt `EmuInstance::updateConsole/loadROM/reset` (EmuInstance.cpp:1224-1455, 1864-1960):
  1. `new NDS/DSi(args, userdata)`
  2. `Reset()`
  3. `SetNDSCart(cart)`
  4. `SetupDirectBoot(romname)`, if direct boot is set or `NeedsDirectBoot()` is true
  5. `Start()`
  6. `RunFrame()`, once per frame

### Platform.h

The core defines no `Platform::` function itself, so everything must be supplied by the frontend. Grouped:

- **File I/O:** `OpenFile`, `OpenLocalFile`, `GetLocalFilePath`, `FileExists`, `LocalFileExists`, `CheckFileWritable`, `CheckLocalFileWritable`, `CloseFile`, `IsEndOfFile`, `FileReadLine`, `FilePosition`, `FileSeek`, `FileRewind`, `FileRead`, `FileFlush`, `FileWrite`, `FileWriteFormatted`, `FileLength`.
- **Threading/sync:** `Thread_Create/Free/Wait`, `Semaphore_Create/Free/Reset/Wait/TryWait/Post`, `Mutex_Create/Free/Lock/Unlock/TryLock`, `Sleep`, `GetMSCount`, `GetUSCount`.
- **Network:** `MP_*` (local multiplayer, 9 functions), `Net_SendPacket`, `Net_RecvPacket`.
- **Camera/mic:** `Camera_Start/Stop/CaptureFrame`, `Mic_Start/Stop/ReadInput`.
- **Misc:**
  - `SignalStop`, `Log`
  - `WriteNDSSave`, `WriteGBASave`, `WriteFirmware`, `WriteDateTime`
  - `AAC_Init/DeInit/Configure/DecodeFrame`, used by DSi DSP HLE
  - `Addon_KeyDown/RumbleStart/RumbleStop/MotionQuery`
  - `DynamicLibrary_Load/Unload/LoadFunction`
- None has a default or no-op implementation upstream.
- `PlatformOGL.h` is only needed with the OpenGL renderer, which is disabled here.

### How the Qt frontend boots DSi mode

**Files and load order** (`updateConsole`):

1. DS ARM9 BIOS, then DS ARM7 BIOS. FreeBIOS is used if "external BIOS" is off.
2. Firmware for the console type. A DSi firmware file must be 128 KB. It is generated if external BIOS is off.
3. DSi ARM7i BIOS, then ARM9i BIOS. FreeBIOS-TWL is used if `DSi.ExternalBIOSEnable` is off.
4. The NAND, opened with `ReadWriteExisting`. It is keyed with the ES key Y at `arm7ibios[0x8308]`.
   - The frontend patches TWLCFG user settings and touch calibration in the NAND through `NANDMount::ApplyUserData`.
5. The optional SD card, as a `FATStorage`.

**Validity checks** (`verifySetup`):
- The DSi BIOSes must each be 64 KB.
- The DSi firmware must be 128 KB.
- The NAND is optional when using FreeBIOS or direct boot.

**DSi-enhanced or DSi-exclusive cart** (`DSi::SetupDirectBoot`, DSi.cpp:470-765):
- The DSi-mode decision is `header.UnitCode & 0x02`.
- It maps NWRAM from header MBK settings, writes boot info into RAM, loads ARM9i/ARM7i, and decrypts modcrypt areas.
- It reads TWLCFG and hardware info from the NAND if one is present, and sets `SCFG_EXT`.
- With no NAND, `DSi::NeedsDirectBoot()` is true and direct boot is forced.

**DSiWare on the NAND:**
- Upstream Qt has no direct-boot path. It boots the firmware, i.e. the real DSi menu (`bootToMenu`), and the user picks the title.
- Titles are imported with the core's `DSi_NAND::NANDMount::ImportTitle(app, tmd)`, which lives in `src/`. Qt's `TitleManagerDialog` is only UI.
- melonds-ds (`src/libretro/console/dsi.cpp`) adds a direct-boot trick, credited to BizHawk/CasualPokePlayer:
  1. Set the BPTWL boot flag.
  2. Write a 0x100-byte "TLNC" auto-load record with the title ID at `MainRAM[0x300]`.
  3. Boot the real DSi menu, which then launches the title.
- This needs real DSi BIOS dumps and a real NAND.

### Framebuffer

- `bool GPU::GetFramebuffers(void** top, void** bottom)` (GPU.h:75). With the software renderer it returns `u32*` arrays of 256x192 for the physical top and bottom screens.
- Format is BGRA in memory, i.e. `0xFFRRGGBB` little-endian, with alpha forced to 0xFF (GPU_Soft.cpp:428-444).
- The buffers are double-buffered. `RunFrame` swaps them in `GPU::FinishFrame`, and the front buffer after `RunFrame` returns is the finished frame. No lock is needed when reading on the emulation thread.
- The software renderer has no upscaling. `RendererSettings::ScaleFactor` is ignored by `SoftRenderer` (GPU_Soft.cpp:88-92).
- Threaded 3D is set with `GetRenderer().SetRenderSettings({.Threaded = true})`.
- There is no separate frame-ready signal. `u32 NDS::RunFrame()` returns once a frame is complete, and its return value is the scanline count (263).

### Audio

- `int SPU::ReadOutput(s16* data, int samples)` (SPU.h:255) returns interleaved stereo s16. `samples` is counted in stereo frames.
- `int SPU::GetOutputSize()` gives the frames available.
- Output is resampled by blip-buf to `NDSArgs::OutputSampleRate` (default 48000; pydsi uses 32768 like PyNDS's player).
- The ring buffer holds 2048 frames. Older data is overwritten if not read.
- There is no way to switch the SPU off.

### Input

- `NDS::SetKeyMask(u32)` is active-low. Bits 0-11 are A, B, Select, Start, Right, Left, Up, Down, R, L, X, Y. This is the same order as PyNDS's `KEY_MAP`.
- Touch uses `NDS::TouchScreen(u16 x, u16 y)` and `NDS::ReleaseScreen()`.
- The lid uses `NDS::SetLidClosed(bool)`.

### Savestates

- To save: `Savestate s; nds->DoSavestate(&s)`, then write `s.Buffer()` and `s.Length()`.
- To load: `Savestate s(buf, len, false); nds->DoSavestate(&s)`, then check `s.Error`.
  - A failed load can leave the emulator half-loaded, so Qt backs up the state first.
  - Loading fails on a console-type mismatch or a DSP-HLE mismatch.
- **Not captured:** NAND and SD contents, firmware flash, `KeyInput`, touch coordinates and framebuffers.

### Memory access

- `NDS::ARM9Read8/16/32`, `ARM9Write*`, `ARM7Read*` and `ARM7Write*` are virtual (DSi overrides them).
  - They are bus accesses only. They do not see ITCM/DTCM and have no timing cost.
  - IO reads have side effects, for example the IPC FIFO and cart data registers.
  - Writes invalidate JIT blocks.
- TCM is available as `nds.ARM9.ITCM` and `nds.ARM9.DTCM`, with `ITCMSize`, `DTCMBase` and `DTCMMask` (ARM.h:334-339).
- Main RAM is `nds.MainRAM[addr & MainRAMMask]`: 4 MB on DS, 16 MB on DSi.

### Save write-back

- Carts call `Platform::WriteNDSSave(fullbuffer, len, off, n, cart_userdata)` on SPI release after a write command. `userdata` comes from `ParseROM`'s argument.
- `NDS::GetNDSSave()` and `GetNDSSaveLength()` read the live SRAM at any time.
- Firmware calls `Platform::WriteFirmware(fw, off, len, nds_userdata)`.
- NAND and SD writes go straight to their files through `Platform::FileWrite`.

### CMake

- The core is `add_library(core STATIC …)` in `src/CMakeLists.txt`. Its only subproject is teakra, the DSi DSP.
- Windows adds the system libs `ole32 comctl32 wsock32 ws2_32`, plus `onecore` with the JIT.
- It needs no zlib, libslirp or enet. Those are frontend only.
- Options set here:
  - `BUILD_QT_SDL=OFF`
  - `ENABLE_OGLRENDERER=OFF`
  - `ENABLE_GDBSTUB=OFF`. This is required: `GDBSTUB_ENABLED` is a directory-scoped define that changes `NDS`/`ARM` layout.
  - `ENABLE_JIT=OFF` by default (see below)
  - `ENABLE_LTO*=OFF`
  - `USE_CCACHE=OFF`
  - `USE_VCPKG=OFF`
- **Windows compilers:**
  - The core does not compile with MSVC `cl.exe`. It uses `__attribute__` and `__builtin_*` and needs C++20 designated initialisers. This was tried locally.
  - Upstream builds Windows with MinGW or with clang in the MSVC environment.
  - Under clang-msvc, `FATStorage.cpp` needs a `dirent.h`, which pydsi fetches from tronkko/dirent.

### PyNDS API mapping

PyNDS wraps NooDS, not melonDS.

**Maps cleanly:**
- `Nds(path, save, isGba)` → `Dsi(path, save, isGba)`
- `run_until_frame`
- `get_top_nds_frame` / `get_bot_nds_frame`
- `get_audio_samples`, `get_audio_buffer_number`
- `save_state` / `load_state`
- `save_game`
- `press_key` / `release_key`, with the same bit indices
- `set_touch_input`, `touch_input`, `release_touch_input`, `clear_touch_input`
- `read_ram_*` / `write_ram_*`
- Config: `direct_boot`, `threaded_3d`, `emulate_audio`, `audio_16_bit`, BIOS and firmware paths, `dsi_mode`

**Needs adaptation:**
- `run_task`: NooDS runs one scheduler task. melonDS's smallest public step is a frame, so `run_task` runs a frame.
- `get_frame`: melonDS gives BGRA, so it is converted to RGBA.
- `get_audio_samples(count)` returns exactly `count` frames: the buffered ones, then silence.
- The `tcm` flag on memory reads/writes is implemented by reading the ARM9 TCM arrays.

**Can't be supported:**
- GBA mode, i.e. `is_gba` and `get_gba_frame`. melonDS has no GBA mode, so `isGba=True` raises.
- `high_res_3d` and `screen_filter`. The software renderer has no upscaling, so these are stored but ignored, and frames are always 256x192.
- `rom_in_ram`, `fps_limiter`, `frame_skip`, `threaded_2d`, `screen_ghost`, `saves/states/cheats_folder`, `arm7_hle` and `gba_bios_path` are NooDS-only. They are stored but ignored.

## Where melonDS (or PyNDS) diverged from the brief

1. **PyNDS wraps NooDS, not melonDS.**
   - Its `Core`, `Settings`, `memory.read<T>(arm7, addr, tcm)`, `spu.getSamples` and `writeSaveToPath` belong to a NooDS fork. They have no melonDS counterpart.
   - pydsi keeps PyNDS's names and signatures and reimplements them on melonDS (see the mapping above).
2. **PyNDS fetches nanobind as a git submodule**, not through FetchContent or pip. pydsi uses pip's `nanobind` from `build-system.requires`. That is the usual scikit-build-core setup, and it provides `nanobind_add_stub` for the `.pyi`.
3. **No upscaling in software.** The brief mentions handling upscaled framebuffers and `renderScale`/`gpuScale`, but melonDS's software renderer can't upscale (`ScaleFactor` is only used by the GL renderers). Frames are always 256x192 and there is no scale setting.
4. **No settable `dsiFullBiosBoot`.** `DSi::FullBIOSBoot` is private. `DSi::Reset` derives it from the BIOS checksums: full boot unless the dumps are the known 32 KB-truncated ones (DSi.cpp:139-153). There is nothing to configure.
5. **DSi mode doesn't need dumps for cartridges.** The brief says to error clearly if a required file is missing in DSi mode. Since #2523, melonDS direct-boots DSi titles with FreeBIOS-TWL and generated DSi firmware, so for cartridges and homebrew nothing is required. pydsi errors only where files really are needed:
   - DSi menu or DSiWare: needs DSi BIOS and NAND.
   - A NAND without the DSi ARM7 BIOS: the NAND key comes from that BIOS.
   - DSi BIOS dumps without DSi firmware: the same rule as melonDS's `verifySetup`.
   - Only one of a BIOS pair, wrong sizes, or missing files.
6. **The NAND is written at boot.** Like melonDS's Qt frontend, pydsi rewrites the NAND's touchscreen calibration on every boot, otherwise touch coordinates are wrong. The tests use copies.
7. **DSiWare boot is not upstream melonDS.**
   - Upstream Qt has no direct DSiWare boot; it boots the DSi menu.
   - pydsi uses melonDS DS's TLNC auto-load record and BPTWL boot flag. That relies on the real DSi menu, so it needs real BIOS and NAND.
   - `install_dsiware` uses the core's `NANDMount::ImportTitle`, which lives in `src/`, so the stretch goal was possible.
8. **`run_task` runs a frame.** It is a NooDS concept. melonDS's smallest public step is `RunFrame`.
9. **`get_audio_samples(count)` always returns exactly `count` rows, like PyNDS.**
   - NooDS resamples to exactly `count`. melonDS can only give what it has buffered, so pydsi returns the buffered samples followed by silence.
   - `get_audio_buffer_number()` (alias `audio_buffered()`), called beforehand, says how many rows are real.
10. **Default audio rate is 32768 Hz.** melonDS's `NDSArgs` default is 48000. 32768 is the DS's native output rate and what PyNDS's audio player expects; melonDS resamples to whatever is configured.
11. **Bit depth.** The `SPU` constructor ignores `NDSArgs::BitDepth` (it is overwritten in the body, SPU.cpp:209 vs 216), so pydsi calls `SPU.SetDegrade10Bit` after construction.
12. **No GBA.** `isGba` is kept for API parity. melonDS has no GBA mode, so `True` raises.
13. **JIT is off by default.**
    - The JIT is on by default upstream, but a JIT build registers a process-wide fault handler in `ARMJIT_Memory`'s constructor even when the JIT is disabled at runtime. On POSIX it is a SIGSEGV/SIGBUS handler. On Windows it is a first-priority vectored exception handler.
    - That handler dereferences `NDS::Current` before checking whether the fault is its own (ARMJIT_Memory.cpp:171-220). `NDS::Current` is a `thread_local` that is null on threads that never ran a frame and dangling after the NDS is destroyed.
    - Inside a Python process this can turn unrelated faults into crashes, so the JIT is a build option (`PYDSI_ENABLE_JIT`) plus a runtime flag.
14. **melonDS doesn't build with MSVC `cl.exe`, so pydsi uses llvm-mingw (UCRT) on Windows.** See "MSVC blockers" below for the exact list.
    - **Why llvm-mingw.** Three toolchains were tested and pass the same tests: llvm-mingw UCRT 20260922, clang-cl 23.1.2, and Strawberry's gcc 13.2 UCRT. llvm-mingw was chosen as the simplest:
      - a single portable zip; no Visual Studio, developer prompt, delvewheel or dirent download
      - the `.pyd` imports only `python3x.dll`, `KERNEL32` and the UCRT, which is the same C runtime CPython uses
      - libc++, libunwind and winpthread are linked statically, so no C++ objects cross into CPython
    - **`MS_WIN64` (any MinGW).** python.org's `pyconfig.h` defines `MS_WIN64` only for MSVC. Without it, the compiler sees a 32-bit `Py_ssize_t` and 15-bit `PyLong` digits, and nanobind silently converted every int ≥ 2^15 to 0. pydsi defines `MS_WIN64` for 64-bit MinGW.
    - **CRT caveat.** A MinGW extension is only safe on the UCRT, never `msvcrt.dll`. An msvcrt toolchain, such as the WinLibs gcc 15.1 in `C:\bin\mingw64`, would mix two C runtimes, which breaks as soon as `malloc`/`free` or `FILE*` cross the boundary.
    - **libc++ include gap.** melonDS's `FreeBIOS.cpp` uses `std::copy` without `#include <algorithm>`. libstdc++ and MSVC's STL include it transitively; libc++ (llvm-mingw, and also macOS) doesn't. pydsi force-includes `<algorithm>` for that one file from its CMake instead of patching. Worth a one-line fix upstream.
    - **clang-cl, kept as an alternative:**
      - It needs a `dirent.h` (FATStorage.cpp). pydsi fetches tronkko/dirent 1.26, header only.
      - melonDS adds `-fwrapv` only `if (NOT MSVC)`, so pydsi adds `/clang:-fwrapv` to `core`. Without it, signed-overflow wrapping the emulation relies on would be undefined behaviour.
      - Its wheels need delvewheel to bundle `MSVCP140.dll`.
    - **`NOMINMAX` / `WIN32_LEAN_AND_MEAN`.** melonDS sets these directory-wide, so pydsi's own target sets them too. Otherwise `windows.h`'s `min`/`max` macros break `std::max`.
15. **Upstream's Windows CI uses `clang.exe` targeting MSVC.** pydsi's choice differs only in the ABI target (MinGW instead of MSVC), and only the C ABI crosses into CPython.
16. **File mode mapping.** The old Qt fopen mapping turned `Write|Preserve` without `Read` on an existing file into `"rb"`, which can't write. pydsi maps it to `"r+b"`. Every other mode matches the Qt table (see `platform_impl.cpp`).
17. **No devkitPro test ROM.** The brief suggests a devkitPro example. The tests instead generate a tiny homebrew ROM from Python (`tests/romgen.py`, hand-written ARM code assembled with keystone and checked in as bytes). It needs no toolchain in CI and reports DS vs DSi mode itself through `SCFG_EXT9`. It is not a third-party binary.
18. **Indentation.** The brief asks for 2-space indent and to match PyNDS where code is copied. PyNDS uses 4 spaces in both C++ and Python.
    - New C++ and test code uses 2 spaces with no space after `if`/`for`.
    - Python files ported from PyNDS (`pydsi/*.py`) keep PyNDS's 4-space style.

## MSVC blockers

These were found by building `core` with `cl.exe` 19.31 (VS 2022 17.1), interpreter only, no GL, and by grepping for the same constructs where the compiler stopped early. melonDS's `src/CMakeLists.txt` says it doesn't support MSVC ("if we ever support it…").

| Construct | Where | What MSVC needs |
|---|---|---|
| C++20 designated initialisers in C++17 mode (a GCC/Clang extension) | ARCodeFile.cpp:140,175,328,344,360; ARDatabaseDAT.cpp:235 | `CXX_STANDARD 20` on `core`. This can be set from the parent without a patch |
| `__builtin_popcount` | ARMInterpreter_LoadStore.cpp:403,479,696 | `__popcnt` |
| `__builtin_ctz` | GPU.cpp:554,561,570,1414,1449 | `_BitScanForward` |
| `__builtin_ctzll`, `__builtin_clzll` | NonStupidBitfield.h:121,178,251,261 | `_BitScanForward64` / `_BitScanReverse64` |
| `__builtin_unreachable` | GPU3D.cpp:2331 | `__assume(0)` |
| `__attribute((always_inline))` on `Bswap128` (MSVC takes the non-GCC branch, which still uses the attribute) | DSi_AES.h:38. Its absence is the cause of the ~40 "Bswap128 not found" errors in DSi.cpp, DSi_AES.cpp and DSi_NAND.cpp | `__forceinline` or nothing |
| Variable-length arrays (a GCC extension) | DSi_NWifi.cpp:1071 (`u8 reply[2 + nchan*2 + 2]`); SPU.cpp:1032 (`s16 temp[avail * 2]`) | Source change: fixed bound / `std::vector` |
| GCC inline asm `asm volatile ("" : : : "memory")` (LTO workaround) | GPU2D_Soft.cpp:389 | Source change: `std::atomic_signal_fence` or `_ReadWriteBarrier()` |
| Parse error in generic lambdas using the member `DSi` (same name as the class) | DSi_DSP.cpp:295-307 | Source change, probably `this->DSi.` |
| `-fwrapv` semantics | Whole core (`target_compile_options(core PUBLIC -fwrapv)` for non-MSVC) | **No MSVC equivalent.** MSVC doesn't promise wrapping signed overflow, so a cl.exe build would rely on undefined behaviour the emulator depends on |
| JIT | `ARMJIT_x64/ARMJIT_Linkage.S` (GAS `.intel_syntax`), x64 emitter | MSVC can't assemble it. Interpreter only |

**Verdict.**
- The builtins and the attribute could be covered by a force-included compatibility header, with no source change.
- Four spots need a real patch (two VLAs, one inline asm, one lambda parse issue), plus C++20 for `core`. That is roughly a 10-line patch in `patches/`.
- But it would leave `-fwrapv` unaddressed: correctness would rest on MSVC not exploiting signed-overflow UB.
- That is why pydsi uses clang on Windows. llvm-mingw UCRT is the default and clang-cl the alternative: both understand melonDS's GCC-isms, honour `-fwrapv`, and use CPython's C runtime.
- These counts are only the first errors per file; a full MSVC port may surface more once these are fixed.

## Upstream melonDS bugs found (worked around in pydsi, worth reporting)

1. **Uninitialised read in the `GPU` constructor, causing an intermittent segfault.**
   - `GPU::GPU` calls `SetRenderer()`, which first calls `SyncAllVRAMCaptures()` (GPU.cpp:320). That reads `u16 VRAMCaptureBlockFlags[16]` (GPU.h:809), which has no initialiser and is only zeroed in `Reset()`.
   - If garbage in it has bit 15 set and bit 13 clear, melonDS calls `Rend->SyncVRAMCapture(...)` while `Rend` is still null.
   - It was introduced by `ba317e2e` (OpenGL 2D renderer, 2026-01-31), so the 1.1 tag doesn't have it.
   - **Why only Linux.** The console object is about 32.5 MB, just under glibc's 32 MB mmap-threshold ceiling. After one such block has been freed, the next comes from recycled heap memory, so a second emulator in one process can start on the previous one's leftovers. Windows serves allocations that size from fresh zeroed pages and never crashed.
   - **Workaround.** pydsi constructs `NDS`/`DSi` on zeroed memory (`makeConsole` in `cdsi/dsi.cpp`).
     - The zeroing goes through a volatile function pointer. `NDS`'s public constructor is inline, and GCC's lifetime dead-store elimination deleted a plain `memset` placed before it.
     - `tests/test_instances.py::test_construct_on_dirty_heap` reproduces the crash deterministically on Linux. It leaves a freed 33 MB block of `0x80` bytes behind, and crashed 3/3 before the fix.
   - **Upstream fix:** `u16 VRAMCaptureBlockFlags[16] {};`
2. **Missing `#include <algorithm>` in FreeBIOS.cpp.** It uses `std::copy`, which fails to compile with libc++. See item 14 above.

## Decisions made by the maintainer (2026-10-06)

1. **Frames: RGBA**, converted from melonDS's BGRA. A raw mode is only to be added if profiling ever shows the conversion matters.
2. **Console type: auto.** DSi if any DSi system file is configured, else DS. The errors for "DSi menu/DSiWare without dumps" are what guide users.
3. **RTC.** It is fixed at melonDS's default (2000-01-01 00:00:00) by default, for reproducibility. `set_rtc_epoch(unix_seconds)` (UTC) and `set_rtc_use_host_time(1)` override it, because some games gate content on the date.
4. **`get_audio(count)` is padded with silence to exactly `count`** (the PyNDS contract). `audio_buffered()` / `get_audio_buffer_number()` gives the number of real rows.
5. **32768 Hz default audio rate.**
6. **NooDS-only settings** are accepted. A one-time `UserWarning` per setting fires when one is set to a non-neutral value, and they don't raise.
7. **Write-back is off by default.**
   - Firmware write-back is now its own flag, `set_firmware_writeback`, default 0.
   - Cartridge-save write-back (`set_save_writeback`) also defaults to 0. This matches PyNDS, where saves only reach disk through `write_save_file`.
   - The DSi NAND is still written in place. melonDS writes it directly, and pydsi also resets touch calibration at boot, so point pydsi at a copy.
8. **Windows toolchain: whichever is simplest to use.** That is llvm-mingw UCRT. It was verified locally (below), and CI builds the same thing.

## Platform functions that are stubs

| Function(s) | Stub behaviour |
|---|---|
| `MP_Begin`, `MP_End`, `MP_Send*`, `MP_Recv*`, `MP_RecvReplies` | No local multiplayer: send/receive report 0 bytes / no replies |
| `Net_SendPacket`, `Net_RecvPacket` | No network: 0 bytes, no packet |
| `Camera_Start`, `Camera_Stop` | No-op |
| `Camera_CaptureFrame` | Black frame. YUV422 packs two pixels per word as `Y0 \| U<<8 \| Y1<<16 \| V<<24` (taken from the Qt CameraManager), so black is `0x80008000`; RGB black is `0xFF000000` |
| `Mic_Start`, `Mic_Stop` | No-op |
| `Mic_ReadInput` | Silence: fills and returns the full requested length |
| `AAC_Init` | Returns null; the DSP HLE AAC ucode handles that. `AAC_Configure`/`AAC_DecodeFrame` return false |
| `Addon_KeyDown`, `Addon_RumbleStart/Stop`, `Addon_MotionQuery` | Nothing connected |
| `DynamicLibrary_*` | Return null. Only used by the Android JIT path and libpcap, neither of which is built |
| `WriteGBASave` | No-op (no GBA slot) |
| `WriteDateTime` | Logs only. Qt stores an RTC offset in its config; pydsi has no host-time sync to persist |
| `GetLocalFilePath` | Identity. A library has no emulator directory, so relative paths are relative to the working directory |

Everything else is real:
- File I/O uses `fopen`, with `_wfopen` UTF-8 paths on Windows.
- Threads, mutexes and condvar semaphores use `std::thread`, `std::mutex` and `std::condition_variable`.
- `GetMSCount`/`GetUSCount` use `steady_clock`.
- `Log` goes to stderr, gated by `config.set_log_enabled`.
- `SignalStop` records the reason, exposed as `is_running()`.
- `WriteNDSSave` and `WriteFirmware` buffer per instance. When their write-back flag is on (both are off by default), the data is written to disk after 60 quiet frames or on destruction. `write_save_file` always works. The Qt frontend waits 2 s.
- Generated firmware is never written back. Qt keeps its Wi-Fi part in `wfcsettings.bin`; pydsi doesn't.

## Process-wide state and multi-instance use

melonDS's core is instance-based. Everything per console hangs off `NDS`/`DSi`, and the Qt frontend runs up to 16 instances in one process. pydsi gives every `Dsi` its own `InstanceContext` as the Platform `userdata`, so two objects share no pydsi state. Remaining process-wide state:

| State | Where | Impact |
|---|---|---|
| fatfs: one volume (`FF_VOLUMES 1`), static `FatFs[]`, static LFN buffer, global disk callbacks | fatfs/ffconf.h, ff.c, FATIO.cpp:25-52 | Not thread-safe. FAT access happens when mounting the NAND (boot, DSi direct boot, title install) and loading/saving an SD image (construction, destruction). pydsi never releases the GIL, so these are serialized; don't call into pydsi from threads with the GIL released |
| `NDS::Current` (`static thread_local NDS*`) | NDS.h:569 | Set by every `RunFrame`. Only read by JIT code and the JIT fault handler, so it is harmless in the default build. With the JIT it is per thread, so several instances on one thread are fine because each `RunFrame` resets it |
| JIT fault handler, `ARMJIT_Global` code slices | ARMJIT_Memory.cpp, ARMJIT_Global.cpp | JIT builds only. Ref-counted and mutex-protected, but see the handler hazard above. Each JIT instance also reserves about 8 GiB of address space |
| Log on/off switch | pydsi `platform_impl.cpp` | `Platform::Log` has no userdata, so logging is process-wide |
| `Config` values | pydsi `config.cpp` | Process-wide like PyNDS, but each `Dsi` copies them at construction |

Construct, destroy, construct; two live instances of different console types; and independence of their RAM, input and frames are all covered by `tests/test_instances.py`.

## Verification done

**Compilers on the development machine:**
- MSVC 19.31: can't build the core (see above).
- Strawberry Perl's MinGW-w64 gcc 13.2 (UCRT).
- WinLibs gcc 15.1 (msvcrt, `C:\bin\mingw64`): not used.
- Cygwin gcc 11.4: targets Cygwin, so it can't build CPython extensions.
- An old llvm-mingw clang 11.
- No MSYS2.
- Added as portable copies from the official release archives:
  - LLVM 23.1.2 in `C:\temp\llvm-23.1.2`, for clang-cl
  - llvm-mingw 20260922 UCRT in `C:\temp\llvm-mingw-20260922-ucrt-x86_64`

**Windows 10, Python 3.10, with the machine's own dumps:**
- **llvm-mingw 20260922 UCRT (clang 23.1.2), the chosen path:**
  - 53/53 tests pass.
  - The `.pyd` imports only `python310`, `KERNEL32` and the UCRT.
  - The plain wheel, with no repair step, passes the suite in a clean venv.
- **clang-cl 23.1.2 + MSVC 14.31 STL + Windows SDK 10.0.19041:**
  - 53/53 tests pass.
  - The `.pyd` imports `python310`, `KERNEL32`, `VCRUNTIME140(_1)`, `MSVCP140` and the UCRT.
  - The delvewheel-repaired wheel bundles `msvcp140` and passes the suite in a clean venv.
- **MinGW-w64 gcc 13.2 UCRT:** 53/53 tests pass. The `.pyd` imports only `python310`, `KERNEL32` and the UCRT.
**Linux: Pop!_OS 24.04 x86_64, gcc 13.3, CMake 3.28, Python 3.12, on a LAN box.**
- A plain `pip install .[test]` in a fresh venv, with build isolation (nanobind and scikit-build-core from PyPI), builds cleanly.
- With the machine's own dumps copied over, the full suite (56 tests) passed 40 runs in a row, after the GPU fix above.
  - Before the fix, about 1 run in 10 segfaulted in `test_dsi_mode_real_files`.
  - That was found with AddressSanitizer (clean) and a preloaded crash handler plus `addr2line`.
- The `.so` links only `libstdc++`, `libm`, `libgcc_s` and `libc`.
- A `PYDSI_ENABLE_JIT=ON` build also works, and `test_jit_flag` passes. On the test ROM, 600 frames take 0.75 s with the JIT and 1.40 s with the interpreter, and both runs are frame-exact (599 VBlanks).
- macOS has not been built.

**`pytest` without dumps:** 49 pass and 4 skip.

**By hand, with real dumps:**
- The DS firmware boots to its first-run settings screen.
- The DSi NAND boots. It has Unlaunch installed, which shows its menu.
- Choosing "Launcher" with key input boots the real DSi menu.
- Tapping the touchscreen gets past the health-and-safety screen into the menu.
- With `rtc_epoch = 1700000000`, the DSi menu clock shows 11/14 22:13.

**Not verified:**
- DSiWare install and boot. The test NAND has no user DSiWare, and no `.app` + `.tmd` was available.
- Whether the TLNC auto-load still works when Unlaunch is installed.
- macOS builds.
- manylinux/musllinux wheels and the aarch64 build (only a native x86_64 Linux build was tested).
- The CI workflow itself, which has never run.
- Firmware write-back, which needs the emulated system to write its firmware.
