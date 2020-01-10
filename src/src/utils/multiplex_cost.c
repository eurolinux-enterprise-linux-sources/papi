/** @file multiplex.c
  * @brief papi_multiplex_cost utility.
  *	@page papi_multiplex_cost
  * @section  NAME
  *		papi_multiplex_cost - computes execution time costs for basic PAPI operations on multiplexed EventSets. 
  *
  *	@section Synopsis
  *		papi_cost [-m, --min <min>] [-x, --max <max>] [-k,-s]
  *
  *	@section Description
  *		papi_multiplex_cost is a PAPI utility program that computes the 
  *		min / max / mean / std. deviation of execution times for PAPI start/stop 
  *		pairs and for PAPI reads on multiplexed eventsets.
  *		This information provides the basic operating cost to a user's program 
  *		for collecting hardware counter data. 
  *		Command line options control display capabilities.
  *
  *	@section Options
  *	<ul>
  *		<li>-m <Min number of events to test>
  *		<li>-x <Max number of events to test>
  *		<li> -k, Do not time kernel multiplexing 
  *		<li> -s, Do not ime software multiplexed EventSets 
  *		<li> -t THREASHOLD, Test with THRESHOLD iterations of counting loop. 
  *	</ul>
  *
  *	@section Bugs
  *		There are no known bugs in this utility. If you find a bug, 
  *		it should be reported to the PAPI Mailing List at <ptools-perfapi@ptools.org>. 
 */


