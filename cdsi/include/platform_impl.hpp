#pragma once

#include <string>
#include <vector>

#include "types.h"
#include "Platform.h"

// Per-emulator state that melonDS hands back to the Platform callbacks as `userdata`.
// Each Dsi owns one, so two Dsi objects never share it.
struct InstanceContext {
  bool saveWriteback = false;
  bool firmwareWriteback = false;

  // Cartridge save (SRAM/EEPROM/flash)
  std::string ndsSavePath;
  std::vector<melonDS::u8> ndsSave;
  bool ndsSaveDirty = false;
  melonDS::u32 ndsSaveAge = 0;

  // Firmware dump the console was booted with (empty when the firmware is generated)
  std::string firmwarePath;
  std::vector<melonDS::u8> firmware;
  bool firmwareDirty = false;
  melonDS::u32 firmwareAge = 0;

  bool stopped = false;
  melonDS::Platform::StopReason stopReason = melonDS::Platform::StopReason::Unknown;

  // Called once per emulated frame: writes pending saves to disk once they've been
  // quiet for `delayFrames` frames (melonDS's Qt frontend waits 2 seconds).
  void tick(melonDS::u32 delayFrames = 60);
  // Writes pending saves now
  void flush();
};

namespace platform {
// Process-wide log switch. Platform::Log has no userdata, so it can't be per instance.
void setLogEnabled(bool enabled);
bool writeFile(const std::string& path, const melonDS::u8* data, size_t len);
bool readFile(const std::string& path, std::vector<melonDS::u8>& out);
}
