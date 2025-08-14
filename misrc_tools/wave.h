
#ifndef WAVE_H
#define WAVE_H

#define WAVE_RIFF 0x46464952
#define WAVE_RF64 0x34364652
#define WAVE_WAVE 0x45564157
#define WAVE_JUNK 0x4B4E554A
#define WAVE_ds64 0x34367364
#define WAVE_fmt  0x20746D66
#define WAVE_data 0x61746164


typedef struct
{
	uint32_t riff;
	uint32_t riffSize;
	uint32_t wave;
	uint32_t ds64junk;
	uint32_t ds64junkSize;
	uint64_t riff64Size;
	uint64_t data64Size;
	uint64_t sampleCount;
	uint32_t extraTableSize;
	uint32_t fmt;
	uint32_t fmtSize;
	uint16_t formatType;
	uint16_t channelCount;
	uint32_t sampleRate;
	uint32_t bytesPerSecond;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
	uint16_t cbSize;
	uint32_t data;
	uint32_t dataSize;
} __attribute__((packed, aligned(2))) wave_header_t;

void create_wave_header(wave_header_t *h, uint64_t samples, uint32_t sample_rate, uint16_t channels, uint16_t bits)
{
	uint64_t data_size = (bits / 8) * channels * samples;
	if (data_size > (2147483647 - sizeof(wave_header_t))) {
		h->riff = WAVE_RF64;
		h->riffSize = 0xffffffff;
		h->ds64junk = WAVE_ds64;
		h->dataSize = 0xffffffff;
	}
	else {
		h->riff = WAVE_RIFF;
		h->riffSize = data_size + sizeof(wave_header_t) - 8;
		h->ds64junk = WAVE_JUNK;
		h->dataSize = data_size;
	}
	h->wave = WAVE_WAVE;
	h->ds64junkSize = 28;
	h->riff64Size = data_size + sizeof(wave_header_t) - 8;
	h->data64Size = data_size;
	h->sampleCount = samples;
	h->extraTableSize = 0;
	h->fmt = WAVE_fmt;
	h->fmtSize = 18;
	h->formatType = 1;
	h->channelCount = channels;
	h->sampleRate = sample_rate;
	h->bytesPerSecond = (bits / 8) * channels * sample_rate;
	h->blockAlign = (bits / 8) * channels;
	h->bitsPerSample = bits;
	h->cbSize = 0;
	h->data = WAVE_data;
}

#endif