/* Open Issues:
 *		Selecting events to add is very primitive right now.
 *		Output format, right now the format targets a gnuplot script I have, 
 *			We will probably end up generating a csv per test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "papi_test.h"
#include "cost_utils.h"

static int first_time = 1;
static int skip = 0;
static FILE* fp;

typedef struct {
  int first_time;
  int force_sw;
  int kernel_mpx;
  int min;
  int max;
} options_t;

static options_t options;

void
do_output( char *fn, char *message, long long* array, int noc )
{
  long long min, max;
  double average, std;

  std = do_stats( array, &min, &max, &average );

  if ( first_time ) {
	skip = 0;

	fp = fopen(fn, "w");
	if (fp == NULL) {
	  fprintf(stderr,"Unable to open output file, %s, output will not be saved.\n", fn);
	  skip = 1;
	} else 
	  fprintf(fp, "###%s\n#number of events\tmin cycles\tmax cycles\tmean cycles\t\
std deviation\tsw min cycles\tsw max cycles\tsw avg cycles\tsw std dev\n", message);

	first_time = 0;
  }

  if ( !skip ) {
	fprintf(fp, "%20d\t%10lld\t%10lld\t%10lf\t%10lf", noc, min, max, average, std);

	std = do_stats( array+num_iters, &min, &max, &average );
	fprintf(fp, "\t%10lld\t%10lld\t%10lf\t%10lf\n", min, max, average, std);
	fflush(fp);
  }
}

void 
init_test(int SoftwareMPX, int KernelMPX, int* Events)
{
  int i;
  int retval;
  PAPI_option_t option;
  const  PAPI_component_info_t *info;

  if ( ( retval = PAPI_assign_eventset_component( SoftwareMPX, 0 ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval);

  if ( ( retval = PAPI_assign_eventset_component( KernelMPX, 0 ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval);

  if ( ( retval = PAPI_set_multiplex( KernelMPX ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );

  info = PAPI_get_component_info(0);
  memset(&option,0x0,sizeof(option));

  option.multiplex.flags = PAPI_MULTIPLEX_FORCE_SW;
  option.multiplex.eventset = SoftwareMPX;
  option.multiplex.ns = info->itimer_ns;

  PAPI_set_opt( PAPI_MULTIPLEX, &option );

  for (i = 0; i < options.min - 1; i++) {
	if ( options.kernel_mpx ) {
	  if ( ( retval = PAPI_add_event( KernelMPX, Events[i]) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval );
	  }
	}

	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[i]) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval );
	  }
	}
  }
}

void
finalize_test(void)
{
  if (fp)
	fclose(fp);
  first_time = 1;
}

static void 
usage(void)
{
  printf( "Usage: papi_multiplex_cost [options]\n\
\t-m num, number of events to count\n\
\t-x num, number of events to count\n\
\t-s, Do not run software multiplexing test.\n\
\t-k, Do not attempt kernel multiplexed test.\n\
\t-t THREASHOLD set the threshold for the number of iterations. Default: 100,000\n" );
}

int
main( int argc, char **argv )
{
  int retval;
  int KernelMPX = PAPI_NULL;
  int SoftwareMPX = PAPI_NULL;
  int *Events = NULL;
  int number_of_counters;
  int i;
  int c;
  int dont_loop_forever;
  long long totcyc, *values = NULL;
  long long *array = NULL;
  int event;

  PAPI_option_t option;
  const  PAPI_component_info_t *info;

  tests_quiet( argc, argv );

  PAPI_set_debug(PAPI_QUIET);
  options.min = 1;
  options.max = 10;
  options.force_sw = 1;
  options.kernel_mpx = 1;

  while ( ( c=getopt(argc, argv, "hm:x:skt:") ) != -1 ) {
	switch (c) {
	  case 'h':
		usage();
		exit(0);
	  case 'm':
		options.min = atoi(optarg);
		break;
	  case 'x':
		options.max = atoi(optarg);
		break;
	  case 's':
		options.force_sw = 0;
		break;
	  case 'k':
		options.kernel_mpx = 0;
		break;
	  case 't':
		num_iters = atoi(optarg);
	  default:
		break;
	}
  }

  if ( options.min > options.max ) {
	test_fail( __FILE__, __LINE__, "Min # of Events > Max # of Events", -1);
	goto cleanup;
  }

  values = (long long*)malloc(sizeof(long long) * options.max);
  array = (long long *)malloc(sizeof(long long) * 2 * num_iters);
  Events = ( int* )malloc(sizeof(int) * options.max);

  if ( ( retval =
		PAPI_library_init( PAPI_VER_CURRENT ) ) != PAPI_VER_CURRENT )
	test_fail( __FILE__, __LINE__, "PAPI_library_init", retval );
  if ( ( retval = PAPI_set_debug( PAPI_QUIET ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_set_debug", retval );

  if ( ( retval = PAPI_multiplex_init( ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_multiplex_init", retval );


  info = PAPI_get_component_info(0);
  options.kernel_mpx &= info->kernel_multiplex;

  if ( options.kernel_mpx && !info->kernel_multiplex ) {
	test_fail( __FILE__, __LINE__, "Kernel multiplexing is not supported on this platform, bailing!\n", PAPI_EINVAL );
	exit(1);
  }


  if ( ( retval = PAPI_create_eventset( &SoftwareMPX ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

  if ( ( retval = PAPI_create_eventset( &KernelMPX ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

  if ( ( retval = PAPI_assign_eventset_component( KernelMPX, 0 ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval);

  if ( ( retval = PAPI_set_multiplex( KernelMPX ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );

  if ( ( retval = PAPI_assign_eventset_component( SoftwareMPX, 0 ) ) != PAPI_OK )
	test_fail( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval);

  memset(&option,0x0,sizeof(option));

  option.multiplex.flags = PAPI_MULTIPLEX_FORCE_SW;
  option.multiplex.eventset = SoftwareMPX;
  option.multiplex.ns = info->itimer_ns;

  PAPI_set_opt( PAPI_MULTIPLEX, &option );

  if ( !options.kernel_mpx && !options.force_sw ) {
	test_fail(__FILE__, __LINE__, "No tests to run.", -1);
	goto cleanup;
  } else {
	fprintf(stderr,"Running test[s]\n");
	if (options.kernel_mpx)
	  fprintf(stderr,"\tKernel multiplexing read\n");
	if (options.force_sw)
	  fprintf(stderr,"\tSoftware Multiplexing read\n");
  }

  event = 0 | PAPI_NATIVE_MASK;
  PAPI_enum_event( &event, PAPI_ENUM_FIRST );

  /* Find some events to run the tests with. */
  for (number_of_counters = 0; number_of_counters < options.max; number_of_counters++) {
	dont_loop_forever = 0;
	
	if ( options.kernel_mpx ) {
	  do {
		PAPI_enum_event( &event, PAPI_ENUM_EVENTS );
		dont_loop_forever++;
	  } while ( ( retval = PAPI_add_event( KernelMPX, event ) ) != PAPI_OK &&
		  dont_loop_forever < 512);
	} else {
	  do {
		PAPI_enum_event( &event, PAPI_ENUM_EVENTS );
		dont_loop_forever++;
	  } while ( ( retval = PAPI_add_event( SoftwareMPX, event) ) != PAPI_OK &&
		  dont_loop_forever < 512);
	}
	if ( dont_loop_forever == 512 )
	  test_fail( __FILE__, __LINE__, "I can't find %d events to count at once.", options.max); 

	Events[number_of_counters] = event;
  }

  PAPI_cleanup_eventset( KernelMPX );
  PAPI_cleanup_eventset( SoftwareMPX );

  /* Start/Stop test */
  init_test(SoftwareMPX, KernelMPX, Events);

  for (number_of_counters = options.min; number_of_counters < options.max; number_of_counters++) {

	if ( options.kernel_mpx ) {
	  if ( ( retval = PAPI_add_event( KernelMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( KernelMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  if ( ( retval = PAPI_stop( KernelMPX, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval );

	  /* KernelMPX Timing loop */
	  for ( i = 0; i < num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		PAPI_start( KernelMPX );
		PAPI_stop( KernelMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 1 timing run */

	} else
	  memset(array, 0, sizeof(long long) * num_iters );

	/* Also test software multiplexing */
	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( SoftwareMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  if ( ( retval = PAPI_stop( SoftwareMPX, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval);

	  /* SoftwareMPX Timing Loop */
	  for ( i = num_iters; i < 2*num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		PAPI_start( SoftwareMPX );
		PAPI_stop( SoftwareMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 2 timing run */

	} else 
	  memset(array+num_iters, 0, sizeof(long long) * num_iters );

	do_output( "papi_startstop.dat", "Multiplexed PAPI_read()", array, number_of_counters );

  } /* End counter loop */
  PAPI_cleanup_eventset( SoftwareMPX );
  PAPI_cleanup_eventset( KernelMPX );
  finalize_test();

  /* PAPI_read() test */
  init_test(SoftwareMPX, KernelMPX, Events);

  for (number_of_counters = options.min; number_of_counters < options.max; number_of_counters++) {

	if ( options.kernel_mpx ) {
	  if ( ( retval = PAPI_add_event( KernelMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( KernelMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( KernelMPX, values );

	  /* KernelMPX Timing loop */
	  for ( i = 0; i < num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_read( KernelMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 1 timing run */

	  PAPI_stop( KernelMPX, values );
	} else
	  memset(array, 0, sizeof(long long) * num_iters );

	/* Also test software multiplexing */
	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( SoftwareMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( SoftwareMPX, values );

	  /* SoftwareMPX Timing Loop */
	  for ( i = num_iters; i < 2*num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_read( SoftwareMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 2 timing run */

	  PAPI_stop( SoftwareMPX, values );
	} else 
	  memset(array+num_iters, 0, sizeof(long long) * num_iters );

	do_output( "papi_read.dat", "Multiplexed PAPI_read()", array, number_of_counters );

  } /* End counter loop */
  PAPI_cleanup_eventset( SoftwareMPX );
  PAPI_cleanup_eventset( KernelMPX );
  finalize_test();



  /* PAPI_read_ts() test */
  init_test( SoftwareMPX, KernelMPX, Events);

  for (number_of_counters = options.min; number_of_counters < options.max; number_of_counters++) {

	if ( options.kernel_mpx ) {
	  if ( (retval = PAPI_add_event( KernelMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( KernelMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read_ts( KernelMPX, values, &totcyc );

	  /* KernelMPX Timing loop */
	  for ( i = 0; i < num_iters; i++ ) {
		retval = PAPI_read_ts( KernelMPX, values, &array[i] ); 
	  } /* End 1 timing run */

	  /* post-process the timing array */
	  for ( i = num_iters - 1; i > 0; i-- ) {
		array[i] -= array[i - 1];
	  }
	  array[0] -= totcyc;

	  PAPI_stop( KernelMPX, values );
	} else
	  memset(array, 0, sizeof(long long) * num_iters );

	/* Also test software multiplexing */
	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( SoftwareMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read_ts( SoftwareMPX, values, &totcyc);

	  /* SoftwareMPX Timing Loop */
	  for ( i = num_iters; i < 2*num_iters; i++ ) {
		retval = PAPI_read_ts( SoftwareMPX, values, &array[i]); 
	  } /* End 2 timing run */

	  PAPI_stop( SoftwareMPX, values );

	  /* post-process the timing array */
	  for ( i = 2*num_iters - 1; i > num_iters; i-- ) {
		array[i] -= array[i - 1];
	  }
	  array[num_iters] -= totcyc;

	} else 
	  memset(array+num_iters, 0, sizeof(long long) * num_iters );

	do_output( "papi_read_ts.dat", "Multiplexed PAPI_read_ts()", array, number_of_counters );

  } /* End counter loop */
  PAPI_cleanup_eventset( SoftwareMPX );
  PAPI_cleanup_eventset( KernelMPX );
  finalize_test();


  /* PAPI_accum() test */
  init_test(SoftwareMPX, KernelMPX, Events);

  for (number_of_counters = options.min; number_of_counters < options.max; number_of_counters++) {

	if ( options.kernel_mpx ) {
	  if ( ( retval = PAPI_add_event( KernelMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( KernelMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( KernelMPX, values );

	  /* KernelMPX Timing loop */
	  for ( i = 0; i < num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_accum( KernelMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 1 timing run */

	  PAPI_stop( KernelMPX, values );
	} else
	  memset(array, 0, sizeof(long long) * num_iters );

	/* Also test software multiplexing */
	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( SoftwareMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( SoftwareMPX, values );

	  /* SoftwareMPX Timing Loop */
	  for ( i = num_iters; i < 2*num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_accum( SoftwareMPX, values ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 2 timing run */

	  PAPI_stop( SoftwareMPX, values );
	} else 
	  memset(array+num_iters, 0, sizeof(long long) * num_iters );

	do_output( "papi_accum.dat", "Multiplexed PAPI_accum()", array, number_of_counters );

  } /* End counter loop */
  PAPI_cleanup_eventset( SoftwareMPX );
  PAPI_cleanup_eventset( KernelMPX );
  finalize_test();

  /* PAPI_reset() test */
  init_test(SoftwareMPX, KernelMPX, Events);

  for (number_of_counters = options.min; number_of_counters < options.max; number_of_counters++) {

	if ( options.kernel_mpx ) {
	  if ( ( retval = PAPI_add_event( KernelMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( KernelMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( KernelMPX, values );

	  /* KernelMPX Timing loop */
	  for ( i = 0; i < num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_reset( KernelMPX ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 1 timing run */

	  PAPI_stop( KernelMPX, values );
	} else
	  memset(array, 0, sizeof(long long) * num_iters );

	/* Also test software multiplexing */
	if ( options.force_sw ) {
	  if ( ( retval = PAPI_add_event( SoftwareMPX, Events[number_of_counters - options.min] ) ) != PAPI_OK ) {
		test_fail( __FILE__, __LINE__, "PAPI_add_event", retval);
		goto cleanup;
	  }

	  if ( ( retval = PAPI_start( SoftwareMPX ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );
	  PAPI_read( SoftwareMPX, values );

	  /* SoftwareMPX Timing Loop */
	  for ( i = num_iters; i < 2*num_iters; i++ ) {
		totcyc = PAPI_get_real_cyc();
		retval = PAPI_reset( SoftwareMPX ); 
		array[i] = PAPI_get_real_cyc() - totcyc; 
	  } /* End 2 timing run */

	  PAPI_stop( SoftwareMPX, values );
	} else 
	  memset(array+num_iters, 0, sizeof(long long) * num_iters );

	do_output( "papi_reset.dat", "Multiplexed PAPI_reset()", array, number_of_counters );

  } /* End counter loop */
  PAPI_cleanup_eventset( SoftwareMPX );
  PAPI_cleanup_eventset( KernelMPX );
  finalize_test();


  test_pass( __FILE__, NULL, 0 );
cleanup:
  if ( KernelMPX != PAPI_NULL)
	PAPI_cleanup_eventset( KernelMPX );
  if ( SoftwareMPX != PAPI_NULL )
	PAPI_cleanup_eventset( KernelMPX );

  if ( values != NULL )
	free(values);
  if ( array != NULL )
	free(array);
  if ( Events != NULL )
	free(Events);

  PAPI_shutdown();
  exit( 1 );
}
