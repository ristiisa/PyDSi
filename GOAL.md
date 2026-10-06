# Task: build `pydsi` — a Python binding for melonDS with DSi support

## Goal

A pip-installable Python package `pydsi` that wraps the melonDS emulator core (not its Qt frontend) via nanobind, exposing a PyNDS-compatible API, and supports booting in DSi mode (DSi BIOS + firmware + NAND) so DSi-enhanced titles, DSi-exclusive titles, and DSiWare can run headlessly for RL/bot use.

Reference project for API shape: https://github.com/unexploredtest/PyNDS — read `cnds/*.cpp`, `cnds/include/*.hpp`, `pynds/`, `CMakeLists.txt`, `pyproject.toml`, `setup.py`, `.github/workflows/` before writing anything. Mirror its API; do not redesign it.

Reference implementations of a non-Qt melonDS frontend (use these to learn the Platform contract, not to copy wholesale):

- melonDS DS libretro core: https://github.com/JesseTG/melonds-ds (look at how it implements `Platform.h`, boots DSi, loads NAND/firmware)
- melonDS Android lib: https://github.com/rafaelvcaetano/melonDS-android-lib

## Hard rules

- Do not assume melonDS's API from memory. Clone upstream `melonDS-emu/melonDS`, read `src/Platform.h`, `src/NDS.h`, `src/DSi.h`, `src/NDSCart.h`, `src/DSi_NAND.h`, `src/SPI_Firmware.h`, `src/Args.h` (or wherever args structs live), `src/CMakeLists.txt`, and `src/frontend/qt_sdl/EmuInstance*.cpp` as the canonical example of core usage. Base all code on what is actually there.
- Pin melonDS to a specific commit as a git submodule under `externals/melonDS`. Record the commit hash in the README.
- Do not modify files inside the submodule. If a change is unavoidable, keep it as a patch file in `patches/` applied at build time and document why.
- No git commits, no `Co-Authored-By` or any attribution trailers anywhere.
- Code style: 2-space indent; no space after `if`, `for`, `while`, `switch`, `catch`; do not wrap lines just for length. Match PyNDS's existing style where you copy from it.
- License: GPL-3.0 (melonDS is GPLv3). Include LICENSE.
- When something in this brief contradicts what you find in melonDS's source, the source wins — note the discrepancy in your final report.

## Repository layout

```
pydsi/
  externals/melonDS/        # submodule, pinned
  cdsi/
    include/
      dsi.hpp
      config.hpp
      platform_impl.hpp
    platform_impl.cpp       # implements melonDS Platform.h
    dsi.cpp                 # emulator wrapper (equivalent of PyNDS cnds/nds.cpp)
    dsi_input.cpp
    dsi_memory.cpp
    config.cpp
    cdsi.cpp                # nanobind module definition
  pydsi/
    __init__.py
    pydsi.py                # Python-facing class, mirrors pynds/pynds.py
    button.py / key enum (mirror PyNDS)
  tests/
  patches/                  # only if needed
  CMakeLists.txt
  pyproject.toml
  setup.py (only if PyNDS's build needs it; prefer scikit-build-core)
  README.md
  LICENSE
  .gitmodules
  .github/workflows/build.yml
```

## Phase 1 — discovery (do this first, report before building)

