/*
* MISRC tools
* Copyright (C) 2024-2025  vrunk11, stefan_o
* 
* based on:
* http://web.archive.org/web/20171026083549/https://lo.calho.st/quick-hacks/employing-black-magic-in-the-linux-page-table/
* Copyright (C) 2017 by Travis Mick
* 
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef _WIN32
#include <windows.h>
#else
#include "shm_anon.h"
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#endif
#include "ringbuffer.h"


int rb_init(ringbuffer_t *rb, char *name, size_t size) {

#ifdef _WIN32

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#define nullptr NULL
#endif

	SYSTEM_INFO sysInfo;
	HANDLE h = nullptr;
	void* maparea = nullptr;

	// First, make sure the size is a multiple of the page size
	GetSystemInfo (&sysInfo);
	if((size % sysInfo.dwAllocationGranularity) != 0) {
		return 1;
	}

	if((maparea = (PCHAR)VirtualAlloc2(nullptr, nullptr, 2*size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0)) == nullptr) {
		return 2;
	}

	if(VirtualFree(maparea, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) == FALSE) {
		return 3;
	}

	if((h = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr)) == nullptr) {
		VirtualFree(maparea, 0, MEM_RELEASE);
		VirtualFree(maparea+size, 0, MEM_RELEASE);
		return 4;
	} 

	if((rb->buffer = MapViewOfFile3(h, nullptr, maparea, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) == nullptr) {
		CloseHandle(h);
		VirtualFree(maparea, 0, MEM_RELEASE);
		VirtualFree(maparea+size, 0, MEM_RELEASE);
		return 5;
	}

	if((rb->_buffer2 = MapViewOfFile3(h, nullptr, maparea+size, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) == nullptr) {
		CloseHandle(h);
		VirtualFree(maparea+size, 0, MEM_RELEASE);
		UnmapViewOfFileEx(rb->buffer, 0);
		return 6;
	}
	CloseHandle(h);
#else
	// First, make sure the size is a multiple of the page size
	if(size % getpagesize() != 0) {
		return 1;
	}

	// Make an anonymous file and set its size
	if((rb->fd = memfd_create(name, 0)) == -1) {
		return 2;
	}

	if(ftruncate(rb->fd, size) == -1) {
		return 3;
	}

	// Ask mmap for an address at a location where we can put both virtual copies of the buffer
	if((rb->buffer = mmap(NULL, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
		return 4;
	}

	// Map the buffer at that address
	if(mmap(rb->buffer, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, rb->fd, 0) == MAP_FAILED)  {
		return 5;
	}

	// Now map it again, in the next virtual page
	if(mmap(rb->buffer + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, rb->fd, 0) == MAP_FAILED)  {
		return 6;
	}
#endif

	// Initialize our buffer indices
	rb->buffer_size = size;
	rb->head = 0;
	rb->tail = 0;
	return 0;
}

int rb_put(ringbuffer_t *rb, void *data, size_t size) {
	if(rb->buffer_size - (rb->tail - rb->head) < size) {
		return 1;
	}
	memcpy(&rb->buffer[rb->tail], data, size);
	rb->tail += size;
	return 0;
}

void* rb_write_ptr(ringbuffer_t *rb, size_t size) {
	if(rb->buffer_size - (rb->tail - rb->head) < size) {
		return NULL;
	}
	return &rb->buffer[rb->tail];
}

int rb_write_finished(ringbuffer_t *rb, size_t size) {
	if(rb->buffer_size - (rb->tail - rb->head) < size) {
		return 1;
	}
	rb->tail += size;
	return 0;
}

void* rb_read_ptr(ringbuffer_t *rb, size_t size) {
	if(rb->tail - rb->head < size){
		return NULL;
	}
	return &rb->buffer[rb->head];
}

int rb_read_finished(ringbuffer_t *rb, size_t size) {
	if(rb->tail - rb->head < size){
		return 1;
	}
	rb->head += size;
	if(rb->head > rb->buffer_size) {
		rb->head -= rb->buffer_size;
		rb->tail -= rb->buffer_size;
	}
	return 0;
}

void rb_close(ringbuffer_t *rb) {
#ifdef _WIN32
	UnmapViewOfFile(rb->buffer);
	UnmapViewOfFile(rb->_buffer2);
#else
	munmap(rb->buffer, rb->buffer_size);
	munmap(rb->buffer+rb->buffer_size, rb->buffer_size);
#endif
}
