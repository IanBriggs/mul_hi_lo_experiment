#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#include "mul_hi_lo.h"

static const int BITS_IN_FLOAT=32;

/********************************************************************************
 * INPUT GENERATION: input values and function filled 2d arrays                 *
 *******************************************************************************/

/* Functions must take in a double and return a double, they can be defined 
 * locally or from a library. Make sure to match NUM_FUNCTIONS and the
 * length of FUNCTIONS
 */
typedef double (*Class2Func)(double);
static const size_t NUM_FUNCTIONS = 2;
static Class2Func FUNCTIONS[] = {&sin, &cos};

float *
gen_input(const float low, const float high, const size_t steps)
{
  assert(low < high);

  float * output = malloc(steps*sizeof(float));
  assert(output != NULL);

  float difference = high-low;
  float step_size = difference/steps;
  
  for (size_t i=0; i<steps; i++) {
    output[i] = low + (i*step_size);
  }

  return output;
}


float **
gen_2d_input(const float low, const float high, const size_t x, const size_t y)
{
  float ** output = malloc(x*sizeof(float*));
  assert(output != NULL);

  float difference = high-low;
  float step_size = difference/x;

  for (size_t index=0; index<x; index++) {
    output[index] = gen_input(low-(step_size*index), high-(step_size*index), y);
  }

  return output;
}


float *
map_func(const size_t func_choice, const size_t steps, const float *input)
{
  assert(func_choice < NUM_FUNCTIONS);
  assert(input != NULL);

  float *output = malloc(steps * sizeof(float));
  assert(output != NULL);

  Class2Func func = FUNCTIONS[func_choice];
  for (size_t i=0; i<steps; i++) {
    output[i] = func(input[i]);
  }

  return output;
}


float**
map_2d_func(const size_t func_choice, const size_t x, const size_t y, 
	    const float **input)
{
  assert(input != NULL);

  float **output = malloc(x*sizeof(float*));
  assert(output != NULL);

  for (size_t index=0; index<x; index++) {
    output[index] = map_func(func_choice, y, input[index]);
  }

  return output;
}












/********************************************************************************
 * DATA CORRUPTION                                                              *
 *******************************************************************************/

size_t
rand_size(size_t low, size_t high)
{
  assert(low <= high);

  size_t range = (high - low);
  return low + (rand() % (range+1));
}


float *
insert_faults(const size_t steps, const float *input,
	      const size_t fault_low_bit, const size_t fault_high_bit, 
	      const uint64_t fault_count, 
	      int32_t **fault_locations_out)
{
  assert(input != NULL);
  assert(fault_low_bit <= fault_high_bit);
  assert(fault_locations_out != NULL);
  assert(fault_count <= steps);

  float *output =  malloc(steps*sizeof(float));
  int32_t *fault_locations = malloc(steps*sizeof(int32_t));
  assert(fault_locations != NULL);
  memset(fault_locations, -1, steps*sizeof(char));
  *fault_locations_out = fault_locations;


  for (size_t tries=0; tries<fault_count; tries++) {
    size_t target_entry = rand_size(0, steps-1);
    if (fault_locations[target_entry] != -1) {
      continue;
    }
    size_t target_bit = rand_size(fault_low_bit, fault_high_bit);
    assert(target_entry < steps);
    assert(target_bit < BITS_IN_FLOAT);

    int32_t hex = transmute(input[target_entry]);
    hex ^= (uint32_t) 1<<target_bit;
    output[target_entry] = untransmute(hex);

    assert(transmute(input[target_entry]) != 
	   transmute(output[target_entry]));

    fault_locations[target_entry] = target_bit;
  }

  for (size_t index=0; index<steps; index++) {
    if (fault_locations[index] == -1) {
      output[index] = input[index];
    }
  }

  return output;
}


