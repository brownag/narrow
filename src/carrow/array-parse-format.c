
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "array.h"
#include "status.h"

void carrow_array_set_primitive(struct CarrowArray* array, enum ArrowType type, uint64_t size) {
  array->type = type;
  array->data_buffer_type = array->type;
  array->n_buffers = 2;
  array->data_buffer_id = 1;
  array->element_size_bytes = size;
}

int carrow_array_parse_format(struct CarrowArray* array, const char* format, struct ArrowStatus* status) {
  carrow_status_reset(status);

  int format_len = strlen(format);
  const char* end_ptr = format + strlen(format) + 1;

  if (format_len == 0) {
    carrow_status_set_error(status, EINVAL, "`format` had zero characters");
    RETURN_IF_NOT_OK(status);
  }

  switch (format[0]) {

  // null type with no buffers
  case 'n':
    array->type = CARROW_TYPE_NA;
    array->n_buffers = 0;
    return 0;

    // types with data or validity + data buffers
  case 'b':
    array->type = CARROW_TYPE_BOOL;
    array->n_buffers = 2;
    array->data_buffer_id = 1;
    return 0;
  case 'c':
    carrow_array_set_primitive(array, CARROW_TYPE_INT8, sizeof(int8_t));
    return 0;
  case 'C':
    carrow_array_set_primitive(array, CARROW_TYPE_UINT8, sizeof(uint8_t));
    return 0;
  case 's':
    carrow_array_set_primitive(array, CARROW_TYPE_INT16, sizeof(int16_t));
    return 0;
  case 'S':
    carrow_array_set_primitive(array, CARROW_TYPE_UINT16, sizeof(uint16_t));
    return 0;
  case 'i':
    carrow_array_set_primitive(array, CARROW_TYPE_INT32, sizeof(int32_t));
    return 0;
  case 'I':
    carrow_array_set_primitive(array, CARROW_TYPE_UINT32, sizeof(uint32_t));
    return 0;
  case 'l':
    carrow_array_set_primitive(array, CARROW_TYPE_INT64, sizeof(int64_t));
    return 0;
  case 'L':
    carrow_array_set_primitive(array, CARROW_TYPE_UINT64, sizeof(uint64_t));
    return 0;
  case 'e':
    carrow_array_set_primitive(array, CARROW_TYPE_HALF_FLOAT, 2);
    return 0;
  case 'f':
    carrow_array_set_primitive(array, CARROW_TYPE_FLOAT, 4);
    return 0;
  case 'g':
    carrow_array_set_primitive(array, CARROW_TYPE_DOUBLE, 8);
    return 0;

    // fixed-width binary
  case 'w':
    array->type = CARROW_TYPE_FIXED_SIZE_BINARY;
    array->n_buffers = 1;
    array->data_buffer_id = 0;
    char* parse_end_ptr;
    array->element_size_bytes = strtol(format + 2, &parse_end_ptr, 10);
    if (parse_end_ptr != end_ptr || format_len <= 2) {
      carrow_status_set_error(status, EINVAL, "Expected format 'w:<width>' but got 'w'");
      RETURN_IF_NOT_OK(status);
    }
    return 0;

    // types with validity + offset + data or offset + data
  case 'z':
    array->type = CARROW_TYPE_BINARY;
    array->n_buffers = 3;
    array->offset_buffer_id = 1;
    array->data_buffer_id = 2;
    return 0;
  case 'u':
    array->type = CARROW_TYPE_STRING;
    array->n_buffers = 3;
    array->offset_buffer_id = 1;
    array->data_buffer_id = 2;
    return 0;

    // types with validity + large_offset + data or large_offset + data
  case 'Z':
    array->type = CARROW_TYPE_LARGE_BINARY;
    array->n_buffers = 3;
    array->large_offset_buffer_id = 1;
    array->data_buffer_id = 2;
    return 0;
  case 'U':
    array->type = CARROW_TYPE_LARGE_STRING;
    array->n_buffers = 3;
    array->large_offset_buffer_id = 1;
    array->data_buffer_id = 2;
    return 0;
  }

  // try to parse nested types
  if (format[0] == '+') {
    switch (format[1]) {

    // list has validity + offset or offset
    case 'l':
      array->type = CARROW_TYPE_LIST;
      array->n_buffers = 2;
      array->offset_buffer_id = 1;
      return 0;

      // large list has validity + large_offset or large_offset
    case 'L':
      array->type = CARROW_TYPE_LARGE_LIST;
      array->n_buffers = 2;
      array->large_offset_buffer_id = 1;
      return 0;

      // if these types have a buffer, it's a validity buffer
    case 'w':
      array->type = CARROW_TYPE_FIXED_SIZE_LIST;
      return 0;
    case 's':
      array->type = CARROW_TYPE_STRUCT;
      return 0;
    case 'm':
      array->type = CARROW_TYPE_MAP;
      return 0;

      // unions
    case 'u':
      switch (format[2]) {
      case 'd':
        array->type = CARROW_TYPE_DENSE_UNION;
        array->n_buffers = 3;
        array->union_type_buffer_id = 1;
        array->offset_buffer_id = 2;
        return 0;
      case 's':
        array->type = CARROW_TYPE_SPARSE_UNION;
        array->n_buffers = 2;
        array->union_type_buffer_id = 1;
        return 0;
      default:
        carrow_status_set_error(status, EINVAL, "Invalid union format string: '%s'", format);
      RETURN_IF_NOT_OK(status);
      }
    }
  }

  // date/time types
  // these are represented using the primitive types but the metadata is
  // embedded within the format string
  if (format[0] == 't') {
    switch (format[1]) {
    // date
    case 'd':
      switch (format[2]) {
      case 'D':
      case 'm':
      default:
        carrow_status_set_error(status, EINVAL, "Invalid date format string: '%s'", format);
        RETURN_IF_NOT_OK(status);
      }

      // time of day
    case 't':
      switch (format[2]) {
      case 's':
      case 'm':
      case 'u':
      case 'n':
      default:
        carrow_status_set_error(status, EINVAL, "Invalid time of day format string: '%s'", format);
        RETURN_IF_NOT_OK(status);
      }

      // timestamp
    case 's':
      switch (format[2]) {
      case 's':
      case 'm':
      case 'u':
      case 'n':
      default:
        carrow_status_set_error(status, EINVAL, "Invalid timestamp format string: '%s'", format);
        RETURN_IF_NOT_OK(status);
      }

      // duration
    case 'D':
      switch (format[2]) {
      case 's':
      case 'm':
      case 'u':
      case 'n':
      default:
        carrow_status_set_error(status, EINVAL, "Invalid duration format string: '%s'", format);
        RETURN_IF_NOT_OK(status);
      }

      // interval
    case 'i':
      switch (format[2]) {
      case 'M':
      case 'D':
      default:
        carrow_status_set_error(status, EINVAL, "Invalid interval format string: '%s'", format);
        RETURN_IF_NOT_OK(status);
      }

    default:
      carrow_status_set_error(status, EINVAL, "Invalid time format string: '%s'", format);
      RETURN_IF_NOT_OK(status);
    }
  }

  carrow_status_set_error(status, EINVAL, "Unknown format string: '%s'", format);
  return EINVAL;
}
