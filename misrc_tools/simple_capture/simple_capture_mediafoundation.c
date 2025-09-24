/*
* simple_capture OS abstraction library - MediaFoundation implementation
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

#define COBJMACROS
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "simple_capture.h"

struct sc_handle {
	IMFSourceReader* reader;
	HANDLE th;
	volatile LONG stop;
	sc_frame_callback_t cb;
	sc_data_info_t data_info;
};

/* Simple one-time MF startup */
static LONG g_mf_started = 0;
static void mf_startup(void) {
	if (InterlockedCompareExchange(&g_mf_started, 1, 0) == 0) {
		CoInitializeEx(NULL, COINIT_MULTITHREADED);
		MFStartup(MF_VERSION, MFSTARTUP_FULL);
	}
}

char* sc_get_impl_name() {
	return "Media Foundation";
}

char* sc_get_impl_name_short() {
	return "mf";
}

/* Local helpers to avoid MFGetAttributeSize/Ratio dependency */
static HRESULT sc_MFGetAttribute(IMFAttributes* a, const GUID* key, UINT32* w, UINT32* h) {
	if (!a || !key) return E_POINTER;
	UINT64 v = 0;
	HRESULT hr = IMFAttributes_GetUINT64(a, key, &v);
	if (FAILED(hr)) return hr;
	if (w) *w = (UINT32)(v >> 32);
	if (h) *h = (UINT32)(v & 0xFFFFFFFFu);
	return S_OK;
}

static HRESULT sc_MFSetAttribute(IMFAttributes* a, const GUID* key, UINT32 w, UINT32 h) {
	if (!a || !key) return E_POINTER;
	UINT64 v = (((UINT64)w) << 32) | (UINT64)h;
	return IMFAttributes_SetUINT64(a, key, v);
}

static char* utf16_to_utf8(const wchar_t* w) {
	if (!w) return _strdup("");
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	if (n <= 0) return _strdup("");
	char* s = (char*)malloc((size_t)n);
	if (!s) return NULL;
	WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
	return s;
}

