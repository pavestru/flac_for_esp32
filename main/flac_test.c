#include "flac_test.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <assert.h>
#include <esp_types.h>
#include <stdio.h>
#include "rom/ets_sys.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include "opus.h"
#include "opusenc.h"
#include "share/compat.h"
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"

#include <sys/time.h>

extern const uint8_t Sample16kHz_raw_start[] asm("_binary_Sample16kHz_raw_start");
extern const uint8_t Sample16kHz_raw_end[] asm("_binary_Sample16kHz_raw_end");

// #define READ_SIZE 512
//#define FRAME_SIZE 960
#define SAMPLE_RATE 16000
#define CHANNELS 2
//#define APPLICATION OPUS_APPLICATION_AUDIO
//#define BITRATE 64000
//#define MAX_FRAME_SIZE 6*960
//#define MAX_PACKET_SIZE (3*1276)

#define MAX_PACKET (1500)

void flac_test()
{
	int error;
	size_t free8start, free32start;
	short int *pcm_samples, *pcm_samples_end;
	int num_samples_encoded = 0, total = 0, frames = 0;
	struct timeval tvalBefore, tvalFirstFrame, tvalAfter;
	unsigned char *page;
	int len;

	OpusEncoder *enc;
	unsigned char packet[MAX_PACKET + 257];

	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = 0;
	FLAC__StreamEncoderInitStatus init_status;

	free8start = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	free32start = heap_caps_get_free_size(MALLOC_CAP_32BIT);
	printf("pre opus_create() free mem8bit: %d mem32bit: %d\n", free8start, free32start);

	pcm_samples = (short int *)Sample16kHz_raw_start;
	pcm_samples_end = (short int *)Sample16kHz_raw_end;

	/* Parameters to fuzz. Some values are duplicated to increase their probability of being tested. */
	int sampling_rates[5] = {8000, 12000, 16000, 24000, 48000};
	int channels[2] = {1, 2};
	int applications[3] = {OPUS_APPLICATION_AUDIO, OPUS_APPLICATION_VOIP, OPUS_APPLICATION_RESTRICTED_LOWDELAY};
	int bitrates[11] = {6000, 12000, 16000, 24000, 32000, 48000, 64000, 96000, 510000, OPUS_AUTO, OPUS_BITRATE_MAX};
	int force_channels[4] = {OPUS_AUTO, OPUS_AUTO, 1, 2};
	int use_vbr[3] = {0, 1, 1};
	int vbr_constraints[3] = {0, 1, 1};
	int complexities[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	int max_bandwidths[6] = {OPUS_BANDWIDTH_NARROWBAND, OPUS_BANDWIDTH_MEDIUMBAND,
													 OPUS_BANDWIDTH_WIDEBAND, OPUS_BANDWIDTH_SUPERWIDEBAND,
													 OPUS_BANDWIDTH_FULLBAND, OPUS_BANDWIDTH_FULLBAND};
	int signals[4] = {OPUS_AUTO, OPUS_AUTO, OPUS_SIGNAL_VOICE, OPUS_SIGNAL_MUSIC};
	int inband_fecs[3] = {0, 0, 1};
	int packet_loss_perc[4] = {0, 1, 2, 5};
	int lsb_depths[2] = {8, 24};
	int prediction_disabled[3] = {0, 0, 1};
	int use_dtx[2] = {0, 1};
	int frame_sizes_ms_x2[9] = {5, 10, 20, 40, 80, 120, 160, 200, 240}; /* x2 to avoid 2.5 ms */

	int sampling_rate = 16000;
	int num_channels = 1;
	int application = OPUS_APPLICATION_AUDIO;

	enc = opus_encoder_create(sampling_rate, num_channels, application, &error);
	if (!enc)
	{
		fprintf(stderr, "Some error: %d\n", error);
		return;
	}

	// Setting values
	int bitrate = 64000;
	int force_channel = OPUS_AUTO;
	int vbr = 1;
	int complexity = 0;
	int max_bw = OPUS_BANDWIDTH_NARROWBAND;
	int sig = OPUS_SIGNAL_VOICE;
	int frame_size_ms_x2 = 20;
	int frame_size = frame_size_ms_x2 * sampling_rate / 2000;

	opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
	opus_encoder_ctl(enc, OPUS_SET_VBR(vbr));
	opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
	opus_encoder_ctl(enc, OPUS_SET_MAX_BANDWIDTH(max_bw));
	opus_encoder_ctl(enc, OPUS_SET_SIGNAL(sig));

	printf("Available Heap Space: %d\n", esp_get_free_heap_size());

	gettimeofday(&tvalBefore, NULL);
	while (pcm_samples_end - pcm_samples > 0)
	{
		// printf("\n=============== opus_encode================ \n");
		/* encode samples. */
		len = opus_encode(enc, pcm_samples, frame_size, packet, MAX_PACKET);
		printf("%d\n", len);

		num_samples_encoded = frame_size;

		if (total == 0)
			gettimeofday(&tvalFirstFrame, NULL);

		/* check for value returned.*/
		if (num_samples_encoded > 1)
		{
			// printf("It seems the conversion was successful.\n");
			total += num_samples_encoded;
			frames += len;
		}
		else
		{
			printf("Error %d.\n", num_samples_encoded);
			return;
		}

		pcm_samples += (frame_size * 2); // nsamples*2 ????
	}

	gettimeofday(&tvalAfter, NULL);

	printf("Available Heap Space: %d\n", esp_get_free_heap_size());

	free8start = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	free32start = heap_caps_get_free_size(MALLOC_CAP_32BIT);
	printf("post encode free mem8bit: %d mem32bit: %d\n", free8start, free32start);

	printf("Fist Frame time in microseconds: %ld microseconds\n",
				 ((tvalFirstFrame.tv_sec - tvalBefore.tv_sec) * 1000000L + tvalFirstFrame.tv_usec) - tvalBefore.tv_usec);

	printf("Total time in microseconds: %ld microseconds\n",
				 ((tvalAfter.tv_sec - tvalBefore.tv_sec) * 1000000L + tvalAfter.tv_usec) - tvalBefore.tv_usec);

	printf("Total frames: %d TotalBytes: %d\n", frames, total);

	while (1)
		vTaskDelay(500 / portTICK_RATE_MS);

	return;
}
