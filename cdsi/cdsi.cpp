#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "dsi.hpp"
#include "config.hpp"

namespace nb = nanobind;

NB_MODULE(cdsi, m) {
  m.doc() = "melonDS (DS/DSi emulator) core bindings";

  nb::class_<Dsi>(m, "Dsi")
    .def(nb::init<const std::string&, const std::string&, bool>(), nb::arg("rom_path"), nb::arg("save_path"), nb::arg("is_gba"))
    .def("run_task", &Dsi::runTask, "Run one frame")
    .def("run_until_frame", &Dsi::runUntilFrame, "Run one frame")
    .def("get_frame", &Dsi::getFrame, "Copy the last finished frame into the buffers returned by get_*_frame")
    .def("get_gba_frame", &Dsi::getGbaFrame)
    .def("get_top_nds_frame", &Dsi::getTopNdsFrame, nb::rv_policy::reference_internal, "Top screen, uint8[192, 256, 4] RGBA (a view, updated by get_frame)")
    .def("get_bot_nds_frame", &Dsi::getBotNdsFrame, nb::rv_policy::reference_internal, "Bottom screen, uint8[192, 256, 4] RGBA (a view, updated by get_frame)")
    .def("get_audio_samples", &Dsi::getAudioSamples, "Exactly `count` stereo frames as int16[count, 2]: buffered samples, then silence")
    .def("get_audio_buffer_number", &Dsi::getAudioBufferNumber, "Stereo frames waiting to be read")

    // Save methods
    .def("save_state", &Dsi::saveState)
    .def("load_state", &Dsi::loadState)
    .def("save_game", &Dsi::saveGame)

    // Input methods
    .def("set_touch_input", &Dsi::setTouchInput)
    .def("clear_touch_input", &Dsi::clearTouchInput)
    .def("touch_input", &Dsi::touchInput)
    .def("release_touch_input", &Dsi::releaseTouchInput)
    .def("press_key", &Dsi::pressKey)
    .def("release_key", &Dsi::releaseKey)
    .def("set_key_mask", &Dsi::setKeyMask, "Set all keys at once: melonDS's active-low 12-bit mask")
    .def("get_key_mask", &Dsi::getKeyMask)
    .def("set_lid_closed", &Dsi::setLidClosed)

    // Read memory methods
    .def("read_ram_u8", &Dsi::readRamu8)
    .def("read_ram_u16", &Dsi::readRamu16)
    .def("read_ram_u32", &Dsi::readRamu32)
    .def("read_ram_u64", &Dsi::readRamu64)
    .def("read_ram_i8", &Dsi::readRami8)
    .def("read_ram_i16", &Dsi::readRami16)
    .def("read_ram_i32", &Dsi::readRami32)
    .def("read_ram_i64", &Dsi::readRami64)
    .def("read_ram_f32", &Dsi::readRamf32)
    .def("read_ram_f64", &Dsi::readRamf64)
    .def("read_map", &Dsi::readMap, "Read `size` bytes into a uint8 numpy array")

    // Write memory methods
    .def("write_ram_u8", &Dsi::writeRamu8)
    .def("write_ram_u16", &Dsi::writeRamu16)
    .def("write_ram_u32", &Dsi::writeRamu32)
    .def("write_ram_i8", &Dsi::writeRami8)
    .def("write_ram_i16", &Dsi::writeRami16)
    .def("write_ram_i32", &Dsi::writeRami32)
    .def("write_ram_f32", &Dsi::writeRamf32)
    .def("write_map", &Dsi::writeMap, "Write a uint8 array starting at `address`")

    // pydsi additions
    .def("get_console_type", &Dsi::getConsoleType, "0 = DS, 1 = DSi")
    .def("is_dsi_mode", &Dsi::isDsiMode, "True if the DSi's extended hardware is enabled (a DSi title, not a DS title on a DSi)")
    .def("is_running", &Dsi::isRunning, "False once the emulated console stopped (e.g. powered itself off)")
    .def("get_frame_count", &Dsi::getFrameCount)
    .def_static("install_dsiware", &Dsi::installDsiware, nb::arg("app_path"), nb::arg("tmd_path"), nb::arg("overwrite") = false, "Install a DSiWare title into Config's DSi NAND");

  // PyNDS's class name
  m.attr("Nds") = m.attr("Dsi");

  nb::class_<Config>(m, "config")
    .def_static("set_console_type", &Config::setConsoleType, "Set console type: -1 = auto, 0 = DS, 1 = DSi")
    .def_static("get_console_type", &Config::getConsoleType, "Get console type")

    .def_static("set_direct_boot", &Config::setDirectBoot, "Set direct boot mode")
    .def_static("get_direct_boot", &Config::getDirectBoot, "Get direct boot mode")

    .def_static("set_rom_in_ram", &Config::setRomInRam, "Set ROM in RAM (no effect in melonDS)")
    .def_static("get_rom_in_ram", &Config::getRomInRam, "Get ROM in RAM")

    .def_static("set_fps_limiter", &Config::setFpsLimiter, "Set FPS limiter (no effect: pydsi never limits speed)")
    .def_static("get_fps_limiter", &Config::getFpsLimiter, "Get FPS limiter")

    .def_static("set_frame_skip", &Config::setFrameSkip, "Set frame skip value (no effect in melonDS)")
    .def_static("get_frame_skip", &Config::getFrameSkip, "Get frame skip value")

    .def_static("set_threaded_2d", &Config::setThreaded2D, "Set threaded 2D rendering (no effect in melonDS)")
    .def_static("get_threaded_2d", &Config::getThreaded2D, "Get threaded 2D rendering status")

    .def_static("set_threaded_3d", &Config::setThreaded3D, "Set threaded 3D rendering")
    .def_static("get_threaded_3d", &Config::getThreaded3D, "Get threaded 3D rendering status")

    .def_static("set_high_res_3d", &Config::setHighRes3D, "Set high resolution 3D (no effect: the software renderer can't upscale)")
    .def_static("get_high_res_3d", &Config::getHighRes3D, "Get high resolution 3D status")

    .def_static("set_screen_ghost", &Config::setScreenGhost, "Set screen ghosting (no effect in melonDS)")
    .def_static("get_screen_ghost", &Config::getScreenGhost, "Get screen ghosting status")

    .def_static("set_emulate_audio", &Config::setEmulateAudio, "Set audio emulation")
    .def_static("get_emulate_audio", &Config::getEmulateAudio, "Get audio emulation status")

    .def_static("set_audio_16_bit", &Config::setAudio16Bit, "Set 16-bit audio (0 = 10-bit like the hardware)")
    .def_static("get_audio_16_bit", &Config::getAudio16Bit, "Get 16-bit audio status")

    .def_static("set_audio_sample_rate", &Config::setAudioSampleRate, "Set audio output sample rate in Hz")
    .def_static("get_audio_sample_rate", &Config::getAudioSampleRate, "Get audio output sample rate")

    .def_static("set_saves_folder", &Config::setSavesFolder, "Set saves folder (no effect in melonDS)")
    .def_static("get_saves_folder", &Config::getSavesFolder, "Get saves folder")

    .def_static("set_states_folder", &Config::setStatesFolder, "Set states folder (no effect in melonDS)")
    .def_static("get_states_folder", &Config::getStatesFolder, "Get states folder")

    .def_static("set_cheats_folder", &Config::setCheatsFolder, "Set cheats folder (no effect in melonDS)")
    .def_static("get_cheats_folder", &Config::getCheatsFolder, "Get cheats folder")

    .def_static("set_screen_filter", &Config::setScreenFilter, "Set screen filter (no effect in melonDS)")
    .def_static("get_screen_filter", &Config::getScreenFilter, "Get screen filter")

    .def_static("set_arm7_hle", &Config::setArm7Hle, "Set ARM7 HLE (no effect in melonDS)")
    .def_static("get_arm7_hle", &Config::getArm7Hle, "Get ARM7 HLE status")

    .def_static("set_dsi_mode", &Config::setDsiMode, "Set DSi mode (sets the console type)")
    .def_static("get_dsi_mode", &Config::getDsiMode, "Get DSi mode")

    .def_static("set_jit_enabled", &Config::setJitEnabled, "Use the JIT recompiler (only in builds with PYDSI_ENABLE_JIT=ON)")
    .def_static("get_jit_enabled", &Config::getJitEnabled, "Get JIT status")

    .def_static("set_dsi_dsp_hle", &Config::setDsiDspHle, "Set DSi DSP high-level emulation")
    .def_static("get_dsi_dsp_hle", &Config::getDsiDspHle, "Get DSi DSP HLE status")

    .def_static("set_save_writeback", &Config::setSaveWriteback, "Write the cartridge save to save_path while running (default 0: only write_save_file writes it)")
    .def_static("get_save_writeback", &Config::getSaveWriteback, "Get save writeback status")

    .def_static("set_firmware_writeback", &Config::setFirmwareWriteback, "Write firmware changes back to the configured firmware dump (default 0)")
    .def_static("get_firmware_writeback", &Config::getFirmwareWriteback, "Get firmware writeback status")

    .def_static("set_rtc_use_host_time", &Config::setRtcUseHostTime, "Start the emulated clock at the host's local time (default 0)")
    .def_static("get_rtc_use_host_time", &Config::getRtcUseHostTime, "Get RTC host time status")

    .def_static("set_rtc_epoch", &Config::setRtcEpoch, "Start the emulated clock at this Unix time (UTC); -1 = melonDS's default, 2000-01-01 00:00:00")
    .def_static("get_rtc_epoch", &Config::getRtcEpoch, "Get RTC start time")

    .def_static("set_log_enabled", &Config::setLogEnabled, "Print melonDS's log on stderr")
    .def_static("get_log_enabled", &Config::getLogEnabled, "Get logging status")

    .def_static("set_bios_9_path", &Config::setBios9Path, "Set BIOS 9 path")
    .def_static("get_bios_9_path", &Config::getBios9Path, "Get BIOS 9 path")

    .def_static("set_bios_7_path", &Config::setBios7Path, "Set BIOS 7 path")
    .def_static("get_bios_7_path", &Config::getBios7Path, "Get BIOS 7 path")

    .def_static("set_firmware_path", &Config::setFirmwarePath, "Set firmware path")
    .def_static("get_firmware_path", &Config::getFirmwarePath, "Get firmware path")

    .def_static("set_gba_bios_path", &Config::setGbaBiosPath, "Set GBA BIOS path (no effect in melonDS)")
    .def_static("get_gba_bios_path", &Config::getGbaBiosPath, "Get GBA BIOS path")

    .def_static("set_dsi_bios_9_path", &Config::setDsiBios9Path, "Set DSi BIOS 9 path")
    .def_static("get_dsi_bios_9_path", &Config::getDsiBios9Path, "Get DSi BIOS 9 path")

    .def_static("set_dsi_bios_7_path", &Config::setDsiBios7Path, "Set DSi BIOS 7 path")
    .def_static("get_dsi_bios_7_path", &Config::getDsiBios7Path, "Get DSi BIOS 7 path")

    .def_static("set_dsi_firmware_path", &Config::setDsiFirmwarePath, "Set DSi firmware path")
    .def_static("get_dsi_firmware_path", &Config::getDsiFirmwarePath, "Get DSi firmware path")

    .def_static("set_dsi_nand_path", &Config::setDsiNandPath, "Set DSi NAND path")
    .def_static("get_dsi_nand_path", &Config::getDsiNandPath, "Get DSi NAND path")

    .def_static("set_dsi_sd_path", &Config::setDsiSdPath, "Set DSi SD card image path")
    .def_static("get_dsi_sd_path", &Config::getDsiSdPath, "Get DSi SD card image path")

    .def_static("set_dsi_sd_read_only", &Config::setDsiSdReadOnly, "Set DSi SD card read-only")
    .def_static("get_dsi_sd_read_only", &Config::getDsiSdReadOnly, "Get DSi SD card read-only")

    .def_static("set_sd_image_path", &Config::setSdImagePath, "Set SD image path (same as the DSi SD card path)")
    .def_static("get_sd_image_path", &Config::getSdImagePath, "Get SD image path")

    .def_static("set_base_path", &Config::setBasePath, "Set base path (no effect in melonDS)")
    .def_static("get_base_path", &Config::getBasePath, "Get base path");
}
