import warnings

import cdsi

# PyNDS (NooDS) settings that melonDS has no equivalent for, with their neutral values.
# They are still stored (so getters round-trip) but have no effect.
IGNORED_SETTINGS = {
    "set_rom_in_ram": 0,
    "set_fps_limiter": 0,
    "set_frame_skip": 0,
    "set_threaded_2d": 0,
    "set_high_res_3d": 0,
    "set_screen_ghost": 0,
    "set_saves_folder": 0,
    "set_states_folder": 0,
    "set_cheats_folder": 0,
    "set_screen_filter": 0,
    "set_arm7_hle": 0,
    "set_gba_bios_path": "",
    "set_base_path": "",
}

_warned = set()


class _Config:
    """pydsi's settings: cdsi.config, plus a one-time warning when a PyNDS-only setting is set
    to something other than its neutral value."""

    def __getattr__(self, name):
        attr = getattr(cdsi.config, name)
        if name not in IGNORED_SETTINGS:
            return attr

        def setter(value):
            if value != IGNORED_SETTINGS[name] and name not in _warned:
                _warned.add(name)
                warnings.warn(f"pydsi: config.{name}() has no effect with melonDS (kept for PyNDS compatibility)", stacklevel=2)
            attr(value)
        return setter

    def __dir__(self):
        return dir(cdsi.config)


config = _Config()

__all__ = ["config", "IGNORED_SETTINGS"]