void
insert_2d_faults(const size_t A, float **input, size_t H,
		 const size_t fault_low_bit, const size_t fault_high_bit, 
		 const uint64_t fault_count, const size_t x, const size_t y)
{
  size_t grid_width = A/H;
  for (size_t tries=0; tries < fault_count; tries++) {
    size_t xi = rand_size(grid_width*x, grid_width*(x+1)-1);
    size_t yi = rand_size(grid_width*y, grid_width*(y+1)-1);
    size_t target_bit = rand_size(fault_low_bit, fault_high_bit);
    
    int32_t hex = transmute(input[xi][yi]);
    hex ^= (uint32_t) 1<<target_bit;
    input[xi][yi] = untransmute(hex);
  }
}


void
insert_full_faults(const size_t A, float **input, size_t H,
		   const size_t fault_low_bit, const size_t fault_high_bit, 
		   const uint64_t fault_count)
{
  size_t flts = fault_count / (H*H);
  for (size_t x=0; x<H; x++) {
    for (size_t y=0; y<H; y++) {
      insert_2d_faults(A, input, H, fault_low_bit, fault_high_bit, flts, x, y);
    }
  }
}


/********************************************************************************
 * L1 NORM CALCULATION                                                          *
 *******************************************************************************/

static const unsigned int L = 3;

float
calc_norm(const size_t A, const float **full_array, const size_t grids, 
	  const size_t x, const size_t y)
{
  assert(full_array != NULL);

  size_t grid_width = (A/grids);
  size_t x_start = grid_width*x;
  size_t y_start = grid_width*y;

  float sum = 0;
  for (size_t ix=x_start; ix < x_start+grid_width; ix++) {
    for (size_t iy=y_start; iy < y_start+grid_width; iy++) {
      sum += full_array[ix][iy];
    }
  }

  return sum;
}

float**
calc_2d_norm(const size_t A, const float **full_array, const size_t grids)
{
  assert(full_array != NULL);

  float **output = malloc(grids*sizeof(float*));
  assert(output != NULL);

  for (size_t ix=0; ix < grids; ix++) {
    output[ix] = malloc(grids*sizeof(float));
    assert(output != NULL);

    for (size_t iy=0; iy < grids; iy++) {  
      output[ix][iy] = calc_norm(A, full_array, grids, ix, iy);
    }
  }

  return output;
}






/********************************************************************************
 * FEATURE VECTOR CREATION                                                      *
 *******************************************************************************/
char *original_file;
char *high_file;
char *low_file;
void
print_features(int example_type, size_t grids, const float **norms, size_t H, int32_t m)
{
  assert(example_type == 1 || example_type == -1);


  int32_t **y_hi = malloc(grids * sizeof(int32_t*)); assert(y_hi!=NULL);
  int32_t **y_lo = malloc(grids * sizeof(int32_t*)); assert(y_lo!=NULL);
  for (size_t x=0; x<grids; x++) {
    for (size_t y=0; y<grids; y++) {
      y_hi[x] = malloc(grids * sizeof(int32_t)); assert(y_hi[x]!=NULL);
      y_lo[x] = malloc(grids * sizeof(int32_t)); assert(y_lo[x]!=NULL);
    }
  }
  split_2d_array(grids, grids, norms, m,  &y_hi, &y_lo);

  FILE *original_fp = fopen(original_file, "a");
  FILE *high_fp = fopen(high_file, "a");
  FILE *low_fp = fopen(low_file, "a");

  for (size_t x=0; x<H; x++) {
    for (size_t y=0; y<H; y++) {
      fprintf(original_fp, "%s1 ", (example_type==1) ? "+" : "-");
      int i=1;
      for (size_t subx=x*L; subx<(x+1)*L; subx++) {
	for (size_t suby=y*L; suby<(y+1)*L; suby++) {
	  fprintf(original_fp, "%d:%f ", i++, norms[subx][suby]);
	}
      }
      fprintf(original_fp, "\n");

      fprintf(high_fp, "%s1 ", (example_type==1) ? "+" : "-");
      i=1;
      for (size_t subx=x*L; subx<(x+1)*L; subx++) {
	for (size_t suby=y*L; suby<(y+1)*L; suby++) {
	  fprintf(high_fp, "%d:%d ", i++, y_hi[subx][suby]);
	}
      }
      fprintf(high_fp, "\n");

      fprintf(low_fp, "%s1 ", (example_type==1) ? "+" : "-");
      i=1;
      for (size_t subx=x*L; subx<(x+1)*L; subx++) {
	for (size_t suby=y*L; suby<(y+1)*L; suby++) {
	  fprintf(low_fp, "%d:%d ", i++, y_lo[subx][suby]);
	}
      }
      fprintf(low_fp, "\n");
    }
  }
  fclose(original_fp);
  fclose(high_fp);
  fclose(low_fp);
}



