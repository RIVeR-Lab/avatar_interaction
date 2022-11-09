#include <csignal>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <string>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */
#include <errno.h>

#include <Processing.NDI.Advanced.h>

// ALSA
#include <alsa/asoundlib.h>

static int err;
static short *buffer;
static unsigned int rate = 48000;
static unsigned int channel = 2;
static snd_pcm_t *handle;
static snd_pcm_hw_params_t *hw_params;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
static snd_pcm_uframes_t frames = 2000;   // frames per period
static unsigned int periods_per_buffer = 2;
static char snd_name[255] = "";
static char playback[255] = "";
static char output_name[255] = "";
static float voice_thred = 1200;
static char shm_path[25] = "voice_shm";
static bool *voice_detected = NULL;

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int)
{
  shm_unlink(shm_path);
  exit_loop = true;
}

double rms(short *buffer, int buffer_size)
{
    int i;
    long int square_sum = 0.0;
    for(i=0; i<buffer_size; i++)
        square_sum += (buffer[i] * buffer[i]);

    double result = sqrt(square_sum/buffer_size);
    return result;
}

int init_device(char* snd, snd_pcm_stream_t stream_t)
{
  /* Open PCM. The last parameter of this function is the mode. */
  /* If this is set to 0, the standard mode is used. Possible   */
  /* other values are SND_PCM_NONBLOCK and SND_PCM_ASYNC.       */
  /* If SND_PCM_NONBLOCK is used, read / write access to the    */
  /* PCM device will return immediately. If SND_PCM_ASYNC is    */
  /* specified, SIGIO will be emitted whenever a period has     */
  /* been completely processed by the soundcard.                */
  if ((err = snd_pcm_open (&handle, snd, stream_t, 0)) < 0) {
		fprintf(stderr, "cannot open audio device %s (%s)\n",
						snd,
						snd_strerror(err));
		exit (1);
  }

  fprintf(stdout, "audio interface opened\n");
		   
  snd_pcm_hw_params_alloca (&hw_params);

  fprintf(stdout, "hw_params allocated\n");
				 
  /* Before we can write PCM data to the soundcard, we have to specify 
  access type, sample format, sample rate, number of channels, number 
  of periods and period size. First, we initialize the hwparams structure 
  with the full configuration space of the soundcard. */
  if ((err = snd_pcm_hw_params_any (handle, hw_params)) < 0) {
    fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
             snd_strerror (err));
    exit (1);
  }

  fprintf(stdout, "hw_params initialized\n");
	
  if ((err = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf (stderr, "cannot set access type (%s)\n",
             snd_strerror (err));
    exit (1);
  }

  fprintf(stdout, "hw_params access set to MMAP Interleaved\n");
	
  if ((err = snd_pcm_hw_params_set_format (handle, hw_params, format)) < 0) {
    fprintf (stderr, "cannot set sample format (%s)\n",
             snd_strerror (err));
    exit (1);
  }

  fprintf(stdout, "hw_params format setted\n");
	
  if ((err = snd_pcm_hw_params_set_rate_near (handle, hw_params, &rate, 0)) < 0) {
    fprintf (stderr, "cannot set sample rate (%s)\n",
             snd_strerror (err));
    exit (1);
  }

	
  fprintf(stdout, "hw_params rate setted\n");

  if ((err = snd_pcm_hw_params_set_channels (handle, hw_params, channel)) < 0) {
    fprintf (stderr, "cannot set channel count (%s)\n",
             snd_strerror (err));
    exit (1);
  }

	fprintf(stdout, "hw_params channels setted\n");

	int dir;
  if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &frames, &dir)) < 0)
	{
		fprintf (stderr, "cannot set period size (%s)\n",
             snd_strerror (err));
    exit (1);
	};
	printf("Set frame size to: %lu\n", frames);

	/* Use a buffer large enough to hold one period */
  snd_pcm_uframes_t period_size = frames * 2; /* 2 bytes/sample, 2 channels */
	printf("Actual period size: %lu \n", period_size);

  // have a buffer with 2 periods
  buffer = (short *) malloc(period_size * periods_per_buffer);

	// latency is then calculated by 
	// bytes_of_buffer / (sample_rate * bytes_per_frame)
	// The sample rate will always be the same as the frame rate for PCM
	// https://stackoverflow.com/a/19589884
	printf("Buffer size: %lu\n", period_size * periods_per_buffer);

	snd_pcm_uframes_t req_buff_size = period_size * periods_per_buffer;
	if (snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &req_buff_size) < 0) {
		fprintf(stderr, "Error setting buffersize.\n");
		return(-1);
	}
	printf("Actual buffer size is %lu\n", req_buff_size);

	
  if ((err = snd_pcm_hw_params (handle, hw_params)) < 0) {
    fprintf (stderr, "cannot set parameters (%s)\n",
             snd_strerror (err));
    exit (1);
  }
  fprintf(stdout, "hw_params setted\n");

	unsigned int time;
	snd_pcm_hw_params_get_buffer_time(hw_params, &time, &dir);
	printf("Buffer time: %u\n", time);

	float latency = req_buff_size/ float(rate * 4);
	printf("The estimated latency with current setting is: %.2fms\n", 1000*latency);
  if ((err = snd_pcm_prepare (handle)) < 0) {
    fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
             snd_strerror (err));
    exit (1);
  }

  fprintf(stdout, "audio interface prepared\n");
	return 0;

}

