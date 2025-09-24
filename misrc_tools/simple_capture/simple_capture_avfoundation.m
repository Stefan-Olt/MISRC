/*
* simple_capture OS abstraction library - AVFoundation implementation
* Copyright (C) 2025 stefan_o
* Most parts of this code are AI-generated
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

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "simple_capture.h"

@interface SCAVFDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
@property (nonatomic, assign) struct sc_handle* handle;
@end

struct sc_handle {
	AVCaptureSession* session;
	AVCaptureDevice* device;
	AVCaptureDeviceInput* input;
	AVCaptureVideoDataOutput* output;
	dispatch_queue_t queue;
	SCAVFDelegate* delegate;
	volatile int stop;
	sc_frame_callback_t cb;
	sc_data_info_t data_info;
};

char* sc_get_impl_name() {
	return "AVFoundation";
}

char* sc_get_impl_name_short() {
	return "avf";
}

@implementation SCAVFDelegate
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection {
	(void)output; (void)connection;
	struct sc_handle* hc = self.handle;
	if (!hc || hc->stop) return;
	if (!hc->cb) return;

	CVImageBufferRef img = CMSampleBufferGetImageBuffer(sampleBuffer);
	if (!img) return;

	CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);

	/* This implementation assumes a single-plane buffer (true for YUY2/UYVY/RGB24/GREY).
	   If a device delivers planar formats (e.g., NV12), base address is the first plane. */
	size_t height = CVPixelBufferGetHeight(img);
	size_t bpr = CVPixelBufferGetBytesPerRow(img);
	void* base = CVPixelBufferGetBaseAddress(img);
	size_t len = bpr * height;

	hc->data_info.data = base;
	hc->data_info.len = (uint32_t)len;
	hc->cb(&(hc->data_info));

	CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
}
@end

static AVCaptureDevice* sc_find_device_by_id(NSString* uid) {
	NSArray<AVCaptureDevice*>* devs;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
	AVCaptureDeviceDiscoverySession* ds = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeBuiltInWideAngleCamera]
																								mediaType:AVMediaTypeVideo
																								position:AVCaptureDevicePositionUnspecified];
	devs = ds.devices;
#else
	devs = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif
	for (AVCaptureDevice* d in devs) {
		if ([d.uniqueID isEqualToString:uid]) return d;
	}
	return nil;
}

size_t sc_get_devices(sc_capture_dev_t **dev_list) {
	if (!dev_list) return 0;
	@autoreleasepool {
		sc_capture_dev_t* list = (sc_capture_dev_t*)calloc(64, sizeof(sc_capture_dev_t));
		if (!list) return 0;
		size_t cnt = 0;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
		AVCaptureDeviceDiscoverySession* ds = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeBuiltInWideAngleCamera]
																									mediaType:AVMediaTypeVideo
																									position:AVCaptureDevicePositionUnspecified];
		NSArray<AVCaptureDevice*>* devs = ds.devices;
#else
		NSArray<AVCaptureDevice*>* devs = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif
		for (AVCaptureDevice* d in devs) {
			if (cnt >= 64) break;
			const char* name = d.localizedName.UTF8String ?: "Unknown";
			const char* uid = d.uniqueID.UTF8String ?: "";
			list[cnt].name = strdup(name);
			list[cnt].device_id = strdup(uid);
			cnt++;
		}
		*dev_list = list;
		return cnt;
	}
}

/* Helpers to group formats */
static int find_format_index(sc_formatlist_t* fmts, size_t nf, sc_codec_t codec) {
	for (size_t i=0; i<nf; ++i) {
		if (SC_CODEC_EQUAL(fmts[i].codec, codec)) return (int)i;
	}
	return -1;
}
static int find_size_index(sc_size_t* sizes, size_t ns, uint32_t w, uint32_t h) {
	for (size_t i=0; i<ns; ++i) {
		if (sizes[i].w == w && sizes[i].h == h) return (int)i;
	}
	return -1;
}

static uint32_t gcd(uint32_t a, uint32_t b)
{
	while (b)
	{
		uint32_t t = b;
		b = a % b;
		a = t;
	}
	return a;
}

