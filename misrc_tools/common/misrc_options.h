#ifndef MISRC_OPTIONS_H
#define MISRC_OPTIONS_H

#include "misrc.h"


#define MISRC_OPT_RESAMPLE_A       256
#define MISRC_OPT_RESAMPLE_B       257
#define MISRC_OPT_RF_FLAC_12BIT    258
#define MISRC_OPT_RF_FLAC_BITS     258
#define MISRC_OPT_AUDIO_4CH_OUT    259
#define MISRC_OPT_AUDIO_2CH_12_OUT 260
#define MISRC_OPT_AUDIO_2CH_34_OUT 261
#define MISRC_OPT_AUDIO_1CH_1_OUT  262
#define MISRC_OPT_AUDIO_1CH_2_OUT  263
#define MISRC_OPT_AUDIO_1CH_3_OUT  264
#define MISRC_OPT_AUDIO_1CH_4_OUT  265
#define MISRC_OPT_RESAMPLE_QUAL_A  266
#define MISRC_OPT_RESAMPLE_QUAL_B  267
#define MISRC_OPT_RESAMPLE_GAIN_A  268
#define MISRC_OPT_RESAMPLE_GAIN_B  269
#define MISRC_OPT_8BIT_A           270
#define MISRC_OPT_8BIT_B           271


#define MISRC_SET_OPTION(t,s,o,x,v) (*(((t*)(((void*)s)+(o->setting_offset)))+x)=(t)v)

static char* sox_quality_options[] = { "QQ", "LQ", "MQ", "HQ", "VHQ" };
static char* flac_bits_options[] = { "auto", "12", "16" };

static int mirsc_opt_type_cnt[] = { 1, 1, 1, 2, 1, 2, 4 };

static misrc_option_t misrc_option_list[] = 
{
  {'d', "Input device", "device", "device index/name", NULL, "device index/name to use for capture", MISRC_OPTTYPE_CAPTURE_ALL, MISRC_ARGTYPE_STR, 0, { .s="0" }, { .i=0 }, { .i=0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, device) },
  {'n', "Number of samples to capture", "count", "n", "samples", "number of samples to capture", MISRC_OPTTYPE_CAPTURE_ALL, MISRC_ARGTYPE_INT, 0, { 0 }, { 0 }, { 0 }, "0 means infinite", NULL, NULL, offsetof(misrc_settings_t, total_samples_before_exit) },
  {'t', "Capture duration", "time", "time", "s, m:s or h:m:s", "time to capture", MISRC_OPTTYPE_CAPTURE_ALL, MISRC_ARGTYPE_STR, MISRC_OPTFLAG_CLIONLY, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, capture_time) },
  {'w', "Overwrite output", "overwrite", NULL, NULL, "overwrite any files without asking", MISRC_OPTTYPE_CAPTURE_ALL, MISRC_ARGTYPE_BOOL, 0, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, overwrite_files) },
  {'a', "RF A output file", "rf-a", "filename", NULL, "device index/name to use for capture", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_OUTFILE, 0, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_names_rf), },
  {'x', "AUX output file", "aux", "filename", NULL, "AUX output file", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_OUTFILE, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_name_aux) },
  {'r', "RAW data output file", "raw", "filename", NULL, "raw data output file", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_OUTFILE, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_name_raw) },
  {'p', "Pad RF output data", "pad", NULL, NULL, "pad lower 4 bits of 16 bit output with 0 instead of upper 4", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, pad)},
  {'L', "RF peak level display", "level", NULL, NULL, "display peak level of RF ADCs", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_CLIONLY, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, calc_level)},
  {'A', "Suppress clipping display", "suppress-clip-rf-a", NULL, NULL, "suppress clipping messages for this RF channel", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_CLIONLY, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, disable_clip)},
