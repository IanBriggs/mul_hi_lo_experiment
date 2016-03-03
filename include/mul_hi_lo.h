#ifndef MUL_HI_LO_H
#define MUL_HI_LO_H

#include <stdint.h>
#include <assert.h>
#include <string.h>

/** 
 * transmute: Reinterprest the bits of a float as an int32_t
 *
 * Ensures: - no crash can occur
 *          - output is assigned as described
 *
 */
int32_t
transmute(const float x)
{
  int32_t out;
  memcpy(&out, &x, 4);
  return out;
}

/** 
 * untransmute: Reinterprest the bits of an int32_t as a float
 *
 * Ensures: - no crash can occur
 *          - output is assigned as described
 *
 */
float
untransmute(const int32_t x)
{
  float out;
  memcpy(&out, &x, 4);
  return out;
}


/**
 * split_float: Given an input multiplies the int memory reinterperetation
 *     of that float by the int 'm'. The high bits of the output are stored in
 *     *out_hi and the low bits in *out_lo
 *
 * Requires: - out_hi is a valid *int32_t
 *           - out_lo is a valid *int32_t
 *
 * Ensures: - no crash can occur
 *          - inout variables are assigned as described
 *
 * Notes: - not thread safe
 *        - if requirements are not met then ensures are not garanteed
 *        - will halt on violation of checkable requirements
 *
 */
void
split_float(const float x, const int32_t m, 
	    int32_t *hi_out, int32_t *lo_out)
{
  assert(hi_out != NULL);
  assert(lo_out != NULL);
  
  int32_t x_int = transmute(x);
  int64_t y = (int64_t) x_int * m;
  *hi_out = y >> 32;
  *lo_out = ((y << 32) >> 32);
}


/**
 * split_array: Given an input array multiplies the int memory reinterperetation
 *     of that array by the int 'm'. The high bits of the output are stored in
 *     *out_hi and the low bits in *out_lo
 *
 * Requires: - in_size is the valid length of the in_array
 *           - in_array is a valid float array
 *           - out_hi is a valid **int32_t
 *           - *out_hi is a valid array of type int32_t and length in_size
 *           - out_lo is a valid **int32_t
 *           - *out_lo is a valid array of type int32_t and length in_size
 *
 * Ensures: - no crash can occur
 *          - inout variables are assigned as described
 *
 * Notes: - not thread safe
 *        - if requirements are not met then ensures are not garanteed
 *        - will halt on violation of checkable requirements
 *
 */
void
split_array(const size_t in_size, const float *in_array, const int32_t m, 
	    int32_t **out_hi, int32_t **out_lo)
{
  assert(in_array != NULL);
  assert(out_hi != NULL);
  assert(out_lo != NULL);
  assert(*out_hi != NULL);
  assert(*out_lo != NULL);

  int32_t *hi = *out_hi;
  int32_t *lo = *out_lo;

  for (size_t index=0; index < in_size; index++) {
    split_float(in_array[index], m, &(hi[index]), &(lo[index]));
  } 
}


/**
 * split_2d_subgrid: TODO
 *
 * Requirements: - TODO
 *
 * Ensures: - no crash can occur
 *          - inout variables are assigned as described
 *
 * Notes: - not thread safe
 *        - if requirements are not met then ensures are not garanteed
 *        - will halt on violation of checkable requirements
 *
 */
void
split_2d_subgrid(const size_t in_x, const size_t in_y, const float **in_array, 
		 const int32_t m,
		 const size_t sub_x_start, const size_t sub_x_end,
		 const size_t sub_y_start, const size_t sub_y_end,
		 int32_t ***out_hi, int32_t ***out_lo)
{
  assert(in_array != NULL);
  assert(out_hi != NULL);
  assert(out_lo != NULL);
  assert(sub_x_start < sub_x_end);
  assert(sub_y_start < sub_y_end);
  assert(sub_x_end <= in_x);
  assert(sub_y_end <= in_y);

  int32_t **hi = *out_hi;
  int32_t **lo = *out_lo;

  size_t y_width = sub_y_end - sub_y_start;
  for (size_t index=sub_x_start; index < sub_x_end; index++) {
    const float *sub_array = &(in_array[index][sub_y_start]);
    int32_t *sub_hi = &(hi[index][sub_y_start]);
    int32_t *sub_lo = &(lo[index][sub_y_start]);
    split_array(y_width, sub_array, m, &sub_hi, &sub_lo);
  }
}

/**
 * split_2d_array: Given an input array of pointers to arrays multiplies the 
 *     int memory reinterperetation of those arrays by the int 'm'. The high 
 *     bits of the output are stored in **out_hi and the low bits in **out_lo
 *
 * Requires: - TODO
 *
 * Ensures: - no crash can occur
 *          - inout variables are assigned as described
 *
 * Notes: - not thread safe
 *        - if requirements are not met then ensures are not garanteed
 *        - will halt on violation of checkable requirements
 *
 */
void
split_2d_array(const size_t in_x, const size_t in_y, const float **in_array, 
	       const int32_t m,
	       int32_t ***out_hi, int32_t ***out_lo)
{
  assert(in_array != NULL);
  assert(out_hi != NULL);
  assert(out_lo != NULL);

  split_2d_subgrid(in_x, in_y, in_array, m, 0, in_x, 0, in_y, out_hi, out_lo);
}



#endif
