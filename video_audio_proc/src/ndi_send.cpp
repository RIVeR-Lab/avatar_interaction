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
#include <math.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <atomic>

// C++ libraries
#include <iostream>
#include <chrono>

// NDI headers
#include <Processing.NDI.Advanced.h>

// Config file parser
#include <libconfig.h++>

/*-------------------FFMPEG--------------------*/
extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/avutil.h>
	#include <libswscale/swscale.h>
}

// ALSA
#include <alsa/asoundlib.h>

#define now        std::chrono::high_resolution_clock::now();
using time_point = std::chrono::high_resolution_clock::time_point;
using duration   = std::chrono::duration<double>;


// Video related
static char dev_name[255] = "/dev/video0";
static char output_name[255] = "test";
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
static bool view_playback = false;

static AVCodecContext *decoder_ctx;
static AVCodec *pCodec;
static AVPacket packet_in;
static AVFrame *decoded_frame;
static void *decoded_mjpeg_buf = calloc(fmt_height*fmt_width, sizeof(uint16_t));

// Audio related
static int err;
static char *buffer;
static unsigned int rate = 48000;
static unsigned int channel = 2;
static snd_pcm_t *handle;
static snd_pcm_hw_params_t *hw_params;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
static snd_pcm_uframes_t frames = 500;   // frames per period
static unsigned int periods_per_buffer = 2;
static char snd_name[255] = "";
static NDIlib_audio_frame_interleaved_16s_t NDI_audio_frame;


static std::atomic<bool> exit_loop(false);