1. Clone melonDS, check out the latest tagged release unless `master` is clearly required for DSi fixes; justify the choice.
2. Produce a short written summary of:
   - the exact class(es) used to construct an emulator instance (`melonDS::NDS` / `melonDS::DSi` or whatever they are now), their constructors/args structs, and how ROM + save + DSi system files are passed in;
   - every function in `Platform.h` that must be implemented, grouped into: file I/O, threading/sync, network, camera/mic, misc; note which have default/no-op implementations upstream;
   - how the Qt frontend boots DSi mode: which files it loads (DSi BIOS7/BIOS9, DS BIOS7/BIOS9, DSi firmware, NAND), in what order, and how direct-boot of a DSi-enhanced cart vs. DSiWare-on-NAND differs;
   - how the frontend gets the framebuffer out (`GPU.Framebuffer`, `GPU::ReadFramebuffer`, or whatever is current), its format, and how it handles the frame-ready signal;
   - how audio samples are read (`SPU::ReadOutput` or current equivalent);
   - how input is set (`NDS::SetKeyMask`, touchscreen functions);
   - save state API (`Savestate`, `DoSavestate`);
   - memory read/write helpers for arbitrary addresses on ARM9 (`ARM9Read8/16/32` / `ARM9Write*` or current equivalent) — this is what PyNDS's `nds_memory.cpp` wraps;
   - save-file (SRAM/NAND save) write-back path;
   - what the core CMake target is called and its dependencies (zlib, libslirp, enet, etc.) and which can be disabled for headless use.
3. List which PyNDS API methods map cleanly, which need adaptation, and which can't be supported.

Stop and write this up. Then continue.

## Phase 2 — build the core as a static library

- Add melonDS as submodule. In top-level `CMakeLists.txt`, `add_subdirectory(externals/melonDS/src)` or equivalent to build only the core target. Disable the Qt frontend and any optional networking/camera features via CMake options if they exist; if not, implement the Platform functions as stubs instead.
- Confirm it builds on Linux first. Then macOS. Windows last.
- Use nanobind (fetch via FetchContent or pip's nanobind package, matching PyNDS's approach). Python ≥ 3.9.
- Build system: `scikit-build-core` with `pyproject.toml`. Only fall back to `setup.py` if there's a concrete reason.

## Phase 3 — Platform implementation (`platform_impl.cpp`)

Implement every required `Platform::` function:

- File I/O: real implementations using `fopen`/`fread`/`fseek` etc. NAND and SD images must be opened read/write; BIOS/firmware read-only. Honour melonDS's file-open mode enums exactly.
- Threads/mutex/semaphore: real implementations with `std::thread`, `std::mutex`, `std::counting_semaphore` or a condvar-based one. melonDS uses threads internally for the GPU (threaded 3D) — don't stub these.
- Network (LAN/Wi-Fi/slirp): no-op stubs returning "no packet".
- Camera: stub returning a black frame of the expected size/format. Mic: stub returning silence.
- `Platform::Log`: route to `stderr`, gated by a config flag.
- `SignalStop`, `WriteNDSSave`, `WriteGBASave`, `WriteFirmware`, `WriteDateTime`, any `*Save` writeback hooks: implement writeback to the configured paths.
- Any "instance ID" / multi-instance plumbing: support a single instance per Python object; ensure two `pydsi` objects in one process do not share global state. If melonDS still has process-wide globals that make this impossible, document it and enforce one-instance-per-process with a clear error.

## Phase 4 — wrapper class (`dsi.cpp`, mirror PyNDS `nds.cpp`)

Constructor signature should match PyNDS: `Dsi(std::string romPath, std::string savePath, bool isGba)`. GBA mode may be stubbed as unsupported initially; keep the parameter for API parity.

Methods to expose (match PyNDS names exactly where they exist):

- `runTask()` / `runUntilFrame()` — advance one frame.
- `getFrame()`, `getTopNdsFrame()`, `getBotNdsFrame()` — return numpy `uint8[height, width, 4]` RGBA (or document BGRA/XRGB and convert; PyNDS returns 4-channel). Handle upscaled framebuffers if the scale config is >1.
- `getAudioSamples(count)`, `getAudioBufferNumber()`.
- `saveState(path)`, `loadState(path)`.
- `saveGame(path, alwaysSave)`.
- Input: mirror PyNDS `nds_input.cpp` (button press/release/mask, touch set/release).
- Memory: mirror PyNDS `nds_memory.cpp` (read/write u8/u16/u32, bulk read into numpy).

DSi specifics:

- Mode is chosen at construction from config (`Config::setConsoleType(0|1)` or similar). Default: DS mode if no DSi files configured, DSi if they are. Error clearly if DSi mode requested and a required file is missing.
- Support both: (a) direct-booting a `.nds` DSi-enhanced/exclusive cart in DSi mode, and (b) booting DSiWare already installed on the NAND image. Optional stretch: an `installDsiware(path)` helper using melonDS's NAND title-import code, if that code lives in the core and not in the Qt frontend.
- Save state in DSi mode must round-trip.

## Phase 5 — config (`config.cpp`)

Mirror PyNDS `config.cpp` as getters/setters on a `Config` class. Required:

- `bios9Path`, `bios7Path`, `firmwarePath` (DS)
- `dsiBios9Path`, `dsiBios7Path`, `dsiFirmwarePath`, `dsiNandPath`, `dsiSdPath` (optional SD image)
- `consoleType` (DS/DSi), `directBoot`, `dsiFullBiosBoot` if melonDS has it
- `threaded3D`, `renderScale` / `gpuScale` if available (software renderer only; do not wire OpenGL/Vulkan)
- `audioEnabled`, `logEnabled`
- Any Platform-side setting (e.g. save-file writeback on/off)

Values must be read at construction time; document that changes after construction don't apply.

## Phase 6 — nanobind module and Python package

- `cdsi.cpp` binds `Dsi` and `Config` with the same names/signatures PyNDS binds in `cnds.cpp`.
- `pydsi/pydsi.py`: port `pynds/pynds.py` with the class renamed; `pydsi.pydsi(rom_path, save_path=None)` plus `tick()`, `get_frame()`, `get_audio()`, `button_press/release`, `set_touch`, `save_state/load_state`, memory helpers.
- Expose config as `pydsi.config` (module-level, same as PyNDS if that's how it does it — verify).
- Type stubs (`.pyi`) via nanobind's stubgen.

## Phase 7 — tests

- No copyrighted BIOS/firmware/NAND/ROMs in the repo. Tests must read file locations from env vars (`PYDSI_DSI_BIOS9` etc.) and skip if unset.
- Homebrew test ROM for DS mode: build or fetch a tiny public homebrew `.nds` (e.g. from devkitPro examples, built in CI) and check that `tick()` produces non-black frames within N frames. For DSi mode, use a DSi homebrew that reports its mode (e.g. one that checks `REG_SCFG_EXT` / `isDSiMode()`); assert via memory read or frame diff that DSi mode was entered.
- Round-trip test: save state → tick 60 → load state → frames equal.
- Memory read/write test against a known homebrew address.
- Smoke-test that two instances in sequence (construct, destroy, construct) work.

## Phase 8 — CI and wheels

- Port PyNDS's GitHub workflow: cibuildwheel for manylinux (x86_64, aarch64), musllinux, macOS (x86_64, arm64), Windows x86_64; Python 3.9–3.13.
- Cache the melonDS build.
- Upload wheels as artifacts; publish step gated on tags (don't configure PyPI tokens; leave the step present but commented).

## Deliverables / final report

1. Working repo per the layout above, building on at least Linux locally.
2. `README.md`: install, required system files for DS vs DSi mode and how to obtain them from your own console (link to melonDS's docs; do not link ROM/BIOS download sites), API reference table with the PyNDS equivalents, known limitations, pinned melonDS commit.
3. A `NOTES.md` with: the Phase 1 discovery summary, every place melonDS's API diverged from this brief, every Platform function stubbed, and a list of upstream globals or coupling that constrain multi-instance use.
4. A list of open questions for the maintainer rather than guessed decisions — if a design choice is ambiguous (e.g. framebuffer channel order, default console type), ask; do not pick silently.

## Out of scope

OpenGL/Vulkan rendering, netplay/LAN, real camera/mic input, GBA slot emulation beyond what's needed to compile, a GUI, JIT (build with the interpreter unless the JIT is on by default and works headlessly — if so, expose it as a config flag).