void init_shm()
{
  // The shared memory file will be created at /dev/shm/<shm_path> by default
  int fd = shm_open(shm_path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1)
  {
    printf("Shared memory error: %s\n", strerror(errno));
  }
  
  // File size must be allocated first or Bus error would be thrown
  if (ftruncate(fd, sizeof(bool)) == -1)
    printf("ftruncate error: %s\n", strerror(errno));

  voice_detected = (bool*)mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (voice_detected == (void*)-1)
  {
    printf("MMAP error: %s\n", strerror(errno));
  }
  *voice_detected = false;
  printf("Shared memory initiated\n");
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
	if (argc < 5){
		help(argv[0]);
		exit(-1);
	}

	while ((opt = getopt(argc, argv, "hi:c:o:")) != -1)
	{
		switch (opt) {
			case 'c':
        strcpy(snd_name, optarg);
				break;
			case 'a':
        strcpy(playback, optarg);
				init_device(playback, SND_PCM_STREAM_PLAYBACK);
				printf("Audio device initiated\n");
				break;
      case 'o':
        strcpy(output_name, optarg);
        init_device(snd_name, SND_PCM_STREAM_CAPTURE);
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
  init_shm();

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
  int a = 0;
  while (!exit_loop) {
    rc = snd_pcm_readi(handle, buffer, frames);
    if (rc == -EPIPE) {
      /* EPIPE means overrun */
      fprintf(stderr, "overrun occurred\n");
      snd_pcm_prepare(handle);
    } else if (rc < 0) {
      fprintf(stderr,
              "error from read: %s\n",
              snd_strerror(rc));
    } else if (rc != (int)frames) {
      fprintf(stderr, "short read, read %d frames\n", rc);
    }
    double volume = rms(buffer, frames)*0.45255;
    if (volume > voice_thred)
    {
      *voice_detected = true;
      printf("Detected sound %f\n", volume);
    }
    else
    {
      *voice_detected = false;
    }
		NDI_audio_frame.p_data = (int16_t*)buffer;
		NDIlib_util_send_send_audio_interleaved_16s(pNDI_send, &NDI_audio_frame);

  }

	snd_pcm_drain(handle);
  snd_pcm_close(handle);
	// Free the video frame
	free(NDI_audio_frame.p_data);

	// Destroy the NDI finder
	NDIlib_send_destroy(pNDI_send);

	// Not required, but nice
	NDIlib_destroy();

	// Success
	return 0;
}