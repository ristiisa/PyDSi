import os

import pytest

import cdsi
import pydsi
from pydsi import config
from conftest import tick_until_running


def test_gba_unsupported(tmp_path):
  rom = tmp_path / "game.gba"
  rom.write_bytes(b"\0" * 0x1000)
  with pytest.raises(RuntimeError, match="GBA"):
    pydsi.pydsi(str(rom))


def test_missing_rom(tmp_path):
  with pytest.raises(FileNotFoundError):
    pydsi.pydsi(str(tmp_path / "missing.nds"))


def test_invalid_rom(tmp_path):
  rom = tmp_path / "bad.nds"
  rom.write_bytes(b"\xff" * 0x2000)
  with pytest.raises(RuntimeError, match="valid"):
    pydsi.pydsi(str(rom))


def test_missing_bios_file(test_rom, tmp_path):
  config.set_bios_9_path(str(tmp_path / "missing9.bin"))
  config.set_bios_7_path(str(tmp_path / "missing7.bin"))
  with pytest.raises(RuntimeError, match="BIOS"):
    pydsi.pydsi(test_rom)


def test_only_one_dsi_bios(test_rom, tmp_path):
  bios = tmp_path / "bios9i.bin"
  bios.write_bytes(b"\0" * 0x10000)
  config.set_dsi_bios_9_path(str(bios))
  with pytest.raises(RuntimeError, match="DSi ARM9 and ARM7 BIOS"):
    pydsi.pydsi(test_rom)


def test_wrong_bios_size(test_rom, tmp_path):
  for name in ("b9", "b7"):
    (tmp_path / name).write_bytes(b"\0" * 10)
  config.set_bios_9_path(str(tmp_path / "b9"))
  config.set_bios_7_path(str(tmp_path / "b7"))
  with pytest.raises(RuntimeError, match="size"):
    pydsi.pydsi(test_rom)


def test_dsi_requires_firmware_with_bios(test_rom, tmp_path):
  for name in ("b9i", "b7i"):
    (tmp_path / name).write_bytes(b"\0" * 0x10000)
  config.set_dsi_bios_9_path(str(tmp_path / "b9i"))
  config.set_dsi_bios_7_path(str(tmp_path / "b7i"))
  with pytest.raises(RuntimeError, match="firmware"):
    pydsi.pydsi(test_rom)


def test_nand_requires_dsi_bios(test_rom, tmp_path):
  nand = tmp_path / "nand.bin"
  nand.write_bytes(b"\0" * 0x1000)
  config.set_dsi_nand_path(str(nand))
  with pytest.raises(RuntimeError, match="BIOS"):
    pydsi.pydsi(test_rom)


def test_no_rom_needs_system_files():
  with pytest.raises(RuntimeError, match="firmware"):
    pydsi.pydsi(None)
  config.set_console_type(1)
  with pytest.raises(RuntimeError, match="NAND"):
    pydsi.pydsi(None)


def test_dsiware_needs_nand(dsiware_rom):
  config.set_console_type(1)
  with pytest.raises(RuntimeError, match="NAND"):
    pydsi.pydsi(dsiware_rom)


def test_dsiware_rom_in_ds_mode_runs_from_cart(dsiware_rom):
  # In DS mode a DSiWare header is just a cart; melonDS boots it in DS mode
  config.set_console_type(0)
  emu = pydsi.pydsi(dsiware_rom)
  tick_until_running(emu)


def test_jit_flag(test_rom):
  config.set_jit_enabled(1)
  try:
    emu = pydsi.pydsi(test_rom)
  except RuntimeError as e:
    assert "JIT" in str(e)
  else:
    tick_until_running(emu)


def test_install_dsiware_needs_config(dsiware_rom, tmp_path):
  with pytest.raises(RuntimeError, match="NAND"):
    cdsi.Dsi.install_dsiware(dsiware_rom, str(tmp_path / "x.tmd"))


def test_save_round_trip(retail_rom, tmp_path):
  # melonDS gives unknown non-homebrew ROMs an 8 KB EEPROM
  save = tmp_path / "game.sav"
  data = bytes(range(256)) * 32
  save.write_bytes(data)

  emu = pydsi.pydsi(retail_rom, str(save))
  tick_until_running(emu)
  out = tmp_path / "out.sav"
  emu.write_save_file(str(out), True)
  assert out.read_bytes() == data

  # Nothing written by the game, so a non-forced save writes nothing
  out2 = tmp_path / "out2.sav"
  emu.write_save_file(str(out2), False)
  assert not out2.exists()


@pytest.mark.parametrize("writeback", [1, 0])
def test_save_writeback(retail_rom, tmp_path, writeback):
  # Loading a savestate makes melonDS push the cart's save memory through Platform::WriteNDSSave,
  # the same path a game's save writes take; pydsi writes it to disk after ~60 quiet frames.
  config.set_save_writeback(writeback)
  save = tmp_path / "game.sav"
  emu = pydsi.pydsi(retail_rom, str(save))
  tick_until_running(emu)
  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.load_state(str(state))
  emu.tick(30)
  assert not save.exists()
  emu.tick(40)
  assert save.exists() == bool(writeback)
  if writeback:
    assert save.stat().st_size == 8192


def test_save_flushed_on_destruction(retail_rom, tmp_path):
  config.set_save_writeback(1)
  save = tmp_path / "game.sav"
  emu = pydsi.pydsi(retail_rom, str(save))
  tick_until_running(emu)
  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.load_state(str(state))
  del emu
  import gc
  gc.collect()
  assert save.stat().st_size == 8192


def test_writeback_off_by_default(retail_rom, tmp_path):
  assert config.get_save_writeback() == 0 and config.get_firmware_writeback() == 0
  save = tmp_path / "game.sav"
  emu = pydsi.pydsi(retail_rom, str(save))
  tick_until_running(emu)
  state = tmp_path / "state.mln"
  emu.save_state(str(state))
  emu.load_state(str(state))
  emu.tick(70)
  del emu
  import gc
  gc.collect()
  assert not save.exists()


def test_rtc_settings(test_rom):
  config.set_rtc_epoch(1700000000)  # 2023-11-14
  pydsi.pydsi(test_rom).tick()
  config.set_rtc_epoch(-1)
  config.set_rtc_use_host_time(1)
  pydsi.pydsi(test_rom).tick()
  config.set_rtc_use_host_time(0)
  config.set_rtc_epoch(900000000)  # 1998: before the DS clock's range
  with pytest.raises(RuntimeError, match="2000-2099"):
    pydsi.pydsi(test_rom)


def test_ignored_settings_warn_once():
  with pytest.warns(UserWarning, match="set_high_res_3d"):
    config.set_high_res_3d(1)
  assert config.get_high_res_3d() == 1
  import warnings
  with warnings.catch_warnings():
    warnings.simplefilter("error")
    config.set_high_res_3d(1)  # second time: no warning
    config.set_frame_skip(0)  # neutral value: no warning
  config.set_high_res_3d(0)


def test_empty_save_is_created_with_cart_size(retail_rom, tmp_path):
  save = tmp_path / "new.sav"
  emu = pydsi.pydsi(retail_rom, str(save))
  emu.tick()
  emu.write_save_file("", True)
  assert save.stat().st_size == 8192
