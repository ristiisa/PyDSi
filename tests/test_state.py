import numpy as np
import pytest

import pydsi
from pydsi import config
from conftest import results, tick_until_running


def snapshot(emu):
  top, bot = emu.get_frame()
  return top.copy(), bot.copy(), results(emu)


@pytest.mark.parametrize("console_type", [0, 1])
def test_savestate_round_trip(test_rom, tmp_path, console_type):
  config.set_console_type(console_type)
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  emu.tick(10)

  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.tick(60)
  top_a, bot_a, res_a = snapshot(emu)

  emu.load_state(str(state))
  emu.tick(60)
  top_b, bot_b, res_b = snapshot(emu)

  # The test ROM draws a row per frame, so the picture depends on the emulated frame count
  assert np.array_equal(top_a, top_b)
  assert np.array_equal(bot_a, bot_b)
  assert res_a == res_b


def test_savestate_round_trip_dsi_files(test_rom, tmp_path, dsi_files):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.tick(60)
  a = snapshot(emu)
  emu.load_state(str(state))
  emu.tick(60)
  b = snapshot(emu)
  assert np.array_equal(a[0], b[0]) and a[2] == b[2]


def test_savestate_keeps_input(test_rom, tmp_path):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.button_press("a")
  emu.load_state(str(state))
  emu.tick(2)
  # melonDS doesn't store input in savestates; pydsi re-applies the current input after loading
  assert results(emu)[3] & 1 == 0


def test_loading_state_of_other_console_type_fails(test_rom, tmp_path):
  config.set_console_type(1)
  dsi = pydsi.pydsi(test_rom)
  tick_until_running(dsi)
  state = tmp_path / "dsi.mln"
  dsi.save_state(str(state))

  config.set_console_type(0)
  ds = pydsi.pydsi(test_rom)
  tick_until_running(ds)
  before = results(ds)
  with pytest.raises(RuntimeError):
    ds.load_state(str(state))
  # The failed load is rolled back
  assert results(ds) == before
  ds.tick()


def test_load_missing_state(test_rom, tmp_path):
  emu = pydsi.pydsi(test_rom)
  with pytest.raises(FileNotFoundError):
    emu.load_state(str(tmp_path / "missing.mln"))
