#include "utils.h"

uint32_t bit_update(uint32_t data, bool is_inc, int len) {
  uint32_t ret;
  uint32_t data_max = (1 << len) - 1;
  if (is_inc) {
    if (data >= data_max)
      ret = data_max;
    else
      ret = data + 1;

  } else {
    if (data == 0)
      ret = 0;
    else
      ret = data - 1;
  }
  return ret & data_max;
}
