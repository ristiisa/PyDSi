import os
from typing import Optional, Tuple, Union
import numpy as np

import cdsi
from .memory import Memory
from .button import Button, KEY_MAP
from .config import config


class PyDSi:
    def __init__(self, path: Optional[str], save_path: Optional[str] = None, auto_detect: bool = True, is_gba: bool = False) -> None:
        path = path or ""
        save_path = save_path or ""
        if (auto_detect):
            is_gba = path.endswith(".gba")

        self.is_gba = is_gba
        self.save_path = save_path

        if (path):
            self.check_file_exist(path)
        self._nds = cdsi.Dsi(path, save_path, is_gba)

        self.button = Button(self._nds)
        self.memory = Memory(self._nds)
        self._window = None
        self._audio = None

    @property
    def window(self):
        if (self._window is None):
            from .window import Window
            self._window = Window(self)
        return self._window

    @property
    def audio(self):
        if (self._audio is None):
            from .audio import Audio
            self._audio = Audio(self)
        return self._audio

    def get_is_gba(self) -> bool:
        return self.is_gba

    def is_dsi_mode(self) -> bool:
        return self._nds.is_dsi_mode()

    def get_console_type(self) -> int:
        return self._nds.get_console_type()

    def tick(self, count: int = 1) -> None:
        for i in range(count):
            self._nds.run_until_frame()
            self._nds.get_frame()

    def get_frame(self) -> Union[Tuple[np.ndarray, np.ndarray], np.ndarray]:
        top_frame = self._nds.get_top_nds_frame()
        bot_frame = self._nds.get_bot_nds_frame()
        return (top_frame, bot_frame)

    def get_frame_shape(self) -> Tuple[int, int, int]:
        # melonDS's software renderer is always native resolution
        return (192, 256, 4)

    def get_audio(self, count: int = 699) -> np.ndarray:
        return self._nds.get_audio_samples(count)

    def get_audio_buffer_number(self) -> int:
        return self._nds.get_audio_buffer_number()

    def audio_buffered(self) -> int:
        # How many of the next get_audio() rows are real samples rather than padding
        return self._nds.get_audio_buffer_number()

    def button_press(self, key: str) -> None:
        self.button.press_key(key)

    def button_release(self, key: str) -> None:
        self.button.release_key(key)

    def set_touch(self, x: int, y: int) -> None:
        self.button.set_touch(x, y)
        self.button.touch()

    def release_touch(self) -> None:
        self.button.release_touch()

    def open_window(self, width: int = 800, height: int = 800) -> None:
        if (self.window.running):
            self.window.close()

        self.window.init(width, height)

    def close_window(self) -> None:
        if (self._window is not None):
            self._window.close()

    def open_audio(self) -> None:
        if (self.audio.running):
            self.audio.close()

        self.audio.start()

    def close_audio(self) -> None:
        if (self._audio is not None):
            self._audio.close()

    def render(self) -> None:
        if (self._window is not None and self._window.running):
            self._window.render()

    def save_state_to_file(self, path: str) -> None:
        self._nds.save_state(path)

    def load_state_from_file(self, path: str) -> None:
        self.check_file_exist(path)
        self._nds.load_state(path)

    def save_state(self, path: str) -> None:
        self.save_state_to_file(path)

    def load_state(self, path: str) -> None:
        self.load_state_from_file(path)

    def set_save_path(self, path: str):
        self.save_path = path

    def write_save_file(self, path: str = "", always_save: bool = True) -> None:
        path = self.save_path if path == "" else path
        self._nds.save_game(path, always_save)

    @staticmethod
    def install_dsiware(app_path: str, tmd_path: str, overwrite: bool = False) -> None:
        cdsi.Dsi.install_dsiware(app_path, tmd_path, overwrite)

    @staticmethod
    def check_file_exist(path: str) -> None:
        if not os.path.isfile(path):
            raise FileNotFoundError(f"File '{path}' does not exist.")


__all__ = ["PyDSi", "KEY_MAP"]
