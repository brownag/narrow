
#pragma once

#include "array.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif


int carrow_array_copy(struct CarrowArray* array_dst, int64_t dst_offset,
                      struct CarrowArray* array_src, int64_t src_offset,
                      int64_t n_elements, int32_t which_buffers,
                      struct ArrowStatus* status);

#ifdef __cplusplus
}
#endif
