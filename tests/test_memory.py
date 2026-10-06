import numpy as np
import pytest

import pydsi
from pydsi import config
from conftest import MAGIC, RESULTS, results, tick_until_running


@pytest.mark.parametrize("console_type", [0, 1])
def test_write_is_seen_by_rom(test_rom, console_type):
  config.set_console_type(console_type)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)

  # The ROM copies mailbox + 1 to RESULTS + 20 every VBlank
  emu.memory.write_u32(False, RESULTS + 16, 41)
  emu.tick(2)
  assert emu.memory.read_u32(False, RESULTS + 20) == 42

  emu.memory.write_i32(False, RESULTS + 16, -10)
  emu.tick(2)
  assert emu.memory.read_i32(False, RESULTS + 20) == -9


def test_read_widths(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  mem = emu.memory
  assert mem.read_u32(False, RESULTS) == MAGIC
  assert mem.read_u16(False, RESULTS) == MAGIC & 0xFFFF
  assert mem.read_u8(False, RESULTS + 3) == MAGIC >> 24
  assert mem.read_u64(False, RESULTS) == MAGIC | (results(emu)[1] << 32)

  mem.write_f32(False, RESULTS + 0x40, 1.5)
  assert mem.read_f32(False, RESULTS + 0x40) == 1.5
  mem.write_i16(False, RESULTS + 0x44, -2)
  assert mem.read_i16(False, RESULTS + 0x44) == -2
  assert mem.read_u16(False, RESULTS + 0x44) == 0xFFFE
  mem.write_i8(False, RESULTS + 0x46, -3)
  assert mem.read_i8(False, RESULTS + 0x46) == -3


def test_read_map(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  data = emu.memory.read_map(False, RESULTS, 24)
  assert data.dtype == np.uint8 and data.shape == (24,)
  assert tuple(data.view("<u4")) == results(emu)

  emu.memory.write_map(False, RESULTS + 0x80, np.arange(16, dtype=np.uint8))
  assert emu.memory.read_map(False, RESULTS + 0x80, 16).tolist() == list(range(16))


def test_arm7_sees_main_ram(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  assert emu.memory.read_u32(True, RESULTS) == MAGIC


def test_tcm(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  # ITCM is mapped at address 0 for the ARM9 after direct boot; the bus (tcm=False) can't see it
  emu.memory.write_u32(False, 0x100, 0x12345678, True)
  assert emu.memory.read_u32(False, 0x100, True) == 0x12345678
  assert emu.memory.read_u32(False, 0x100, False) != 0x12345678


def test_io_register(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  # DISPCNT as set by the ROM: VRAM display mode, bank A
  assert emu.memory.read_u32(False, 0x04000000) == 0x00020000
