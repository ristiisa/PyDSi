from .pydsi import PyDSi
from .config import config

# PyNDS-style lowercase name: pydsi.pydsi(rom_path)
pydsi = PyDSi

__all__ = ["PyDSi", "pydsi", "config"]