size_t sc_get_devices(sc_capture_dev_t **dev_list) {
	if (!dev_list) return 0;
	*dev_list = NULL;
	mf_startup();

	IMFAttributes* attr = NULL;
	IMFActivate** devs = NULL;
	UINT32 count = 0;
	size_t out_count = 0;

	if (FAILED(MFCreateAttributes(&attr, 1))) goto done;
	if (FAILED(IMFAttributes_SetGUID(attr, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) goto done;
	if (FAILED(MFEnumDeviceSources(attr, &devs, &count))) goto done;

	sc_capture_dev_t* list = (sc_capture_dev_t*)calloc(64, sizeof(sc_capture_dev_t));
	if (!list) goto done;

	for (UINT32 i=0; i<count && out_count<64; ++i) {
		wchar_t* wname = NULL;
		wchar_t* wlink = NULL;
		UINT32 cch = 0;

		IMFActivate_GetAllocatedString(devs[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &wname, &cch);
		IMFActivate_GetAllocatedString(devs[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wlink, &cch);

		list[out_count].name = utf16_to_utf8(wname);
		list[out_count].device_id = utf16_to_utf8(wlink);
		out_count++;

		if (wname) CoTaskMemFree(wname);
		if (wlink) CoTaskMemFree(wlink);
	}
	*dev_list = list;

done:
	if (devs) {
		for (UINT32 i=0; i<count; ++i) if (devs[i]) IMFActivate_Release(devs[i]);
		CoTaskMemFree(devs);
	}
	if (attr) IMFAttributes_Release(attr);
	return out_count;
}

static int fmt_index_for(sc_formatlist_t* fl, size_t nf, const sc_codec_t* codec) {
	for (size_t i=0; i<nf; ++i) if (SC_CODEC_EQUAL(fl[i].codec, *codec)) return (int)i;
	return -1;
}
static int size_index_for(sc_size_t* sz, size_t ns, uint32_t w, uint32_t h) {
	for (size_t i=0; i<ns; ++i) if (sz[i].w==w && sz[i].h==h) return (int)i;
	return -1;
}

size_t sc_get_formats(char* device_id, sc_formatlist_t **fmt_list) {
	if (!device_id || !fmt_list) return 0;
	*fmt_list = NULL;
	mf_startup();

	IMFAttributes* attr = NULL;
	IMFActivate** devs = NULL;
	UINT32 count = 0;
	size_t nfmt = 0;
	sc_formatlist_t* fl = (sc_formatlist_t*)calloc(64, sizeof(sc_formatlist_t));
	if (!fl) return 0;

	if (FAILED(MFCreateAttributes(&attr, 1))) goto done;
	if (FAILED(IMFAttributes_SetGUID(attr, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) goto done;
	if (FAILED(MFEnumDeviceSources(attr, &devs, &count))) goto done;

	wchar_t wreq[1024];
	MultiByteToWideChar(CP_UTF8, 0, device_id, -1, wreq, 1024);

	IMFMediaSource* src = NULL;
	for (UINT32 i=0; i<count; ++i) {
		wchar_t* wlink = NULL; UINT32 cch = 0;
		if (SUCCEEDED(IMFActivate_GetAllocatedString(devs[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wlink, &cch))) {
			if (wlink && wcscmp(wreq, wlink)==0) {
				IMFActivate_ActivateObject(devs[i], &IID_IMFMediaSource, (void**)&src);
			}
			if (wlink) CoTaskMemFree(wlink);
		}
		if (src) break;
	}
	if (!src) goto done;

	IMFSourceReader* reader = NULL;
	if (FAILED(MFCreateSourceReaderFromMediaSource(src, NULL, &reader))) {
		IMFMediaSource_Release(src);
		goto done;
	}

	for (DWORD i=0;; ++i) {
		IMFMediaType* mt = NULL;
		HRESULT hr = IMFSourceReader_GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mt);
		if (hr == MF_E_NO_MORE_TYPES) { break; }
		if (FAILED(hr) || !mt) { break; }

		GUID major = {0}, sub = {0};
		UINT32 w = 0, h = 0;
		UINT32 fr_num = 0, fr_den = 0;
		UINT32 frmin_num = 0, frmin_den = 0;
		UINT32 frmax_num = 0, frmax_den = 0;
		if (FAILED(IMFMediaType_GetGUID(mt, &MF_MT_MAJOR_TYPE, &major)) || !IsEqualGUID(&major, &MFMediaType_Video)) {
			IMFMediaType_Release(mt); continue;
		}
		if (FAILED(IMFMediaType_GetGUID(mt, &MF_MT_SUBTYPE, &sub))) {
			IMFMediaType_Release(mt); continue;
		}

		if (FAILED(sc_MFGetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_SIZE, &w, &h))) {
			IMFMediaType_Release(mt); continue;
		}
		
		BOOL haveRange = SUCCEEDED(sc_MFGetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_RATE_RANGE_MIN, &frmin_num, &frmin_den)) &&
				 SUCCEEDED(sc_MFGetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_RATE_RANGE_MAX, &frmax_num, &frmax_den));

		if (!(haveRange && frmin_den && frmax_den)) {
			haveRange = FALSE;
			if (FAILED(sc_MFGetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_RATE, &fr_num, &fr_den))) {
				fr_num = 0; fr_den = 0;
			}
		}
		
		sc_codec_t codec = sub;

		int fi = fmt_index_for(fl, nfmt, &codec);
		if (fi < 0 && nfmt < 64) {
			fi = (int)nfmt++;
			fl[fi].codec = codec;
			fl[fi].sizes = (sc_size_t*)calloc(64, sizeof(sc_size_t));
			fl[fi].n_sizes = 0;
		}
		if (fi < 0) { IMFMediaType_Release(mt); continue; }

		int si = size_index_for(fl[fi].sizes, fl[fi].n_sizes, w, h);
		if (si < 0 && fl[fi].n_sizes < 64) {
			si = (int)fl[fi].n_sizes++;
			fl[fi].sizes[si].w = w;
			fl[fi].sizes[si].h = h;
			fl[fi].sizes[si].fps = (sc_fps_t*)calloc(64, sizeof(sc_fps_t));
			fl[fi].sizes[si].n_fps = 0;
		}
		if (si >= 0) {
			size_t nf = fl[fi].sizes[si].n_fps;
			if (nf < 64) {
				sc_fps_t* fp = &fl[fi].sizes[si].fps[nf];
				if (haveRange) {
					fp->num = 0;
					fp->den = 0;
					fp->min_num = frmin_num;
					fp->min_den = frmin_den;
					fp->max_num = frmax_num;
					fp->max_den = frmax_den;
				}
				else {
					fp->num = fr_num;
					fp->den = fr_den == 0 ? 1 : fr_den;
				}
				fl[fi].sizes[si].n_fps++;
			}
		}

		IMFMediaType_Release(mt);
	}

	*fmt_list = fl;

	IMFSourceReader_Release(reader);
	IMFMediaSource_Release(src);

done:
	if (!*fmt_list) free(fl);
	if (devs) {
		for (UINT32 i=0; i<count; ++i) if (devs[i]) IMFActivate_Release(devs[i]);
		CoTaskMemFree(devs);
	}
	if (attr) IMFAttributes_Release(attr);
	return nfmt;
}

static unsigned __stdcall mf_thread(void* p) {
	struct sc_handle* hc = (struct sc_handle*)p;
	for (;;) {
		if (InterlockedCompareExchange(&hc->stop, 0, 0)) break;

		DWORD stream = 0, flags = 0;
		LONGLONG ts = 0;
		IMFSample* sample = NULL;
		HRESULT hr = IMFSourceReader_ReadSample(hc->reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream, &flags, &ts, &sample);
		if (FAILED(hr)) { Sleep(1); continue; }
		if (!sample) continue;

		IMFMediaBuffer* buf = NULL;
		hr = IMFSample_ConvertToContiguousBuffer(sample, &buf);
		if (SUCCEEDED(hr) && buf) {
			BYTE* pData = NULL;
			DWORD maxLen = 0, curLen = 0;
			if (SUCCEEDED(IMFMediaBuffer_Lock(buf, &pData, &maxLen, &curLen))) {
				hc->data_info.data = pData;
				hc->data_info.len = (uint32_t)curLen;
				if (hc->cb && hc->data_info.data && hc->data_info.len > 0) {
					hc->cb(&(hc->data_info));
				}
				IMFMediaBuffer_Unlock(buf);
			}
			IMFMediaBuffer_Release(buf);
		}
		IMFSample_Release(sample);
	}
	return 0;
}

int sc_start_capture(const char* device_id, uint32_t width, uint32_t height, sc_codec_t codec,
    uint32_t fps_num, uint32_t fps_den, sc_frame_callback_t cb, void* cb_ctx, sc_handle_t** out_handle)
{
	if (!out_handle || !device_id || !cb || width==0 || height==0 || fps_den==0 || fps_num==0) return -1;
	*out_handle = NULL;
	mf_startup();

	IMFAttributes* attr = NULL;
	IMFActivate** devs = NULL;
	UINT32 count = 0;
	IMFMediaSource* src = NULL;
	IMFSourceReader* reader = NULL;
	int ret = -1;

	if (FAILED(MFCreateAttributes(&attr, 1))) goto done;
	if (FAILED(IMFAttributes_SetGUID(attr, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) goto done;
	if (FAILED(MFEnumDeviceSources(attr, &devs, &count))) goto done;

	wchar_t wreq[1024];
	MultiByteToWideChar(CP_UTF8, 0, device_id, -1, wreq, 1024);

	for (UINT32 i=0; i<count; ++i) {
		wchar_t* wlink = NULL; UINT32 cch=0;
		if (SUCCEEDED(IMFActivate_GetAllocatedString(devs[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wlink, &cch))) {
			if (wlink && wcscmp(wreq, wlink)==0) {
				IMFActivate_ActivateObject(devs[i], &IID_IMFMediaSource, (void**)&src);
			}
			if (wlink) CoTaskMemFree(wlink);
		}
		if (src) break;
	}
	if (!src) goto done;

	if (FAILED(MFCreateSourceReaderFromMediaSource(src, NULL, &reader))) goto done;

	/* Request the exact type directly using sc_codec_t (GUID) */
	IMFMediaType* mt = NULL;
	if (FAILED(MFCreateMediaType(&mt))) goto done;
	if (FAILED(IMFMediaType_SetGUID(mt, &MF_MT_MAJOR_TYPE, &MFMediaType_Video))) goto done;
	if (FAILED(IMFMediaType_SetGUID(mt, &MF_MT_SUBTYPE, &codec))) goto done;
	if (FAILED(sc_MFSetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_SIZE, width, height))) goto done;
	if (FAILED(sc_MFSetAttribute((IMFAttributes*)mt, &MF_MT_FRAME_RATE, fps_num, fps_den))) goto done;

	/* Best-effort set; some drivers adjust to nearest supported type */
	IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, mt);
	IMFMediaType_Release(mt);
	mt = NULL;

	/* Query actual set type to reflect final dimensions */
	IMFMediaType* act = NULL;
	if (SUCCEEDED(IMFSourceReader_GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, &act))) {
		UINT32 w=0,h=0;
		if (SUCCEEDED(sc_MFGetAttribute((IMFAttributes*)act, &MF_MT_FRAME_SIZE, &w, &h))) {
			width = w; height = h;
		}
		GUID sub = {0};
		if (SUCCEEDED(IMFMediaType_GetGUID(act, &MF_MT_SUBTYPE, &sub))) {
			codec = sub; /* update to actual subtype if driver chose a close match */
		}
		IMFMediaType_Release(act);
	}

	struct sc_handle* hc = (struct sc_handle*)calloc(1, sizeof(struct sc_handle));
	if (!hc) goto done;

	hc->reader = reader;
	reader = NULL;
	hc->cb = cb;
	hc->data_info.ctx = cb_ctx;
	hc->data_info.width = width;
	hc->data_info.height = height;
	hc->data_info.codec = codec;
	hc->stop = 0;

	uintptr_t th = _beginthreadex(NULL, 0, mf_thread, hc, 0, NULL);
	if (!th) { free(hc); goto done; }
	hc->th = (HANDLE)th;

	*out_handle = hc;
	ret = 0;

done:
	if (reader) IMFSourceReader_Release(reader);
	if (src) IMFMediaSource_Release(src);
	if (devs) {
		for (UINT32 i=0; i<count; ++i) if (devs[i]) IMFActivate_Release(devs[i]);
		CoTaskMemFree(devs);
	}
	if (attr) IMFAttributes_Release(attr);
	return ret;
}

void sc_stop_capture(sc_handle_t *handle) {
	if (!handle) return;
	InterlockedExchange(&handle->stop, 1);
	if (handle->th) {
		WaitForSingleObject(handle->th, INFINITE);
		CloseHandle(handle->th);
	}
	if (handle->reader) IMFSourceReader_Release(handle->reader);
	free(handle);
}