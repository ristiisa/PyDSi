cmake_minimum_required(VERSION 3.18)
project(CDSI LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "melonDS does not build with MSVC (cl.exe), see NOTES.md. On Windows use llvm-mingw's UCRT toolchain (CC=clang CXX=clang++), or clang-cl from a Visual Studio developer environment.")
endif()

find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)

# nanobind comes from the pip package listed in pyproject.toml's build requirements
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

# melonDS core only: no Qt frontend, no OpenGL, no GDB stub.
# ENABLE_GDBSTUB must stay OFF: melonDS sets GDBSTUB_ENABLED with a directory-scoped
# add_definitions(), and it changes the layout of NDS/ARM, which our sources include.
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT_PROFILING OFF CACHE BOOL "" FORCE)
set(USE_VCPKG OFF CACHE BOOL "" FORCE)
set(USE_CCACHE OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC OFF CACHE BOOL "" FORCE)
# Off by default: a JIT build of melonDS installs a process-wide SIGSEGV handler / first-chance
# vectored exception handler (even when the JIT is disabled at runtime) that dereferences
# NDS::Current, so an unrelated crash anywhere in the Python process can be turned into a
# crash inside melonDS. See NOTES.md.
option(PYDSI_ENABLE_JIT "Build melonDS's JIT recompiler (still off at runtime unless Config.set_jit_enabled(1))" OFF)
set(ENABLE_JIT ${PYDSI_ENABLE_JIT} CACHE BOOL "" FORCE)
set(ENABLE_LTO_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO OFF CACHE BOOL "" FORCE)

# FATStorage.cpp includes <dirent.h>; clang targeting the MSVC runtime has none, so use tronkko/dirent
if(WIN32 AND NOT MINGW)
  include(FetchContent)
  # Header only: SOURCE_SUBDIR has no CMakeLists.txt, so dirent's own project (and its install rules) isn't added
  FetchContent_Declare(dirent
    GIT_REPOSITORY https://github.com/tronkko/dirent
    GIT_TAG 1.26
    SOURCE_SUBDIR include)
  FetchContent_MakeAvailable(dirent)
  set(DIRENT_INCLUDE_DIRS "${dirent_SOURCE_DIR}/include" CACHE PATH "" FORCE)
endif()

add_subdirectory(externals/melonDS EXCLUDE_FROM_ALL)

# melonDS's FreeBIOS.cpp uses std::copy without including <algorithm>; libstdc++ and MSVC's STL
# include it transitively, LLVM's libc++ (macOS, llvm-mingw) doesn't. Force-include it rather than patching.
if(MSVC)
  set(PYDSI_FORCE_ALGORITHM /FIalgorithm)
else()
  set(PYDSI_FORCE_ALGORITHM -include algorithm)
endif()
set_source_files_properties(externals/melonDS/src/FreeBIOS.cpp DIRECTORY externals/melonDS/src PROPERTIES COMPILE_OPTIONS "${PYDSI_FORCE_ALGORITHM}")

if(MSVC)
  # clang-cl: melonDS adds -fwrapv (signed overflow wraps, which its CPU emulation relies on)
  # only for GCC-style compilers (src/CMakeLists.txt "if (NOT MSVC)"), so add it here
  target_compile_options(core PUBLIC /clang:-fwrapv)
endif()

if(MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  # python.org's pyconfig.h only defines MS_WIN64 for MSVC. Without it, gcc gets a 32-bit
  # Py_ssize_t and 15-bit PyLong digits, which silently breaks nanobind's integer conversion.
  add_compile_definitions(MS_WIN64)
endif()

nanobind_add_module(cdsi
  cdsi/cdsi.cpp
  cdsi/dsi.cpp
  cdsi/dsi_input.cpp
  cdsi/dsi_memory.cpp
  cdsi/config.cpp
  cdsi/platform_impl.cpp)
target_include_directories(cdsi PRIVATE cdsi/include)
target_link_libraries(cdsi PRIVATE core)
if(WIN32)
  # Same as melonDS's directory-scoped definitions (its headers include windows.h)
  target_compile_definitions(cdsi PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  if(NOT MINGW)
    target_compile_definitions(cdsi PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endif()

if(MINGW)
  # Don't make the wheel depend on MinGW runtime DLLs (libgcc/libstdc++ with gcc, libunwind/libc++ with
  # llvm-mingw, and winpthread); the module then imports only the UCRT, like CPython itself
  target_link_options(cdsi PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif()

nanobind_add_stub(cdsi_stub
  MODULE cdsi
  OUTPUT cdsi.pyi
  PYTHON_PATH $<TARGET_FILE_DIR:cdsi>
  DEPENDS cdsi)

install(TARGETS cdsi LIBRARY DESTINATION .)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cdsi.pyi DESTINATION .)
