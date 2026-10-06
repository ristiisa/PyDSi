#include <bit>
#include <cstring>

#include "MemConstants.h"
#include "NDS.h"

#include "dsi.hpp"

using namespace melonDS;


// Memory goes through melonDS's bus handlers (NDS::ARM9Read* etc.), which is what the CPUs
// see minus the ARM9's tightly-coupled memory. With tcm=true, ARM9 accesses that hit ITCM or
// DTCM (as currently mapped by CP15) use those instead, like the ARM9 itself would.
// Reading I/O registers can have side effects (e.g. popping the IPC FIFO).

uint8_t* Dsi::tcmPointer(uint32_t address, uint32_t size) {
  ARMv5& arm9 = m_nds->ARM9;
  if(address < arm9.ITCMSize) {
    uint32_t offset = address & (ITCMPhysicalSize - 1);
    return offset + size <= ITCMPhysicalSize ? &arm9.ITCM[offset] : nullptr;
  }
  if((address & arm9.DTCMMask) == arm9.DTCMBase) {
    uint32_t offset = address & (DTCMPhysicalSize - 1);
    return offset + size <= DTCMPhysicalSize ? &arm9.DTCM[offset] : nullptr;
  }
  return nullptr;
}

template<typename T>
T Dsi::readRam(bool arm7, uint32_t address, bool tcm) {
  if(!arm7 && tcm) {
    if(uint8_t* p = tcmPointer(address, sizeof(T))) {
      T value;
      std::memcpy(&value, p, sizeof(T));
      return value;
    }
  }
  if constexpr(sizeof(T) == 1) {
    return arm7 ? m_nds->ARM7Read8(address) : m_nds->ARM9Read8(address);
  } else if constexpr(sizeof(T) == 2) {
    return arm7 ? m_nds->ARM7Read16(address) : m_nds->ARM9Read16(address);
  } else if constexpr(sizeof(T) == 4) {
    return arm7 ? m_nds->ARM7Read32(address) : m_nds->ARM9Read32(address);
  } else {
    uint64_t lo = readRam<uint32_t>(arm7, address, tcm);
    uint64_t hi = readRam<uint32_t>(arm7, address + 4, tcm);
    return lo | (hi << 32);
  }
}

template<typename T>
void Dsi::writeRam(bool arm7, uint32_t address, T value, bool tcm) {
  if(!arm7 && tcm) {
    if(uint8_t* p = tcmPointer(address, sizeof(T))) {
      std::memcpy(p, &value, sizeof(T));
      // The bus write handlers invalidate JIT blocks themselves; TCM writes have to do it here
      if(m_nds->IsJITEnabled()) {
        m_nds->JIT.ResetBlockCache();
      }
      return;
    }
  }
  if constexpr(sizeof(T) == 1) {
    arm7 ? m_nds->ARM7Write8(address, value) : m_nds->ARM9Write8(address, value);
  } else if constexpr(sizeof(T) == 2) {
    arm7 ? m_nds->ARM7Write16(address, value) : m_nds->ARM9Write16(address, value);
  } else {
    arm7 ? m_nds->ARM7Write32(address, value) : m_nds->ARM9Write32(address, value);
  }
}

// Read memory methods
uint8_t Dsi::readRamu8(bool arm7, uint32_t address, bool tcm) {
  return readRam<uint8_t>(arm7, address, tcm);
}

uint16_t Dsi::readRamu16(bool arm7, uint32_t address, bool tcm) {
  return readRam<uint16_t>(arm7, address, tcm);
}

uint32_t Dsi::readRamu32(bool arm7, uint32_t address, bool tcm) {
  return readRam<uint32_t>(arm7, address, tcm);
}

uint64_t Dsi::readRamu64(bool arm7, uint32_t address, bool tcm) {
  return readRam<uint64_t>(arm7, address, tcm);
}

int8_t Dsi::readRami8(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<int8_t>(readRam<uint8_t>(arm7, address, tcm));
}

int16_t Dsi::readRami16(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<int16_t>(readRam<uint16_t>(arm7, address, tcm));
}

int32_t Dsi::readRami32(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<int32_t>(readRam<uint32_t>(arm7, address, tcm));
}

int64_t Dsi::readRami64(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<int64_t>(readRam<uint64_t>(arm7, address, tcm));
}

float Dsi::readRamf32(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<float>(readRam<uint32_t>(arm7, address, tcm));
}

double Dsi::readRamf64(bool arm7, uint32_t address, bool tcm) {
  return std::bit_cast<double>(readRam<uint64_t>(arm7, address, tcm));
}

nb::ndarray<nb::numpy, uint8_t> Dsi::readMap(bool arm7, uint32_t address, uint32_t size, bool tcm) {
  uint8_t* buffer = new uint8_t[std::max<uint32_t>(size, 1)];
  for(uint32_t i = 0; i < size; i++) {
    buffer[i] = readRam<uint8_t>(arm7, address + i, tcm);
  }
  nb::capsule owner(buffer, [](void* p) noexcept {
    delete[] (uint8_t*)p;
  });
  return nb::ndarray<nb::numpy, uint8_t>(buffer, {(size_t)size}, owner);
}

// Write memory methods
void Dsi::writeRamu8(bool arm7, uint32_t address, uint8_t value, bool tcm) {
  writeRam<uint8_t>(arm7, address, value, tcm);
}

void Dsi::writeRamu16(bool arm7, uint32_t address, uint16_t value, bool tcm) {
  writeRam<uint16_t>(arm7, address, value, tcm);
}

void Dsi::writeRamu32(bool arm7, uint32_t address, uint32_t value, bool tcm) {
  writeRam<uint32_t>(arm7, address, value, tcm);
}

void Dsi::writeRami8(bool arm7, uint32_t address, int8_t value, bool tcm) {
  writeRam<uint8_t>(arm7, address, std::bit_cast<uint8_t>(value), tcm);
}

void Dsi::writeRami16(bool arm7, uint32_t address, int16_t value, bool tcm) {
  writeRam<uint16_t>(arm7, address, std::bit_cast<uint16_t>(value), tcm);
}

void Dsi::writeRami32(bool arm7, uint32_t address, int32_t value, bool tcm) {
  writeRam<uint32_t>(arm7, address, std::bit_cast<uint32_t>(value), tcm);
}

void Dsi::writeRamf32(bool arm7, uint32_t address, float value, bool tcm) {
  writeRam<uint32_t>(arm7, address, std::bit_cast<uint32_t>(value), tcm);
}

void Dsi::writeMap(bool arm7, uint32_t address, nb::ndarray<const uint8_t, nb::ndim<1>, nb::c_contig> data, bool tcm) {
  const uint8_t* bytes = data.data();
  for(size_t i = 0; i < data.shape(0); i++) {
    writeRam<uint8_t>(arm7, address + (uint32_t)i, bytes[i], tcm);
  }
}
