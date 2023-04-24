#include <chrono>
#include <Processing.NDI.Advanced.h>
// sudo apt install libsdl2-dev
#include <SDL2/SDL.h>
// sudo apt install libsdl2-image-dev
#include <SDL2/SDL_image.h>
// sudo apt install libsdl2-ttf-dev
#include <SDL2/SDL_ttf.h>
#include <string>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <stdio.h>
#include <atomic>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */
#include <errno.h>

// ALSA
#include <alsa/asoundlib.h>

// utils
#include "utils/video_frame_proc.h"

// ROS
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <franka_control/PInfo.h>

// Video config
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Texture *text_texture = NULL;
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
static int  audio_buffer = 100;
static bool* voice_detected = NULL;
static char shm_path[15] = "voice_shm";
static bool echo_cancel = false;
static SDL_Rect ui_rect;
static double mute_delay = 1.0;

// ROS variables
static float left_weight = 0.0;
static int   left_catching = 0;
static float right_weight = 0.0;
static int   right_catching = 0;


#define current_time   std::chrono::high_resolution_clock::now()
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
static void init_ui()
{
	unsigned int ui_width = 1000;
	unsigned int ui_height = 100;
	unsigned int ui_box_x  = 100;
	unsigned int ui_box_y  = 0;

	ui_rect.x = ui_box_x;
	ui_rect.y = ui_box_y;
	ui_rect.h = ui_height;
	ui_rect.w = ui_width;
}

static SDL_Texture* generate_UI()
{
	// UI
	TTF_Font* Font = TTF_OpenFont("/usr/share/fonts/truetype/abyssinica/AbyssinicaSIL-Regular.ttf", 100);

	if (Font == NULL)
	{
		printf("Failed to get specified font! Error: %s\n", TTF_GetError());
		exit(-1);
	}
	SDL_Color fg = {255, 0, 0, 125};
	SDL_Color bg = {0, 0, 0, 125};
	std::string left_weight_text;
	std::string right_weight_text;
	std::string catching_text;
	if (left_catching || right_catching)
	{
		catching_text = "Recalibrating, please slow down.\n";
	}
	else
	{
		catching_text = "";
	}
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2) << left_weight;
	left_weight_text = "Left arm payload: " + ss.str() + "\n";
	ss << std::fixed << std::setprecision(2) << right_weight;
	right_weight_text = "Right arm payload: " + ss.str() + "\n";
	SDL_Surface *surfaceMessage =
			TTF_RenderText_Shaded(Font, (left_weight_text + right_weight_text + catching_text).c_str(),fg, bg);
	SDL_Texture *Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
	SDL_FreeSurface(surfaceMessage);
	TTF_CloseFont(Font);

	return Message;
}

