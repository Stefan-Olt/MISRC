/*
* MISRC capture
* Copyright (C) 2024-2026  vrunk11, stefan_o
* 
* based on:
* hsdaoh - High Speed Data Acquisition over MS213x USB3 HDMI capture sticks
* Copyright (C) 2024 by Steve Markgraf <steve@steve-m.de>
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

#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>

#if !defined(_WIN32) || defined(__MINGW32__)
	#include <getopt.h>
#else
	#include "getopt/getopt.h"
#endif

#if defined(_WIN32)
	#include <windows.h>
#endif

#include "misrc.h"
#include "misrc_options.h"

#include "../version.h"

#define OPT_LIST_DEVICES     1000
#define OPT_FULL_HELP        1001

static int new_line = 1;

char* strdup_inc(const char *s, size_t n, int m)
{
	if (!s) return NULL;
	size_t len = strlen(s);
	char *out = (char *)malloc(len + 1);
	if (!out) return NULL;
	memcpy(out, s, len + 1);
	size_t start = (n >= len) ? 0 : (len - n);
	for (size_t i = start; i < len; ++i) {
		unsigned char c = (unsigned char)out[i];
		c = (unsigned char)(c + m);
		out[i] = (char)c;
	}

	return out;
}

static void create_getopt_string(char *getopt_string, struct option *getopt_long_options)
{
	char* s = getopt_string;
	int i = 0;
	while (1) {
		if (getopt_long_options[i].name == 0) break;
		if(getopt_long_options[i].val < 256) {
			*s = getopt_long_options[i].val;
			s++;
			if(getopt_long_options[i].has_arg != no_argument) {
				*s = ':';
				s++;
			}
		}
		i++;
	}
	*s = 0;
}


static void print_option(misrc_option_t *opt, int add)
{
	fprintf(stderr,"  ");
	if(opt->short_opt < 256) fprintf(stderr,"-%c, ",opt->short_opt+add);
	if (add==0) fprintf(stderr,"--%s",opt->long_opt);
	else {
		char *s = strdup_inc(opt->long_opt, (opt->opt_type==MISRC_OPTTYPE_CAPTURE_AUDIO_STEREO) ? 2:1, add);
		fprintf(stderr,"--%s",s);
		free(s);
	}
	if(opt->arg_type!=MISRC_ARGTYPE_BOOL && opt->arg_name!=NULL) fprintf(stderr," [%s]\n",opt->arg_name);
	else fprintf(stderr,"\n");
	fprintf(stderr,"     %s",opt->desc);
	if(opt->arg_name!=NULL && opt->arg_unit!=NULL) fprintf(stderr,", %s in %s\n",opt->arg_name, opt->arg_unit);
	else fprintf(stderr,"\n");
	if(opt->arg_type == MISRC_ARGTYPE_INT || opt->arg_type == MISRC_ARGTYPE_FLOAT) {
		if (opt->min_arg.i != opt->max_arg.i) {
			if(opt->arg_type == MISRC_ARGTYPE_INT) fprintf(stderr, "     allowed range from %lli ", opt->min_arg.i);
			else fprintf(stderr, "     allowed range from %.1f ", opt->min_arg.f);
			if(opt->arg_unit!=NULL) fprintf(stderr, "%s ", opt->arg_unit);
			if(opt->min_desc!=NULL) fprintf(stderr, "(%s) ", opt->min_desc);
			fprintf(stderr, "to ");
			if(opt->arg_type == MISRC_ARGTYPE_INT) fprintf(stderr, "%lli", opt->max_arg.i);
			else fprintf(stderr, "%.1f", opt->max_arg.f);
			if(opt->arg_unit!=NULL) fprintf(stderr, " %s", opt->arg_unit);
			if(opt->max_desc!=NULL) fprintf(stderr, " (%s)", opt->max_desc);
			fprintf(stderr, "\n");
		}
		if(opt->arg_type == MISRC_ARGTYPE_INT) fprintf(stderr, "     default value: %lli", opt->default_arg.i);
		else fprintf(stderr, "     default value: %.1f", opt->default_arg.f);
		if(opt->arg_unit!=NULL) fprintf(stderr, " %s\n", opt->arg_unit);
		else fprintf(stderr, "\n");
	}
	else if(opt->arg_type == MISRC_ARGTYPE_LIST) {
		if(opt->arg_name!=NULL) fprintf(stderr, "     possible values for %s: ",opt->arg_name);
		else fprintf(stderr, "     possible values: ");
		int64_t i;
		fprintf(stderr, "%s",opt->arg_list[opt->min_arg.i]);
		if(opt->min_desc!=NULL) fprintf(stderr, " (%s), ", opt->min_desc);
		else fprintf(stderr, ", ");
		for(i=opt->min_arg.i+1;i<opt->max_arg.i;i++) fprintf(stderr, "%s, ",opt->arg_list[i]);
		fprintf(stderr, "%s",opt->arg_list[i]);
		if(opt->max_desc!=NULL) fprintf(stderr, " (%s)\n", opt->max_desc);
		else fprintf(stderr, "\n");
		fprintf(stderr, "     default value: %s\n", opt->arg_list[opt->default_arg.i]);
	}
	else if(opt->default_arg.s!=NULL) fprintf(stderr, "     default value: %s\n", opt->default_arg.s);
}

static void usage(bool advanced)
{
	if (advanced) fprintf(stderr, "Usage (all options):\n");
	else fprintf(stderr, "Usage (common options):\n");
	misrc_option_t *cur_opt = misrc_option_list;
	fprintf(stderr,"\n General options:\n");
	fprintf(stderr,"  --devices:\n");
	fprintf(stderr,"     list available devices\n");
	fprintf(stderr,"  --full-help:\n");
	fprintf(stderr,"     show help with all advanced options\n");
	fprintf(stderr,"\n Common capture options:\n");
	while (cur_opt->short_opt!=0) {
		if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_ALL) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
			print_option(cur_opt, 0);
		}
		cur_opt++;
	}
	fprintf(stderr,"\n Common RF capture options:\n");
	cur_opt = misrc_option_list;
	while (cur_opt->short_opt!=0) {
		if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_RF) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
			print_option(cur_opt, 0);
		}
		cur_opt++;
	}
	for(int i=0; i<2; i++) {
		fprintf(stderr,"\n RF Channel %i capture options:\n",i+1);
		cur_opt = misrc_option_list;
		while (cur_opt->short_opt!=0) {
			if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_RFC) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
				print_option(cur_opt, i);
			}
			cur_opt++;
		}
	}
	fprintf(stderr,"\n Audio capture options (all channels):\n");
	cur_opt = misrc_option_list;
	while (cur_opt->short_opt!=0) {
		if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_AUDIO_ALL) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
			print_option(cur_opt, 0);
		}
		cur_opt++;
	}
	for(int i=0; i<3; i+=2) {
		fprintf(stderr,"\n Audio capture options (stereo channels %i/%i):\n",i+1,i+2);
		cur_opt = misrc_option_list;
		while (cur_opt->short_opt!=0) {
			if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_AUDIO_STEREO) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
				print_option(cur_opt, i);
			}
			cur_opt++;
		}
	}
	for(int i=0; i<4; i++) {
		fprintf(stderr,"\n Audio capture options (mono channel %i):\n",i+1);
		cur_opt = misrc_option_list;
		while (cur_opt->short_opt!=0) {
			if ((cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_AUDIO_MONO) && ((cur_opt->opt_flags&MISRC_OPTFLAG_LEGACY) == 0) && (advanced || ((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == 0))) {
				print_option(cur_opt, i);
			}
			cur_opt++;
		}
	}
	exit(1);
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
	if (CTRL_C_EVENT == signum) {
		fprintf(stderr, "Signal caught, exiting!\n");
		misrc_stop_capture();
		return true;
	}
	return FALSE;
}
#else
static void sighandler(int UNUSED(signum))
{
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "Signal caught, exiting!\n");
	misrc_stop_capture();
}
#endif

static bool ask_overwrite(void UNUSED(*ctx), char *filename)
{
	char ch = 0;
	fprintf(stderr, "File '%s' already exists. Overwrite? (y/n) ", filename);
	(void)scanf(" %c",&ch);
	if (ch != 'y' && ch != 'Y') return false;
	return true;
}

static void print_misrc_message(void UNUSED(*ctx), enum misrc_msg_level level, const char *format, ...)
{
	char msg[256];
	char *msg_level[] = {"", "WARNING: ", "ERROR: ", "CRITICAL ERROR: "};
	va_list args;
	va_start(args, format);
	vsnprintf(msg,256,format,args);
	va_end(args);
	fprintf(stderr, "%s%s\n",msg_level[level],msg);
	new_line = 1;
}

static void print_misrc_count(void UNUSED(*ctx), enum misrc_count_type type, size_t count)
{
	switch(type) {
	case MISRC_COUNT_NONSYNC_FRAMES:
		if(count%5==0) {
			if(new_line) fprintf(stderr,"\n");
			fprintf(stderr,"\033[A\33[2K\r Received %lu frames without sync...\n",count);
		}
		break;
	case MISRC_COUNT_TOTAL_SAMPLES_END:
		new_line=1;
		fprintf(stderr,"%" PRIu64 " total samples have been collected, exiting early!\n",count);
		break;
	}
}

static void print_misrc_sync(void UNUSED(*ctx), misrc_sync_info_t *info)
{
	fprintf(stderr, "Syncronized to HDMI input stream\n MISRC uses CRC: %s\n MISRC uses stream ids: %s\n\n", info->use_crc ? "yes" : "no", info->use_stream_id ? "yes" : "no");
	new_line=1;
}

static void print_level(char ch, uint16_t level)
{
	float db_level = 20.0f * log10((float)level / 2048.0f);
	// the idea is a non-linear scale similar to vu meters
	uint8_t count = (uint8_t) lroundf( 70.0f/(1.0f + exp(0.163f*(-15.0f - db_level))));
	char full[] = "################################################################";
	char none[] = "                                                                ";
	full[count] = 0;
	none[64-count] = 0;
	fprintf(stderr, "\33[2K\r %c [%s%s] %5.1f dB\n", ch, full, none, db_level);
}

static void print_progress_level_clip(void *ctx, size_t count, size_t *clip, uint16_t *level)
{
	misrc_settings_t *set = (misrc_settings_t*)ctx;
	char rfi[] = {'A','B'};

	if(count % (BUFFER_READ_SIZE<<1) != 0) return;

	for(int i=0; i<2; i++) {
		if (!set->disable_clip[i] && clip[i]>0 && (set->output_names_rf[i]!=NULL || set->output_name_raw!=NULL)) {
			fprintf(stderr,"RF %c: %zu samples clipped\n",rfi[i],clip[i]);
			clip[i] = 0;
			new_line = 1;
		}
	}
	if(new_line) {
		fprintf(stderr,"\n");
		if(set->calc_level) fprintf(stderr,"\n\n");
	}
	new_line = 0;

	if (set->calc_level) {
		fprintf(stderr,"\033[A\033[A\033[A");
		for(int i=0; i<2; i++) print_level(rfi[i], level[i]);
	}
	else fprintf(stderr,"\033[A");
	fprintf(stderr,"\33[2K\r Progress: %13" PRIu64 " samples, %2uh %2um %2us\n", count, (uint32_t)(count/(144000000000)), (uint32_t)((count/(2400000000)) % 60), (uint32_t)((count/(40000000)) % 60));
	fflush(stderr);
}

static void print_invalid_arg(misrc_option_t *opt, char *optarg)
{
	if(opt->short_opt<256) fprintf(stderr, "CRITICAL ERROR: The argument \"%s\" is invalid for option \"-%c\"\n\n", optarg, opt->short_opt);
	else fprintf(stderr, "CRITICAL ERROR: The argument \"%s\" is invalid for option \"--%s\"\n\n", optarg, opt->arg_name);
}

static void list_devices()
{
	misrc_device_info_t *dev_list;
	size_t n;
	char* device_type[] = {"hsdaoh with libusb/libuvc access", misrc_sc_capture_impl_name() };
	misrc_list_devices(&dev_list, &n);
	fprintf(stderr, "Devices that could potentially be used for misrc_capture\n");
	for (size_t i=0; i<n; i++) {
		fprintf(stderr," %s: %s (using %s)\n",dev_list[i].dev_id, dev_list[i].dev_name, device_type[dev_list[i].type]);
	}
	fprintf(stderr, "\nDevice names can change when devices are connected/disconnected!\nUsing %s requires that the device does not modify the video data!\n\n", misrc_sc_capture_impl_name());
	exit(2);
}

int main(int argc, char **argv)
{

#ifndef _WIN32
	struct sigaction sigact;
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
#else
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, true );
#endif

	// generate options
	struct option *getopt_long_options, *cur_gl_opt;
	misrc_option_t *cur_opt = misrc_option_list;
	misrc_settings_t set;
	// getopt string
	char getopt_string[256];
	int r, i, opt;

	memset(&set, 0, sizeof(misrc_settings_t));

	fprintf(stderr,
		"MISRC capture " MIRSC_TOOLS_VERSION"\n"
		"A simple program to capture from MISRC using hsdaoh\n"
		MIRSC_TOOLS_COPYRIGHT "\n\n"
	);

	if(argc < 2) usage(false);

	// that is definitley enough, as each option can result into 4 command line options maximum
	getopt_long_options = malloc(sizeof(struct option)*4*sizeof(misrc_option_list)/sizeof(misrc_option_t));
	cur_opt = misrc_option_list;
	cur_gl_opt = getopt_long_options;
	do {
		if ((cur_opt->opt_flags & MISRC_OPTFLAG_GUIONLY) == 0) {
			*cur_gl_opt = (struct option){ cur_opt->long_opt, (cur_opt->arg_type == MISRC_ARGTYPE_BOOL) ? no_argument : required_argument, 0, cur_opt->short_opt };
			if (cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_RFC) {
				cur_gl_opt++;
				*cur_gl_opt = (struct option){ strdup_inc(cur_opt->long_opt, 1, 1), (cur_opt->arg_type == MISRC_ARGTYPE_BOOL) ? no_argument : required_argument, 0, cur_opt->short_opt + 1 }; /*XXXX*/
			}
			else if (cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_AUDIO_STEREO) {
				cur_gl_opt++;
				*cur_gl_opt = (struct option){ strdup_inc(cur_opt->long_opt, 2, 2), (cur_opt->arg_type == MISRC_ARGTYPE_BOOL) ? no_argument : required_argument, 0, cur_opt->short_opt + 1 }; /*XXXX*/
			}
			else if (cur_opt->opt_type == MISRC_OPTTYPE_CAPTURE_AUDIO_MONO) {
				for(i=1; i<4; i++) {
					cur_gl_opt++;
					*cur_gl_opt = (struct option){ strdup_inc(cur_opt->long_opt, 1, i), (cur_opt->arg_type == MISRC_ARGTYPE_BOOL) ? no_argument : required_argument, 0, cur_opt->short_opt + i }; /*XXXX*/
				}
			}
			cur_gl_opt++;
		}
		cur_opt++;
	} while (cur_opt->short_opt != 0);
	*cur_gl_opt = (struct option){ "devices", no_argument, 0, OPT_LIST_DEVICES };
	cur_gl_opt++;
	*cur_gl_opt = (struct option){ "full-help", no_argument, 0, OPT_FULL_HELP };
	cur_gl_opt++;
	*cur_gl_opt = (struct option){ NULL, 0, 0, 0 };

	create_getopt_string(getopt_string, getopt_long_options);
	misrc_capture_set_default(&set,misrc_option_list);

	int index_ptr;
	while ((opt = getopt_long(argc, argv, getopt_string, getopt_long_options, &index_ptr)) != -1) {
		cur_opt = misrc_option_list;
		char *endptr;
		double arg_f;
		int64_t arg_i;
		while(cur_opt->short_opt != 0) {
			if(opt >= cur_opt->short_opt  && opt < cur_opt->short_opt+mirsc_opt_type_cnt[cur_opt->opt_type]) {
				switch(cur_opt->arg_type) {
				case MISRC_ARGTYPE_BOOL:
					MISRC_SET_OPTION(bool,&set,cur_opt,opt-cur_opt->short_opt,true);
					break;
				case MISRC_ARGTYPE_LIST:
					for(i=0;i<=cur_opt->max_arg.i;i++) if(strcmp(optarg,cur_opt->arg_list[i])==0) break;
					if(i<=cur_opt->max_arg.i) {
						MISRC_SET_OPTION(int64_t,&set,cur_opt,opt-cur_opt->short_opt,i);
						break;
					}
					// fall through
				case MISRC_ARGTYPE_INT:
					arg_i = strtoll(optarg, &endptr, 10);
					if (*endptr != '\0' || arg_i < cur_opt->min_arg.i || arg_i > cur_opt->max_arg.i) {
						print_invalid_arg(cur_opt, optarg);
						usage((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == MISRC_OPTFLAG_ADVANCED);  
					}
					MISRC_SET_OPTION(int64_t,&set,cur_opt,opt-cur_opt->short_opt,arg_i);
					break;
				case MISRC_ARGTYPE_FLOAT:
					arg_f = strtod(optarg, &endptr);
					if (*endptr != '\0' || arg_f < cur_opt->min_arg.f || arg_f > cur_opt->max_arg.f) {
						print_invalid_arg(cur_opt, optarg);
						usage((cur_opt->opt_flags&MISRC_OPTFLAG_ADVANCED) == MISRC_OPTFLAG_ADVANCED);  
					}
					MISRC_SET_OPTION(double,&set,cur_opt,opt-cur_opt->short_opt,arg_f);
					break;
				default:
					MISRC_SET_OPTION(char*,&set,cur_opt,opt-cur_opt->short_opt,optarg);
					break;
				}
				break;
			}
			cur_opt++;
		}
		if(opt == OPT_FULL_HELP) {
			usage(true);
		}
		else if (opt == OPT_LIST_DEVICES) {
			list_devices();
		}
		else if (cur_opt->short_opt == 0) {
			usage(false);
		}
	}

	set.overwrite_cb = ask_overwrite;
	set.msg_cb = print_misrc_message;
	set.stats_cb = print_progress_level_clip;
	set.count_cb = print_misrc_count;
	set.sync_cb = print_misrc_sync;
	set.stats_cb_ctx = &set;

	if(set.disable_clip[0]) {
		fprintf(stderr, "Suppressing clipping messages from RF ADC A\n");
	}
	if(set.disable_clip[1]) {
		fprintf(stderr, "Suppressing clipping messages from RF ADC B\n");
	}
	if(set.total_samples_before_exit > 0) {
		fprintf(stderr, "Capturing %" PRIu64 " samples before exiting\n", set.total_samples_before_exit);
	}

	r = misrc_run_capture(&set);

	return r;
}
