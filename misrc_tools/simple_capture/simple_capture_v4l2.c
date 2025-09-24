/*
* simple_capture OS abstraction library - V4L2 implementation
* Copyright (C) 2025 stefan_o
* Some parts of this code are AI-generated
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

#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "simple_capture.h"

typedef struct {
    void* start;
    size_t length;
} _buf_t;

struct sc_handle {
	int fd;
	_buf_t* bufs;
	int nbufs;
	pthread_t th;
	volatile int stop;
	sc_frame_callback_t cb;
	sc_data_info_t data_info;
};

char* sc_get_impl_name() {
	return "Video4Linux2";
}

char* sc_get_impl_name_short() {
	return "v4l2";
}


size_t sc_get_devices(sc_capture_dev_t **dev_list) {
	size_t cnt = 0;
	sc_capture_dev_t *dev_l = malloc(64*sizeof(sc_capture_dev_t));
	memset(dev_l,0,64*sizeof(sc_capture_dev_t));
	for (int n = 0; n < 64; ++n) {
		char node[64];
		int r;
		snprintf(node, sizeof(node), "/dev/video%d", n);
		int fd = open(node, O_RDONLY | O_NONBLOCK);
		if (fd < 0) continue;
		struct v4l2_capability cap = {0};
		r = ioctl(fd, VIDIOC_QUERYCAP, &cap);
		close(fd);
		if (r == 0) {
			uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
			fprintf(stderr,"caps %04x devcaps %04x\n", cap.capabilities,cap.device_caps);
			fprintf(stderr,"caps %04x capt %i capt_mplane %i\n", caps, caps&V4L2_CAP_VIDEO_CAPTURE, caps&V4L2_CAP_VIDEO_CAPTURE_MPLANE);
			if(!((caps&V4L2_CAP_VIDEO_CAPTURE) || (caps&V4L2_CAP_VIDEO_CAPTURE_MPLANE))) continue;
			dev_l[cnt].device_id = strdup(node);
			dev_l[cnt].name = strdup((char*)cap.card);
			cnt++;
		}
	}
	*dev_list = dev_l;
	return cnt;
}

size_t sc_get_formats(char* device_id, sc_formatlist_t **fmt_list) {
	size_t cnt = 0;
	int fd = open(device_id, O_RDONLY | O_NONBLOCK);
	if (fd < 0) return 0;

	sc_formatlist_t *fmt_l = calloc(64,sizeof(sc_formatlist_t));
	for (int typei = 0; typei < 2; ++typei) {
		struct v4l2_fmtdesc fmt;
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = (typei == 0) ? V4L2_BUF_TYPE_VIDEO_CAPTURE : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		for (fmt.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0 && cnt < 64; ++fmt.index) {
			struct v4l2_frmsizeenum fse;
			size_t cnt_s = 0;
			fmt_l[cnt].codec = fmt.pixelformat;
			memset(&fse, 0, sizeof(fse));
			fse.pixel_format = fmt.pixelformat;
			fmt_l[cnt].sizes = calloc(64,sizeof(sc_size_t));
			for (fse.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fse) == 0 && cnt_s < 64; ++fse.index) {
				if (fse.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
					struct v4l2_frmivalenum fie;
					size_t cnt_f = 0;
					fmt_l[cnt].sizes[cnt_s].w = fse.discrete.width;
					fmt_l[cnt].sizes[cnt_s].h = fse.discrete.height;
					memset(&fie, 0, sizeof(fie));
					fie.pixel_format = fmt.pixelformat;
					fie.width = fse.discrete.width;
					fie.height = fse.discrete.height;
					fmt_l[cnt].sizes[cnt_s].fps = calloc(64,sizeof(sc_fps_t));
					for (fie.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie) == 0 && cnt_f < 64; ++fie.index) {
						if (fie.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].den = fie.discrete.numerator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].num = fie.discrete.denominator;
							if (fmt_l[cnt].sizes[cnt_s].fps[cnt_f].den == 0) fmt_l[cnt].sizes[cnt_s].fps[cnt_f].den = 1;
						} else if (fie.type == V4L2_FRMIVAL_TYPE_STEPWISE || fie.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].min_den = fie.stepwise.max.denominator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].min_num = fie.stepwise.max.numerator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].max_den = fie.stepwise.min.denominator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].max_num = fie.stepwise.min.numerator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].step_den = fie.stepwise.min.denominator;
							fmt_l[cnt].sizes[cnt_s].fps[cnt_f].step_num = fie.stepwise.min.numerator;
							if (fmt_l[cnt].sizes[cnt_s].fps[cnt_f].min_den == 0) fmt_l[cnt].sizes[cnt_s].fps[cnt_f].min_den = 1;
							if (fmt_l[cnt].sizes[cnt_s].fps[cnt_f].max_den == 0) fmt_l[cnt].sizes[cnt_s].fps[cnt_f].max_den = 1;
							if (fmt_l[cnt].sizes[cnt_s].fps[cnt_f].step_den == 0) fmt_l[cnt].sizes[cnt_s].fps[cnt_f].step_den = 1;
						}
						cnt_f++;
					}
					fmt_l[cnt].sizes[cnt_s].n_fps = cnt_f;
					cnt_s++;
				}
			}
			fmt_l[cnt].n_sizes = cnt_s;
			cnt++;
		}
	}
	close(fd);
	*fmt_list = fmt_l;
	return cnt;
}

static void* v4l2_thread(void* p) {
	sc_handle_t* hc = (sc_handle_t*)p;
	while (!hc->stop) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(hc->fd, &fds);
		struct timeval tv = {2, 0};
		int r = select(hc->fd + 1, &fds, NULL, NULL, &tv);
		//fprintf(stderr,"A\n");
		if (r <= 0) continue;
		//fprintf(stderr,"B\n");
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl(hc->fd, VIDIOC_DQBUF, &buf) == 0) {
			//fprintf(stderr,"C\n");
			if (buf.index < (uint32_t)hc->nbufs) {
				//fprintf(stderr,"D %i\n",buf.bytesused);
				hc->data_info.data = (uint8_t*)hc->bufs[buf.index].start;
				hc->data_info.len = (uint32_t) buf.bytesused;
				if (hc->cb && hc->data_info.data && hc->data_info.len > 0) {
					hc->cb(&(hc->data_info));
				}
			}
			ioctl(hc->fd, VIDIOC_QBUF, &buf);
		}
	}
	return NULL;
}

int sc_start_capture(const char* device_id, uint32_t width, uint32_t height, sc_codec_t codec,
    uint32_t fps_num, uint32_t fps_den, sc_frame_callback_t cb, void* cb_ctx, sc_handle_t** out_handle)
{
	if (!out_handle) return -1;
	*out_handle = NULL;
	if (!device_id || !cb || width == 0 || height == 0 || fps_den == 0 || fps_num == 0) return -1;

	int fd = open(device_id, O_RDWR | O_NONBLOCK);
	if (fd < 0) return -1;

	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.pixelformat = codec;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { close(fd); return -1; }

	struct v4l2_streamparm parm;
	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* V4L2 uses intervalls not fps, therefore num and den are swapped */
	parm.parm.capture.timeperframe.numerator = fps_den;
	parm.parm.capture.timeperframe.denominator = fps_num;
	ioctl(fd, VIDIOC_S_PARM, &parm); /* best effort; ignore failure */

	struct v4l2_requestbuffers req; 
	memset(&req, 0, sizeof(req));
	req.count = 8;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) { close(fd); return -1; }

	_buf_t* bufs = (_buf_t*)calloc(req.count, sizeof(_buf_t));
	if (!bufs) { close(fd); return -1; }

	for (unsigned i = 0; i < req.count; ++i) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) { free(bufs); close(fd); return -1; }
		bufs[i].length = buf.length;
		bufs[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (bufs[i].start == MAP_FAILED) { free(bufs); close(fd); return -1; }
	}
	for (unsigned i = 0; i < req.count; ++i) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		ioctl(fd, VIDIOC_QBUF, &buf);
	}

	enum v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &bt) < 0) {
		for (unsigned i = 0; i < req.count; ++i) if (bufs[i].start) munmap(bufs[i].start, bufs[i].length);
		free(bufs); close(fd); return -1;
	}
	bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sc_handle_t* hc = (sc_handle_t*)calloc(1, sizeof(sc_handle_t));
	if (!hc) {
		ioctl(fd, VIDIOC_STREAMOFF, &bt);
		for (unsigned i = 0; i < req.count; ++i) if (bufs[i].start) munmap(bufs[i].start, bufs[i].length);
		free(bufs); close(fd); return -1;
	}
	hc->fd = fd;
	hc->bufs = bufs;
	hc->nbufs = req.count;
	hc->cb = cb;
	hc->data_info.ctx = cb_ctx;
	hc->data_info.width = width;
	hc->data_info.height = height;
	hc->data_info.codec = codec;
	hc->stop = 0;

	if (pthread_create(&hc->th, NULL, v4l2_thread, hc) != 0) {
		bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ioctl(fd, VIDIOC_STREAMOFF, &bt);
		for (unsigned i = 0; i < req.count; ++i) if (bufs[i].start) munmap(bufs[i].start, bufs[i].length);
		free(bufs);
		close(fd);
		free(hc);
		return -1;
	}

	*out_handle = hc;
	return 0;
}

void sc_stop_capture(sc_handle_t* handle) {
	if (!handle) return;
	handle->stop = 1;
	pthread_join(handle->th, NULL);
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(handle->fd, VIDIOC_STREAMOFF, &type);
	for (int i = 0; i < handle->nbufs; ++i) if (handle->bufs[i].start) munmap(handle->bufs[i].start, handle->bufs[i].length);
	free(handle->bufs);
	close(handle->fd);
	free(handle);
}