size_t sc_get_formats(char* device_id, sc_formatlist_t **fmt_list) {
	if (!device_id || !fmt_list) return 0;
	@autoreleasepool {
		NSString* uid = [NSString stringWithUTF8String:device_id];
		AVCaptureDevice* dev = sc_find_device_by_id(uid);
		if (!dev) return 0;

		sc_formatlist_t* fl = (sc_formatlist_t*)calloc(64, sizeof(sc_formatlist_t));
		if (!fl) return 0;
		size_t nfmt = 0;

		for (AVCaptureDeviceFormat* f in dev.formats) {
			CMFormatDescriptionRef desc = f.formatDescription;
			if (!desc) continue;

			/* Use the native subtype directly as sc_codec_t */
			FourCharCode sub = CMFormatDescriptionGetMediaSubType(desc);
			sc_codec_t codec = (sc_codec_t)sub;

			CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(desc);
			uint32_t w = (uint32_t)dim.width;
			uint32_t h = (uint32_t)dim.height;

			int fi = find_format_index(fl, nfmt, codec);
			if (fi < 0 && nfmt < 64) {
				fi = (int)nfmt++;
				fl[fi].codec = codec;
				fl[fi].sizes = (sc_size_t*)calloc(64, sizeof(sc_size_t));
				fl[fi].n_sizes = 0;
			}
			if (fi < 0) continue;

			int si = find_size_index(fl[fi].sizes, fl[fi].n_sizes, w, h);
			if (si < 0 && fl[fi].n_sizes < 64) {
				si = (int)fl[fi].n_sizes++;
				fl[fi].sizes[si].w = w;
				fl[fi].sizes[si].h = h;
				fl[fi].sizes[si].fps = (sc_fps_t*)calloc(64, sizeof(sc_fps_t));
				fl[fi].sizes[si].n_fps = 0;
			}
			if (si < 0) continue;

			/* Add the device-reported frame rate ranges as range entries (num==0 indicates range) */
			NSArray<AVFrameRateRange*>* ranges = f.videoSupportedFrameRateRanges;
			size_t cnt_f = fl[fi].sizes[si].n_fps;
			for (AVFrameRateRange* r in ranges) {
				if (cnt_f >= 64) break;
				if (!CMTIME_IS_VALID(r.minFrameDuration) || !CMTIME_IS_VALID(r.maxFrameDuration)) continue;
				sc_fps_t* fp = &fl[fi].sizes[si].fps[cnt_f++];
				if (CMTIME_COMPARE_INLINE(r.minFrameDuration, ==, r.maxFrameDuration)) {
					fp->num = (uint32_t) r.minFrameDuration.timescale;
					fp->den = (uint32_t) llabs(r.minFrameDuration.value);
					if (fp->den == 0) fp->den = 1;
					else {
						uint32_t d = gcd(fp->num,fp->den);
						fp->num /= d;
						fp->den /= d;
					}
				}
				else {
					fp->min_num = (uint32_t) r.minFrameDuration.timescale;
					fp->min_den = (uint32_t) llabs(r.minFrameDuration.value);
					fp->max_num = (uint32_t) r.maxFrameDuration.timescale;
					fp->max_den = (uint32_t) llabs(r.maxFrameDuration.value);
					if (fp->min_den == 0) fp->min_den = 1;
					else {
						uint32_t d = gcd(fp->min_num,fp->min_den);
						fp->min_num /= d;
						fp->min_den /= d;
					}
					if (fp->max_den == 0) fp->max_den = 1;
					else {
						uint32_t d = gcd(fp->max_num,fp->max_den);
						fp->max_num /= d;
						fp->max_den /= d;
					}
					fp->num = 0;
					fp->den = 0;
					fp->step_num = 0;
					fp->step_den = 0;
				}
			}
			fl[fi].sizes[si].n_fps = cnt_f;
		}
		*fmt_list = fl;
		return nfmt;
	}
}

