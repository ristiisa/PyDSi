import numpy as np
import pytest

import pydsi
from pydsi import config
from conftest import MAGIC, results, tick_until_running

RED = [251, 0, 0, 255]  # 0x001F expanded from 6 bits per channel
GREEN = [0, 251, 0, 255]


def test_ds_mode_freebios(test_rom):
  config.set_console_type(0)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  emu.tick(10)

  top, bot = emu.get_frame()
  assert top.shape == (192, 256, 4) and top.dtype == np.uint8
  assert bot.shape == (192, 256, 4)
  assert top[:, :, :3].any(), "top screen is black"
  assert top[150, 100].tolist() == GREEN

  magic, scfg, vblanks, _, _, _ = results(emu)
  assert magic == MAGIC
  assert scfg == 0
  assert vblanks > 0
  assert emu.get_console_type() == 0
  assert not emu.is_dsi_mode()


def test_dsi_mode_freebios(test_rom):
  # melonDS can direct-boot DSi titles without any dumps (FreeBIOS + generated firmware)
  config.set_console_type(1)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)

  top, _ = emu.get_frame()
  assert top[150, 100].tolist() == RED

  scfg = results(emu)[1]
  assert scfg & 0x80000000, f"SCFG_EXT9 = {scfg:08x}"
  assert emu.get_console_type() == 1
  assert emu.is_dsi_mode()


def test_default_console_is_ds_without_dsi_files(test_rom):
  emu = pydsi.pydsi(test_rom)
  assert emu.get_console_type() == 0


def test_ds_mode_real_bios(test_rom, ds_files):
  config.set_console_type(0)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  assert results(emu)[1] == 0


def test_dsi_mode_real_files(test_rom, dsi_files):
  # Console type -1 (auto) picks DSi because DSi files are configured
  emu = pydsi.pydsi(test_rom)
  assert emu.get_console_type() == 1
  tick_until_running(emu)
  assert results(emu)[1] & 0x80000000
  assert emu.is_dsi_mode()


def test_dsi_menu_boots(dsi_files):
  # No ROM: boot the DSi menu from the NAND; it should draw something within a few seconds
  emu = pydsi.pydsi(None)
  for _ in range(600):
    emu.tick()
    top, bot = emu.get_frame()
    if top[:, :, :3].any() or bot[:, :, :3].any():
      break
  else:
    pytest.fail("DSi menu didn't draw anything in 600 frames")


def test_threaded_3d(test_rom):
  config.set_threaded_3d(1)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  emu.tick(30)
  assert results(emu)[2] > 0


def test_frame_counts_progress(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  before = results(emu)[2]
  emu.tick(60)
  assert results(emu)[2] - before == 60
