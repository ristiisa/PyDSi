import time
import threading

import sounddevice as sd
import numpy as np

from .config import config


class Audio:
    def __init__(self, pydsi) -> None:
        self._pynds = pydsi

        self.running = False

    def start(self) -> None:
        self.stream = sd.OutputStream(samplerate=config.get_audio_sample_rate(), channels=2, dtype='int16', blocksize=0)
        self.running = True

        self.audio_thread = threading.Thread(target=self.play_audio)
        self.audio_thread.start()

    def close(self) -> None:
        if (self.running):
            self.running = False
            self.audio_thread.join()

    def play_audio(self):
        self.stream.start()

        while (self.running):
            samples = self._pynds.get_audio(699)
            data_to_write = np.ascontiguousarray(samples)
            self.stream.write(data_to_write)

            time.sleep(0.001)

        self.stream.stop()
        self.stream.close()
