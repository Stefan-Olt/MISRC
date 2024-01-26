cmake_minimum_required(VERSION 3.14)
project (misrc_extract)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckCPUArch)

#Set C language version
set(CMAKE_C_STANDARD 11)


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
	add_executable(misrc_extract main.c extract.asm)
else()
	add_executable(misrc_extract main.c)
endif()

set(INSTALL_TARGETS misrc_extract)

if(WIN32 AND NOT MINGW)
target_link_libraries(misrc_extract libgetopt_static)
endif()

install(TARGETS ${INSTALL_TARGETS})
