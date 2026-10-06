#pragma once

#include <cstdint>
#include <string>

// Process-wide settings, mirroring PyNDS's static Config class.
// A Dsi object copies these when it is constructed; changing them afterwards
// does not affect emulators that already exist.
//
// Settings kept only for PyNDS API compatibility (melonDS has no equivalent, they are
// stored and returned but otherwise ignored): romInRam, fpsLimiter, frameSkip, threaded2D,
// highRes3D, screenGhost, savesFolder, statesFolder, cheatsFolder, screenFilter, arm7Hle,
// gbaBiosPath, basePath.
class Config {
  public:
    // Console type: -1 = auto (DSi if any DSi system file is configured, else DS), 0 = DS, 1 = DSi
    static void setConsoleType(int consoleType);
    static int getConsoleType();

    // PyNDS name for the console type: 0 = DS, 1 = DSi (writes consoleType)
    static void setDsiMode(int dsiMode);
    static int getDsiMode();

    // Direct Boot
    static void setDirectBoot(int directBoot);
    static int getDirectBoot();

    // ROM in RAM
    static void setRomInRam(int romInRam);
    static int getRomInRam();

    // FPS Limiter
    static void setFpsLimiter(int fpsLimiter);
    static int getFpsLimiter();

    // Frame Skip
    static void setFrameSkip(int frameSkip);
    static int getFrameSkip();

    // Threaded 2D
    static void setThreaded2D(int threaded2D);
    static int getThreaded2D();

    // Threaded 3D (software renderer)
    static void setThreaded3D(int threaded3D);
    static int getThreaded3D();

    // High Resolution 3D
    static void setHighRes3D(int highRes3D);
    static int getHighRes3D();

    // Screen Ghost
    static void setScreenGhost(int screenGhost);
    static int getScreenGhost();

    // Emulate Audio: 0 = samples are discarded every frame and getAudioSamples returns nothing
    static void setEmulateAudio(int emulateAudio);
    static int getEmulateAudio();

    // Audio 16 Bit: 0 = degrade output to 10 bits like real hardware, 1 = full 16 bit
    static void setAudio16Bit(int audio16Bit);
    static int getAudio16Bit();

    // Output sample rate in Hz (melonDS resamples to this)
    static void setAudioSampleRate(int audioSampleRate);
    static int getAudioSampleRate();

    // Saves Folder
    static void setSavesFolder(int savesFolder);
    static int getSavesFolder();

    // States Folder
    static void setStatesFolder(int statesFolder);
    static int getStatesFolder();

    // Cheats Folder
    static void setCheatsFolder(int cheatsFolder);
    static int getCheatsFolder();

    // Screen Filter
    static void setScreenFilter(int screenFilter);
    static int getScreenFilter();

    // ARM7 HLE
    static void setArm7Hle(int arm7Hle);
    static int getArm7Hle();

    // JIT recompiler (only if the module was built with it)
    static void setJitEnabled(int jitEnabled);
    static int getJitEnabled();

    // DSi DSP high-level emulation
    static void setDsiDspHle(int dsiDspHle);
    static int getDsiDspHle();

    // Write the cartridge save to save_path as the game changes it (off: only write_save_file writes it, like PyNDS)
    static void setSaveWriteback(int saveWriteback);
    static int getSaveWriteback();

    // Write firmware changes made by the emulated system back to the configured firmware dump
    static void setFirmwareWriteback(int firmwareWriteback);
    static int getFirmwareWriteback();

    // RTC start time. useHostTime=1: the host's local time. Otherwise rtcEpoch, in Unix seconds
    // (UTC fields), or melonDS's default of 2000-01-01 00:00:00 if rtcEpoch is -1.
    // The DS RTC only stores years 2000-2099.
    static void setRtcUseHostTime(int rtcUseHostTime);
    static int getRtcUseHostTime();

    static void setRtcEpoch(int64_t rtcEpoch);
    static int64_t getRtcEpoch();

    // melonDS log output on stderr (process-wide, takes effect immediately)
    static void setLogEnabled(int logEnabled);
    static int getLogEnabled();

    // DS BIOS/firmware paths (empty = melonDS's FreeBIOS and generated firmware)
    static void setBios9Path(const std::string& bios9Path);
    static std::string getBios9Path();

    static void setBios7Path(const std::string& bios7Path);
    static std::string getBios7Path();

    static void setFirmwarePath(const std::string& firmwarePath);
    static std::string getFirmwarePath();

    static void setGbaBiosPath(const std::string& gbaBiosPath);
    static std::string getGbaBiosPath();

    // DSi system files (empty = FreeBIOS / generated firmware / no NAND)
    static void setDsiBios9Path(const std::string& dsiBios9Path);
    static std::string getDsiBios9Path();

    static void setDsiBios7Path(const std::string& dsiBios7Path);
    static std::string getDsiBios7Path();

    static void setDsiFirmwarePath(const std::string& dsiFirmwarePath);
    static std::string getDsiFirmwarePath();

    static void setDsiNandPath(const std::string& dsiNandPath);
    static std::string getDsiNandPath();

    // DSi SD card image (PyNDS's sdImagePath is the same setting)
    static void setDsiSdPath(const std::string& dsiSdPath);
    static std::string getDsiSdPath();

    static void setDsiSdReadOnly(int dsiSdReadOnly);
    static int getDsiSdReadOnly();

    static void setSdImagePath(const std::string& sdImagePath);
    static std::string getSdImagePath();

    static void setBasePath(const std::string& basePath);
    static std::string getBasePath();

    struct Values {
      int consoleType = -1;
      int directBoot = 1;
      int romInRam = 0;
      int fpsLimiter = 0;
      int frameSkip = 0;
      int threaded2D = 0;
      int threaded3D = 0;
      int highRes3D = 0;
      int screenGhost = 0;
      int emulateAudio = 1;
      int audio16Bit = 1;
      int audioSampleRate = 32768;
      int savesFolder = 0;
      int statesFolder = 0;
      int cheatsFolder = 0;
      int screenFilter = 0;
      int arm7Hle = 0;
      int jitEnabled = 0;
      int dsiDspHle = 0;
      int saveWriteback = 0;
      int firmwareWriteback = 0;
      int rtcUseHostTime = 0;
      int64_t rtcEpoch = -1;
      int logEnabled = 0;
      std::string bios9Path;
      std::string bios7Path;
      std::string firmwarePath;
      std::string gbaBiosPath;
      std::string dsiBios9Path;
      std::string dsiBios7Path;
      std::string dsiFirmwarePath;
      std::string dsiNandPath;
      std::string dsiSdPath;
      int dsiSdReadOnly = 0;
      std::string basePath;
    };

    // Copy of the current values, taken by Dsi's constructor
    static Values snapshot();

  private:
    static Values s_values;
};
