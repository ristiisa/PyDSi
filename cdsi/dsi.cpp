#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include "Args.h"
#include "DSi.h"
#include "DSi_I2C.h"
#include "DSi_NAND.h"
#include "DSi_TMD.h"
#include "FATStorage.h"
#include "FreeBIOS.h"
#include "NDS.h"
#include "NDSCart.h"
#include "NDS_Header.h"
#include "Platform.h"
#include "SPI.h"
#include "SPI_Firmware.h"
#include "Savestate.h"

#include "dsi.hpp"

using namespace melonDS;

namespace {

// DSi launcher "auto-load" record, read by the DSi menu at boot from main RAM + 0x300.
// Layout and boot flags from melonDS DS (src/libretro/console/dsi.hpp), originally from BizHawk.
struct DSiAutoLoad {
  u8 ID[4]; // "TLNC"
  u8 Unknown1; // usually 01h
  u8 Length; // starting from PrevTitleID
  u16 CRC16; // covering Length bytes
  u8 PrevTitleID[8];
  u8 NewTitleID[8];
  u32 Flags; // bit 0: valid, bits 1-3: boot type (3 = DSiWare)
  u32 Unused1;
  u8 Unused2[0xE0];
};
static_assert(sizeof(DSiAutoLoad) == 0x100, "DSiAutoLoad must be 256 bytes");

[[noreturn]] void fail(const std::string& message) {
  throw std::runtime_error(message);
}

std::vector<u8> readFileOrFail(const std::string& path, const char* what) {
  std::vector<u8> data;
  if(!platform::readFile(path, data)) {
    fail(std::string(what) + " not found or not readable: " + path);
  }
  return data;
}

template<size_t N>
std::unique_ptr<std::array<u8, N>> loadBios(const std::string& path, const char* what) {
  std::vector<u8> data = readFileOrFail(path, what);
  if(data.size() != N) {
    fail(std::string(what) + " has the wrong size (expected " + std::to_string(N) + " bytes): " + path);
  }
  auto bios = std::make_unique<std::array<u8, N>>();
  std::memcpy(bios->data(), data.data(), N);
  return bios;
}

Firmware loadFirmware(const std::string& path, const char* what, std::initializer_list<size_t> sizes) {
  std::vector<u8> data = readFileOrFail(path, what);
  bool sizeOk = false;
  for(size_t size : sizes) {
    sizeOk = sizeOk || data.size() == size;
  }
  if(!sizeOk) {
    fail(std::string(what) + " is not a valid firmware dump: " + path);
  }
  Firmware firmware(data.data(), (u32)data.size());
  if(!firmware.Buffer()) {
    fail(std::string("Couldn't load ") + what + ": " + path);
  }
  return firmware;
}

// Opens the DSi NAND read/write. Like melonDS's Qt frontend (EmuInstance::loadNAND), this
// rewrites the touchscreen calibration in the NAND's user settings so that touch input maps 1:1.
DSi_NAND::NANDImage loadNand(const std::string& path, const std::array<u8, DSiBIOSSize>& arm7ibios) {
  Platform::FileHandle* file = Platform::OpenFile(path, Platform::FileMode::ReadWriteExisting);
  if(!file) {
    fail("DSi NAND not found or not writable: " + path);
  }

  // Takes ownership of the file
  DSi_NAND::NANDImage nand(file, &arm7ibios[0x8308]);
  if(!nand) {
    fail("Not a valid DSi NAND image (no nocash footer): " + path);
  }

  // Scoped: only one FAT volume can be mounted at a time, process-wide
  {
    DSi_NAND::NANDMount mount(nand);
    if(!mount) {
      fail("Couldn't mount the DSi NAND (wrong DSi ARM7 BIOS for this NAND?): " + path);
    }

    DSi_NAND::DSiFirmwareSystemSettings settings {};
    if(!mount.ReadUserData(settings)) {
      fail("Couldn't read the user settings from the DSi NAND: " + path);
    }

    settings.TouchCalibrationADC1 = {0, 0};
    settings.TouchCalibrationPixel1 = {0, 0};
    settings.TouchCalibrationADC2 = {255 << 4, 191 << 4};
    settings.TouchCalibrationPixel2 = {255, 191};
    settings.UpdateHash();

    if(!mount.ApplyUserData(settings)) {
      fail("Couldn't write the user settings to the DSi NAND: " + path);
    }
  }

  return nand;
}

bool titleInstalled(DSi_NAND::NANDImage& nand, const NDSHeader& header) {
  DSi_NAND::NANDMount mount(nand);
  return mount && mount.TitleExists(header.DSiTitleIDHigh, header.DSiTitleIDLow);
}

// Same as melonDS DS's SetUpDSiWareDirectBoot: ask the DSi menu to launch a NAND title
void setupDsiwareBoot(DSi& dsi, const NDSHeader& header) {
  dsi.I2C.GetBPTWL()->SetBootFlag(true);

  DSiAutoLoad autoLoad {};
  std::memcpy(autoLoad.ID, "TLNC", sizeof(autoLoad.ID));
  autoLoad.Unknown1 = 0x01;
  autoLoad.Length = 0x18;
  std::memcpy(autoLoad.NewTitleID, &header.DSiTitleIDLow, sizeof(autoLoad.NewTitleID));
  autoLoad.Flags |= (0x03 << 1) | 0x01 | (1 << 4);
  autoLoad.CRC16 = CRC16(autoLoad.PrevTitleID, autoLoad.Length, 0xFFFF);
  std::memcpy(&dsi.MainRAM[0x300], &autoLoad, sizeof(autoLoad));
}

std::filesystem::path pathFromUtf8(const std::string& s) {
  return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::string pathToUtf8(const std::filesystem::path& p) {
  std::u8string s = p.u8string();
  return std::string(s.begin(), s.end());
}

std::string baseName(const std::string& path) {
  return pathToUtf8(pathFromUtf8(path).filename());
}

}

Dsi::Dsi(std::string romPath, std::string savePath, bool isGba) {
  if(isGba) {
    fail("GBA mode is not supported: melonDS only emulates the DS and DSi");
  }

  m_cfg = Config::snapshot();

  bool dsiFilesConfigured = !m_cfg.dsiBios9Path.empty() || !m_cfg.dsiBios7Path.empty() || !m_cfg.dsiFirmwarePath.empty() || !m_cfg.dsiNandPath.empty();
  if(m_cfg.consoleType == -1) {
    m_consoleType = dsiFilesConfigured ? 1 : 0;
  } else if(m_cfg.consoleType == 0 || m_cfg.consoleType == 1) {
    m_consoleType = m_cfg.consoleType;
  } else {
    fail("Config console type must be -1 (auto), 0 (DS) or 1 (DSi)");
  }

  // Validate the setup up front, following melonDS's EmuInstance::verifySetup
  bool dsBios = !m_cfg.bios9Path.empty() || !m_cfg.bios7Path.empty();
  if(dsBios && (m_cfg.bios9Path.empty() || m_cfg.bios7Path.empty())) {
    fail("Set both the DS ARM9 and ARM7 BIOS paths, or neither (to use FreeBIOS)");
  }
  bool dsiBios = !m_cfg.dsiBios9Path.empty() || !m_cfg.dsiBios7Path.empty();
  if(dsiBios && (m_cfg.dsiBios9Path.empty() || m_cfg.dsiBios7Path.empty())) {
    fail("Set both the DSi ARM9 and ARM7 BIOS paths, or neither (to use FreeBIOS)");
  }

  if(m_consoleType == 1) {
    if(dsiBios && m_cfg.dsiFirmwarePath.empty()) {
      fail("DSi mode with DSi BIOS dumps also needs a DSi firmware dump (Config.set_dsi_firmware_path)");
    }
    if(!m_cfg.dsiNandPath.empty() && !dsiBios) {
      fail("Using a DSi NAND needs the DSi BIOS dumps it was made with (Config.set_dsi_bios_7_path); the NAND is encrypted with a key from the ARM7 BIOS");
    }
    if(romPath.empty() && (m_cfg.dsiNandPath.empty() || !dsiBios)) {
      fail("Booting the DSi menu without a ROM needs DSi BIOS dumps, DSi firmware and a DSi NAND");
    }
  } else if(romPath.empty() && (!dsBios || m_cfg.firmwarePath.empty())) {
    fail("Booting the DS firmware without a ROM needs DS BIOS dumps and a DS firmware dump");
  }

  // Cartridge, parsed before the console exists so a bad ROM fails early
  std::unique_ptr<NDSCart::CartCommon> cart;
  NDSHeader header {};
  bool nandTitle = false;
  m_ctx = std::make_unique<InstanceContext>();
  m_ctx->saveWriteback = m_cfg.saveWriteback;
  m_ctx->firmwareWriteback = m_cfg.firmwareWriteback;
  if(!romPath.empty()) {
    std::vector<u8> rom = readFileOrFail(romPath, "ROM");
    if(rom.size() >= sizeof(NDSHeader)) {
      std::memcpy(&header, rom.data(), sizeof(NDSHeader));
    }

    std::unique_ptr<u8[]> sram;
    u32 sramLength = 0;
    if(!savePath.empty()) {
      std::vector<u8> save;
      if(platform::readFile(savePath, save) && !save.empty()) {
        sramLength = (u32)save.size();
        sram = std::make_unique<u8[]>(sramLength);
        std::memcpy(sram.get(), save.data(), sramLength);
      } else if(m_cfg.saveWriteback && !Platform::CheckFileWritable(savePath)) {
        Platform::Log(Platform::LogLevel::Warn, "pydsi: save file %s is not writable\n", savePath.c_str());
      }
      m_ctx->ndsSavePath = savePath;
    }

    auto romData = std::make_unique<u8[]>(rom.size());
    std::memcpy(romData.get(), rom.data(), rom.size());
    NDSCart::NDSCartArgs cartArgs {
      .SDCard = std::nullopt,
      .SRAM = std::move(sram),
      .SRAMLength = sramLength,
    };
    cart = NDSCart::ParseROM(std::move(romData), (u32)rom.size(), m_ctx.get(), std::move(cartArgs));
    if(!cart) {
      fail("Not a valid DS/DSi ROM: " + romPath);
    }

    // DSiWare runs from the NAND, not the cart slot (homebrew flagged as DSiWare still runs from the slot)
    nandTitle = m_consoleType == 1 && header.IsDSiWare() && !header.IsHomebrew();
    if(nandTitle && (m_cfg.dsiNandPath.empty() || !dsiBios)) {
      fail("DSiWare runs from the DSi NAND: configure DSi BIOS dumps, DSi firmware and a NAND with the title installed (see Dsi.install_dsiware)");
    }
  }

  // BIOS, firmware and system storage
  auto arm9bios = dsBios ? loadBios<ARM9BIOSSize>(m_cfg.bios9Path, "DS ARM9 BIOS") : std::make_unique<ARM9BIOSImage>(FreeBIOSGetNtrArm9());
  auto arm7bios = dsBios ? loadBios<ARM7BIOSSize>(m_cfg.bios7Path, "DS ARM7 BIOS") : std::make_unique<ARM7BIOSImage>(FreeBIOSGetNtrArm7());

  std::string firmwarePath = m_consoleType == 1 ? m_cfg.dsiFirmwarePath : m_cfg.firmwarePath;
  std::optional<Firmware> firmware;
  if(firmwarePath.empty()) {
    firmware.emplace(m_consoleType);
  } else if(m_consoleType == 1) {
    firmware.emplace(loadFirmware(firmwarePath, "DSi firmware", {0x20000}));
  } else {
    firmware.emplace(loadFirmware(firmwarePath, "DS firmware", {0x20000, 0x40000, 0x80000}));
  }
  m_ctx->firmwarePath = firmwarePath;

  std::optional<JITArgs> jitArgs = std::nullopt;
  if(m_cfg.jitEnabled) {
#ifdef JIT_ENABLED
    jitArgs = JITArgs();
#else
    fail("This build of pydsi doesn't include the JIT (build with PYDSI_ENABLE_JIT=ON)");
#endif
  }

  NDSArgs ndsArgs {
    std::move(arm9bios),
    std::move(arm7bios),
    std::move(*firmware),
    jitArgs,
    m_cfg.audio16Bit ? AudioBitDepth::_16Bit : AudioBitDepth::_10Bit,
    AudioInterpolation::None,
    (double)m_cfg.audioSampleRate,
    std::nullopt,
    nullptr,
  };

  if(m_consoleType == 1) {
    auto arm9ibios = dsiBios ? loadBios<DSiBIOSSize>(m_cfg.dsiBios9Path, "DSi ARM9 BIOS") : std::make_unique<DSiBIOSImage>(FreeBIOSGetTwlArm9());
    auto arm7ibios = dsiBios ? loadBios<DSiBIOSSize>(m_cfg.dsiBios7Path, "DSi ARM7 BIOS") : std::make_unique<DSiBIOSImage>(FreeBIOSGetTwlArm7());

    std::optional<DSi_NAND::NANDImage> nand;
    if(!m_cfg.dsiNandPath.empty()) {
      nand.emplace(loadNand(m_cfg.dsiNandPath, *arm7ibios));
      if(nandTitle && !titleInstalled(*nand, header)) {
        fail("This DSiWare title is not installed in the NAND: " + romPath + " (install it with Dsi.install_dsiware)");
      }
    }

    std::optional<FATStorage> sdcard;
    if(!m_cfg.dsiSdPath.empty()) {
      std::string sdPath = pathToUtf8(std::filesystem::absolute(pathFromUtf8(m_cfg.dsiSdPath)));
      sdcard.emplace(sdPath, 0, (bool)m_cfg.dsiSdReadOnly, std::nullopt);
    }

    DSiArgs dsiArgs {
      std::move(ndsArgs),
      std::move(arm9ibios),
      std::move(arm7ibios),
      std::move(nand),
      std::move(sdcard),
      (bool)m_cfg.dsiDspHle,
    };
    m_nds = std::make_unique<DSi>(std::move(dsiArgs), m_ctx.get());
  } else {
    m_nds = std::make_unique<NDS>(std::move(ndsArgs), m_ctx.get());
  }

  // Boot, in the same order as melonDS's EmuInstance::updateConsole + loadROM
  m_nds->Reset();
  m_nds->SPU.SetDegrade10Bit(m_cfg.audio16Bit ? AudioBitDepth::_16Bit : AudioBitDepth::_10Bit);

  RendererSettings renderSettings {1, (bool)m_cfg.threaded3D, false, false};
  m_nds->GetRenderer().SetRenderSettings(renderSettings);

  if(cart && !nandTitle) {
    m_nds->SetNDSCart(std::move(cart));
    m_nds->Reset();
    if(m_cfg.directBoot || m_nds->NeedsDirectBoot()) {
      m_nds->SetupDirectBoot(baseName(romPath));
    }
  } else if(nandTitle) {
    setupDsiwareBoot(static_cast<DSi&>(*m_nds), header);
  } else if(m_nds->NeedsDirectBoot()) {
    fail("The configured firmware can't boot to the system menu");
  }

  // Like melonDS's frontend, set the clock after booting. Default: leave melonDS's fixed
  // 2000-01-01 00:00:00 so runs are reproducible.
  if(m_cfg.rtcUseHostTime || m_cfg.rtcEpoch >= 0) {
    std::time_t t = m_cfg.rtcUseHostTime ? std::time(nullptr) : (std::time_t)m_cfg.rtcEpoch;
    std::tm* tm = m_cfg.rtcUseHostTime ? std::localtime(&t) : std::gmtime(&t);
    if(!tm) {
      fail("Config RTC epoch is out of range");
    }
    if(tm->tm_year < 100 || tm->tm_year > 199) {
      fail("The DS clock only supports years 2000-2099");
    }
    m_nds->RTC.SetDateTime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  }

  applyInput();
  m_nds->Start();
}

Dsi::~Dsi() {
  if(m_ctx) {
    m_ctx->flush();
  }
  // The NAND and SD images are closed (and the SD card synced) by the DSi destructor
  m_nds.reset();
}

void Dsi::afterFrame() {
  m_ctx->tick();
  if(!m_cfg.emulateAudio) {
    m_nds->SPU.DrainOutput();
  }
}

void Dsi::runTask() {
  m_nds->RunFrame();
  afterFrame();
}

void Dsi::runUntilFrame() {
  m_nds->RunFrame();
  afterFrame();
}

bool Dsi::getFrame() {
  void* top = nullptr;
  void* bottom = nullptr;
  if(!m_nds->GPU.GetFramebuffers(&top, &bottom) || !top || !bottom) {
    return false;
  }

  // melonDS: 0xAARRGGBB words (BGRA bytes) -> RGBA bytes
  const uint32_t size = NDS_WIDTH * NDS_HEIGHT;
  const uint32_t* src[2] = {static_cast<const uint32_t*>(top), static_cast<const uint32_t*>(bottom)};
  for(int screen = 0; screen < 2; screen++) {
    uint32_t* dst = m_framebuffer + screen * size;
    for(uint32_t i = 0; i < size; i++) {
      uint32_t px = src[screen][i];
      dst[i] = (px & 0xFF00FF00) | ((px >> 16) & 0xFF) | ((px & 0xFF) << 16);
    }
  }
  return true;
}

nb::ndarray<nb::numpy, uint8_t> Dsi::getGbaFrame() {
  return nb::ndarray<nb::numpy, uint8_t>();
}

nb::ndarray<nb::numpy, uint8_t> Dsi::getTopNdsFrame() {
  return nb::ndarray<nb::numpy, uint8_t>(reinterpret_cast<void*>(m_framebuffer), {NDS_HEIGHT, NDS_WIDTH, 4});
}

nb::ndarray<nb::numpy, uint8_t> Dsi::getBotNdsFrame() {
  return nb::ndarray<nb::numpy, uint8_t>(reinterpret_cast<void*>(m_framebuffer + NDS_WIDTH * NDS_HEIGHT), {NDS_HEIGHT, NDS_WIDTH, 4});
}

// Always returns exactly `count` stereo frames, like PyNDS: what's buffered, then silence.
// getAudioBufferNumber() beforehand tells how many are real.
nb::ndarray<nb::numpy, int16_t> Dsi::getAudioSamples(int count) {
  count = std::max(count, 0);
  int16_t* samples = new int16_t[std::max(count, 1) * 2]();
  int wanted = std::min(count, m_nds->SPU.GetOutputSize());
  if(wanted > 0) {
    m_nds->SPU.ReadOutput(samples, wanted);
  }
  nb::capsule owner(samples, [](void* p) noexcept {
    delete[] (int16_t*)p;
  });
  return nb::ndarray<nb::numpy, int16_t>(samples, {(size_t)count, 2}, owner);
}

uint32_t Dsi::getAudioBufferNumber() {
  return m_nds->SPU.GetOutputSize();
}

void Dsi::saveState(std::string path) {
  Savestate state;
  if(state.Error || !m_nds->DoSavestate(&state) || state.Error) {
    fail("Couldn't create a savestate");
  }
  if(!platform::writeFile(path, (const u8*)state.Buffer(), state.Length())) {
    fail("Couldn't write savestate file: " + path);
  }
}

void Dsi::loadState(std::string path) {
  std::vector<u8> buffer = readFileOrFail(path, "Savestate");

  // A failed load can leave the console half-loaded, so keep a backup (like melonDS's frontend)
  Savestate backup;
  if(backup.Error || !m_nds->DoSavestate(&backup) || backup.Error) {
    fail("Couldn't back up the current state before loading");
  }

  Savestate state(buffer.data(), (u32)buffer.size(), false);
  if(state.Error || !m_nds->DoSavestate(&state) || state.Error) {
    backup.Rewind(false);
    m_nds->DoSavestate(&backup);
    applyInput();
    fail("Couldn't load savestate (corrupt, from another melonDS version, or made with a different console type or DSP setting): " + path);
  }

  // Input isn't part of melonDS savestates
  applyInput();
}

void Dsi::saveGame(const std::string& path, bool alwaysSave) {
  const u8* save = m_nds->GetNDSSave();
  u32 length = m_nds->GetNDSSaveLength();
  if(!save || length == 0) {
    return;
  }
  if(!alwaysSave && !m_ctx->ndsSaveDirty) {
    return;
  }
  if(!platform::writeFile(path, save, length)) {
    fail("Couldn't write save file: " + path);
  }
}

int Dsi::getConsoleType() {
  return m_consoleType;
}

bool Dsi::isDsiMode() {
  if(m_consoleType != 1) {
    return false;
  }
  // SCFG_EXT9 bit 31: the extended DSi hardware is enabled (cleared when a DS-mode title runs)
  return (static_cast<DSi&>(*m_nds).SCFG_EXT[0] & 0x80000000) != 0;
}

bool Dsi::isRunning() {
  return m_nds->IsRunning() && !m_ctx->stopped;
}

uint32_t Dsi::getFrameCount() {
  return m_nds->NumFrames;
}

void Dsi::installDsiware(const std::string& appPath, const std::string& tmdPath, bool overwrite) {
  Config::Values cfg = Config::snapshot();
  if(cfg.dsiNandPath.empty() || cfg.dsiBios7Path.empty()) {
    fail("Installing DSiWare needs Config's DSi NAND path and DSi ARM7 BIOS path");
  }

  std::vector<u8> app = readFileOrFail(appPath, "DSiWare app");
  if(app.size() < sizeof(NDSHeader)) {
    fail("Not a DSiWare title: " + appPath);
  }
  NDSHeader header {};
  std::memcpy(&header, app.data(), sizeof(NDSHeader));
  if(!header.IsDSiWare()) {
    fail("Not a DSiWare title: " + appPath);
  }

  std::vector<u8> tmdData = readFileOrFail(tmdPath, "TMD");
  if(tmdData.size() < sizeof(DSi_TMD::TitleMetadata)) {
    fail("Not a valid TMD file: " + tmdPath);
  }
  DSi_TMD::TitleMetadata tmd {};
  std::memcpy(&tmd, tmdData.data(), sizeof(tmd));
  if(tmd.GetCategory() != header.DSiTitleIDHigh || tmd.GetID() != header.DSiTitleIDLow) {
    fail("The title ID in the TMD doesn't match the app");
  }

  auto arm7ibios = loadBios<DSiBIOSSize>(cfg.dsiBios7Path, "DSi ARM7 BIOS");
  Platform::FileHandle* file = Platform::OpenFile(cfg.dsiNandPath, Platform::FileMode::ReadWriteExisting);
  if(!file) {
    fail("DSi NAND not found or not writable: " + cfg.dsiNandPath);
  }
  DSi_NAND::NANDImage nand(file, &(*arm7ibios)[0x8308]);
  if(!nand) {
    fail("Not a valid DSi NAND image: " + cfg.dsiNandPath);
  }
  DSi_NAND::NANDMount mount(nand);
  if(!mount) {
    fail("Couldn't mount the DSi NAND (wrong DSi ARM7 BIOS for this NAND?)");
  }
  if(mount.TitleExists(header.DSiTitleIDHigh, header.DSiTitleIDLow)) {
    if(!overwrite) {
      fail("This title is already installed (pass overwrite=True to replace it)");
    }
    mount.DeleteTitle(header.DSiTitleIDHigh, header.DSiTitleIDLow);
  }
  if(!mount.ImportTitle(app.data(), app.size(), tmd, false)) {
    fail("Importing the title into the NAND failed");
  }
}
