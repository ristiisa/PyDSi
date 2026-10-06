import numpy as np
import pytest

import pydsi
from pydsi import config
from pydsi.button import KEY_MAP
from conftest import results, tick_until_running


def keyinput(emu):
  return results(emu)[3]


def test_keys(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  assert keyinput(emu) == 0x3FF

  for key, bit in KEY_MAP.items():
    if bit >= 10:
      continue  # X and Y aren't in KEYINPUT (they're read by the ARM7)
    emu.button_press(key)
    emu.tick(2)
    assert keyinput(emu) == 0x3FF & ~(1 << bit), key
    emu.button_release(key)
    emu.tick(2)
    assert keyinput(emu) == 0x3FF


def test_key_mask(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  emu._nds.set_key_mask(0xFFF & ~0b101)
  emu.tick(2)
  assert keyinput(emu) == 0x3FF & ~0b101
  assert emu._nds.get_key_mask() == 0xFFF & ~0b101


def test_touch_calls(test_rom):
  emu = pydsi.pydsi(test_rom)
  tick_until_running(emu)
  emu.set_touch(100, 50)
  emu.tick()
  emu.button.clear_touch()
  emu.release_touch()
  emu.tick()


def test_audio_samples(test_rom):
  emu = pydsi.pydsi(test_rom)
  emu.tick(5)
  available = emu.get_audio_buffer_number()
  assert available > 0
  samples = emu.get_audio(100)
  assert samples.dtype == np.int16
  assert samples.shape == (100, 2)
  assert emu.get_audio_buffer_number() == available - 100


def test_audio_padded_to_count(test_rom):
  # PyNDS contract: always exactly `count` rows; rows past what was buffered are silence
  emu = pydsi.pydsi(test_rom)
  emu.tick(1)
  emu.get_audio(4096)
  assert emu.audio_buffered() == 0
  emu.tick(1)
  real = emu.audio_buffered()
  samples = emu.get_audio(real + 300)
  assert samples.shape == (real + 300, 2)
  assert not samples[real:].any()
  assert emu.audio_buffered() == 0


def test_audio_disabled(test_rom):
  config.set_emulate_audio(0)
  emu = pydsi.pydsi(test_rom)
  emu.tick(5)
  assert emu.get_audio_buffer_number() == 0
  samples = emu.get_audio(100)
  assert samples.shape == (100, 2)
  assert not samples.any()


def test_audio_rate(test_rom):
  config.set_audio_sample_rate(48000)
  emu = pydsi.pydsi(test_rom)
  emu.get_audio(4096)
  emu.tick(1)
  # One frame at 48 kHz is about 800 samples (59.83 fps)
  assert 790 <= emu.get_audio_buffer_number() <= 815
