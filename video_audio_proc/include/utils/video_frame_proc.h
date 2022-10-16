#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <assert.h>

// (x1, y1) top left point, (x2, y2) bottom right point, stride: bytes of a row
static void crop_uyvy(const uint8_t *src_buf, uint16_t src_width, uint16_t src_height,
      uint8_t* dst_buf, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  // round x coordinates to an integer divisable by 2 because uyvy is aligned in a macropixel = 2 pixels
  // Each pixel in uyvy contains 2 bytes
  assert(x1 > 0 && x1 < src_width);
  assert(x2 > 1 && x2 < src_width);
  assert(y1 > 0 && y1 < src_height);
  assert(y2 > y1 && y2 < src_height);

  uint16_t stride = src_width * 2;
  const uint8_t* row_start = src_buf + y1 * stride + x1 * 2;
  uint16_t width = x2 - x1 + 1;
  uint16_t height = y2 - y1 + 1;

  assert(dst_buf != NULL);

  for (int i = 0; i < height; i++)
  {
    memcpy(dst_buf, row_start, width*2);
    row_start += stride;
    dst_buf += width*2;
  }

}