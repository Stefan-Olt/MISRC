cmake_minimum_required(VERSION 3.14)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

project (misrc_tools)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckCPUArch)
include(FindLibFLAC)
include(GitVersion)

#Set C language version
set(CMAKE_C_STANDARD 11)

get_version_from_git()

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

if(PKG_CONFIG_FOUND AND NOT LIBHSDAOH_FOUND)
    message(FATAL_ERROR "libhsdaoh required to build misrc_capture")
endif()

if(LIBFLAC_FOUND)
    add_compile_definitions(LIBFLAC_ENABLED=1)
else()
    message(WARNING "libflac not found, disabling flac encoding")
    add_compile_definitions(LIBFLAC_ENABLED=0)
endif()

if(WIN32 AND NOT MINGW)
add_library(libgetopt_static STATIC
	getopt/getopt.c
)
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

#this is taken from xiph.org/flac
check_cpu_arch_x64(MISRC__CPU_X86_64)

if(MISRC__CPU_X86_64)
	#Enable ASM provided by NASM
	enable_language(ASM_NASM)
	if(APPLE)
		#Add leading underscore when building macho64 object files
		string(APPEND CMAKE_ASM_NASM_FLAGS "--prefix _") # is that correct for C code?
	endif(APPLE)
	add_executable(misrc_extract misrc_extract.c extract.c extract.asm)
	add_executable(misrc_capture misrc_capture.c extract.c ringbuffer.c extract.asm)
else()
	add_executable(misrc_extract misrc_extract.c extract.c)
	add_executable(misrc_capture misrc_capture.c extract.c ringbuffer.c)
endif()

set(INSTALL_TARGETS misrc_extract misrc_capture)
target_include_directories(misrc_capture PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(misrc_extract PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(misrc_capture hsdaoh ${LIBFLAC_LIBRARIES} ${AVUTIL_LIBRARIES})

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
