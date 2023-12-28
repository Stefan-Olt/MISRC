cmake_minimum_required(VERSION 3.14)
project (misrc_extract)

#Set C language version
set(CMAKE_C_STANDARD 11)
#Enable ASM provided by NASM
enable_language(ASM_NASM)

if(WIN32 AND NOT MINGW)
add_library(libgetopt_static STATIC
    getopt/getopt.c
)
endif()

if(APPLE)
	#Add leading underscore when building macho64 object files
	string(APPEND CMAKE_ASM_NASM_FLAGS "--prefix _") # is that correct for C code?
endif(APPLE)


add_executable(misrc_extract main.c extract.asm)

set(INSTALL_TARGETS misrc_extract)

install(TARGETS ${INSTALL_TARGETS})