static void draw_YUV(uint8_t *buffer)
{
	int pitch = 2*width;
	init_ui();

	auto Message = generate_UI();
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	// SDL_RenderCopy(renderer, Message, NULL, &ui_rect);
	SDL_Point center = {100 , 100};
	SDL_RenderCopyEx(renderer, Message, NULL, &ui_rect, -0, &center, SDL_FLIP_NONE);
	SDL_RenderPresent(renderer);
	SDL_DestroyTexture(Message);

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

	text_texture = SDL_CreateTexture(
			renderer,
			SDL_PIXELFORMAT_BGRA32,
			SDL_TEXTUREACCESS_STREAMING,
			500,
			500);
	SDL_SetTextureBlendMode(text_texture, SDL_BLENDMODE_BLEND);

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
		printf("Unspported NDI format\n");	
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

void init_shm()
{
  int fd = shm_open(shm_path, O_RDONLY, 0);
  if (fd == -1)
  {
    printf("Shared memory error: %s\n", strerror(errno));
  }
  voice_detected = (bool*)mmap(NULL, sizeof(bool), PROT_READ, MAP_SHARED, fd, 0);
}


static int help(char *prog_name)
{
	printf("Usage: %s [-i source_name -a snd_output -e [turn on echo cancellation]]\n", prog_name);
	return 0;
}

void left_arm_info_callback(const franka_control::PInfoConstPtr& msg)
{
	left_weight = msg->external_load_mass;
	left_catching = msg->slow_catching_index;
}

void right_arm_info_callback(const franka_control::PInfoConstPtr& msg)
{
	right_weight = msg->external_load_mass;
	right_catching = msg->slow_catching_index;
}


int main(int argc, char* argv[])
{
	int opt;
	char* source_name = nullptr;
	char* audio_output = nullptr;

	#ifdef DEBUG_PRINT
		printf("Debug message\n");
	#endif

	while ((opt = getopt(argc, argv, "hi:a:e:")) != -1)
	{
		switch (opt) {
			case 'i':
				source_name = optarg;
				break;
			case 'a':
				audio_output = optarg;
				printf("Audio device initiated\n");
				break;
			case 'e':
				echo_cancel = true;
				printf("Turned on echo cancellation");
				break;
			case 'h':
				return help(argv[0]);
				break;
		}
	}

  ros::init(argc, argv, "operator_ui_node");
	ros::NodeHandle nh;
	ros::Subscriber left_panda_sub = nh.subscribe("/left_panda_info", 1, left_arm_info_callback); 
	ROS_INFO_STREAM("Subscribing to left panda");
	ros::Subscriber right_panda_sub = nh.subscribe("/right_panda_info", 1, right_arm_info_callback); 
	ROS_INFO_STREAM("Subscribing to left panda");
	
	signal(SIGINT, sighandler);
  	// Not required, but "correct" (see the SDK documentation).
	if (!NDIlib_initialize())
		return 0;
	
	TTF_Init();

	// Create a finder
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v3();
	if (!pNDI_find)
		return 0;

	// We now have at least one source, so we create a receiver to look at it.
	NDIlib_recv_create_v3_t create_settings;
	create_settings.color_format = NDIlib_recv_color_format_fastest;
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

	// init shared memory for voice detection flag
	if(echo_cancel)
		init_shm();

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
	uint32_t f0 = 0;
	time_point t0 = current_time;
	float frame = 0.0;

	time_point silence_timer = current_time;
	bool full_screen = false;
	for (;;) {
		// Without this the SDL window will appear as no responding the the OS.
		while (SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					return 0;
			 	case SDL_KEYDOWN:
					if (event.key.keysym.scancode == SDL_SCANCODE_F)
					{
						full_screen = !full_screen;
						if (full_screen)
						{
							SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
						}
						else
						{
							SDL_SetWindowFullscreen(window, 0);
						}
					}
					if (event.key.keysym.scancode == SDL_SCANCODE_Q)
					{
						return 0;
					}
				break;
			}
		}
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
					width = video_frame.xres; 
					height = video_frame.yres;
					if (init_view(video_frame.FourCC) < 0)
					{
						errno_exit("Failed to initialize SDL window");
					}
					view_inited = true;
				}
				auto f1 = total_f.video_frames;
				if (f1 -f0 >= 120)
				{
					time_point t1 = current_time;
					auto duration = t1 - t0;
					frame = 120.0 / duration.count() * 1000000000;
					f0 = f1;
					t0 = t1;
				}
				std::cout << "Framerate: " << frame << std::endl;
				std::cout << "Total video frame: " << total_f.video_frames << std::endl;
				std::cout << "Dropped video frame: " << drop_f.video_frames << std::endl;
				int64_t current = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				// std::cout << "Current: " << current << std::endl;
				// std::cout << "Timestamp: " << video_frame.timecode/10 << std::endl;
				std::cout << "Latency: "<< (current - video_frame.timecode/10) / 1000 << "ms" << std::endl;
				// printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
				render(video_frame.FourCC, video_frame.p_data);
				// Free buffer queue, necessary or the memory will keep accumulating
				NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
				
				// output default video fourcc is UYVY
				break;
			}
			default:
				break;
		}
	}
	shm_unlink(shm_path);
  snd_pcm_drain(handle);
	snd_pcm_close(handle);
	// Destroy the receiver
	NDIlib_recv_destroy(pNDI_recv);

	// Not required, but nice
	NDIlib_destroy();
	// Finished
	return 0;
}