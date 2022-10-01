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



static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static unsigned int width = 1920;
static unsigned int height = 1080;
std::string pixelformat = "UYVY";
static void *buffer_start   = NULL;
static int length = 0;

static void errno_exit(const char *s) 
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

void sighandler(int signum) {
	errno_exit("SIGINT received");
}

static void draw_YUV(void *buffer)
{
	// YUYV is two bytes per pixel, so multiple line width by 2
	int pitch = width*2;
	// SDL_LockTexture(texture, NULL, &buffer_start, &pitch);
	// SDL_UnlockTexture(texture);
	std::cout << "in draw YUV" << std::endl;
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	std::cout << "updated texture" << std::endl;
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static void draw_MJPEG(void *buffer)
{
	SDL_RWops *buf_stream = SDL_RWFromMem(buffer, (int)length);
	SDL_Surface *frame = IMG_Load_RW(buf_stream, 0);
	SDL_Texture *tx = SDL_CreateTextureFromSurface(renderer, frame);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, tx, NULL, NULL);
	SDL_RenderPresent(renderer);

	SDL_DestroyTexture(tx);
	SDL_FreeSurface(frame);
	SDL_RWclose(buf_stream);
}

static void draw_NV12(void *buffer)
{
	int pitch = width;
	SDL_UpdateTexture(texture, NULL, buffer, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static int init_view()
{
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Unable to initialize SDL\n");
		return -1;
	}

	if (SDL_CreateWindowAndRenderer(width
					, height
					, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
					, &window
					, &renderer)) {
		printf("SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
		return -1;
	}

	if (pixelformat == "YUYV") {
		texture = SDL_CreateTexture(renderer
						// YUY2 is also know as YUYV in SDL
						, SDL_PIXELFORMAT_YUY2
						, SDL_TEXTUREACCESS_STREAMING
						, width
						, height);
		if (!texture) {
			printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
			return -1;
		}
	} else if (pixelformat == "MJPEG") {
		if (!IMG_Init(IMG_INIT_JPG)) {
			printf("Unable to initialize IMG\n");
			return -1;
		}
	} 
	else if (pixelformat == "NV12")
	{
		texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_NV12,
							SDL_TEXTUREACCESS_STREAMING,
							width,
							height);
		if (!texture) {
			printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
			return -1;
		}
	}
	else if (pixelformat == "UYVY")
	{
		texture = SDL_CreateTexture(
			renderer,
			SDL_PIXELFORMAT_UYVY,
			SDL_TEXTUREACCESS_STREAMING,
			width,
			height
		);
		if (!texture) {
			printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
			return -1;
		}
	}
	return 0;
}

static int render(void* buffer)
{
	if (pixelformat == "YUV" || pixelformat == "UYVY")
		draw_YUV(buffer);
	else if (pixelformat == "MJPEG")
		draw_MJPEG(buffer);
	else if (pixelformat == "NV12")
		draw_NV12(buffer);

	return 0;
}

int main(int argc, char* argv[])
{
	signal(SIGINT, sighandler);
  	// Not required, but "correct" (see the SDK documentation).
	if (!NDIlib_initialize())
		return 0;

	// Create a finder
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v3();
	if (!pNDI_find)
		return 0;

	// Wait until there is one source
	uint32_t no_sources = 0;
	const NDIlib_source_t* p_sources = NULL;
	while (!no_sources) {
		// Wait until the sources on the network have changed
		printf("Looking for sources ...\n");
		NDIlib_find_wait_for_sources(pNDI_find, 1000/* One second */);
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
		printf("current source name: %s\n", p_sources->p_ndi_name);
	}

	// We now have at least one source, so we create a receiver to look at it.
	NDIlib_recv_create_v3_t recv_param;
	recv_param.color_format = NDIlib_recv_color_format_fastest;
	NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3(&recv_param);

	if (!pNDI_recv)
		return 0;

	// Connect to our sources
	NDIlib_recv_connect(pNDI_recv, p_sources + 0);

	// Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
	NDIlib_find_destroy(pNDI_find);
	// The descriptors
	static NDIlib_video_frame_v2_t video_frame;
	static NDIlib_audio_frame_v2_t audio_frame;
	video_frame.p_data = (uint8_t*)malloc(width * height * 4);
	// memset(video_frame.p_data, 0, 4*width*height);

	if (init_view() < 0)
	{
		errno_exit("Failed to initialize SDL window");
	}

	int count = 0;
	// Run for one minute
	using namespace std::chrono;
	SDL_Event event;
	for (;;) {
		// Without this the SDL window will appear as no responding the the OS.
		while (SDL_PollEvent(&event))
		if (event.type == SDL_QUIT)
			return 0;
		switch (NDIlib_recv_capture_v2(pNDI_recv, &video_frame, &audio_frame, nullptr, 5000)) {
			// No data
			case NDIlib_frame_type_none:
				printf("No data received.\n");
				break;

				// Video data
			case NDIlib_frame_type_video:
				printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
				render(video_frame.p_data);
				// Free buffer queue, necessary or the memory will keep accumulating
				NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
				
				// output default video fourcc is UYVY
				std::cout << count++ <<std::endl;
				break;

				// Audio data
			case NDIlib_frame_type_audio:
				printf("Audio data received (%d samples).\n", audio_frame.no_samples);
				NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
				break;
			
			default:
				break;
		}
	}

	// Destroy the receiver
	NDIlib_recv_destroy(pNDI_recv);

	// Not required, but nice
	NDIlib_destroy();

	// Finished
	return 0;
}