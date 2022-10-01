// sudo apt install libsdl2-dev
#include <SDL2/SDL.h>
// sudo apt install libsdl2-image-dev
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

// C++ libraries
#include <iostream>
#include <chrono>

// NDI headers
#include <Processing.NDI.Advanced.h>

static char *dev_name = (char*)"/dev/video0";
static int fd         = -1;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static void *buffer_start   = NULL;
static size_t length        = 0;
static struct v4l2_format fmt;
static unsigned int fmt_width = 1920;
static unsigned int fmt_height = 1080;
static unsigned int fps = 60;
static NDIlib_video_frame_v2_t NDI_video_frame;
static NDIlib_send_instance_t pNDI_send;
static bool publish_ndi = true;

#define now        std::chrono::high_resolution_clock::now();
using time_point = std::chrono::high_resolution_clock::time_point;
using duration   = std::chrono::duration<double>;

static void errno_exit(const char *s) 
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static void draw_YUV()
{
	// YUYV is two bytes per pixel, so multiple line width by 2

	int pitch = fmt.fmt.pix.width * 2;
	// SDL_LockTexture(texture, NULL, &buffer_start, &pitch);
	// SDL_UnlockTexture(texture);
	SDL_UpdateTexture(texture, NULL, buffer_start, pitch);


	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static void draw_MJPEG()
{
	SDL_RWops *buf_stream = SDL_RWFromMem(buffer_start, (int)length);
	SDL_Surface *frame = IMG_Load_RW(buf_stream, 0);
	SDL_Texture *tx = SDL_CreateTextureFromSurface(renderer, frame);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, tx, NULL, NULL);
	SDL_RenderPresent(renderer);

	SDL_DestroyTexture(tx);
	SDL_FreeSurface(frame);
	SDL_RWclose(buf_stream);
}

static void draw_NV12()
{
	int pitch = fmt.fmt.pix.width;
	SDL_UpdateTexture(texture, NULL, buffer_start, pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static void send_NDI()
{
	time_point start = now;
	// NDIlib_send_send_video_v2(pNDI_send, &NDI_video_frame);
	NDIlib_send_send_video_async_v2(pNDI_send, &NDI_video_frame);
	time_point end   = now;
	duration d = end - start;
	std::cout << "NDI processing time: " << d.count() << std::endl;
}

static int sdl_filter(void *userdata, SDL_Event *event)
{
	(void)userdata;
	return event->type == SDL_QUIT;
}

static void init_mmap(void)
{
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count  = 1;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		errno_exit("VIDIOC_REQBUFS");

	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
		errno_exit("VIDIOC_QUERYBUF");

	length = buf.length;
	buffer_start = mmap(NULL /* start anywhere */,
			length,
			PROT_READ | PROT_WRITE /* required */,
			MAP_SHARED /* recommended */,
			fd, buf.m.offset);

	if (buffer_start == MAP_FAILED)
		errno_exit("mmap");
}

static void get_pixelformat()
{
	struct v4l2_fmtdesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	// iterate over all formats, and prefer MJPEG when available
	std::cout << "This camera supports the following formats:" << std::endl;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
		desc.index++;
		switch (desc.pixelformat) {
			case V4L2_PIX_FMT_MJPEG:
				std::cout << "MJPEG" << std::endl;
				break;
			case V4L2_PIX_FMT_NV12:
				std::cout << "NV12" << std::endl;
				break;
			case V4L2_PIX_FMT_YUYV:
				std::cout << "YUYV" << std::endl;
				break;
			default:
				std::cout << "Unsupported" << std::endl;
				break;
		}
	}
	std::cout << "Please type a format from above options:" << std::endl;
	std::string format;
	std::cin  >> format;
	if ("MJPEG" == format)
	{
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	}
	else if ("NV12" == format)
	{
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	}
	else if ("YUYV" == format)
	{
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	}
}

static void open_device(char* dev_name)
{
	if ((fd = open(dev_name, O_RDWR)) < 0) {
		perror("Can't open device");
		exit(1);
	}
}
static void close_device(void)
{
	if (close (fd) == -1)
		errno_exit("close");
}

static void init_device(void)
{
	struct v4l2_capability cap;

	memset(&cap, 0, sizeof(cap));
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
		errno_exit("VIDIOC_QUERYCAP");

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
		exit(EXIT_FAILURE);
	}

	// Default to YUYV
	memset(&fmt, 0, sizeof(fmt));
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	get_pixelformat();

	// it'll adjust to the bigger screen available in the driver
	fmt.fmt.pix.width  = fmt_width;
	fmt.fmt.pix.height = fmt_height;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
		errno_exit("VIDIOC_S_FMT");
	
	// Set framerate
	struct v4l2_streamparm streamparm;
	memset(&streamparm, 0, sizeof(streamparm));
	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_PARM, &streamparm) < 0)
	{
		errno_exit("Failed to obtain frame info");
	}

	streamparm.parm.capture.capability |= V4L2_CAP_TIMEPERFRAME;
	streamparm.parm.capture.timeperframe.numerator = 1;
	streamparm.parm.capture.timeperframe.denominator = fps;

	if (ioctl(fd, VIDIOC_S_PARM, &streamparm) < 0)
	{
		fprintf(stderr, "Faild to set framerate at %d fps", fps);
	}
	init_mmap ();
}

