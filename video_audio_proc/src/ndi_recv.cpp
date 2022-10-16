#include <cstdio>
#include <chrono>
#include <Processing.NDI.Advanced.h>
// sudo apt install libsdl2-dev
#include <SDL2/SDL.h>
// sudo apt install libsdl2-image-dev
#include <SDL2/SDL_image.h>
#include <string>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <stdio.h>
#include <atomic>
#include <thread>

// ALSA
#include <alsa/asoundlib.h>

// Video config
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static unsigned int width = 1920;
static unsigned int height = 1080;
bool view_inited = false;

// Audio config
static int err;
static char *buffer;
static unsigned int rate = 48000;
static unsigned int channel = 2;
static snd_pcm_t *handle;
static snd_pcm_hw_params_t *hw_params;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
static snd_pcm_uframes_t frames = 1024;   // frames per period
static unsigned int periods_per_buffer = 10;
static bool audio_inited = false;
static int  audio_buffer = 100;

#define now        std::chrono::high_resolution_clock::now();
using time_point = std::chrono::high_resolution_clock::time_point;
using duration   = std::chrono::duration<double>;

static void errno_exit(const char *s) 
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

void sighandler(int signum) {
	errno_exit("SIGINT received");
}

static void draw_YUV(uint8_t *buffer)
{
	int pitch = 2*width;
	// SDL_UpdateYUVTexture(texture,
	// 										 NULL,
	// 										 buffer, width,
	// 										 buffer , width / 2,
	// 										 buffer, width / 2);
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static void draw_BGRA(void *buffer)
{
	int pitch = width*4;
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

}

static void draw_NV12(void *buffer)
{
	int pitch = width;
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static int init_view(NDIlib_FourCC_type_e format)
{
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Unable to initialize SDL\n");
		return -1;
	}

	if (SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &window, &renderer))
	{
		printf("SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
		return -1;
	}

	// TODO: Convert from NDI UYUV to YUYV
	// if (pixelformat == "YUYV") {
	// 	texture = SDL_CreateTexture(renderer
	// 					// YUY2 is also know as YUYV in SDL
	// 					, SDL_PIXELFORMAT_YUY2
	// 					, SDL_TEXTUREACCESS_STREAMING
	// 					, width
	// 					, height);
	// 	if (!texture) {
	// 		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
	// 		return -1;
	// 	}
	// }
	
	uint32_t render_fmt;
	// if (format != NDIlib_FourCC_video_type_BGRA)
	// {
	// 	std::cout << format << std::endl;
	// }
	switch (format)
	{
	case NDIlib_FourCC_video_type_BGRA:
		render_fmt = SDL_PIXELFORMAT_BGRA8888;
		break;
	case NDIlib_FourCC_video_type_NV12:
		render_fmt = SDL_PIXELFORMAT_NV12;
		break;
	case NDIlib_FourCC_video_type_UYVY:
		render_fmt = SDL_PIXELFORMAT_UYVY;
		break;
	default:
		return -1;
	}
	texture = SDL_CreateTexture(renderer,
															render_fmt,
															SDL_TEXTUREACCESS_STREAMING,
															width,
															height);
	if (!texture)
	{
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		return -1;
	}
	
	return 0;
}

static int render(NDIlib_FourCC_type_e format, uint8_t* buffer)
{
	if (format == NDIlib_FourCC_video_type_UYVY)
		draw_YUV(buffer);
	else if (format == NDIlib_FourCC_video_type_BGRA)
		draw_BGRA(buffer);
	else if (format == NDIlib_FourCC_video_type_NV12)
		draw_NV12(buffer);

	return 0;
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
  snd_pcm_uframes_t period_size = frames * 4; /* 2 bytes/sample, 2 channels */
	printf("Actual period size: %lu \n", period_size);

  // have a buffer with 2 periods
  buffer = (char *) malloc(period_size * periods_per_buffer);

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

static int help(char *prog_name)
{
	printf("Usage: %s [-i source_name -a snd_output]\n", prog_name);
	return 0;
}



int main(int argc, char* argv[])
{
	int opt;
	char* source_name = nullptr;
	char* audio_output = nullptr;

	while ((opt = getopt(argc, argv, "hi:a:")) != -1)
	{
		switch (opt) {
			case 'i':
				source_name = optarg;
				break;
			case 'a':
				audio_output = optarg;
				init_device(audio_output, SND_PCM_STREAM_PLAYBACK);
				printf("Audio device initiated\n");
				break;
			case 'h':
				return help(argv[0]);
				break;
		}
	}

	
	signal(SIGINT, sighandler);
  	// Not required, but "correct" (see the SDK documentation).
	if (!NDIlib_initialize())
		return 0;
	

	// Create a finder
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v3();
	if (!pNDI_find)
		return 0;

	// We now have at least one source, so we create a receiver to look at it.
	NDIlib_recv_create_v3_t create_settings;
	create_settings.color_format = (NDIlib_recv_color_format_e)NDIlib_recv_color_format_compressed_v3;
	NDIlib_source_t selected_source;
	// create_settings.bandwidth = NDIlib_recv_bandwidth_highest;
	if (source_name == nullptr) 
	{
		// number of sources found
		uint32_t no_sources = 0;
		const NDIlib_source_t *p_sources = NULL;
		// Wait until there is one source
		while (!no_sources)
		{
			// Wait until the sources on the network have changed
			printf("Receiving from first device found by NDI finder\n");
			printf("Looking for sources ...\n");
			NDIlib_find_wait_for_sources(pNDI_find, 1000 /* One second */);
			p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
			selected_source = p_sources[0];
			create_settings.source_to_connect_to.p_ndi_name = selected_source.p_ndi_name;
			printf("current source name: %s\n", p_sources->p_ndi_name);
		}
	}
	else 
	{
		// number of sources found
		uint32_t no_sources = 0;
		bool source_found = false;
		const NDIlib_source_t *p_sources = NULL;
		// Wait until there is one source
		while (!source_found)
		{
			// Wait until the sources on the network have changed
			printf("Looking for source: %s\n", source_name);
			NDIlib_find_wait_for_sources(pNDI_find, 1000 /* One second */);
			p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
			for (uint32_t i = 0; i < no_sources; i++)
			{
				if (std::string(p_sources[i].p_ndi_name).find(source_name) != std::string::npos)
				{
					selected_source = p_sources[i];
					create_settings.source_to_connect_to.p_ndi_name = selected_source.p_ndi_name;
					printf("current source name: %s\n", selected_source.p_ndi_name);
					source_found = true;
				}
			}
		}
	}



	SDL_Event event;
	snd_pcm_sframes_t rc;

	NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3(&create_settings);
	if (!pNDI_recv)
	{
		printf("Can't create receiver\n");
		return 0;
	}
	// Add custom allocator here

	NDIlib_recv_connect(pNDI_recv, &selected_source);
	
	// This sleep helps with underrun
	// Don't know why but seems like one frame takes about buffertime/10000 from the sender side
	// eg. buffertime from sender side is 83500, the latency is then 8.35ms / frame
	std::this_thread::sleep_for(std::chrono::milliseconds(audio_buffer));
	// Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
	NDIlib_find_destroy(pNDI_find);

	NDIlib_recv_performance_t total_f, drop_f;
	NDIlib_recv_get_performance(pNDI_recv, &total_f, &drop_f);
	std::cout << "Total audio frame: " << total_f.audio_frames << std::endl;
	std::cout << "Dropped audio frame: " << drop_f.audio_frames << std::endl;
	// getchar();
	NDIlib_video_frame_v2_t video_frame;
	NDIlib_audio_frame_v2_t audio_frame;


	for (;;) {
		// Without this the SDL window will appear as no responding the the OS.
		while (SDL_PollEvent(&event))
		if (event.type == SDL_QUIT)
			return 0;
		NDIlib_recv_get_performance(pNDI_recv, &total_f, &drop_f);
		switch (NDIlib_recv_capture_v2(pNDI_recv, &video_frame, &audio_frame, nullptr, 5000)) {
			// No data
			case NDIlib_frame_type_none:
				printf("No data received.\n");
				break;

				// Video data
			case NDIlib_frame_type_video:
			{
				if (!view_inited)
				{
					if (init_view(video_frame.FourCC) < 0)
					{
						errno_exit("Failed to initialize SDL window");
					}
					view_inited = true;
				}
				std::cout << "Total video frame: " << total_f.video_frames << std::endl;
				std::cout << "Dropped video frame: " << drop_f.video_frames << std::endl;
				// printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
				render(video_frame.FourCC, video_frame.p_data);
				// Free buffer queue, necessary or the memory will keep accumulating
				NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
				
				// output default video fourcc is UYVY
				break;
			}

				// Audio data
			case NDIlib_frame_type_audio:
			{
				if (audio_output == nullptr)
				{
					printf("Audio output not specified, skip\n");
					break;
				}
				std::cout << "Total audio frame: " << total_f.audio_frames << std::endl;
				std::cout << "Dropped audio frame: " << drop_f.audio_frames << std::endl;
				printf("Audio data received (%d samples).\n", audio_frame.no_samples);
				// Allocate enough space for 16bpp interleaved buffer
				time_point start = now;
				NDIlib_audio_frame_interleaved_16s_t audio_frame_16bpp_interleaved;
				audio_frame_16bpp_interleaved.reference_level = 0; // We are going to have 20dB of headroom
				audio_frame_16bpp_interleaved.p_data = new short[audio_frame.no_samples * audio_frame.no_channels];
				// Convert it
				NDIlib_util_audio_to_interleaved_16s_v2(&audio_frame, &audio_frame_16bpp_interleaved);
				// time_point end = now;
				// duration d = end - start;
				// std::cout << "Conversion time: " << d.count() << std::endl;

				// Maybe we don't need to clear buffer?
				NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);

				// start = now;
				rc = snd_pcm_writei(handle, audio_frame_16bpp_interleaved.p_data, audio_frame.no_samples);
				if (rc == -EPIPE)
				{
					/* EPIPE means underrun */
					fprintf(stderr, "underrun occurred\n");
					snd_pcm_prepare(handle);
				}
				else if (rc < 0)
				{
					fprintf(stderr,
									"error from writei: %s\n",
									snd_strerror(rc));
				}
				else if (rc != (int)audio_frame.no_samples)
				{
					fprintf(stderr,
									"short write, write %ld frames\n", rc);
				}
				delete[] audio_frame_16bpp_interleaved.p_data;
				time_point end = now;
				duration d = end - start;
				std::cout << "Audio processing time: " << d.count() << std::endl;
				break;
			}
			default:
				break;
		}
	}
  snd_pcm_drain(handle);
	snd_pcm_close(handle);
	// Destroy the receiver
	NDIlib_recv_destroy(pNDI_recv);

	// Not required, but nice
	NDIlib_destroy();

	// Finished
	return 0;
}