#if LIBSOXR_ENABLED == 1
  {MISRC_OPT_8BIT_A, "Reduce to 8 Bit", "8bit-rf-a", NULL, NULL, "reduce output from 12 bit to 8 bit for this RF channel", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, reduce_8bit) },
  {MISRC_OPT_RESAMPLE_A, "Resample to", "resample-rf-a", "samplerate", "kHz", "resample this RF channel to given sample rate", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_FLOAT, 0, { .f=40000.0 }, { .f=1.0  }, { .f=40000.0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, resample_rate) },
  {MISRC_OPT_RESAMPLE_QUAL_A, "Resample quality", "resample-rf-quality-a", "quality", NULL, "resample quality of this RF channel", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_LIST, MISRC_OPTFLAG_ADVANCED, { 3 }, { 0 }, { 4 }, "lowest quality, less processing intensive", "very high quality, more processing intensive", sox_quality_options, offsetof(misrc_settings_t, resample_qual) },
  {MISRC_OPT_RESAMPLE_GAIN_A, "Gain during resampling", "resample-rf-gain-a", "gain", "dB", "apply gain during resampling of this RF channel", MISRC_OPTTYPE_CAPTURE_RFC, MISRC_ARGTYPE_FLOAT, MISRC_OPTFLAG_ADVANCED, { .f=0.0 }, { .f=-72.0  }, { .f=72.0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, resample_gain) },
#endif
#if LIBFLAC_ENABLED == 1
  {'f', "FLAC compression", "rf-flac", NULL, NULL, "compress RF ADC output as FLAC", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_BOOL, 0, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, flac_enable) },
  {MISRC_OPT_RF_FLAC_12BIT, "FLAC 12 Bit", "rf-flac-12bit", NULL, NULL, "set RF FLAC bith depth to 12 bit", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_ADVANCED | MISRC_OPTFLAG_LEGACY, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, flac_12bit) },
  {MISRC_OPT_RF_FLAC_BITS, "FLAC bit depth", "rf-flac-bits", "bits", NULL, "Set the RF FLAC bit depth field", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_LIST, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 2 }, NULL, NULL, flac_bits_options, offsetof(misrc_settings_t, flac_bits) },
  {'l', "FLAC compression level", "rf-flac-level", "level", NULL, "set RF flac compression level", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_INT, MISRC_OPTFLAG_ADVANCED, { 1 }, { 0 }, { 8 }, "lowest compression, less processing intensive", "highest compression, more processing intensive", NULL, offsetof(misrc_settings_t, flac_level) },
  {'v', "FLAC encoding validation", "rf-flac-verification", NULL, NULL, "enable verification of RF flac encoder output", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_BOOL, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, flac_verify) },
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
  {'c', "FLAC encoding threads", "rf-flac-threads", "threads", NULL, "number of RF flac encoding threads per file", MISRC_OPTTYPE_CAPTURE_RF, MISRC_ARGTYPE_INT, MISRC_OPTFLAG_ADVANCED, { 0 }, { 0 }, { 128 }, "0 means automatic detection", NULL, NULL, offsetof(misrc_settings_t, flac_threads) },
#endif
#endif
  {MISRC_OPT_AUDIO_4CH_OUT, "4ch output file", "audio-4ch", "filename", NULL, "4 channel audio output", MISRC_OPTTYPE_CAPTURE_AUDIO_ALL, MISRC_ARGTYPE_OUTFILE, 0, { .i=0 }, { .i=0 }, { .i=0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_name_4ch_audio) },
  {MISRC_OPT_AUDIO_2CH_12_OUT, "Stereo output file", "audio-2ch-12", "filename", NULL, "stereo audio output", MISRC_OPTTYPE_CAPTURE_AUDIO_STEREO, MISRC_ARGTYPE_OUTFILE, 0, { .i=0 }, { .i=0 }, { .i=0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_names_2ch_audio) },
  {MISRC_OPT_AUDIO_1CH_1_OUT, "Mono output file", "audio-1ch-1", "filename", NULL, "mono audio output", MISRC_OPTTYPE_CAPTURE_AUDIO_MONO, MISRC_ARGTYPE_OUTFILE, 0, { .i=0 }, { .i=0 }, { .i=0 }, NULL, NULL, NULL, offsetof(misrc_settings_t, output_names_1ch_audio) },
  { 0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, { .i=0 }, { .i=0 }, { .i=0 }, NULL, NULL, NULL, 0 },
};

#endif // MISRC_OPTIONS_H