static void uninit_device(void)
{
	if (munmap(buffer_start, length) == -1)
		errno_exit("munmap");
}

static int init_ndi()
{
	if (!NDIlib_initialize()) 
		errno_exit("Failed to initialize NDIlib");

	pNDI_send = NDIlib_send_create();
	if (!pNDI_send) 
		errno_exit("Failed to initialize NDI send instance");
	
	NDI_video_frame.xres = fmt.fmt.pix.width;
	NDI_video_frame.yres = fmt.fmt.pix.height;
	NDI_video_frame.FourCC = NDIlib_FourCC_type_NV12;
	// NDI_video_frame.p_data = (uint8_t*)malloc(NDI_video_frame.xres * NDI_video_frame.yres * 3/2);
	NDI_video_frame.p_data = (uint8_t*)buffer_start;
	return 0;
}

static int init_view()
{
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Unable to initialize SDL\n");
		return -1;
	}

	if (SDL_CreateWindowAndRenderer(fmt.fmt.pix.width
					, fmt.fmt.pix.height
					, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
					, &window
					, &renderer)) {
		printf("SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
		return -1;
	}

	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
		texture = SDL_CreateTexture(renderer
						// YUY2 is also know as YUYV in SDL
						, SDL_PIXELFORMAT_YUY2
						, SDL_TEXTUREACCESS_STREAMING
						, fmt.fmt.pix.width
						, fmt.fmt.pix.height);
		if (!texture) {
			printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
			return -1;
		}
	} else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
		if (!IMG_Init(IMG_INIT_JPG)) {
			printf("Unable to initialize IMG\n");
			return -1;
		}
	} 
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
	{
		texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_NV12,
							SDL_TEXTUREACCESS_STREAMING,
							fmt.fmt.pix.width,
							fmt.fmt.pix.height);
		if (!texture) {
			printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
			return -1;
		}
	}

	printf("Device: %s\nWidth: %d\nHeight: %d\nFramerate: %d\n"
		, dev_name, fmt.fmt.pix.width, fmt.fmt.pix.height, fps);

	return 0;
}

static void start_capturing(void)
{
	struct v4l2_buffer buf;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	memset(&buf, 0, sizeof(buf));
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		errno_exit("VIDIOC_QBUF ... !!!");

	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
		errno_exit("VIDIOC_STREAMON");
}

static void stop_capturing(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
		errno_exit("VIDIOC_STREAMOFF");
}

static int read_frame()
{
	time_point start = now;
	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit("VIDIOC_DQBUF");
		}
	}

	time_point end = now;
	duration d = end - start;
	std::cout << "DQBUF: " << d.count() <<std::endl;

	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
		draw_YUV();
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
		draw_MJPEG();
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
		draw_NV12();

	if (publish_ndi) 
		send_NDI();

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		errno_exit("VIDIOC_QBUF");


	return 1;
}

static void mainloop(void)
{
	SDL_Event event;
	for (;;) {
		while (SDL_PollEvent(&event))
			if (event.type == SDL_QUIT)
				return;
		for (;;) {
			// fd_set fds;
			// struct timeval tv;
			// int r;

			// // Wait for file descriptor to be ready to read
			// FD_ZERO(&fds);
			// FD_SET(fd, &fds);

			// tv.tv_sec = 2;
			// tv.tv_usec = 0;

			// r = select(fd + 1, &fds, NULL, NULL, &tv);
			// if (-1 == r)
			// {
			// 	if (EINTR == errno)
			// 		continue;

			// 	errno_exit("select");
			// }

			// if (0 == r)
			// {
			// 	fprintf(stderr, "select timeout\n");
			// 	exit(EXIT_FAILURE);
			// }

			time_point start = now;
			if (read_frame())
			{
				time_point end   = now;
				duration d = end - start;
				std::cout << "One loop takes " << d.count() << "s" << std::endl;
				break;
			}

			// EAGAIN - continue select loop.
		}
	}
}

static void close_view()
{
	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
		SDL_DestroyTexture(texture);

	SDL_DestroyRenderer(renderer);

	SDL_DestroyWindow(window);

	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
		IMG_Quit();

	if (SDL_WasInit(SDL_INIT_VIDEO)) 
		SDL_Quit();
}

static int help(char *prog_name)
{
	printf("Usage: %s [-d dev_name]\n", prog_name);
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "hd:")) != -1)
	{
		switch (opt) {
			case 'd':
				dev_name = optarg;
				break;

			case 'h':
				return help(argv[0]);
		}
	}

	if (argc == 1){
		printf("Capturing from default device: %s\n", dev_name);
	}
	open_device(dev_name);
	init_device();

	if(init_view()) {
		close_device();
		return -1;
	}		

	if (init_ndi()) {
		// Destroy the NDI sender
		NDIlib_send_destroy(pNDI_send);
		// Not required, but nice
		NDIlib_destroy();
		return -1;
	}

	SDL_SetEventFilter(sdl_filter, NULL);

	start_capturing();
	mainloop();
	stop_capturing();
	close_view();
	uninit_device();
	close_device();
	// Destroy the NDI sender
	NDIlib_send_destroy(pNDI_send);
	// Not required, but nice
	NDIlib_destroy();
	return 0;
}