static void errno_exit(const char *s) 
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static void draw_YUV()
{
	// YUYV is two bytes per pixel, so multiple line width by 2

	int pitch = fmt.fmt.pix.width;
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

static void encode_video_ndi(unsigned int format)
{
	if (format == V4L2_PIX_FMT_NV12 || format == V4L2_PIX_FMT_H264)
	{
	}
	else if (format == V4L2_PIX_FMT_MJPEG)
	{
		time_point start = now;
		// The decoded mjpeg will be in YUVJ422P format
		// This is a 16bpp format, meaning each channel is expressed in 8bit, a pixel is expressed by a total of 16bit
		// swscale solution
		uint8_t *p_image = (uint8_t *)decoded_mjpeg_buf;
		auto dst_fmt = AV_PIX_FMT_UYVY422;
		struct SwsContext *conversion = sws_getContext(fmt_width,
																									 fmt_height,
																									 decoder_ctx->sw_pix_fmt,
																									 fmt_width,
																									 fmt_height,
																									 dst_fmt,
																									 SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
																									 NULL,
																									 NULL,
																									 NULL);
		uint8_t* dst_buff[1] = {p_image};
		int dst_linesize[1] = {fmt_width*2}; 
	  sws_scale(conversion, decoded_frame->data, decoded_frame->linesize, 0, fmt_height, dst_buff, dst_linesize);
		sws_freeContext(conversion);

		// forloop solution, they take about the same time.
		// uint16_t *p_image = (uint16_t *)decoded_mjpeg_buf;
		// for (size_t i = 0; i < fmt_height; i++)
		// {
		// 	for (size_t j = 0; j < fmt_width; j++)
		// 	{
		// 		// U0 Y0 V0 Y1 packing
		// 		auto p_idx = j + i * fmt_width;
		// 		if (p_idx % 2 == 0)
		// 		{
		// 			auto uy = ((uint16_t)decoded_frame->data[0][p_idx] << 8) + decoded_frame->data[1][p_idx / 2];
		// 			p_image[p_idx] = uy;
		// 		}
		// 		else
		// 		{
		// 			auto uv = ((uint16_t)decoded_frame->data[0][p_idx] << 8) + decoded_frame->data[2][p_idx / 2];
		// 			p_image[p_idx] = uv;
		// 		}

		// 	}
		// }

		NDI_video_frame.p_data = (uint8_t *)decoded_mjpeg_buf;
		time_point end = now;
		duration d = end - start;
		std::cout << "Codec conversion time: " << d.count() << std::endl;
	}
	else
	{
		printf("Unsupported NDI format");
		exit(-1);
	}
}

static void encode_audio_ndi()
{
	int rc;
	rc = snd_pcm_readi(handle, buffer, frames);
	if (rc == -EPIPE)
	{
		/* EPIPE means overrun */
		fprintf(stderr, "overrun occurred\n");
		snd_pcm_prepare(handle);
	}
	else if (rc < 0)
	{
		fprintf(stderr,
						"error from read: %s\n",
						snd_strerror(rc));
	}
	else if (rc != (int)frames)
	{
		fprintf(stderr, "short read, read %d frames\n", rc);
	}
	NDI_audio_frame.p_data = (int16_t *)buffer;
	NDIlib_util_send_send_audio_interleaved_16s(pNDI_send, &NDI_audio_frame);
}

static void send_NDI(unsigned int format)
{
	encode_video_ndi(format);
	encode_audio_ndi();
	time_point start = now;
	NDIlib_util_send_send_audio_interleaved_16s(pNDI_send, &NDI_audio_frame);
	// NDIlib_send_send_video_v2(pNDI_send, &NDI_video_frame);
	NDIlib_send_send_video_async_v2(pNDI_send, &NDI_video_frame);
	time_point end = now;
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
			case V4L2_PIX_FMT_H264:
				std::cout << "H264" << std::endl;
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
	else if ("H264" == format)
	{
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
	}
	else 
	{
		errno_exit("Unsupported format!");
	}
}

static void open_device(const char* dev_name)
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
int init_audio_device(char* snd, snd_pcm_stream_t stream_t)
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
static void init_video_device(void)
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

static void uninit_video_device(void)
{
	if (munmap(buffer_start, length) == -1)
		errno_exit("munmap");
}

static int init_ndi(const char* src_name)
{
	if (!NDIlib_initialize()) 
		errno_exit("Failed to initialize NDIlib");
  auto format = fmt.fmt.pix.pixelformat;
	
	NDIlib_send_create_t send_create_t;
	send_create_t.clock_audio = false;
	send_create_t.clock_video = false;
	send_create_t.p_ndi_name  = src_name;

	pNDI_send = NDIlib_send_create(&send_create_t);
	if (!pNDI_send) 
		errno_exit("Failed to initialize NDI send instance");
	
	// NDI video
	NDI_video_frame.xres = fmt.fmt.pix.width;
	NDI_video_frame.yres = fmt.fmt.pix.height;
	NDI_video_frame.frame_rate_N = fps*1000;
	NDI_video_frame.frame_rate_D = 1000;

	// NDI audio
	NDI_audio_frame.sample_rate = rate;
	NDI_audio_frame.no_channels = channel;
	NDI_audio_frame.no_samples = frames ;
	NDI_audio_frame.reference_level = 0;
	// NDI_audio_frame.p_data = (short*)malloc(1920 * 2 * sizeof(short));

	switch (format) {
		case V4L2_PIX_FMT_NV12:
			NDI_video_frame.FourCC = NDIlib_FourCC_type_NV12;
			// NDI_video_frame.p_data = (uint8_t*)malloc(NDI_video_frame.xres * NDI_video_frame.yres * 3/2);
			NDI_video_frame.p_data = (uint8_t *)buffer_start;
			break;
			// TODO: Add YUYV support here
		// case V4L2_PIX_FMT_YUYV:
		// 	NDI_video_frame.FourCC = NDIlib_FourCC_type_UYVY;
		// 	NDI_video_frame.p_data = (uint8_t *)buffer_start;
		// 	break;
		case V4L2_PIX_FMT_MJPEG:
			// somehow the ffmpeg decoder would decode this into YUVJ422P, assuming it's equivalent to P216
			NDI_video_frame.FourCC = NDIlib_FourCC_type_UYVY;
			break;
		case V4L2_PIX_FMT_H264:
			NDI_video_frame.FourCC = (NDIlib_FourCC_video_type_e)NDIlib_FourCC_type_H264_highest_bandwidth;
			break;
		default:
			errno_exit("Unspported format for NDI\n");
	}
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
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_H264)
	{
		printf("Input format is H264, skip rendering");
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


	if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV && view_playback)
		draw_YUV();
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
	{
		if (view_playback)
			draw_MJPEG();
		packet_in.data = (uint8_t*)buffer_start;
		packet_in.size = buf.length;
		if(AVERROR(EAGAIN) == (avcodec_send_packet(decoder_ctx, &packet_in)))
		{
			fprintf(stderr,"Flushing input raw frames\n");
			avcodec_flush_buffers(decoder_ctx);
		}
		if ((avcodec_receive_frame(decoder_ctx, decoded_frame)) == AVERROR(EAGAIN))
		{
			fprintf(stderr,"Flushing output raw frames\n");
			avcodec_flush_buffers(decoder_ctx);
		}
	}
	else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12 && view_playback)
		draw_NV12();


	if (publish_ndi) 
		send_NDI(fmt.fmt.pix.pixelformat);
	time_point end = now;
	duration d = end - start;
	std::cout << "avcodec time: " << d.count() <<std::endl;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		errno_exit("VIDIOC_QBUF");


	return 1;
}

