#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>

#ifndef _WIN32
	#include <unistd.h>
	#define sleep_ms(ms)	usleep(ms*1000)
	#else
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
	#define sleep_ms(ms)	Sleep(ms)
#endif

#define _FILE_OFFSET_BITS 64

#ifdef _WIN64
#define FSEEK fseeko64
#else
#define FSEEK fseeko
#endif

void usage(void)
{
	fprintf(stderr,
		"MISRC extract, a simple program for extracting captured data into separate file\n\n"
		"Usage:\n"
		"\t[-i input file (use '-' to read from stdin)]\n"
		"\t[-a ADC A output file (use '-' to write on stdout)]\n"
		"\t[-b ADC B output file (use '-' to write on stdout)]\n"
		"\t[-x AUX output file (use '-' to write on stdout)]\n"
	);
	exit(1);
}

int main(int argc, char **argv)
{
//set pipe mode to binary in windows
#ifdef _WIN32 || _WIN64
	_setmode(_fileno(stdout), O_BINARY);
	_setmode(_fileno(stdin), O_BINARY);	
#endif

	int opt;
	
	//file adress
	char *input_name_1    = NULL;
	char *output_name_1   = NULL;
	char *output_name_2   = NULL;
	char *output_name_aux = NULL;
	
	//input file
	FILE *input_1;
	
	//output files
	FILE *output_1;
	FILE *output_2;
	FILE *output_aux;
	
	//bufer
	uint32_t *buf_tmp = malloc(sizeof(uint32_t)*65000);
	uint16_t *buf_1   = malloc(sizeof(uint16_t)*65000);
	uint16_t *buf_2   = malloc(sizeof(uint16_t)*65000);
	uint8_t  *buf_aux = malloc(sizeof(uint8_t) *65000);
	
	//number of byte read
	int nb_block;
	
	//bits masking
	const uint32_t mask_1      = 0xFFF;
	const uint32_t mask_clip_1 = 0x1000;
	const uint32_t mask_2      = 0xFFF00000;
	const uint32_t mask_clip_2 = 0x2000;
	const uint32_t mask_aux    = 0xFC000;
	
	//clipping state
	int clip_A = 0;
	int clip_B = 0;
	
	while ((opt = getopt(argc, argv, "i:a:b:x:")) != -1) {
		switch (opt) {
		case 'i':
			input_name_1 = optarg;
			break;
		case 'a':
			output_name_1 = optarg;
			break;
		case 'b':
			output_name_2 = optarg;
			break;
		case 'x':
			output_name_aux = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	
	if(input_name_1 == NULL || (output_name_1 != NULL && output_name_2 != NULL && output_name_aux != NULL))
	{
		usage();
	}
	
	//reading file 1
	if(input_name_1 != NULL)
	{
		if (strcmp(input_name_1, "-") == 0)// Read samples from stdin
		{
			input_1 = stdin;
		}
		else 
		{
			input_1 = fopen(input_name_1, "rb");
			if (!input_1) {
				fprintf(stderr, "(1) : Failed to open %s\n", input_1);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_1 != NULL)
	{
		//opening output file 1
		if (strcmp(output_name_1, "-") == 0)// Read samples from stdin
		{
			output_1 = stdout;
		}
		else
		{
			output_1 = fopen(output_name_1, "wb");
			if (!output_1) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_1);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_2 != NULL)
	{
		//opening output file 2
		if (strcmp(output_name_2, "-") == 0)// Read samples from stdin
		{
			output_2 = stdout;
		}
		else if(output_name_2 != NULL)
		{
			output_2 = fopen(output_name_2, "wb");
			if (!output_2) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_2);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_aux != NULL)
	{
		//opening output file aux
		if (strcmp(output_name_aux, "-") == 0)// Read samples from stdin
		{
			output_aux = stdout;
		}
		else if(output_name_aux != NULL)
		{
			output_aux = fopen(output_name_aux, "wb");
			if (!output_aux) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_aux);
				return -ENOENT;
			}
		}
	}
	
	if(input_name_1 != NULL && (output_name_1 != NULL || output_name_2 != NULL || output_name_aux != NULL))
	{
		while(!feof(input_1))
		{
			nb_block = fread(buf_tmp  ,65000,4,input_1);
			
			for(int i = 0; i < nb_block; i++)
			{
				buf_1[i]   = (buf_tmp[i] & mask_1);
				buf_2[i]   = (buf_tmp[i] & mask_2) >> 20;
				buf_aux[i] = (buf_tmp[i] & mask_aux) >> 13;
				
				if(((buf_tmp[i] & mask_clip_1) >> 19) == 1)
				{
					clip_A++;
				}
				
				if(((buf_tmp[i] & mask_clip_2) >> 12) == 1)
				{
					clip_B++;
				}
			}
			
			if(clip_A > 0)
			{
				fprintf(stderr,"ADC A : %d sample clipped",clip_A);
				clip_A = 0; 
			}
			
			if(clip_B > 0)
			{
				fprintf(stderr,"ADC B : %d sample clipped",clip_B);
				clip_B = 0; 
			}
			
			//write output
			if(output_name_1   != NULL){fwrite(buf_1, nb_block,2,output_1);}
			if(output_name_2   != NULL){fwrite(buf_2, nb_block,2,output_2);}
			if(output_name_aux != NULL){fwrite(buf_aux,8,1,output_aux);}
			
			fflush(output_1);
			fflush(output_2);
			fflush(output_aux);
		}
	}

////ending of the program
	
	free(buf_1);
	free(buf_2);
	free(buf_aux);
	
	//Close file 1
	if(input_name_1 != NULL)
	{
		if (input_1 && (input_1 != stdin))
		{
			fclose(input_1);
		}
	}
	
	if(output_name_1 != NULL)
	{
		//Close out file
		if (output_1 && (output_1 != stdout))
		{
			fclose(output_1);
		}
	}
	
	if(output_name_2 != NULL)
	{
		if (output_2 && (output_2 != stdout))
		{
			fclose(output_2);
		}
	}
	
	if(output_name_aux != NULL)
	{
		if (output_aux && (output_aux != stdout))
		{
			fclose(output_aux);
		}
	}

	return 0;
}
