cmake_minimum_required(VERSION 3.14)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

project (misrc_tools)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckCPUArch)
include(FindLibFLAC)

#Set C language version
set(CMAKE_C_STANDARD 11)
if(MSVC)
  add_compile_options(/std:c11)
endif()

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBHSDAOH libhsdaoh IMPORTED_TARGET)
    if(LIBHSDAOH_LINK_LIBRARIES)
        set(LIBHSDAOH_LIBRARIES "${LIBHSDAOH_LINK_LIBRARIES}")
    endif()
else()
    set(LIBHSDAOH_LIBRARIES "" CACHE STRING "manual libhsdaoh path")
    set(LIBHSDAOH_INCLUDE_DIRS "" CACHE STRING "manual libhsdaoh includepath")
endif()

if(PKG_CONFIG_FOUND AND NOT LIBHSDAOH_FOUND AND NOT WIN32)
    message(FATAL_ERROR "libhsdaoh required to build misrc_capture")
endif()

# For Windows, look for hsdaoh in HSDAOH_ROOT environment variable
if(WIN32 AND NOT LIBHSDAOH_FOUND)
    if(DEFINED ENV{HSDAOH_ROOT})
        find_library(HSDAOH_LIBRARY NAMES hsdaoh libhsdaoh PATHS $ENV{HSDAOH_ROOT} PATH_SUFFIXES lib)
        find_path(HSDAOH_INCLUDE_DIR hsdaoh.h PATHS $ENV{HSDAOH_ROOT} PATH_SUFFIXES include)
        
        if(HSDAOH_LIBRARY AND HSDAOH_INCLUDE_DIR)
            set(LIBHSDAOH_LIBRARIES ${HSDAOH_LIBRARY})
            set(LIBHSDAOH_INCLUDE_DIRS ${HSDAOH_INCLUDE_DIR})
            set(LIBHSDAOH_FOUND TRUE)
            message(STATUS "Found hsdaoh: ${HSDAOH_LIBRARY}")
        else()
            message(WARNING "hsdaoh library or headers not found in HSDAOH_ROOT")
        endif()
    else()
        message(WARNING "libhsdaoh not found on Windows, attempting to build without it")
        set(LIBHSDAOH_LIBRARIES "")
        set(LIBHSDAOH_INCLUDE_DIRS "")
    endif()
endif()

if(LIBFLAC_FOUND)
    add_compile_definitions(LIBFLAC_ENABLED=1)
else()
    message(WARNING "libflac not found, disabling flac encoding")
endif()

if(WIN32 AND NOT MINGW)
add_library(libgetopt_static STATIC
	getopt/getopt.c
)
endif()

#this is taken from xiph.org/flac
check_cpu_arch_x64(MISRC__CPU_X86_64)

if(MISRC__CPU_X86_64)
	#Enable ASM provided by NASM
	enable_language(ASM_NASM)
	if(APPLE)
		#Add leading underscore when building macho64 object files
		string(APPEND CMAKE_ASM_NASM_FLAGS "--prefix _") # is that correct for C code?
	endif(APPLE)
	# Set NASM specific flags, avoiding C compiler flags
	if(WIN32)
		set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
		set_property(SOURCE extract.asm PROPERTY LANGUAGE ASM_NASM)
		set_property(SOURCE extract.asm PROPERTY COMPILE_FLAGS "-fwin64")
	endif()
	add_executable(misrc_extract misrc_extract.c extract.c extract.asm)
	add_executable(misrc_capture misrc_capture.c extract.c ringbuffer.c extract.asm)
else()
	add_executable(misrc_extract misrc_extract.c extract.c)
	add_executable(misrc_capture misrc_capture.c extract.c ringbuffer.c)
endif()

set(INSTALL_TARGETS misrc_extract misrc_capture)

if(LIBFLAC_FOUND)
    target_include_directories(misrc_capture PRIVATE ${LIBFLAC_INCLUDE_DIRS})
    if(NOT WIN32)
        target_link_libraries(misrc_capture hsdaoh ${LIBFLAC_LIBRARIES})
    else()
        # On Windows, link hsdaoh and FLAC libraries
        if(LIBHSDAOH_FOUND AND LIBHSDAOH_LIBRARIES)
            target_include_directories(misrc_capture PRIVATE ${LIBHSDAOH_INCLUDE_DIRS})
            target_link_libraries(misrc_capture ${LIBHSDAOH_LIBRARIES} ${LIBFLAC_LIBRARIES})
        else()
            target_link_libraries(misrc_capture ${LIBFLAC_LIBRARIES})
        endif()
    endif()
else()
    if(NOT WIN32)
        target_link_libraries(misrc_capture hsdaoh)
    else()
        # On Windows, link hsdaoh if available
        if(LIBHSDAOH_FOUND AND LIBHSDAOH_LIBRARIES)
            target_link_libraries(misrc_capture ${LIBHSDAOH_LIBRARIES})
        endif()
    endif()
endif()

if(WIN32)
	add_compile_definitions(_WIN32_WINNT=0x0A00)
	add_compile_definitions(NTDDI_VERSION=0x0A000005)
	target_link_libraries(misrc_capture onecore)
endif()

if(WIN32 AND NOT MINGW)
target_link_libraries(misrc_extract libgetopt_static)
target_link_libraries(misrc_capture libgetopt_static)
endif()

install(TARGETS ${INSTALL_TARGETS})