static void init_decoder(void)
{
	pCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	if(pCodec == NULL)
	{
		fprintf(stderr, "Unsupported codec\n");
		exit(EXIT_FAILURE);
	}
	decoder_ctx = avcodec_alloc_context3(pCodec);
	decoder_ctx->codec_id = AV_CODEC_ID_MJPEG;
	decoder_ctx->flags = AV_CODEC_FLAG_LOW_DELAY;
	decoder_ctx->width = fmt_width;
	decoder_ctx->coded_width = fmt_width;
	decoder_ctx->height = fmt_height;
	decoder_ctx->coded_height = fmt_height;
	// The decoder will override the format.
	// For MJPEG the common decoded format is YUVJ422P
	decoder_ctx->pix_fmt = AV_PIX_FMT_UYVY422;
	decoder_ctx->framerate = AVRational{1,60};
	decoder_ctx->thread_count = 0;
	if (pCodec->capabilities | AV_CODEC_CAP_FRAME_THREADS)
		decoder_ctx->thread_type = FF_THREAD_FRAME;
	else if (pCodec->capabilities | AV_CODEC_CAP_SLICE_THREADS)
		decoder_ctx->thread_type = FF_THREAD_SLICE;
	else
	{
		decoder_ctx->thread_count = 1; // don't use multithreading
		printf("The decoder doesn't support multithreading\n");
	}

	av_init_packet(&packet_in);
	//packet_in.buf = av_buffer_create(buffers->start, buffers->length, free_buffers, decoded_frame->opaque, AV_BUFFER_FLAG_READONLY);
	if(packet_in.buf == NULL)
	{
		printf("Error starting AVBufferRef\n");
		av_packet_unref(&packet_in);
	}
	
	if(avcodec_open2(decoder_ctx, pCodec, NULL) < 0)
	{
		fprintf(stderr, "Error opening codec\n");
		exit(EXIT_FAILURE);
	}
	
	decoded_frame = av_frame_alloc();
	if(decoded_frame == NULL)
	{
		fprintf(stderr, "Error allocating decoded data\n");
		exit(EXIT_FAILURE);
	}
	decoded_frame->width = fmt_width;
	decoded_frame->height = fmt_height;
	decoded_frame->format = decoder_ctx->pix_fmt;
}

static void mainloop(void)
{
	SDL_Event event;
	for (;;) {
		while (SDL_PollEvent(&event))
			if (event.type == SDL_QUIT)
				return;
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			// Wait for file descriptor to be ready to read
			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);
			if (-1 == r)
			{
				if (EINTR == errno)
					continue;

				errno_exit("select");
			}

			if (0 == r)
			{
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

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

static int help(const char *prog_name)
{
	printf("Usage: %s [-c config_path]\n", prog_name);
	return 0;
}

static int init_config(const char * config_path)
{
	using namespace libconfig;
	Config cfg;
	// Read the file. If there is an error, report it and exit.
  try
  {
		cfg.readFile(config_path);
	}
  catch(const FileIOException &fioex)
  {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

	if (!cfg.lookupValue("fmt_width", fmt_width))
		printf("fmt_width is not set, using default value\n");
	if (!cfg.lookupValue("fmt_height", fmt_height))
		printf("fmt_height is not set, using default value\n");
	if (!cfg.lookupValue("fps", fps))
		printf("fps is not set, using default value\n");
	memset(dev_name, 0, sizeof(dev_name));
	memset(output_name, 0, sizeof(output_name));
	memset(snd_name, 0, sizeof(snd_name));
	strcpy(dev_name, cfg.lookup("dev_name").c_str());
	strcpy(output_name, cfg.lookup("output_name").c_str());
	strcpy(snd_name, cfg.lookup("snd_name").c_str());


	return 0;
}

int main(int argc, char **argv)
{
	int opt;

	if (argc != 3){
		help(argv[0]);
		exit(-1);
	}

	while ((opt = getopt(argc, argv, "hc:")) != -1)
	{
		switch (opt) {
			case 'c':
				if(init_config(argv[2]) != 0)
					exit(-1);
				break;
			case 'h':
				return help(argv[1]);
		}
	}

	open_device(dev_name);
	init_video_device();
	printf("Video device initiated");
	init_audio_device(snd_name, SND_PCM_STREAM_CAPTURE);
	printf("Audio device initiated");

	if (view_playback)
		if (init_view())
		{
			close_device();
			return -1;
		}
	init_decoder();

	if (init_ndi(output_name)) {
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
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	// Free the video frame
	free(NDI_audio_frame.p_data);
	uninit_video_device();
	close_device();
	// Destroy the NDI sender
	NDIlib_send_destroy(pNDI_send);
	// Not required, but nice
	NDIlib_destroy();
	free(decoded_mjpeg_buf);
	return 0;
}
