import gc

import numpy as np

import pydsi
from pydsi import config
from conftest import results, tick_until_running


def test_construct_destroy_construct(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  first = results(emu)
  del emu
  gc.collect()

  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  assert results(emu)[0] == first[0]


def test_two_instances_are_independent(test_rom):
  config.set_console_type(0)
  ds = pydsi.pydsi(test_rom)
  config.set_console_type(1)
  dsi = pydsi.pydsi(test_rom)
  tick_until_running(ds)
  tick_until_running(dsi)

  ds.memory.write_u32(False, 0x02100010, 100)
  dsi.memory.write_u32(False, 0x02100010, 200)
  ds.tick(30)
  dsi.tick(2)

  assert ds.memory.read_u32(False, 0x02100014) == 101
  assert dsi.memory.read_u32(False, 0x02100014) == 201
  assert results(ds)[1] == 0
  assert results(dsi)[1] & 0x80000000
  assert results(ds)[2] > results(dsi)[2]

  ds_top, _ = ds.get_frame()
  dsi_top, _ = dsi.get_frame()
  assert not np.array_equal(ds_top[150], dsi_top[150])


def test_config_changes_dont_affect_existing_instances(test_rom):
  config.set_console_type(1)
  emu = pydsi.pydsi(test_rom)
  config.set_console_type(0)
  config.set_emulate_audio(0)
  tick_until_running(emu)
  assert emu.get_console_type() == 1
  assert emu.get_audio_buffer_number() > 0


def test_frame_view_survives_owner(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  top, _ = emu.get_frame()
  expected = top.copy()
  del emu
  gc.collect()
  # The array keeps the emulator alive
  assert np.array_equal(top, expected)
