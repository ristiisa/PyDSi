#include "config.hpp"
#include "platform_impl.hpp"


Config::Values Config::s_values;

Config::Values Config::snapshot() {
  return s_values;
}

// Console Type
void Config::setConsoleType(int consoleType) {
  s_values.consoleType = consoleType;
}

int Config::getConsoleType() {
  return s_values.consoleType;
}

// DSi Mode
void Config::setDsiMode(int dsiMode) {
  s_values.consoleType = dsiMode ? 1 : 0;
}

int Config::getDsiMode() {
  return s_values.consoleType == 1;
}

// Direct Boot
void Config::setDirectBoot(int directBoot) {
  s_values.directBoot = directBoot;
}

int Config::getDirectBoot() {
  return s_values.directBoot;
}

// ROM in RAM
void Config::setRomInRam(int romInRam) {
  s_values.romInRam = romInRam;
}

int Config::getRomInRam() {
  return s_values.romInRam;
}

// FPS Limiter
void Config::setFpsLimiter(int fpsLimiter) {
  s_values.fpsLimiter = fpsLimiter;
}

int Config::getFpsLimiter() {
  return s_values.fpsLimiter;
}

// Frame Skip
void Config::setFrameSkip(int frameSkip) {
  s_values.frameSkip = frameSkip;
}

int Config::getFrameSkip() {
  return s_values.frameSkip;
}

// Threaded 2D
void Config::setThreaded2D(int threaded2D) {
  s_values.threaded2D = threaded2D;
}

int Config::getThreaded2D() {
  return s_values.threaded2D;
}

// Threaded 3D
void Config::setThreaded3D(int threaded3D) {
  s_values.threaded3D = threaded3D;
}

int Config::getThreaded3D() {
  return s_values.threaded3D;
}

// High Resolution 3D
void Config::setHighRes3D(int highRes3D) {
  s_values.highRes3D = highRes3D;
}

int Config::getHighRes3D() {
  return s_values.highRes3D;
}

// Screen Ghost
void Config::setScreenGhost(int screenGhost) {
  s_values.screenGhost = screenGhost;
}

int Config::getScreenGhost() {
  return s_values.screenGhost;
}

// Emulate Audio
void Config::setEmulateAudio(int emulateAudio) {
  s_values.emulateAudio = emulateAudio;
}

int Config::getEmulateAudio() {
  return s_values.emulateAudio;
}

// Audio 16 Bit
void Config::setAudio16Bit(int audio16Bit) {
  s_values.audio16Bit = audio16Bit;
}

int Config::getAudio16Bit() {
  return s_values.audio16Bit;
}

// Audio Sample Rate
void Config::setAudioSampleRate(int audioSampleRate) {
  s_values.audioSampleRate = audioSampleRate;
}

int Config::getAudioSampleRate() {
  return s_values.audioSampleRate;
}

// Saves Folder
void Config::setSavesFolder(int savesFolder) {
  s_values.savesFolder = savesFolder;
}

int Config::getSavesFolder() {
  return s_values.savesFolder;
}

// States Folder
void Config::setStatesFolder(int statesFolder) {
  s_values.statesFolder = statesFolder;
}

int Config::getStatesFolder() {
  return s_values.statesFolder;
}

// Cheats Folder
void Config::setCheatsFolder(int cheatsFolder) {
  s_values.cheatsFolder = cheatsFolder;
}

int Config::getCheatsFolder() {
  return s_values.cheatsFolder;
}

// Screen Filter
void Config::setScreenFilter(int screenFilter) {
  s_values.screenFilter = screenFilter;
}

int Config::getScreenFilter() {
  return s_values.screenFilter;
}

// ARM7 HLE
void Config::setArm7Hle(int arm7Hle) {
  s_values.arm7Hle = arm7Hle;
}

int Config::getArm7Hle() {
  return s_values.arm7Hle;
}

// JIT
void Config::setJitEnabled(int jitEnabled) {
  s_values.jitEnabled = jitEnabled;
}

int Config::getJitEnabled() {
  return s_values.jitEnabled;
}

