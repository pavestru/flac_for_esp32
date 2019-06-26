#include "flac_test.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <assert.h>
#include <esp_types.h>
#include <stdio.h>
#include "esp32/rom/ets_sys.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include "share/compat.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"

#include <sys/time.h>

extern const uint8_t Sample16kHz_raw_start[] asm("_binary_Sample16kHz_raw_start");
extern const uint8_t Sample16kHz_raw_end[] asm("_binary_Sample16kHz_raw_end");

#define READSIZE 1024

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define BPS 16

static FLAC__int32 pcm[READSIZE/*samples*/ * CHANNELS];

int num_samples_encoded = 0;
int total = 0;
int frames = 0;
int  total_samples = 0;

struct timeval tvalBefore, tvalFirstFrame, tvalAfter;

/**
 * Callback that is called from 
 */
static FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);

void flac_test()
{
	short int *pcm_samples, *pcm_samples_end;

	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = 0;
	FLAC__StreamEncoderInitStatus init_status;

	pcm_samples = (short int *)Sample16kHz_raw_start;
	pcm_samples_end = (short int *)Sample16kHz_raw_end;

	int total_bytes = pcm_samples_end - pcm_samples;
	printf("total bytes %d\n", total_bytes);

	total_samples =  total_bytes / 2; // 16 bit sample

	printf("Available Heap Space before allocation: %d\n", esp_get_free_heap_size());

	// Create encoder
	if ((encoder = FLAC__stream_encoder_new()) == NULL)
	{
		fprintf(stderr, "ERROR: allocating encoder\n");
		return;
	}

	printf("Available Heap Space after allocation: %d\n", esp_get_free_heap_size());

	// Setting encoder
	ok &= FLAC__stream_encoder_set_compression_level(encoder, 8);
	ok &= FLAC__stream_encoder_set_channels(encoder, CHANNELS);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, BPS);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder, SAMPLE_RATE);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

	/* initialize encoder */
	if (ok)
	{
		init_status = FLAC__stream_encoder_init_stream(encoder, write_callback, /*seek_callback=*/NULL, /*tell_callback=*/NULL, /*metadata_callback=*/NULL, /*client_data=*/NULL);
		if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		{
			fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
			fprintf(stderr, "State: %d\n", FLAC__stream_encoder_get_state(encoder));

			ok = false;
		}
	}

	printf("Available Heap Space after initialization: %d\n", esp_get_free_heap_size());

	if (FLAC__stream_encoder_get_state(encoder) > 0)
	{
		fprintf(stderr, "Encoder in ERROR state: %d\n", FLAC__stream_encoder_get_state(encoder));
		ok = false;
	}
	
	FLAC__byte *buffer;

	gettimeofday(&tvalBefore, NULL);
	while ((pcm_samples_end - pcm_samples) > 0)
	{
		size_t left = (pcm_samples_end - pcm_samples) / 2 / CHANNELS;
		size_t need = (left > READSIZE ? (size_t)READSIZE : (size_t)left);

		buffer = (FLAC__byte *)pcm_samples;

		/* convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buffer for libFLAC */
		size_t i;
		for(i = 0; i < need*CHANNELS; i++) {
			/* inefficient but simple and works on big- or little-endian machines */
			pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2*i+1] << 8) | (FLAC__int16)buffer[2*i]);
		}

		ok = FLAC__stream_encoder_process_interleaved(encoder, pcm, need);

		pcm_samples += (need * 2);
	}

	gettimeofday(&tvalAfter, NULL);

	// Wait for all write callbacks to finish
	vTaskDelay(5000 / portTICK_RATE_MS);

	printf("Available Heap Space: %d\n", esp_get_free_heap_size());

	printf("Fist Frame time in microseconds: %ld microseconds\n",
				 ((tvalFirstFrame.tv_sec - tvalBefore.tv_sec) * 1000000L + tvalFirstFrame.tv_usec) - tvalBefore.tv_usec);

	printf("Total time in microseconds: %ld microseconds\n",
				 ((tvalAfter.tv_sec - tvalBefore.tv_sec) * 1000000L + tvalAfter.tv_usec) - tvalBefore.tv_usec);

	printf("Total frames: %d TotalBytes: %d\n", frames, 2 * total);

	while (1)
		vTaskDelay(500 / portTICK_RATE_MS);

	return;
}

FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{

	(void)encoder, (void)client_data;

	if (samples > 0)
	{
		if (total == 0)
			gettimeofday(&tvalFirstFrame, NULL);

		total += samples;
		frames = current_frame;
		fprintf(stderr, "encoded %d samples in %d byte frame, total %d/%u samples, %u frames\n", samples, bytes, total, total_samples, current_frame);
		gettimeofday(&tvalAfter, NULL);
		printf("Total time in microseconds: %ld microseconds\n",
			((tvalAfter.tv_sec - tvalBefore.tv_sec) * 1000000L + tvalAfter.tv_usec) - tvalBefore.tv_usec);
	}
	return 0; // FLAC__STREAM_ENCODER_WRITE_STATUS_OK == 0,	FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR == 1
}