int sc_start_capture(const char* device_id, uint32_t width, uint32_t height, sc_codec_t codec,
    uint32_t fps_num, uint32_t fps_den, sc_frame_callback_t cb, void* cb_ctx, sc_handle_t** out_handle)
{
	if (!out_handle || !device_id || !cb || width==0 || height==0 || fps_den==0 || fps_num==0) return -1;
	*out_handle = NULL;

	@autoreleasepool {
		NSString* uid = [NSString stringWithUTF8String:device_id];
		AVCaptureDevice* dev = sc_find_device_by_id(uid);
		if (!dev) return -1;

		/* Find an AVCaptureDeviceFormat whose subtype and dimensions match exactly */
		AVCaptureDeviceFormat* match = nil;
		for (AVCaptureDeviceFormat* f in dev.formats) {
			CMFormatDescriptionRef desc = f.formatDescription;
			if (!desc) continue;
			FourCharCode sub = CMFormatDescriptionGetMediaSubType(desc);
			CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(desc);
			if ((uint32_t)dim.width == width && (uint32_t)dim.height == height && sub == (FourCharCode)codec) {
				match = f;
				break;
			}
		}
		if (!match) return -1;

		NSError* err = nil;
		AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
		if (!input || err) return -1;

		AVCaptureSession* session = [[AVCaptureSession alloc] init];
		if (![session canAddInput:input]) return -1;
		[session addInput:input];

		AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
		/* Request the same pixel format on the output */
		NSDictionary* settings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(codec) };
		output.videoSettings = settings;
		output.alwaysDiscardsLateVideoFrames = YES;

		if (![session canAddOutput:output]) return -1;
		[session addOutput:output];

		/* Apply the format and select a supported frame duration close to the request */
		if ([dev lockForConfiguration:&err]) {
			dev.activeFormat = match;

			// Requested frame duration (seconds per frame = fps_den / fps_num)
			CMTime reqDur = CMTimeMake((int64_t)fps_den, (int32_t)fps_num);

			// Choose a target duration:
			// 1) if reqDur is within any supported range, use reqDur
			// 2) else use the largest maxFrameDuration that is <= reqDur (i.e., lowest fps above requested)
			// 3) else use the smallest minFrameDuration (i.e., highest possible fps)
			BOOL foundInRange = NO;
			CMTime bestLE = kCMTimeInvalid;      // best duration <= reqDur (closest from below => higher fps)
			CMTime fastest = kCMTimeInvalid;     // smallest minFrameDuration across ranges (highest fps)

			for (AVFrameRateRange* r in match.videoSupportedFrameRateRanges) {
				if (!CMTIME_IS_NUMERIC(r.minFrameDuration) || !CMTIME_IS_NUMERIC(r.maxFrameDuration)) {
					continue;
				}

				// Track highest possible fps (smallest duration)
				if (!CMTIME_IS_VALID(fastest) || CMTIME_COMPARE_INLINE(r.minFrameDuration, <, fastest)) {
					fastest = r.minFrameDuration;
				}

				// Check if requested duration is within this range
				if (CMTIME_COMPARE_INLINE(reqDur, >=, r.minFrameDuration) &&
					CMTIME_COMPARE_INLINE(reqDur, <=, r.maxFrameDuration)) {
					foundInRange = YES;
					break;
				}

				// If the requested duration is longer than the range's maxDuration (i.e., requested fps too low),
				// the closest allowed duration ≤ reqDur is r.maxFrameDuration.
				if (CMTIME_COMPARE_INLINE(r.maxFrameDuration, <=, reqDur)) {
					if (!CMTIME_IS_VALID(bestLE) || CMTIME_COMPARE_INLINE(r.maxFrameDuration, >, bestLE)) {
						bestLE = r.maxFrameDuration;
					}
				}
			}

			CMTime targetDur = reqDur;
			if (!foundInRange) {
				if (CMTIME_IS_VALID(bestLE)) {
					// Lowest possible frame rate higher than requested (duration just below or equal to reqDur)
					targetDur = bestLE;
				} else if (CMTIME_IS_VALID(fastest)) {
					// No higher rate than requested is supported -> pick highest possible rate
					targetDur = fastest;
				}
			}

			if (CMTIME_IS_NUMERIC(targetDur) && targetDur.value > 0 && targetDur.timescale > 0) {
				dev.activeVideoMinFrameDuration = targetDur;
				dev.activeVideoMaxFrameDuration = targetDur;
			}

			[dev unlockForConfiguration];
		} else {
			return -1;
		}

		struct sc_handle* hc = (struct sc_handle*)calloc(1, sizeof(struct sc_handle));
		if (!hc) return -1;
		hc->device = dev;
		hc->input = input;
		hc->session = session;
		hc->output = output;
		hc->cb = cb;
		hc->data_info.ctx = cb_ctx;
		hc->data_info.width = width;
		hc->data_info.height = height;
		hc->data_info.codec = codec;
		hc->stop = 0;

		SCAVFDelegate* del = [[SCAVFDelegate alloc] init];
		del.handle = hc;
		hc->delegate = del;

		dispatch_queue_t q = dispatch_queue_create("simple_capture.avf.queue", DISPATCH_QUEUE_SERIAL);
		hc->queue = q;
		[output setSampleBufferDelegate:del queue:q];

		[session startRunning];

		*out_handle = hc;
		return 0;
	}
}

void sc_stop_capture(sc_handle_t *handle) {
	if (!handle) return;
	@autoreleasepool {
		handle->stop = 1;
		if (handle->session) [handle->session stopRunning];
		if (handle->output) {
			[handle->output setSampleBufferDelegate:nil queue:NULL];
		}
		handle->session = nil;
		handle->output = nil;
		handle->input = nil;
		handle->device = nil;
		handle->delegate = nil;
		handle->queue = NULL;
		free(handle);
	}
}