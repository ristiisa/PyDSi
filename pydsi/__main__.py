"""Command-line runner: boot a ROM headless, drive input, save screenshots, read memory.

  python -m pydsi game.nds --dsi --frames 600 --press a@300:5 --png out.png --read 0x02000000:u32

System files are taken from the same environment variables as the tests (PYDSI_BIOS9,
PYDSI_BIOS7, PYDSI_FIRMWARE, PYDSI_DSI_BIOS9, PYDSI_DSI_BIOS7, PYDSI_DSI_FIRMWARE, PYDSI_DSI_NAND).
The NAND and firmware are copied to a temporary directory first, so the originals are never written.
"""

import argparse
import os
import shutil
import struct
import sys
import tempfile
import zlib

import numpy as np

from .pydsi import PyDSi
from .button import KEY_MAP
from .config import config

ENV_PATHS = {
  "PYDSI_BIOS9": config.set_bios_9_path,
  "PYDSI_BIOS7": config.set_bios_7_path,
  "PYDSI_FIRMWARE": config.set_firmware_path,
  "PYDSI_DSI_BIOS9": config.set_dsi_bios_9_path,
  "PYDSI_DSI_BIOS7": config.set_dsi_bios_7_path,
  "PYDSI_DSI_FIRMWARE": config.set_dsi_firmware_path,
  "PYDSI_DSI_NAND": config.set_dsi_nand_path,
}
COPIED = ("PYDSI_FIRMWARE", "PYDSI_DSI_FIRMWARE", "PYDSI_DSI_NAND")


def write_png(path, rgba):
  height, width, _ = rgba.shape
  raw = b"".join(b"\0" + rgba[y].tobytes() for y in range(height))

  def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
  with open(path, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def parse_event(text, what):
  """'X@FRAME' or 'X@FRAME:DURATION' -> (X, frame, duration)"""
  target, _, when = text.partition("@")
  if not when:
    raise argparse.ArgumentTypeError(f"{what} needs @FRAME, e.g. {target}@120")
  frame, _, duration = when.partition(":")
  return target, int(frame), int(duration or 1)


def main(argv=None):
  p = argparse.ArgumentParser(prog="python -m pydsi", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  p.add_argument("rom", nargs="?", help="ROM to boot (omit to boot the system menu; needs dumps)")
  p.add_argument("--save", help="cartridge save file to load")
  mode = p.add_mutually_exclusive_group()
  mode.add_argument("--ds", action="store_true", help="DS mode")
  mode.add_argument("--dsi", action="store_true", help="DSi mode (default: DSi if DSi dumps are configured, else DS)")
  p.add_argument("--frames", type=int, default=300, help="frames to run (default 300, about 5 s)")
  p.add_argument("--press", action="append", default=[], metavar="KEY@FRAME[:N]", help="hold KEY (" + ",".join(KEY_MAP) + ") for N frames starting at FRAME; repeatable")
  p.add_argument("--touch", action="append", default=[], metavar="X,Y@FRAME[:N]", help="touch the bottom screen at X,Y for N frames; repeatable")
  p.add_argument("--png", help="save the final top+bottom screens as one 256x384 PNG")
  p.add_argument("--shot-every", type=int, metavar="N", help="also save a PNG every N frames (needs --png; frame number is added to the name)")
  p.add_argument("--read", action="append", default=[], metavar="ADDR[:TYPE]", help="print ARM9 memory at the end; TYPE u8/u16/u32 (default)/i32/f32; repeatable")
  p.add_argument("--rtc-epoch", type=int, help="clock start as Unix time (default: 2000-01-01)")
  p.add_argument("--log", action="store_true", help="show melonDS's log")
  args = p.parse_args(argv)

  presses = [parse_event(e, "--press") for e in args.press]
  for key, _, _ in presses:
    if key not in KEY_MAP:
      p.error(f"unknown key {key!r}; use one of {', '.join(KEY_MAP)}")
  touches = []
  for e in args.touch:
    xy, frame, duration = parse_event(e, "--touch")
    x, y = (int(v) for v in xy.split(","))
    touches.append(((x, y), frame, duration))

  tmp = tempfile.mkdtemp(prefix="pydsi-")
  emu = None
  try:
    for var, setter in ENV_PATHS.items():
      path = os.environ.get(var)
      if path:
        if var in COPIED:
          copy = os.path.join(tmp, os.path.basename(path))
          shutil.copyfile(path, copy)
          path = copy
        setter(path)
    if args.ds:
      config.set_console_type(0)
    elif args.dsi:
      config.set_console_type(1)
    if args.rtc_epoch is not None:
      config.set_rtc_epoch(args.rtc_epoch)
    config.set_log_enabled(1 if args.log else 0)

    emu = PyDSi(args.rom, args.save)
    print(f"console: {'DSi' if emu.get_console_type() == 1 else 'DS'}, DSi mode active: {emu.is_dsi_mode()}")

    base, ext = os.path.splitext(args.png or "")
    for frame in range(args.frames):
      for key, start, duration in presses:
        if frame == start:
          emu.button_press(key)
        elif frame == start + duration:
          emu.button_release(key)
      for (x, y), start, duration in touches:
        if frame == start:
          emu.set_touch(x, y)
        elif frame == start + duration:
          emu.release_touch()
      emu.tick()
      if args.png and args.shot_every and (frame + 1) % args.shot_every == 0:
        write_png(f"{base}_{frame + 1:05d}{ext or '.png'}", np.concatenate(emu.get_frame(), axis=0))

    if args.png:
      write_png(args.png, np.concatenate(emu.get_frame(), axis=0))
      print(f"saved {args.png}")

    readers = {"u8": emu.memory.read_u8, "u16": emu.memory.read_u16, "u32": emu.memory.read_u32, "i32": emu.memory.read_i32, "f32": emu.memory.read_f32}
    for item in args.read:
      addr, _, kind = item.partition(":")
      value = readers[kind or "u32"](False, int(addr, 0))
      print(f"{addr} ({kind or 'u32'}) = {value} ({value:#x})" if isinstance(value, int) else f"{addr} ({kind}) = {value}")
    print(f"running: {emu._nds.is_running()}, frames: {emu._nds.get_frame_count()}")
  finally:
    # Release the emulator first: it holds the copied NAND open
    emu = None
    shutil.rmtree(tmp, ignore_errors=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
