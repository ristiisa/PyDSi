// Implementation of melonDS's Platform.h for a headless, single-threaded host.
// Reference: src/frontend/qt_sdl/Platform.cpp in melonDS (the fopen-based version before
// commit 1609a0f9 switched it to QFile).

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Platform.h"
#include "SPI_Firmware.h"
#include "platform_impl.hpp"

using namespace melonDS;

namespace {

std::atomic<bool> s_logEnabled{false};

FILE* openUtf8(const std::string& path, const char* mode) {
#ifdef _WIN32
  auto widen = [](const std::string& s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    if(len > 1) {
      MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    }
    return w;
  };
  return _wfopen(widen(path).c_str(), widen(mode).c_str());
#else
  return fopen(path.c_str(), mode);
#endif
}

bool exists(const std::string& path) {
  FILE* f = openUtf8(path, "rb");
  if(!f) {
    return false;
  }
  fclose(f);
  return true;
}

// FileMode -> fopen mode string, following the Qt frontend's former mapping:
// Append -> "a"; no Write, NoCreate, or (Preserve and the file exists) -> "r"; else "w";
// "+" when the file must be both readable and writable; "b" unless Text.
// One deliberate difference: Write|Preserve (or Write|NoCreate) without Read on an existing
// file gave "rb" in Qt, which can't write; here it gives "r+b" so Write is honoured.
std::string modeString(Platform::FileMode mode, bool fileExists) {
  using namespace Platform;
  std::string m;
  if(mode & Append) {
    m = "a";
  } else if(!(mode & Write)) {
    m = "r";
  } else if(mode & NoCreate) {
    m = "r";
  } else if((mode & Preserve) && fileExists) {
    m = "r";
  } else {
    m = "w";
  }

  if(((mode & ReadWrite) == ReadWrite) || ((mode & Write) && m == "r")) {
    m += "+";
  }

  if(!(mode & Text)) {
    m += "b";
  }
  return m;
}

FILE* toFile(Platform::FileHandle* file) {
  return reinterpret_cast<FILE*>(file);
}

const std::chrono::steady_clock::time_point s_startTime = std::chrono::steady_clock::now();

}

void InstanceContext::tick(u32 delayFrames) {
  if(ndsSaveDirty && ++ndsSaveAge >= delayFrames) {
    flush();
  }
  if(firmwareDirty && ++firmwareAge >= delayFrames) {
    flush();
  }
}

void InstanceContext::flush() {
  if(ndsSaveDirty) {
    if(saveWriteback && !ndsSavePath.empty()) {
      platform::writeFile(ndsSavePath, ndsSave.data(), ndsSave.size());
    }
    ndsSaveDirty = false;
    ndsSaveAge = 0;
  }
  if(firmwareDirty) {
    if(firmwareWriteback && !firmwarePath.empty()) {
      platform::writeFile(firmwarePath, firmware.data(), firmware.size());
    }
    firmwareDirty = false;
    firmwareAge = 0;
  }
}