// DSi DSP HLE
void Config::setDsiDspHle(int dsiDspHle) {
  s_values.dsiDspHle = dsiDspHle;
}

int Config::getDsiDspHle() {
  return s_values.dsiDspHle;
}

// Save Writeback
void Config::setSaveWriteback(int saveWriteback) {
  s_values.saveWriteback = saveWriteback;
}

int Config::getSaveWriteback() {
  return s_values.saveWriteback;
}

// Firmware Writeback
void Config::setFirmwareWriteback(int firmwareWriteback) {
  s_values.firmwareWriteback = firmwareWriteback;
}

int Config::getFirmwareWriteback() {
  return s_values.firmwareWriteback;
}

// RTC
void Config::setRtcUseHostTime(int rtcUseHostTime) {
  s_values.rtcUseHostTime = rtcUseHostTime;
}

int Config::getRtcUseHostTime() {
  return s_values.rtcUseHostTime;
}

void Config::setRtcEpoch(int64_t rtcEpoch) {
  s_values.rtcEpoch = rtcEpoch;
}

int64_t Config::getRtcEpoch() {
  return s_values.rtcEpoch;
}

// Logging
// Takes effect immediately: melonDS's log callback has no per-instance context
void Config::setLogEnabled(int logEnabled) {
  s_values.logEnabled = logEnabled;
  platform::setLogEnabled(logEnabled);
}

int Config::getLogEnabled() {
  return s_values.logEnabled;
}

// DSi SD Read Only
void Config::setDsiSdReadOnly(int dsiSdReadOnly) {
  s_values.dsiSdReadOnly = dsiSdReadOnly;
}

int Config::getDsiSdReadOnly() {
  return s_values.dsiSdReadOnly;
}

// Paths
void Config::setBios9Path(const std::string& bios9Path) {
  s_values.bios9Path = bios9Path;
}

std::string Config::getBios9Path() {
  return s_values.bios9Path;
}

void Config::setBios7Path(const std::string& bios7Path) {
  s_values.bios7Path = bios7Path;
}

std::string Config::getBios7Path() {
  return s_values.bios7Path;
}

void Config::setFirmwarePath(const std::string& firmwarePath) {
  s_values.firmwarePath = firmwarePath;
}

std::string Config::getFirmwarePath() {
  return s_values.firmwarePath;
}

void Config::setGbaBiosPath(const std::string& gbaBiosPath) {
  s_values.gbaBiosPath = gbaBiosPath;
}

std::string Config::getGbaBiosPath() {
  return s_values.gbaBiosPath;
}

void Config::setDsiBios9Path(const std::string& dsiBios9Path) {
  s_values.dsiBios9Path = dsiBios9Path;
}

std::string Config::getDsiBios9Path() {
  return s_values.dsiBios9Path;
}

void Config::setDsiBios7Path(const std::string& dsiBios7Path) {
  s_values.dsiBios7Path = dsiBios7Path;
}

std::string Config::getDsiBios7Path() {
  return s_values.dsiBios7Path;
}

void Config::setDsiFirmwarePath(const std::string& dsiFirmwarePath) {
  s_values.dsiFirmwarePath = dsiFirmwarePath;
}

std::string Config::getDsiFirmwarePath() {
  return s_values.dsiFirmwarePath;
}

void Config::setDsiNandPath(const std::string& dsiNandPath) {
  s_values.dsiNandPath = dsiNandPath;
}

std::string Config::getDsiNandPath() {
  return s_values.dsiNandPath;
}

void Config::setDsiSdPath(const std::string& dsiSdPath) {
  s_values.dsiSdPath = dsiSdPath;
}

std::string Config::getDsiSdPath() {
  return s_values.dsiSdPath;
}

void Config::setBasePath(const std::string& basePath) {
  s_values.basePath = basePath;
}

std::string Config::getBasePath() {
  return s_values.basePath;
}

// PyNDS name for the SD card image
void Config::setSdImagePath(const std::string& sdImagePath) {
  s_values.dsiSdPath = sdImagePath;
}

std::string Config::getSdImagePath() {
  return s_values.dsiSdPath;
}
