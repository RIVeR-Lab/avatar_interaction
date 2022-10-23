#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "opencv2/highgui.hpp"


namespace video_frame_proc
{
  // (x1, y1) top left point, (x2, y2) bottom right point, stride: bytes of a row
  /**
   * @brief Crop an image encoded in UYUV. This is majorly for NDI.
   * 
   * @param src_buf source frame buffer
   * @param src_width source frame width
   * @param src_height sourc frame height
   * @param dst_buf destination frame buffer
   * @param x1 upper left pixel of the cropping region
   * @param y1 lower left pixel of the cropping region
   * @param x2 upper right pixel of the cropping region
   * @param y2 lower right pixel of the cropping region
   */
  void crop_uyvy(const uint8_t *src_buf, uint16_t src_width, uint16_t src_height,
                 uint8_t *dst_buf, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
  {
    // round x coordinates to an integer divisable by 2 because uyvy is aligned in a macropixel = 2 pixels
    // Each pixel in uyvy contains 2 bytes
    assert(x1 > 0 && x1 < src_width);
    assert(x2 > 1 && x2 < src_width);
    assert(y1 > 0 && y1 < src_height);
    assert(y2 > y1 && y2 < src_height);

    uint16_t stride = src_width * 2;
    const uint8_t *row_start = src_buf + y1 * stride + x1 * 2;
    uint16_t width = x2 - x1 + 1;
    uint16_t height = y2 - y1 + 1;

    assert(dst_buf != NULL);

    for (int i = 0; i < height; i++)
    {
      memcpy(dst_buf, row_start, width * 2);
      row_start += stride;
      dst_buf += width * 2;
    }
  }

  /**
   * @brief Fill an SDL texture with OpenCV Mat type data
   * 
   * @param texture destination SDL texture
   * @param mat source OpenCV mat data
   */
  void fill_texture(SDL_Texture *texture, cv::Mat const &mat)
  {
    unsigned char *texture_data = NULL;
    int texture_pitch = 0;

    SDL_LockTexture(texture, 0, (void **)&texture_data, &texture_pitch);
    memcpy(texture_data, mat.data, mat.rows * mat.cols * mat.channels());
    SDL_UnlockTexture(texture);
  }
}