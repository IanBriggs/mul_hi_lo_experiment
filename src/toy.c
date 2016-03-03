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

#include "static_assert.h"

#ifdef USE_32_BIT
typedef float myfloat;
typedef uint32_t myuint;
typedef uint64_t myulong;
const char *RESULT_FORMAT_STRING="%f, %f, %f, %d, %u, %u, %u, %u\n";
const long MYUINT_MAX = UINT32_MAX;

#elif defined USE_64_BIT
typedef double myfloat;
typedef uint64_t myuint;
typedef __uint128_t myulong;
const char *RESULT_FORMAT_STRING="%f, %f, %f, %d, %lu, %lu, %lu, %lu\n";
const long MYUINT_MAX = UINT64_MAX;

#else
#error "Must define either USE_32_BIT or USE_64_BIT"
#endif


STATIC_ASSERT(sizeof(myfloat) == sizeof(myuint), 
	      "myfloat and myuint must have the same size");
STATIC_ASSERT(sizeof(myulong) == 2*sizeof(myuint), 
	      "myulong must be twice as big as myuint");


static size_t BITS_IN_MYFLOAT = 8*sizeof(myfloat);

/* Functions must tak in a double and return a double, they can be defined 
 * locally or from a library. Make sure to match NUM_FUNCTIONS and the
 * length of FUCNTIONS
 */
typedef double (*Class2Func)(double);
static const size_t NUM_FUNCTIONS = 2;
static Class2Func FUNCTIONS[] = {&sin, &cos};

typedef union _transmutor {
  myfloat fl;
  myuint  ui;
} transmutor;


myuint
transmute_fl_to_ui(const myfloat f)
{
  transmutor t; t.fl = f;
  return t.ui;
}


myfloat
transmute_ui_to_fl(const myuint u)
{
  transmutor t; t.ui = u;
  return t.fl;
}


myfloat *
gen_input(const myfloat low, const myfloat high, const size_t steps)
{
  assert(low < high);

  myfloat * output = malloc(steps*sizeof(myfloat));
  assert(output != NULL);

  myfloat difference = high-low;
  myfloat step_size = difference/steps;
  
  for (size_t i=0; i<steps; i++) {
    output[i] = low + (i*step_size);
  }

  return output;
}


myfloat *
map_func(const size_t func_choice, const size_t steps, const myfloat *input)
{
  assert(func_choice < NUM_FUNCTIONS);
  assert(input != NULL);

  myfloat *output = malloc(steps * sizeof(myfloat));
  assert(output != NULL);

  Class2Func func = FUNCTIONS[func_choice];
  for (size_t i=0; i<steps; i++) {
    output[i] = func(input[i]);
  }

  return output;
}


size_t
rand_size(size_t low, size_t high)
{
  assert(low <= high);

  size_t range = (high - low);
  return low + (rand() % (range+1));
}


