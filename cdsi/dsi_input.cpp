#include <algorithm>

#include "NDS.h"

#include "dsi.hpp"


// melonDS key mask: active-low, bits 0-11 = A, B, Select, Start, Right, Left, Up, Down, R, L, X, Y
// (the same order as PyNDS's KEY_MAP)
void Dsi::applyInput() {
  m_nds->SetKeyMask(m_keyMask);
  if(m_touching) {
    m_nds->TouchScreen(m_touchX, m_touchY);
  } else {
    m_nds->ReleaseScreen();
  }
}

void Dsi::setTouchInput(int x, int y) {
  m_touchX = std::clamp(x, 0, NDS_WIDTH - 1);
  m_touchY = std::clamp(y, 0, NDS_HEIGHT - 1);
  applyInput();
}

void Dsi::clearTouchInput() {
  m_touchX = 0;
  m_touchY = 0;
  m_touching = false;
  applyInput();
}

void Dsi::touchInput() {
  m_touching = true;
  applyInput();
}

void Dsi::releaseTouchInput() {
  m_touching = false;
  applyInput();
}

void Dsi::pressKey(int key) {
  if(key >= 0 && key < 12) {
    m_keyMask &= ~(1u << key);
    applyInput();
  }
}

void Dsi::releaseKey(int key) {
  if(key >= 0 && key < 12) {
    m_keyMask |= 1u << key;
    applyInput();
  }
}

void Dsi::setKeyMask(uint32_t mask) {
  m_keyMask = mask & 0xFFF;
  applyInput();
}

uint32_t Dsi::getKeyMask() {
  return m_keyMask;
}

void Dsi::setLidClosed(bool closed) {
  m_nds->SetLidClosed(closed);
}