namespace platform {

void setLogEnabled(bool enabled) {
  s_logEnabled = enabled;
}

bool writeFile(const std::string& path, const u8* data, size_t len) {
  FILE* f = openUtf8(path, "wb");
  if(!f) {
    Platform::Log(Platform::LogLevel::Error, "pydsi: can't open %s for writing\n", path.c_str());
    return false;
  }
  bool ok = len == 0 || fwrite(data, len, 1, f) == 1;
  ok = (fclose(f) == 0) && ok;
  return ok;
}

bool readFile(const std::string& path, std::vector<u8>& out) {
  FILE* f = openUtf8(path, "rb");
  if(!f) {
    return false;
  }
  out.clear();
  u8 buf[65536];
  size_t n;
  while((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    out.insert(out.end(), buf, buf + n);
  }
  bool ok = !ferror(f);
  fclose(f);
  return ok;
}

}

namespace melonDS::Platform {

void SignalStop(StopReason reason, void* userdata) {
  if(auto* ctx = static_cast<InstanceContext*>(userdata)) {
    ctx->stopped = true;
    ctx->stopReason = reason;
  }
}

// File I/O

std::string GetLocalFilePath(const std::string& filename) {
  // No emulator directory in a library: relative paths are relative to the working directory
  return filename;
}

FileHandle* OpenFile(const std::string& path, FileMode mode) {
  if((mode & (ReadWrite | Append)) == None) {
    Log(LogLevel::Error, "pydsi: invalid file mode for %s\n", path.c_str());
    return nullptr;
  }

  bool fileExists = exists(path);
  if((mode & NoCreate) && !fileExists) {
    return nullptr;
  }

  std::string m = modeString(mode, fileExists);
  FILE* f = openUtf8(path, m.c_str());
  if(!f) {
    Log(LogLevel::Debug, "pydsi: can't open %s with mode %s\n", path.c_str(), m.c_str());
    return nullptr;
  }
  return reinterpret_cast<FileHandle*>(f);
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode) {
  return OpenFile(GetLocalFilePath(path), mode);
}

bool FileExists(const std::string& name) {
  return exists(name);
}

bool LocalFileExists(const std::string& name) {
  return exists(GetLocalFilePath(name));
}

bool CheckFileWritable(const std::string& filepath) {
  if(exists(filepath)) {
    FILE* f = openUtf8(filepath, "ab");
    if(!f) {
      return false;
    }
    fclose(f);
    return true;
  }
  // Don't leave an empty file behind
  FILE* f = openUtf8(filepath, "wb");
  if(!f) {
    return false;
  }
  fclose(f);
  std::remove(filepath.c_str());
  return true;
}

bool CheckLocalFileWritable(const std::string& filepath) {
  return CheckFileWritable(GetLocalFilePath(filepath));
}

bool CloseFile(FileHandle* file) {
  return fclose(toFile(file)) == 0;
}

bool IsEndOfFile(FileHandle* file) {
  return feof(toFile(file)) != 0;
}

bool FileReadLine(char* str, int count, FileHandle* file) {
  return fgets(str, count, toFile(file)) != nullptr;
}

u64 FilePosition(FileHandle* file) {
#ifdef _WIN32
  return _ftelli64(toFile(file));
#else
  return ftello(toFile(file));
#endif
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin) {
  int whence = SEEK_SET;
  switch(origin) {
    case FileSeekOrigin::Start: whence = SEEK_SET; break;
    case FileSeekOrigin::Current: whence = SEEK_CUR; break;
    case FileSeekOrigin::End: whence = SEEK_END; break;
  }
#ifdef _WIN32
  return _fseeki64(toFile(file), offset, whence) == 0;
#else
  return fseeko(toFile(file), offset, whence) == 0;
#endif
}

void FileRewind(FileHandle* file) {
  rewind(toFile(file));
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file) {
  return fread(data, size, count, toFile(file));
}

bool FileFlush(FileHandle* file) {
  return fflush(toFile(file)) == 0;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file) {
  return fwrite(data, size, count, toFile(file));
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...) {
  if(fmt == nullptr) {
    return 0;
  }
  va_list args;
  va_start(args, fmt);
  int ret = vfprintf(toFile(file), fmt, args);
  va_end(args);
  return ret < 0 ? 0 : ret;
}

u64 FileLength(FileHandle* file) {
  u64 pos = FilePosition(file);
  if(!FileSeek(file, 0, FileSeekOrigin::End)) {
    return 0;
  }
  u64 len = FilePosition(file);
  FileSeek(file, pos, FileSeekOrigin::Start);
  return len;
}

// Logging

void Log(LogLevel level, const char* fmt, ...) {
  if(fmt == nullptr || !s_logEnabled) {
    return;
  }
  static const char* const prefixes[] = {"[debug] ", "[info] ", "[warn] ", "[error] "};
  fputs(prefixes[level & 3], stderr);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

// Threads and synchronisation (used by the threaded software 3D renderer)

struct Thread {
  std::thread thread;
};

Thread* Thread_Create(std::function<void()> func) {
  return new Thread{std::thread(std::move(func))};
}

void Thread_Free(Thread* thread) {
  if(thread->thread.joinable()) {
    thread->thread.join();
  }
  delete thread;
}

void Thread_Wait(Thread* thread) {
  if(thread->thread.joinable()) {
    thread->thread.join();
  }
}

struct Semaphore {
  std::mutex mutex;
  std::condition_variable cv;
  int count = 0;
};

Semaphore* Semaphore_Create() {
  return new Semaphore();
}

void Semaphore_Free(Semaphore* sema) {
  delete sema;
}

void Semaphore_Reset(Semaphore* sema) {
  std::lock_guard<std::mutex> lock(sema->mutex);
  sema->count = 0;
}

void Semaphore_Wait(Semaphore* sema) {
  std::unique_lock<std::mutex> lock(sema->mutex);
  sema->cv.wait(lock, [sema] { return sema->count > 0; });
  sema->count--;
}

bool Semaphore_TryWait(Semaphore* sema, int timeout_ms) {
  std::unique_lock<std::mutex> lock(sema->mutex);
  if(timeout_ms == 0) {
    if(sema->count == 0) {
      return false;
    }
  } else if(!sema->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [sema] { return sema->count > 0; })) {
    return false;
  }
  sema->count--;
  return true;
}

void Semaphore_Post(Semaphore* sema, int count) {
  {
    std::lock_guard<std::mutex> lock(sema->mutex);
    sema->count += count;
  }
  if(count == 1) {
    sema->cv.notify_one();
  } else {
    sema->cv.notify_all();
  }
}

struct Mutex {
  std::mutex mutex;
};

Mutex* Mutex_Create() {
  return new Mutex();
}

void Mutex_Free(Mutex* mutex) {
  delete mutex;
}

void Mutex_Lock(Mutex* mutex) {
  mutex->mutex.lock();
}

void Mutex_Unlock(Mutex* mutex) {
  mutex->mutex.unlock();
}

bool Mutex_TryLock(Mutex* mutex) {
  return mutex->mutex.try_lock();
}

void Sleep(u64 usecs) {
  std::this_thread::sleep_for(std::chrono::microseconds(usecs));
}

u64 GetMSCount() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s_startTime).count();
}

