#ifndef MISRC_H
#define MISRC_H

#include <stddef.h>
#include <stdbool.h> 
#include <stdint.h>

#if LIBFLAC_ENABLED == 1
#include "FLAC/export.h"
#endif

#define BUFFER_AUDIO_TOTAL_SIZE 65536*256
#define BUFFER_AUDIO_READ_SIZE 65536*3
#define BUFFER_TOTAL_SIZE 65536*1024
#define BUFFER_READ_SIZE 65536*32

enum misrc_msg_level {
	MISRC_MSG_INFO=0,		/* message that informs about normal opartion */
	MISRC_MSG_WARNING,		/* warning, may cause data loss/corruption */
	MISRC_MSG_ERROR,		/* error, data is lost/corrupted */
	MISRC_MSG_CRITICAL		/* critical error, capture cannot continue */
};

enum misrc_count_type {
	MISRC_COUNT_NONSYNC_FRAMES,		/* count of frames without sync */
	MISRC_COUNT_TOTAL_SAMPLES_END	/* capturing finished with this number of samples */
};

enum misrc_device_type {
	MISRC_DEV_HSDAOH=0,		/* access over libusb/libuvc/libhsdaoh */
	MISRC_DEV_GENERIC_SC	/* access using OS capture API */
};

typedef struct {
	enum misrc_device_type type;
	char *dev_id;
	char *dev_name;
} misrc_device_info_t;

typedef struct {
	bool use_crc;
	bool use_stream_id;
} misrc_sync_info_t;

typedef bool(*misrc_overwrite_cb_t)(void *ctx, char *filename);
typedef void(*misrc_stats_cb_t)(void *ctx, size_t count, size_t *clip, uint16_t *level);
typedef void(*misrc_count_cb_t)(void *ctx, enum misrc_count_type count_type, size_t count);
typedef void(*misrc_message_cb_t)(void *ctx, enum misrc_msg_level level, const char *format, ...);
typedef void(*misrc_sync_cb_t)(void *ctx, misrc_sync_info_t *sync_info);

typedef struct {
	int short_opt;
	char *opt_name; // for GUI
	char *long_opt; // for CLI
	char *arg_name;
	char *arg_unit;
	char *desc; // for help
	int opt_type;
	int arg_type;
	int opt_flags;
	union {
		int64_t i;
		double f;
		char *s;
	} default_arg;
	union {
		int64_t i;
		double f;
	} min_arg;
	union {
		int64_t i;
		double f;
	} max_arg;
	char *min_desc;
	char *max_desc;
	char **arg_list; // for list type
	intptr_t setting_offset;
} misrc_option_t;

typedef struct {
	char *device;
	bool pad;
	bool calc_level;
	bool disable_clip[2];
#if LIBFLAC_ENABLED == 1
	uint64_t flac_level;
	bool flac_enable;
	bool flac_verify;
	bool flac_12bit;
	uint64_t flac_bits;
	uint64_t flac_threads;
#endif
#if LIBSOXR_ENABLED == 1
	double resample_rate[2];
	uint64_t resample_qual[2];
	double resample_gain[2];
	bool reduce_8bit[2];
#endif
	// output file names
	char *output_names_rf[2];
	char *output_name_aux;
	char *output_name_raw;
	char *output_name_4ch_audio;
	char *output_names_2ch_audio[2];
	char *output_names_1ch_audio[4];
	//overwrite option
	bool overwrite_files;
	//number of samples to take
	uint64_t total_samples_before_exit;
	char *capture_time;
	// callbacks:
	misrc_count_cb_t count_cb;
	misrc_message_cb_t msg_cb;
	misrc_overwrite_cb_t overwrite_cb;
	misrc_stats_cb_t stats_cb;
	misrc_sync_cb_t sync_cb;
	void *count_cb_ctx;
	void *msg_cb_ctx;
	void *overwrite_cb_ctx;
	void *stats_cb_ctx;
	void *sync_cb_ctx;
	// input data
	// output data
} misrc_settings_t;

#define MISRC_OPTTYPE_OTHER                0
#define MISRC_OPTTYPE_CAPTURE_ALL          1 // options that effect the eintire capture
#define MISRC_OPTTYPE_CAPTURE_RF           3 // options that effect both RF captures
#define MISRC_OPTTYPE_CAPTURE_RFC          4 // options that effect each RF channel independentaly (option is array[2])
#define MISRC_OPTTYPE_CAPTURE_AUDIO_ALL    5 // options that effect all channels
#define MISRC_OPTTYPE_CAPTURE_AUDIO_STEREO 6 // options that effect all stereo audio (option is array[2])
#define MISRC_OPTTYPE_CAPTURE_AUDIO_MONO   7 // options that effect each audio channel independently (option is array[4])

#define MISRC_OPTFLAG_ADVANCED 1
#define MISRC_OPTFLAG_CLIONLY  2
#define MISRC_OPTFLAG_GUIONLY  4
#define MISRC_OPTFLAG_LEGACY   8

/* all argtypes are 64 bit */
// int types
#define MISRC_ARGTYPE_BOOL    0
#define MISRC_ARGTYPE_INT     1
#define MISRC_ARGTYPE_LIST    2
// float types
#define MISRC_ARGTYPE_FLOAT   3
// char* types
#define MISRC_ARGTYPE_INFILE  4
#define MISRC_ARGTYPE_OUTFILE 5
#define MISRC_ARGTYPE_STR     6

/* return values */
#define MISRC_RET_CAPTURE_OK         0
#define MISRC_RET_INVALID_SETTINGS  -1
#define MISRC_RET_HARDWARE_ERROR    -2
#define MISRC_RET_FILE_ERROR        -3
#define MISRC_RET_USER_ABORT        -4
#define MISRC_RET_THREAD_ERROR      -5
#define MISRC_RET_MEMORY_ERROR      -6

#if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

int misrc_run_capture(misrc_settings_t *set);
void misrc_stop_capture();
void misrc_capture_set_default(misrc_settings_t *set, misrc_option_t *opt);
void misrc_list_devices(misrc_device_info_t **dev_info, size_t *n);
char* misrc_sc_capture_impl_name();

#endif // MISRC_H
