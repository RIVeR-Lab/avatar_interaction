#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

#include <linux/videodev2.h>


int main(void) {
  int fd;

	// Open the device

	if ((fd = open("/dev/video0", O_RDWR)) < 0) {
		perror("Open");
		exit(1);
	}

	// Retrieve the device's capabilities

	struct v4l2_capability cap;

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		perror("VIDIOC_QUERYCAP");
		exit(1);
	}

	// printf("%u\n", cap.capabilities & V4L2_CAP_STREAMING);
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "This device does not have video streaming capabilties.");
		exit(1);
	}

	// Set the video format

	struct v4l2_format format;
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.width = 1920;
	format.fmt.pix.height = 1080;

	if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
		perror("VIDIOC_S_FMT");
		exit(1);
	}

	// Inform the device about future buffers

	struct v4l2_requestbuffers req_buf;
	req_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req_buf.memory = V4L2_MEMORY_MMAP;
	req_buf.count = 1;

	if (ioctl(fd, VIDIOC_REQBUFS, &req_buf) < 0) {
		perror("VIDIOC_REQBUFS");
		exit(1);
	}

	// Create the buffers and memory map

	struct v4l2_buffer buffer_info;
	memset(&buffer_info, 0, sizeof(buffer_info));

	buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer_info.memory = V4L2_MEMORY_MMAP;
	buffer_info.index = 0;

	if (ioctl(fd, VIDIOC_QUERYBUF, &buffer_info) < 0) {
		perror("VIDIOC_QUERYBUF");
		exit(1);
	}

	void* buffer_start = mmap(NULL, buffer_info.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer_info.m.offset);

	if (buffer_start == MAP_FAILED) {
		perror("map");
		exit(1);
	}

	memset(buffer_start, 0, buffer_info.length);

	int type = buffer_info.type;

	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		perror("VIDIOC_STREAMON");
		exit(1);
	}

	// Below processes the frames and outputs them to terminal
	// direct output to a file to save them

	static int count = 0;
	const int num_frames = 10;
	time_t start, end;
	start = clock();

	// This is where the buffers are queried and the frame pointer is updated
	while (count < num_frames) {
		
		if (ioctl(fd, VIDIOC_QBUF, &buffer_info) < 0) {
			perror("VIDIOC_QBUF");
			exit(1);
		}

		if (ioctl(fd, VIDIOC_DQBUF, &buffer_info) < 0) {
			perror("VIDIOC_DQBUF");
			exit(1);
		}
		
		// Write what is contained in the buffer's memory address to the output
		// TODO: INSTEAD OF OUTPUTTING THIS, SEND THROUGH NDI!!!
		fwrite(buffer_start, buffer_info.length, 1, stdout);

		count++;
	}
	end = clock();
	
	double total_time = (double)(end - start)/CLOCKS_PER_SEC;

	fprintf(stderr, "Elapsed time = %f seconds", total_time);

	close(fd);
	return EXIT_SUCCESS;
}
