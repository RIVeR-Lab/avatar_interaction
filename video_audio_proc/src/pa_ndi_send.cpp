#include <csignal>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <string>

#ifdef _WIN32
#include <windows.h>

#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.Advanced.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.Advanced.x86.lib")
#endif // _WIN64

#endif

#include <Processing.NDI.Advanced.h>

// PULSEAUDIO 
#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
 
#include <pulse/simple.h>
#include <pulse/error.h>

static unsigned int rate = 48000;
static unsigned int channel = 2;
static int err;
static char *buffer;
static unsigned long frames = 500;   // frames per period
static unsigned int periods_per_buffer = 2;
static unsigned long period_size = frames * 4; /* 2 bytes/sample, 2 channels */

// Audio config
static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = rate,
		.channels = channel};

static const pa_buffer_attr ba = {
	.maxlength = (uint32_t) - 1,
	.tlength = (uint32_t) pa_usec_to_bytes(10*1000, &ss),
	.prebuf = 100,
	.minreq = 100,
	.fragsize = (uint32_t) - 1,
};
static pa_simple *s = NULL;
int ret = 1;
int error;

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int) { exit_loop = true; }

void init_audio(char* sound_dev)
{
	if (!(s = pa_simple_new(NULL, "Capture", PA_STREAM_RECORD, sound_dev, "Capture", &ss, NULL, &ba, &error)))
	{
		printf(": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(-1);
	}
  buffer = (char *) malloc(period_size * periods_per_buffer);
}

static int help(char *prog_name)
{
  printf("Usage: %s [-c source_dev -o output_name -a playback_dev], -a is optional\n Example: %s -c plughw:0,0 -o spatial\n",
         prog_name, prog_name);
  return 0;
}

int main(int argc, char* argv[])
{
  int opt;
  char* source_dev = nullptr;
  char* output_name = nullptr;
  char* playback = nullptr;

	if (argc < 5){
		help(argv[0]);
		exit(-1);
	}

	while ((opt = getopt(argc, argv, "hi:c:o:")) != -1)
	{
		switch (opt) {
			case 'c':
        source_dev = optarg;
				break;
			case 'a':
        playback = optarg;
				break;
      case 'o':
        output_name = optarg;
        init_audio(source_dev);
        printf("Audio device initiated\n");
        break;
      case 'h':
				return help(argv[0]);
				break;
		}
	}
	// Not required, but "correct" (see the SDK documentation).
	if (!NDIlib_initialize()) {
		// Cannot run NDI. Most likely because the CPU is not sufficient (see SDK documentation).
		// you can check this directly with a call to NDIlib_is_supported_CPU()
		printf("Cannot run NDI.");
		return 0;
	}

	// Catch interrupt so that we can shut down gracefully
	signal(SIGINT, sigint_handler);

	// Create an NDI source that is called "My 16bpp Audio" and is clocked to the audio.
	NDIlib_send_create_t NDI_send_create_desc;
	NDI_send_create_desc.p_ndi_name = std::string(output_name).c_str();
	NDI_send_create_desc.clock_audio = true;

	// We create the NDI finder
	NDIlib_send_instance_t pNDI_send = NDIlib_send_create(&NDI_send_create_desc);
	if (!pNDI_send)
		return 0;

	NDIlib_audio_frame_interleaved_16s_t NDI_audio_frame;
	NDI_audio_frame.sample_rate = rate;
	NDI_audio_frame.no_channels = channel;
	NDI_audio_frame.no_samples = frames ;
	NDI_audio_frame.reference_level = 0;
	// NDI_audio_frame.p_data = (short*)malloc(1920 * 2 * sizeof(short));

	int rc;
  while (!exit_loop) {
    uint8_t buf[1024];
    /* Record some data ... */
    if (pa_simple_read(s, buf, sizeof(buf), &error) < 0)
    {
      fprintf(stderr, __FILE__ ": pa_simple_read() failed: %s\n", pa_strerror(error));
      exit(-1);
    }

    NDI_audio_frame.p_data = (int16_t*)buf;
		NDIlib_util_send_send_audio_interleaved_16s(pNDI_send, &NDI_audio_frame);
    printf("NDI audio frame sent\n");
    pa_usec_t latency;
    if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t)-1)
    {
      printf(": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
      exit(-1);
    }

    printf("Latency: %0.0f usec    \n", (float)latency);
  }

  pa_simple_drain(s, &error);
	// Free the video frame
	free(NDI_audio_frame.p_data);

	// Destroy the NDI finder
	NDIlib_send_destroy(pNDI_send);

	// Not required, but nice
	NDIlib_destroy();

	// Success
	return 0;
}