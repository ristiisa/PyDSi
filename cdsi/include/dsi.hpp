#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <nanobind/ndarray.h>

#include "config.hpp"
#include "platform_impl.hpp"

namespace melonDS {
class NDS;
}

namespace nb = nanobind;

// melonDS emulator instance, the equivalent of PyNDS's Nds class.
// The console (DS or DSi) and every setting are taken from Config when it is constructed.
class Dsi {
  public:
    // romPath may be empty to boot the system menu (needs real BIOS/firmware, plus a NAND on DSi).
    // isGba must be false: melonDS has no GBA mode.
    Dsi(std::string romPath, std::string savePath, bool isGba);
    ~Dsi();

    Dsi(const Dsi&) = delete;
    Dsi& operator=(const Dsi&) = delete;

    // Both run one frame: melonDS's smallest public unit of emulation
    void runTask();
    void runUntilFrame();
    // Copies the finished frame into the buffers returned by get*Frame (converted to RGBA)
    bool getFrame();

    // Grab emulator frame methods: uint8[192, 256, 4] RGBA views of the last getFrame() copy
    nb::ndarray<nb::numpy, uint8_t> getGbaFrame();
    nb::ndarray<nb::numpy, uint8_t> getTopNdsFrame();
    nb::ndarray<nb::numpy, uint8_t> getBotNdsFrame();

    // Audio methods: exactly `count` stereo frames as int16[count, 2], padded with silence
    nb::ndarray<nb::numpy, int16_t> getAudioSamples(int count);
    uint32_t getAudioBufferNumber();

    // Savestate methods
    void saveState(std::string path);
    void loadState(std::string path);

    // Save game methods
    void saveGame(const std::string& path, bool alwaysSave);

    // Touch input methods
    void setTouchInput(int x, int y);
    void clearTouchInput();
    void touchInput();
    void releaseTouchInput();

    // Joystick input methods
    void pressKey(int key);
    void releaseKey(int key);
    void setKeyMask(uint32_t mask);
    uint32_t getKeyMask();
    void setLidClosed(bool closed);

    // Read ram methods
    uint8_t readRamu8(bool arm7, uint32_t address, bool tcm);
    uint16_t readRamu16(bool arm7, uint32_t address, bool tcm);
    uint32_t readRamu32(bool arm7, uint32_t address, bool tcm);
    uint64_t readRamu64(bool arm7, uint32_t address, bool tcm);

    int8_t readRami8(bool arm7, uint32_t address, bool tcm);
    int16_t readRami16(bool arm7, uint32_t address, bool tcm);
    int32_t readRami32(bool arm7, uint32_t address, bool tcm);
    int64_t readRami64(bool arm7, uint32_t address, bool tcm);

    float readRamf32(bool arm7, uint32_t address, bool tcm);
    double readRamf64(bool arm7, uint32_t address, bool tcm);

    nb::ndarray<nb::numpy, uint8_t> readMap(bool arm7, uint32_t address, uint32_t size, bool tcm);

    // Write ram methods
    void writeRamu8(bool arm7, uint32_t address, uint8_t value, bool tcm);
    void writeRamu16(bool arm7, uint32_t address, uint16_t value, bool tcm);
    void writeRamu32(bool arm7, uint32_t address, uint32_t value, bool tcm);

    void writeRami8(bool arm7, uint32_t address, int8_t value, bool tcm);
    void writeRami16(bool arm7, uint32_t address, int16_t value, bool tcm);
    void writeRami32(bool arm7, uint32_t address, int32_t value, bool tcm);

    void writeRamf32(bool arm7, uint32_t address, float value, bool tcm);

    void writeMap(bool arm7, uint32_t address, nb::ndarray<const uint8_t, nb::ndim<1>, nb::c_contig> data, bool tcm);

    // pydsi additions
    int getConsoleType();
    bool isDsiMode();
    bool isRunning();
    uint32_t getFrameCount();

    // Installs a DSiWare title (decrypted .app/.nds plus its .tmd) into the NAND configured in
    // Config. Needs Config's DSi ARM7 BIOS (for the NAND key). No Dsi object using the same NAND
    // may exist while this runs.
    static void installDsiware(const std::string& appPath, const std::string& tmdPath, bool overwrite);

  private:
    static const uint16_t NDS_WIDTH = 256;
    static const uint16_t NDS_HEIGHT = 192;

    template<typename T> T readRam(bool arm7, uint32_t address, bool tcm);
    template<typename T> void writeRam(bool arm7, uint32_t address, T value, bool tcm);
    uint8_t* tcmPointer(uint32_t address, uint32_t size);
    void afterFrame();
    void applyInput();

    std::unique_ptr<InstanceContext> m_ctx;
    std::unique_ptr<melonDS::NDS> m_nds;
    Config::Values m_cfg;
    int m_consoleType = 0;
    bool m_isGba = false;

    uint32_t m_framebuffer[NDS_WIDTH * NDS_HEIGHT * 2] = {};

    uint32_t m_keyMask = 0xFFF;
    int m_touchX = 0;
    int m_touchY = 0;
    bool m_touching = false;
};