u64 GetUSCount() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - s_startTime).count();
}

// Save write-back: buffered in the InstanceContext, written by InstanceContext::tick/flush

void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata) {
  auto* ctx = static_cast<InstanceContext*>(userdata);
  if(!ctx) {
    return;
  }
  ctx->ndsSave.assign(savedata, savedata + savelen);
  ctx->ndsSaveDirty = true;
  ctx->ndsSaveAge = 0;
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata) {
  // No GBA slot support
}

void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void* userdata) {
  auto* ctx = static_cast<InstanceContext*>(userdata);
  // Generated firmware has no file to go back to (the Qt frontend keeps its Wi-Fi settings in
  // wfcsettings.bin; pydsi doesn't)
  if(!ctx || ctx->firmwarePath.empty() || firmware.GetHeader().Identifier == GENERATED_FIRMWARE_IDENTIFIER) {
    return;
  }
  ctx->firmware.assign(firmware.Buffer(), firmware.Buffer() + firmware.Length());
  ctx->firmwareDirty = true;
  ctx->firmwareAge = 0;
}

void WriteDateTime(int year, int month, int day, int hour, int minute, int second, void* userdata) {
  // The Qt frontend stores this as an offset from host time in its config. pydsi doesn't sync the
  // RTC to the host, so there is nothing to persist.
  Log(LogLevel::Debug, "pydsi: guest set RTC to %04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);
}

// Local multiplayer and networking: not supported

void MP_Begin(void* userdata) {}
void MP_End(void* userdata) {}
int MP_SendPacket(u8* data, int len, u64 timestamp, void* userdata) { return 0; }
int MP_RecvPacket(u8* data, u64* timestamp, void* userdata) { return 0; }
int MP_SendCmd(u8* data, int len, u64 timestamp, void* userdata) { return 0; }
int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid, void* userdata) { return 0; }
int MP_SendAck(u8* data, int len, u64 timestamp, void* userdata) { return 0; }
int MP_RecvHostPacket(u8* data, u64* timestamp, void* userdata) { return 0; }
u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask, void* userdata) { return 0; }

int Net_SendPacket(u8* data, int len, void* userdata) { return 0; }
int Net_RecvPacket(u8* data, void* userdata) { return 0; }

// Camera: black frames. YUV frames are packed two pixels per word as Y0 | U<<8 | Y1<<16 | V<<24.

void Camera_Start(int num, void* userdata) {}
void Camera_Stop(int num, void* userdata) {}

void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv, void* userdata) {
  if(yuv) {
    std::fill(frame, frame + (width * height) / 2, 0x80008000u);
  } else {
    std::fill(frame, frame + width * height, 0xFF000000u);
  }
}

// Microphone: silence

void Mic_Start(void* userdata) {}
void Mic_Stop(void* userdata) {}

int Mic_ReadInput(s16* data, int maxlength, void* userdata) {
  std::memset(data, 0, maxlength * sizeof(s16));
  return maxlength;
}

// AAC decoding for DSi DSP HLE: unavailable (melonDS's AAC ucode handles a null decoder)

AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder* dec) {}
bool AAC_Configure(AACDecoder* dec, int frequency, int channels) { return false; }
bool AAC_DecodeFrame(AACDecoder* dec, const void* input, int inputlen, void* output, int outputlen) { return false; }

// Slot-2 add-ons: nothing connected

bool Addon_KeyDown(KeyType type, void* userdata) { return false; }
void Addon_RumbleStart(u32 len, void* userdata) {}
void Addon_RumbleStop(void* userdata) {}
float Addon_MotionQuery(MotionQueryType type, void* userdata) { return 0.0f; }

// Only used by the JIT on Android and by libpcap networking, neither of which pydsi builds

DynamicLibrary* DynamicLibrary_Load(const char* lib) { return nullptr; }
void DynamicLibrary_Unload(DynamicLibrary* lib) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name) { return nullptr; }

}
