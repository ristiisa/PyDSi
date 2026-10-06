import os
import shutil
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))

import romgen  # noqa: E402
import pydsi  # noqa: E402
from pydsi import config  # noqa: E402

# Results block written by the test ROM (see romgen.py)
RESULTS = romgen.RESULTS
MAGIC = romgen.MAGIC

# System files are never part of the repository; tests that need them read their
# locations from these environment variables and are skipped when they're unset.
DS_FILES = {
  "bios9": "PYDSI_BIOS9",
  "bios7": "PYDSI_BIOS7",
  "firmware": "PYDSI_FIRMWARE",
}
DSI_FILES = {
  "dsi_bios9": "PYDSI_DSI_BIOS9",
  "dsi_bios7": "PYDSI_DSI_BIOS7",
  "dsi_firmware": "PYDSI_DSI_FIRMWARE",
  "dsi_nand": "PYDSI_DSI_NAND",
}


def reset_config():
  config.set_console_type(-1)
  config.set_direct_boot(1)
  config.set_threaded_3d(0)
  config.set_emulate_audio(1)
  config.set_audio_16_bit(1)
  config.set_audio_sample_rate(32768)
  config.set_jit_enabled(0)
  config.set_dsi_dsp_hle(0)
  config.set_save_writeback(0)
  config.set_firmware_writeback(0)
  config.set_rtc_use_host_time(0)
  config.set_rtc_epoch(-1)
  config.set_log_enabled(0)
  for setter in (config.set_bios_9_path, config.set_bios_7_path, config.set_firmware_path, config.set_dsi_bios_9_path, config.set_dsi_bios_7_path, config.set_dsi_firmware_path, config.set_dsi_nand_path, config.set_dsi_sd_path):
    setter("")


@pytest.fixture(autouse=True)
def clean_config():
  reset_config()
  yield
  reset_config()


@pytest.fixture(scope="session")
def test_rom(tmp_path_factory):
  return romgen.write_rom(tmp_path_factory.mktemp("rom") / "pydsi_test.nds")


@pytest.fixture(scope="session")
def retail_rom(tmp_path_factory):
  return romgen.write_rom(tmp_path_factory.mktemp("rom") / "pydsi_retail.nds", homebrew=False)


@pytest.fixture(scope="session")
def dsiware_rom(tmp_path_factory):
  return romgen.write_rom(tmp_path_factory.mktemp("rom") / "pydsi_dsiware.nds", homebrew=False, dsiware=True)


def env_files(names):
  files = {key: os.environ.get(var) for key, var in names.items()}
  missing = [names[key] for key, path in files.items() if not path]
  if missing:
    pytest.skip("set " + ", ".join(missing) + " to run this test")
  for path in files.values():
    if not os.path.isfile(path):
      pytest.fail(f"{path} does not exist")
  return files


@pytest.fixture
def ds_files():
  files = env_files(DS_FILES)
  config.set_bios_9_path(files["bios9"])
  config.set_bios_7_path(files["bios7"])
  config.set_firmware_path(files["firmware"])
  return files


@pytest.fixture
def dsi_files(tmp_path):
  files = env_files(DSI_FILES)
  # pydsi writes to the NAND (touchscreen calibration, like melonDS's frontend) and to the
  # firmware; work on copies so the originals stay untouched
  nand = tmp_path / "nand.bin"
  firmware = tmp_path / "dsi_firmware.bin"
  shutil.copyfile(files["dsi_nand"], nand)
  shutil.copyfile(files["dsi_firmware"], firmware)
  config.set_dsi_bios_9_path(files["dsi_bios9"])
  config.set_dsi_bios_7_path(files["dsi_bios7"])
  config.set_dsi_firmware_path(str(firmware))
  config.set_dsi_nand_path(str(nand))
  return files


def results(emu):
  """Reads the test ROM's results block: (magic, scfg_ext9, vblanks, keyinput, mailbox, mailbox + 1)"""
  return tuple(emu.memory.read_u32(False, RESULTS + 4 * i) for i in range(6))


def tick_until_running(emu, max_frames=120):
  """Ticks until the test ROM is in its main loop (magic written, two VBlanks seen)"""
  for _ in range(max_frames):
    emu.tick()
    magic, _, vblanks, _, _, _ = results(emu)
    if magic == MAGIC and vblanks >= 2:
      return
  pytest.fail("test ROM didn't start")