/********************************************************************************
 * ARGUMENT PARSING                                                             *
 *******************************************************************************/


unsigned long long
get_unsigned_long_long(const char *in)
{
  char * pt;
  unsigned long long out = strtoull(in, &pt, 0);
  if (errno == ERANGE ||
      errno == EINVAL ||
      in+strlen(in) != pt) {
    assert(0);
  }
  return out;
}


float
get_float(const char *in)
{
  char * pt;
  float out = (float) strtod(in, &pt);
  if (errno == ERANGE ||
      errno == EINVAL ||
      in+strlen(in) != pt) {
    assert(0);
  }
  return out; 
}


void
write_2d_int_array(const char *filename, const size_t x, const size_t y, 
		   const int32_t **input)
{
  assert(filename != NULL);
  assert(input != NULL);

  FILE *fp = fopen(filename, "w");
  assert(fp != NULL);

  for (size_t x_index=0; x_index < x; x_index++) {
    for (size_t y_index=0; y_index < y; y_index++) {
      fprintf(fp, "%d, ", input[x_index][y_index]);
    }
    fprintf(fp,"\n");
  }

  fclose(fp);
}


void
write_2d_float_array(const char *filename, const size_t x, const size_t y, 
		     const float **input)
{
  assert(filename != NULL);
  assert(input != NULL);

  FILE *fp = fopen(filename, "w");
  assert(fp != NULL);

  for (size_t x_index=0; x_index < x; x_index++) {
    for (size_t y_index=0; y_index < y; y_index++) {
      fprintf(fp, "%f, ", input[x][y]);
    }
    fprintf(fp,"\n");
  }

  fclose(fp);
}



int
main(int argc, char **argv) 
{
  srand(time(NULL));
  assert(argc > 2);
  char *mode = argv[1];

  if (strcmp(mode, "train") == 0) {
    assert(argc == 14);
    int i = 2;
    size_t func_choice = get_unsigned_long_long(argv[i++]);
    assert(func_choice < NUM_FUNCTIONS);

    float low = get_float(argv[i++]);
    float high = get_float(argv[i++]);
    assert(low < high);

    size_t H = get_unsigned_long_long(argv[i++]);
    size_t grids = H*L;
    assert(H%L == 0);

    size_t A = get_unsigned_long_long(argv[i++]);
    assert(A%grids == 0);

    size_t fault_low_bit = get_unsigned_long_long(argv[i++]);
    size_t fault_high_bit = get_unsigned_long_long(argv[i++]);
    assert(fault_low_bit <= fault_high_bit);

    size_t fault_count = get_unsigned_long_long(argv[i++]);

    int32_t m = get_unsigned_long_long(argv[i++]);

    original_file = argv[i++];
    low_file = argv[i++];
    high_file = argv[i++];

    const float **input = (const float**) gen_2d_input(low, high, A, A);


    // Clean data
    const float **x = (const float**) map_2d_func(func_choice, A, A, input);    

    const float **norms = (const float**) calc_2d_norm(A, x, grids);

    print_features(1, grids, norms, H, m);

    //Corrupted
    float **corrupt_x = map_2d_func(func_choice, A, A, input);    
    insert_full_faults(A, corrupt_x, H, fault_low_bit, fault_high_bit, 
		       fault_count);
    const float **corrupt_norms = (const float**) calc_2d_norm(A, (const float**)corrupt_x, grids);
    print_features(-1, grids, corrupt_norms, H, m);

    return 0;
    

  } else if (strcmp(mode, "OTHER_MODE") == 0) {
    assert(0);
  }

  return 0;
}