myfloat *
insert_faults(const size_t steps, const myfloat *input,
	      const size_t fault_low_bit, const size_t fault_high_bit, 
	      const uint64_t fault_count, 
	      char **fault_locations_out)
{
  assert(input != NULL);
  assert(fault_low_bit <= fault_high_bit);
  assert(fault_locations_out != NULL);

  myfloat *output =  malloc(steps*sizeof(myfloat));
  char *fault_locations = malloc(steps*sizeof(char));
  assert(fault_locations != NULL);
  memset(fault_locations, -1, steps*sizeof(char));
  *fault_locations_out = fault_locations;

  for (size_t iter=0; iter<fault_count; iter++) {
    size_t target_entry = rand_size(0, steps-1);
    size_t target_bit = rand_size(fault_low_bit, fault_high_bit);
    assert(target_entry < steps);
    assert(target_bit < BITS_IN_MYFLOAT);

    myuint hex = transmute_fl_to_ui(input[target_entry]);
    hex ^= 1<<target_bit;
    output[target_entry] = transmute_ui_to_fl(hex);
    assert(transmute_fl_to_ui(input[target_entry]) != 
	   transmute_fl_to_ui(output[target_entry]));

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
mulhi_and_mullo(const size_t steps, const myfloat *input, const myuint m, 
		myuint **hi_out, myuint **lo_out)
{
  assert(input != NULL);
  assert(hi_out != NULL);
  assert(lo_out != NULL);
  
  myuint *hi = malloc(steps * sizeof(myuint));
  assert(hi != NULL);
  *hi_out = hi;
  myuint *lo = malloc(steps * sizeof(myuint));
  assert(lo != NULL);
  *lo_out = lo;

  for (size_t i=0; i<steps; i++) {
    myuint hex = transmute_fl_to_ui(input[i]);
    myulong res = (myulong) hex * m;
    hi[i] = (myuint) (res >> BITS_IN_MYFLOAT);
    lo[i] = (myuint) ((res << BITS_IN_MYFLOAT) >> BITS_IN_MYFLOAT);
  }
}


char * EXECNAME;
void
usage()
{
  printf("usage: %s --function <int:0-%ld>\n", EXECNAME, NUM_FUNCTIONS);
  printf("\t--lower-input <float> --higher-input <float>\n");
  printf("\t--steps <int:1-%ld>\n", SIZE_MAX);
  printf("\t--lower-bit <int:0-%ld> --higher-bit <int:0-%ld>\n", 
	 BITS_IN_MYFLOAT, BITS_IN_MYFLOAT);
  printf("\t--fault-count <int:0-%ld>\n", SIZE_MAX);
  printf("\t--m <int:0-%ld>\n", MYUINT_MAX);
  printf("\n");
}


unsigned long long
get_unsigned_long_long(const char *in)
{
  char * pt;
  unsigned long long out = strtoull(in, &pt, 0);
  if (errno == ERANGE ||
      errno == EINVAL ||
      in+strlen(in) != pt) {
    usage();
    exit(-1);
  }
  return out;
}


myfloat
get_myfloat(const char *in)
{
  char * pt;
  myfloat out = (myfloat) strtod(in, &pt);
  if (errno == ERANGE ||
      errno == EINVAL ||
      in+strlen(in) != pt) {
    usage();
    exit(-1);
  }
  return out; 
}


void
print_results(const size_t steps, const myfloat *input, 
	      const myfloat *x, const myfloat *xp, const char *fault_locations,
	      const myuint *y_hi, const myuint *y_lo,
	      const myuint *yp_hi, const myuint *yp_lo)
{
  assert(input != NULL);
  assert(x != NULL);
  assert(xp != NULL);
  assert(fault_locations != NULL);
  assert(y_hi != NULL);
  assert(y_lo != NULL);
  assert(yp_hi != NULL);
  assert(yp_lo != NULL);

  printf("input. x, x', sdc_tainted, y_hi, y'_hi, y_lo, y'_lo\n");
  for (size_t i=0; i<steps; i++) {
    printf(RESULT_FORMAT_STRING,
	   input[i],
	   x[i], xp[i], fault_locations[i],
	   y_hi[i], yp_hi[i],
	   y_lo[i], yp_lo[i]);
  }
}




int
main(int argc, char **argv) 
{
  EXECNAME = argv[0];
  int c;
  int used_args = 0;
  size_t func_choice, fault_low_bit, fault_high_bit;
  myfloat low, high;
  myuint steps, fault_count, m;
  long temp;
  
  while (1) {
    static struct option long_options[] =
      {
	{"function", required_argument, NULL, 'f'},
	{"lower-input", required_argument, NULL, 'l'},
	{"higher-input", required_argument, NULL, 'h'},
	{"steps", required_argument, NULL, 's'},
	{"lower-bit", required_argument, NULL, 'd'},
	{"higher-bit", required_argument, NULL, 'a'},
	{"fault-count", required_argument, NULL, 'c'},
	{"m", required_argument, NULL, 'm'},
	{0, 0, 0, 0}
      };

    int option_index = 0;

    c = getopt_long (argc, argv, "f:h:l:s:d:a:c:m:", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
      {
      case 0:
	usage();
	exit(-1);
	
      case 'f':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp < 0 || temp > NUM_FUNCTIONS) {
	  printf("argument function must be between 0 and %ld\ngiven %ld\n", 
		 NUM_FUNCTIONS, temp);
	  exit(-1);
	}
	func_choice = (size_t) temp;
	break;

      case 'l':
	used_args++;
	low = get_myfloat(optarg);
	break;

      case 'h':
	used_args++;
	high = get_myfloat(optarg);
	break;

      case 's':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp <= 0) {
	  printf("argument steps must be greater than 0\ngiven %ld\n", 
		 temp);
	  exit(-1);
	}
	steps = (myuint) temp;
	break;

      case 'd':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp < 0 || temp > BITS_IN_MYFLOAT-1) {
	  printf("argument lower-bit must be between 0 and %ld\ngiven %ld\n", 
		 BITS_IN_MYFLOAT-1, temp);
	  exit(-1);
	}
	fault_low_bit = (size_t) temp;
	break;

      case 'a':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp < 0 || temp > BITS_IN_MYFLOAT-1) {
	  printf("argument higher-bit must be between 0 and %ld\ngiven %ld\n", 
		 BITS_IN_MYFLOAT-1, temp);
	  exit(-1);
	}
	fault_high_bit = (size_t) temp;
	break;

      case 'c':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp <= 0) {
	  printf("argument fault-count must be greater than 0\ngiven %ld\n", 
		 temp);
	  exit(-1);
	}
	fault_count = (myuint) temp;
	break;

      case 'm':
	used_args++;
	temp = get_unsigned_long_long(optarg);
	if (temp <= 0) {
	  printf("argument fault-count must be greater than 0\ngiven %ld\n", 
		 temp);
	  exit(-1);
	}
	m = (myuint) temp;
	break;
      }
  }

  if (used_args < 7) {
    usage();
    exit(-1);
  }
  
  if (fault_low_bit > fault_high_bit) {
    printf("higher-bit must be larger, or equal to, lower-bit\n");
    exit(-1);
  }

  if (low > high) {
    printf("higher-input must be larger, or equal to, lower-input\n");
    exit(-1);
  }

  myfloat *input = gen_input(low, high, steps);
  myfloat *x = map_func(func_choice, steps, input);

  char *fault_locations;
  myfloat *xp = insert_faults(steps, x, 
			      fault_low_bit, fault_high_bit, 
			      fault_count, &fault_locations);

  myuint *y_hi, *y_lo, *yp_hi, *yp_lo;
  mulhi_and_mullo(steps, x, m, &y_hi, &y_lo);
  mulhi_and_mullo(steps, xp, m, &yp_hi, &yp_lo);
    
  print_results(steps, input, x, xp, fault_locations, y_hi, y_lo, yp_hi, yp_lo);
  
  return 0;
}
