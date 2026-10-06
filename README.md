## pydsi

A Python interface for the [melonDS](https://github.com/melonDS-emu/melonDS) Nintendo DS / DSi emulator core. It is meant for reinforcement learning and bots: it runs headless, one frame per call, with frames, audio, input, memory and savestates exposed to Python.

The API mirrors [PyNDS](https://github.com/unexploredtest/PyNDS), which wraps NooDS. In most code you can swap `pynds.PyNDS` for `pydsi.PyDSi`. pydsi also boots in **DSi mode**, which covers DSi-enhanced cartridges, DSi-exclusive titles and DSiWare.

melonDS is pinned as a git submodule at commit **`906e9ebb27da8c6a715cd7abab4abfe8a8d29427`** (master, 2026-08-24). See [NOTES.md](NOTES.md) for why this commit was chosen over the 1.1 tag.

### Example
```py
import pydsi

nds = pydsi.pydsi("path/to/rom.nds", "path/to/rom.sav")
nds.tick()                                  # run one frame
frame_top, frame_bottom = nds.get_frame()   # uint8[192, 256, 4] RGBA each
nds.button.press_key("a")
nds.tick(10)
value = nds.memory.read_u32(False, 0x02000000)
nds.save_state("state.mln")
```

DSi mode:
```py
from pydsi import config
config.set_console_type(1)              # or configure DSi system files and leave it on auto
nds = pydsi.pydsi("dsi_enhanced_game.nds")
nds.tick()
nds.is_dsi_mode()                       # True when the DSi's extended hardware is active
```

### Command line
A quick way to check a ROM without writing code:
```sh
python -m pydsi game.nds --dsi --frames 600 --press a@300:5 --touch 128,96@400:3 --png out.png --read 0x02000000:u32
```

It reports whether DSi mode is active, holds keys and touches at the given frames, saves both screens as one PNG (`--shot-every N` saves a series), and prints memory. System files come from the `PYDSI_*` environment variables listed under Tests. The NAND and firmware are copied to a temporary directory first, so the originals are never written. Run `python -m pydsi --help` for every option.

### Install
Wheels are built by CI for:
- Linux (manylinux and musllinux, x86_64 and aarch64)
- macOS (x86_64 and arm64)
- Windows (x86_64)

They cover Python 3.9 to 3.13. Until a release is published to PyPI, build from source.

### Build
Building needs CMake 3.18 or newer, Ninja or Make, and a C++20 compiler. melonDS itself is C++17.

```sh
git clone --recursive <this repository>
cd pydsi
pip install .
```

On Windows, melonDS can't be compiled with MSVC's `cl.exe` (NOTES.md lists the reasons). Use **[llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases)**, the **UCRT** build: a portable zip with no installer and no Visual Studio needed. CI builds with it.

It uses the same C runtime as CPython. Its C++ runtime is linked statically, so the module needs no extra DLLs. Unzip it, put its `bin` directory on `PATH`, then:
```bat
set CC=clang
set CXX=clang++
set CMAKE_GENERATOR=Ninja
pip install .
```

These also work (all three toolchains pass the same tests):
- **clang-cl** from a Visual Studio developer prompt (`CC=clang-cl`, `CXX=clang-cl`). Wheels made this way need `delvewheel` to bundle `MSVCP140.dll`.
- **A UCRT build of MinGW-w64 gcc** (`CC=gcc`, `CXX=g++`), e.g. Strawberry Perl's or MSYS2's `ucrt64`.

Avoid msvcrt-based MinGW toolchains: they put a second C runtime into the Python process.

**JIT.** melonDS's JIT recompiler is left out by default (see Known limitations). Build it in with `PYDSI_ENABLE_JIT=ON pip install .`, then turn it on with `config.set_jit_enabled(1)`.

**Tests.**
```sh
pip install .[test]
pytest
```

The tests generate their own tiny homebrew ROM (`tests/romgen.py`), so no game files are needed. Tests that need real system files are skipped unless these environment variables point at your own dumps:

| Variable | File |
|---|---|
| `PYDSI_BIOS9`, `PYDSI_BIOS7`, `PYDSI_FIRMWARE` | DS ARM9 BIOS (4 KB), ARM7 BIOS (16 KB), DS firmware |
| `PYDSI_DSI_BIOS9`, `PYDSI_DSI_BIOS7`, `PYDSI_DSI_FIRMWARE`, `PYDSI_DSI_NAND` | DSi ARM9i/ARM7i BIOS (64 KB each), DSi firmware (128 KB), DSi NAND |

The DSi tests work on copies of the firmware and NAND.

### System files
None are included, and none are needed for most uses.

| Use | DS BIOS + firmware | DSi BIOS + firmware | DSi NAND |
|---|---|---|---|
| DS games / homebrew, direct boot (default) | optional (FreeBIOS + generated firmware otherwise) | no | no |
| DS firmware menu (`pydsi(None)` in DS mode) | **required** | no | no |
| DSi-enhanced / DSi-exclusive cartridges, DSi homebrew, direct boot | optional | optional (FreeBIOS-TWL + generated firmware otherwise) | optional |
| DSi menu (`pydsi(None)` in DSi mode) | optional | **required** | **required** |
| DSiWare | optional | **required** | **required**, with the title installed |

**Getting the files.** Dump them from your own console. The [melonDS FAQ](https://melonds.kuribo64.net/faq.php) explains which files melonDS needs and how to dump them. Without real dumps, some titles misbehave or refuse to run.

**Writes to your files.** In DSi mode the NAND is opened read/write, and melonDS writes to it:
- when the emulated system writes to it
- at every boot, when the touchscreen calibration in its user settings is reset to a 1:1 mapping (the same as melonDS's Qt frontend)

Point pydsi at a copy of your NAND. Firmware and cartridge saves are only written back if you turn that on (`set_firmware_writeback`, `set_save_writeback`).

**DSiWare.** It runs from the NAND, not the cartridge slot. Install the title once with:
```py
config.set_dsi_bios_7_path(...)
config.set_dsi_nand_path(...)
pydsi.PyDSi.install_dsiware("title.nds", "title.tmd")
```
This uses melonDS's NAND title import. The decrypted app and its TMD are needed.

After that, `pydsi.pydsi("title.nds")` in DSi mode boots it. pydsi asks the real DSi menu to launch the title by setting the BPTWL boot flag and writing a "TLNC" auto-load record to RAM. This is the same mechanism melonDS DS (libretro) uses.

### Configuration
`pydsi.config` is a process-wide set of settings. A `PyDSi` object copies the settings when it is created, so later changes only affect objects created afterwards. The exception is `set_log_enabled`, which applies immediately.

| Setting | Meaning |
|---|---|
| `set_console_type(-1/0/1)` | Auto (default: DSi if any DSi system file is set, else DS), DS, DSi |
| `set_dsi_mode(0/1)` | PyNDS name. Sets the console type to DS or DSi |
| `set_direct_boot` | Boot ROMs directly, skipping the firmware menu (default 1). Forced on when the firmware can't boot |
| `set_bios_9_path`, `set_bios_7_path`, `set_firmware_path` | DS system files. Empty means FreeBIOS and generated firmware |
| `set_dsi_bios_9_path`, `set_dsi_bios_7_path`, `set_dsi_firmware_path`, `set_dsi_nand_path` | DSi system files |
| `set_dsi_sd_path`, `set_dsi_sd_read_only` | DSi SD card image (`set_sd_image_path` is the PyNDS name). The image is created if missing |
| `set_threaded_3d` | Run the software 3D renderer on its own thread |
| `set_emulate_audio` | 0 discards audio every frame, so `get_audio` returns nothing |
| `set_audio_16_bit` | 0 degrades output to 10 bits like the hardware |
| `set_audio_sample_rate` | Output rate in Hz. Default 32768, the DS's native output rate and what PyNDS's audio player uses. melonDS resamples to whatever is set |
| `set_dsi_dsp_hle` | DSi DSP high-level emulation (faster, less accurate) |
| `set_jit_enabled` | Use the JIT recompiler. Needs a build with `PYDSI_ENABLE_JIT=ON` |
| `set_save_writeback` | Write the cartridge save to `save_path` as the game changes it (default 0: like PyNDS, only `write_save_file` writes it) |
| `set_firmware_writeback` | Write firmware changes made by the emulated system back to the configured firmware dump (default 0) |
| `set_rtc_epoch` | Clock start time as a Unix timestamp, read as UTC. Default -1 means melonDS's fixed 2000-01-01 00:00:00, so runs are reproducible. The DS clock covers 2000-2099 |
| `set_rtc_use_host_time` | Start the clock at the host's local time instead (default 0) |
| `set_log_enabled` | Print melonDS's log on stderr |
| `set_rom_in_ram`, `set_fps_limiter`, `set_frame_skip`, `set_threaded_2d`, `set_high_res_3d`, `set_screen_ghost`, `set_saves_folder`, `set_states_folder`, `set_cheats_folder`, `set_screen_filter`, `set_arm7_hle`, `set_gba_bios_path`, `set_base_path` | Accepted for PyNDS compatibility, but melonDS has no equivalent. Setting one to a non-neutral value gives a one-time `UserWarning` |

Every setter has a matching getter.

### API
`pydsi.PyDSi(path, save_path=None, auto_detect=True, is_gba=False)`, also available as `pydsi.pydsi`. Passing `path=None` boots the system menu.

Low-level calls go through the C++ object `pydsi_obj._nds`, a `cdsi.Dsi` (also available as `cdsi.Nds`).

| pydsi | PyNDS equivalent | Notes |
|---|---|---|
| `tick(count=1)` | `tick` | Runs `count` frames |
| `get_frame()` → `(top, bottom)` | `get_frame` | `uint8[192, 256, 4]` RGBA views that update in place on each `tick`. Copy them to keep a frame. melonDS's native BGRA is converted to RGBA |
| `get_frame_shape()` | `get_frame_shape` | Always `(192, 256, 4)` |
| `get_audio(count=699)` | `get_audio` | Exactly `int16[count, 2]`: the buffered samples, then silence |
| `get_audio_buffer_number()` / `audio_buffered()` | `get_audio_buffer_number` | Stereo frames waiting, i.e. how many rows of the next `get_audio` are real. The buffer holds 2048 and older samples are dropped |
| `button.press_key(k)` / `button.release_key(k)` | same | `k` is a `KEY_MAP` name: a, b, select, start, right, left, up, down, r, l, x, y |
| `button_press(k)` / `button_release(k)` | — | Shortcuts for the above |
| `button.set_touch(x, y)`, `touch()`, `release_touch()`, `clear_touch()` | same | Pixel coordinates on the bottom screen |
| `set_touch(x, y)` / `release_touch()` | — | Move and press, or release |
| `button.set_lid_closed(b)` | — | |
| `memory.read_u8/u16/u32/u64/i8/i16/i32/i64/f32/f64(arm7, addr, tcm=False)` | `memory.read_*` | The bus as seen by the ARM9 or ARM7. With `tcm=True`, ARM9 accesses that hit ITCM/DTCM use those. Reading I/O registers can have side effects |
| `memory.write_u8/u16/u32/i8/i16/i32/f32(...)` | `memory.write_*` | |
| `memory.read_map(arm7, addr, size, tcm=False)` → `uint8[size]` | (commented out in PyNDS) | Bulk read |
| `memory.write_map(arm7, addr, data, tcm=False)` | (commented out in PyNDS) | Bulk write |
| `save_state_to_file(p)` / `load_state_from_file(p)` | same | Also `save_state` / `load_state`. A failed load raises and leaves the emulator unchanged |
| `write_save_file(path="", always_save=True)` | same | Cartridge save memory |
| `install_dsiware(app, tmd, overwrite=False)` | — | See DSiWare above |
| `is_dsi_mode()`, `get_console_type()` | — | |
| `open_window()`, `render()`, `close_window()`, `open_audio()`, `close_audio()` | same | Need `pip install pydsi[window,audio]` (pygame, sounddevice) |
| `get_is_gba()` | same | Always False |
| `_nds.run_task()` | `run_task` | Runs a whole frame: melonDS doesn't expose anything smaller |
| `_nds.get_gba_frame()` | same | Empty: there is no GBA mode |

### Known limitations
- **No GBA mode.** melonDS doesn't emulate GBA games, so `is_gba=True` raises.
- **No upscaling.** Only the software renderer is used, and it is fixed at 256x192. OpenGL and Vulkan are out of scope.
- **What savestates don't contain** (melonDS limitations):
  - NAND or SD card contents. In DSi mode, loading a state after the system wrote to the NAND can leave the two out of sync.
  - Firmware contents.
  - Input. pydsi re-applies the current input after loading.
- **No networking, no multiplayer.** There is no local multiplayer or Wi-Fi. Camera frames are black and the microphone is silent.
- **No AAC decoder.** DSi DSP HLE has none, so AAC audio in DSiWare is silent with `set_dsi_dsp_hle(1)`.
- **The JIT is not built by default.** A JIT build of melonDS installs a process-wide crash handler (SIGSEGV on POSIX, a first-chance vectored exception handler on Windows). It does this even when the JIT is disabled at runtime, and the handler dereferences a melonDS thread-local. An unrelated crash elsewhere in the Python process could then turn into a crash inside melonDS.
- **Threads.** All emulator calls run with the GIL held. melonDS's FAT code (used for the NAND and SD card) has process-wide state, so don't drive several emulators from threads that release the GIL. Several `PyDSi` objects in one process are fine. See NOTES.md.
- **RTC.** By default the emulated clock starts at melonDS's default (2000-01-01) and isn't synced to the host, which keeps runs deterministic. See `set_rtc_epoch` and `set_rtc_use_host_time`.
- **DSiWare boot is untested.** Booting DSiWare from the NAND (`install_dsiware` plus the auto-load record) hasn't been tested end to end yet. The DS firmware boot, the DSi menu and the real-dump tests have been run locally against real dumps. CI has no system files, so it skips those tests.

### License
GPL-3.0-or-later, the same as melonDS. See [LICENSE](LICENSE).
