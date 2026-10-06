"""Generate pydsi's homebrew test ROM.

The ROM is a few dozen ARM instructions, written for this project (no SDK, no copyrighted
data). Its header has UnitCode 0x02 (DSi-enhanced), so melonDS's direct boot starts it in
DSi mode on a DSi and in DS mode on a DS.

ARM9 program (assembly in ARM9_ASM below):
  - turns on the top screen in VRAM display mode and fills it with red if SCFG_EXT9 bit 31
    is set (DSi mode), green otherwise
  - writes results to RESULTS (main RAM):
      +0   magic 'PYDS'
      +4   SCFG_EXT9 as read at boot (0 in DS mode)
      +8   VBlank counter
      +12  KEYINPUT (0x04000130) sampled every VBlank
      +16  mailbox: written by the host
      +20  mailbox + 1, updated every VBlank
  - every VBlank, draws row (counter & 127) in a colour derived from the counter, so the
    picture depends on how many frames have run
ARM7 program: an infinite loop.

The machine code is checked in as bytes. `python tests/romgen.py --assemble` re-assembles
it with keystone-engine (dev-only dependency) and fails if the bytes differ.
"""

import struct
import sys

RESULTS = 0x02100000
MAGIC = 0x53445950  # 'PYDS' in memory order

ARM9_RAM = 0x02004000
ARM7_RAM = 0x02380000

ARM9_ASM = """
start:
    ldr r0, lit_powcnt1
    ldr r1, lit_powcnt1_val
    strh r1, [r0]
    ldr r0, lit_vramcnt_a
    mov r1, #0x80
    strb r1, [r0]
    ldr r0, lit_dispcnt
    ldr r1, lit_dispcnt_val
    str r1, [r0]
    ldr r4, lit_results
    ldr r1, lit_magic
    str r1, [r4]
    ldr r0, lit_scfg_ext9
    ldr r2, [r0]
    str r2, [r4, #4]
    tst r2, #0x80000000
    ldrne r3, lit_red
    ldreq r3, lit_green
    ldr r0, lit_vram_a
    mov r1, #0x6000
fill:
    str r3, [r0], #4
    subs r1, r1, #1
    bne fill
    mov r5, #0
loop:
    ldr r0, lit_dispstat
wait_draw:
    ldrh r1, [r0]
    tst r1, #1
    bne wait_draw
wait_vblank:
    ldrh r1, [r0]
    tst r1, #1
    beq wait_vblank
    add r5, r5, #1
    str r5, [r4, #8]
    ldr r0, lit_keyinput
    ldrh r1, [r0]
    str r1, [r4, #12]
    ldr r1, [r4, #16]
    add r1, r1, #1
    str r1, [r4, #20]
    and r6, r5, #127
    ldr r0, lit_vram_a
    add r0, r0, r6, lsl #9
    orr r7, r5, r5, lsl #16
    mov r1, #128
row:
    str r7, [r0], #4
    subs r1, r1, #1
    bne row
    b loop
lit_powcnt1: .word 0x04000304
lit_powcnt1_val: .word 0x00008003
lit_vramcnt_a: .word 0x04000240
lit_dispcnt: .word 0x04000000
lit_dispcnt_val: .word 0x00020000
lit_results: .word 0x02100000
lit_magic: .word 0x53445950
lit_scfg_ext9: .word 0x04004008
lit_red: .word 0x001F001F
lit_green: .word 0x03E003E0
lit_vram_a: .word 0x06800000
lit_dispstat: .word 0x04000004
lit_keyinput: .word 0x04000130
"""

ARM7_ASM = """
start:
    b start
"""

ARM9_CODE = bytes.fromhex(
  "b8009fe5b8109fe5b010c0e1b4009fe58010a0e30010c0e5ac009fe5ac109fe5001080e5a8409fe5a8109fe5"
  "001084e5a4009fe5002090e5042084e5020112e398309f1598309f0598009fe5061aa0e3043080e4011051e2"
  "fcffff1a0050a0e384009fe5b010d0e1010011e3fcffff1ab010d0e1010011e3fcffff0a015085e2085084e5"
  "64009fe5b010d0e10c1084e5101094e5011081e2141084e57f6005e240009fe5860480e0057885e18010a0e3"
  "047080e4011051e2fcffff1ae7ffffea04030004038000004002000400000004000002000000100250594453"
  "084000041f001f00e003e003000080060400000430010004")
ARM7_CODE = bytes.fromhex("feffffea")


def assemble(source):
  import keystone
  ks = keystone.Ks(keystone.KS_ARCH_ARM, keystone.KS_MODE_ARM)
  encoding, _ = ks.asm(source, 0)
  return bytes(encoding)


def crc16(data, crc=0xFFFF):
  for b in data:
    crc ^= b
    for _ in range(8):
      crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
  return crc


def pad(data, align):
  return data + b"\0" * (-len(data) % align)


def build_rom(arm9=None, arm7=None, homebrew=True, dsiware=False):
  """homebrew=False gives a "retail" header (game code PYDT, ARM9 at 0x8000), which melonDS
  doesn't find in its ROM list and so gives a default 8 KB EEPROM save.
  dsiware=True marks the ROM as a DSiWare title (title ID 00030004-50594454)."""
  arm9 = pad(arm9 if arm9 is not None else ARM9_CODE, 0x200)
  arm7 = pad(arm7 if arm7 is not None else ARM7_CODE, 0x200)
  arm9_off = 0x1000 if homebrew else 0x8000
  arm7_off = arm9_off + len(arm9)
  rom_end = arm7_off + len(arm7)

  h = bytearray(0x1000)
  h[0x000:0x00C] = b"PYDSI TEST\0\0"
  h[0x00C:0x010] = b"####" if homebrew else b"PYDT"
  if dsiware:
    h[0x012] = 0x03  # UnitCode: DSi exclusive
    struct.pack_into("<II", h, 0x230, 0x50594454, 0x00030004)
  h[0x010:0x012] = b"00"
  if not dsiware:
    h[0x012] = 0x02  # UnitCode: DSi-enhanced
  h[0x014] = 0x00  # card size: 128KB << 0
  struct.pack_into("<IIII", h, 0x020, arm9_off, ARM9_RAM, ARM9_RAM, len(arm9))
  struct.pack_into("<IIII", h, 0x030, arm7_off, ARM7_RAM, ARM7_RAM, len(arm7))
  struct.pack_into("<II", h, 0x080, rom_end, 0x4000)
  # DSi extended header: MBK settings left at 0 (no NWRAM mapping), all regions allowed
  struct.pack_into("<I", h, 0x1B0, 0xFFFFFFFF)
  struct.pack_into("<I", h, 0x210, rom_end)  # DSiTotalROMSize
  struct.pack_into("<H", h, 0x15E, crc16(h[:0x15E]))
  gap = b"\0" * (arm9_off - len(h))
  return bytes(h) + gap + arm9 + arm7


def write_rom(path, **kwargs):
  with open(path, "wb") as f:
    f.write(build_rom(**kwargs))
  return str(path)


if __name__ == "__main__":
  if "--assemble" in sys.argv:
    arm9 = assemble(ARM9_ASM)
    arm7 = assemble(ARM7_ASM)
    print("ARM9_CODE =", arm9.hex())
    print("ARM7_CODE =", arm7.hex())
    if arm9 != ARM9_CODE or arm7 != ARM7_CODE:
      print("checked-in code differs from the assembly source")
      sys.exit(1)
  else:
    write_rom(sys.argv[1] if len(sys.argv) > 1 else "pydsi_test.nds")
