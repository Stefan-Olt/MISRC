/*
* MISRC tools
* Copyright (C) 2024  vrunk11, stefan_o
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

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdatomic.h>

typedef struct {
	uint8_t      *buffer;
#ifdef _WIN32
	uint8_t      *_buffer2;
#endif
	size_t        buffer_size;
	int           fd;
	atomic_size_t head;
	atomic_size_t tail;
} ringbuffer_t;

int   rb_init(ringbuffer_t *rb, char *name, size_t size);
int   rb_put(ringbuffer_t *rb, void *data, size_t size);
void* rb_read_ptr(ringbuffer_t *rb, size_t size);
int   rb_read_finished(ringbuffer_t *rb, size_t size);
void* rb_write_ptr(ringbuffer_t *rb, size_t size);
int   rb_write_finished(ringbuffer_t *rb, size_t size);
void  rb_close(ringbuffer_t *rb);

#endif // RINGBUFFER_H