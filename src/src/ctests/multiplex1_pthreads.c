/* 
* File:    multiplex1_pthreads.c
* CVS:     $Id: multiplex1_pthreads.c,v 1.54 2010/03/12 22:48:04 vweaver1 Exp $
* Author:  Philip Mucci
*          mucci@cs.utk.edu
* Mods:    <your name here>
*          <your email address>
*/

/* This file tests the multiplex pthread functionality */

#include <pthread.h>
#include "papi_test.h"

int solaris_preset_PAPI_events[PAPI_MPX_DEF_DEG] = {
  PAPI_BR_MSP, PAPI_TOT_CYC, PAPI_L2_TCM, PAPI_L1_ICM, 0
};
int power6_preset_PAPI_events[PAPI_MPX_DEF_DEG] = {
	PAPI_FP_INS, PAPI_TOT_CYC, PAPI_L1_DCM, PAPI_L1_ICM, 0
};
int preset_PAPI_events[PAPI_MPX_DEF_DEG] = {
	PAPI_FP_INS, PAPI_TOT_INS, PAPI_L1_DCM, PAPI_L1_ICM, 0
};
static int PAPI_events[PAPI_MPX_DEF_DEG] = { 0, };
static int PAPI_events_len = 0;

#define CPP_TEST_FAIL(string, retval) test_fail(__FILE__, __LINE__, string, retval)

void
init_papi_pthreads( int *out_events, int *len )
{
	int retval;
	int i, real_len = 0;
	int *in_events = preset_PAPI_events;
	const PAPI_hw_info_t *hw_info;

	/* Initialize the library */
	retval = PAPI_library_init( PAPI_VER_CURRENT );
	if ( retval != PAPI_VER_CURRENT )
		CPP_TEST_FAIL( "PAPI_library_init", retval );

	hw_info = PAPI_get_hardware_info(  );
	if ( hw_info == NULL )
		test_fail( __FILE__, __LINE__, "PAPI_get_hardware_info", 2 );

	if ( strstr( hw_info->model_string, "UltraSPARC" ) ) {
	  in_events = solaris_preset_PAPI_events;
        }

	if ( strcmp( hw_info->model_string, "POWER6" ) == 0 ) {
		in_events = power6_preset_PAPI_events;
		retval = PAPI_set_domain( PAPI_DOM_ALL );
		if ( retval != PAPI_OK )
			CPP_TEST_FAIL( "PAPI_set_domain", retval );
	}

	retval = PAPI_multiplex_init(  );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_multiplex_init", retval );

	if ( ( retval =
		   PAPI_thread_init( ( unsigned
							   long ( * )( void ) ) ( pthread_self ) ) ) !=
		 PAPI_OK ) {
		if ( retval == PAPI_ESBSTR )
			test_skip( __FILE__, __LINE__, "PAPI_thread_init", retval );
		else
			test_fail( __FILE__, __LINE__, "PAPI_thread_init", retval );
	}

	for ( i = 0; in_events[i] != 0; i++ ) {
		char out[PAPI_MAX_STR_LEN];
		/* query and set up the right instruction to monitor */
		retval = PAPI_query_event( in_events[i] );
		if ( retval == PAPI_OK ) {
			out_events[real_len++] = in_events[i];
			PAPI_event_code_to_name( in_events[i], out );
			if ( real_len == *len )
				break;
		} else {
			PAPI_event_code_to_name( in_events[i], out );
			if ( !TESTS_QUIET )
				printf( "%s does not exist\n", out );
		}
	}
	if ( real_len < 1 )
		CPP_TEST_FAIL( "No counters available", 0 );
	*len = real_len;
}

int
do_pthreads( void *( *fn ) ( void * ) )
{
	int i, rc, retval;
	pthread_attr_t attr;
	pthread_t id[NUM_THREADS];

	pthread_attr_init( &attr );
#ifdef PTHREAD_CREATE_UNDETACHED
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_UNDETACHED );
#endif
#ifdef PTHREAD_SCOPE_SYSTEM
	retval = pthread_attr_setscope( &attr, PTHREAD_SCOPE_SYSTEM );
	if ( retval != 0 )
		test_skip( __FILE__, __LINE__, "pthread_attr_setscope", retval );
#endif

	for ( i = 0; i < NUM_THREADS; i++ ) {
		rc = pthread_create( &id[i], &attr, fn, NULL );
		if ( rc )
			return ( FAILURE );
	}
	for ( i = 0; i < NUM_THREADS; i++ )
		pthread_join( id[i], NULL );

	pthread_attr_destroy( &attr );
	pthread_exit( NULL );
	return ( SUCCESS );
}

/* Tests that PAPI_multiplex_init does not mess with normal operation. */

void *
case1_pthreads( void *arg )
{
	( void ) arg;			 /*unused */
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	if ( ( retval = PAPI_register_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_register_thread", retval );

	if ( ( retval = PAPI_create_eventset( &EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

	for ( i = 0; i < PAPI_events_len; i++ ) {
		char out[PAPI_MAX_STR_LEN];

		retval = PAPI_add_event( EventSet, PAPI_events[i] );
		if ( retval != PAPI_OK )
			CPP_TEST_FAIL( "PAPI_add_event", retval );
		PAPI_event_code_to_name( PAPI_events[i], out );
		if ( !TESTS_QUIET )
			printf( "Added %s\n", out );
	}

	do_stuff(  );

	if ( ( retval = PAPI_start( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );

	do_stuff(  );

	if ( ( retval = PAPI_stop( EventSet, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		printf( "case1 thread %4x:", ( unsigned ) pthread_self(  ) );
		test_print_event_header( "", EventSet );
		printf( "case1 thread %4x:", ( unsigned ) pthread_self(  ) );
		printf( TAB2, "", values[0], values[1] );
	}

	if ( ( retval = PAPI_cleanup_eventset( EventSet ) ) != PAPI_OK )	/* JT */
		test_fail( __FILE__, __LINE__, "PAPI_cleanup_eventset", retval );

	if ( ( retval = PAPI_unregister_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_unregister_thread", retval );

	return ( ( void * ) SUCCESS );
}

/* Tests that PAPI_set_multiplex() works before adding events */

void *
case2_pthreads( void *arg )
{
	( void ) arg;			 /*unused */
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	if ( ( retval = PAPI_register_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_register_thread", retval );

	if ( ( retval = PAPI_create_eventset( &EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

	/* In Component PAPI, EventSets must be assigned a component index
	   before you can fiddle with their internals.
	   0 is always the cpu component */
	retval = PAPI_assign_eventset_component( EventSet, 0 );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_assign_eventset_component", retval );

	if ( ( retval = PAPI_set_multiplex( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );
	printf( "++case2 thread %4x:", ( unsigned ) pthread_self(  ) );

	for ( i = 0; i < PAPI_events_len; i++ ) {
		char out[PAPI_MAX_STR_LEN];

		retval = PAPI_add_event( EventSet, PAPI_events[i] );
		if ( retval != PAPI_OK )
			CPP_TEST_FAIL( "PAPI_add_event", retval );
		PAPI_event_code_to_name( PAPI_events[i], out );
		if ( !TESTS_QUIET )
			printf( "Added %s\n", out );
	}

	do_stuff(  );

	if ( ( retval = PAPI_start( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );

	do_stuff(  );

	if ( ( retval = PAPI_stop( EventSet, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		printf( "case2 thread %4x:", ( unsigned ) pthread_self(  ) );
		test_print_event_header( "", EventSet );
		printf( "case2 thread %4x:", ( unsigned ) pthread_self(  ) );
		printf( TAB2, "", values[0], values[1] );
	}

	if ( ( retval = PAPI_cleanup_eventset( EventSet ) ) != PAPI_OK )	/* JT */
		test_fail( __FILE__, __LINE__, "PAPI_cleanup_eventset", retval );

	if ( ( retval = PAPI_unregister_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_unregister_thread", retval );

	return ( ( void * ) SUCCESS );
}

/* Tests that PAPI_set_multiplex() works after adding events */

void *
case3_pthreads( void *arg )
{
	( void ) arg;			 /*unused */
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	if ( ( retval = PAPI_register_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_register_thread", retval );

	if ( ( retval = PAPI_create_eventset( &EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

	for ( i = 0; i < PAPI_events_len; i++ ) {
		char out[PAPI_MAX_STR_LEN];

		retval = PAPI_add_event( EventSet, PAPI_events[i] );
		if ( retval != PAPI_OK )
			CPP_TEST_FAIL( "PAPI_add_event", retval );
		PAPI_event_code_to_name( PAPI_events[i], out );
		if ( !TESTS_QUIET )
			printf( "Added %s\n", out );
	}

	if ( ( retval = PAPI_set_multiplex( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );

	do_stuff(  );

	if ( ( retval = PAPI_start( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );

	do_stuff(  );

	if ( ( retval = PAPI_stop( EventSet, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		printf( "case3 thread %4x:", ( unsigned ) pthread_self(  ) );
		test_print_event_header( "", EventSet );
		printf( "case3 thread %4x:", ( unsigned ) pthread_self(  ) );
		printf( TAB2, "", values[0], values[1] );
	}

	if ( ( retval = PAPI_cleanup_eventset( EventSet ) ) != PAPI_OK )	/* JT */
		test_fail( __FILE__, __LINE__, "PAPI_cleanup_eventset", retval );

	if ( ( retval = PAPI_unregister_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_unregister_thread", retval );

	return ( ( void * ) SUCCESS );
}

/* Tests that PAPI_set_multiplex() works before/after adding events */

void *
case4_pthreads( void *arg )
{
	( void ) arg;			 /*unused */
	int retval, i, EventSet = PAPI_NULL;
	long long values[4];
	char out[PAPI_MAX_STR_LEN];

	if ( ( retval = PAPI_register_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_register_thread", retval );

	if ( ( retval = PAPI_create_eventset( &EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );

	i = 0;
	retval = PAPI_add_event( EventSet, PAPI_events[i] );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_add_event", retval );
	PAPI_event_code_to_name( PAPI_events[i], out );
	printf( "Added %s\n", out );

	if ( ( retval = PAPI_set_multiplex( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );

	i = 1;
	retval = PAPI_add_event( EventSet, PAPI_events[i] );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_add_event", retval );
	PAPI_event_code_to_name( PAPI_events[i], out );
	printf( "Added %s\n", out );

	do_stuff(  );

	if ( ( retval = PAPI_start( EventSet ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_start", retval );

	do_stuff(  );

	if ( ( retval = PAPI_stop( EventSet, values ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		printf( "case4 thread %4x:", ( unsigned ) pthread_self(  ) );
		test_print_event_header( "", EventSet );
		printf( "case4 thread %4x:", ( unsigned ) pthread_self(  ) );
		printf( TAB2, "", values[0], values[1] );
	}

	if ( ( retval = PAPI_cleanup_eventset( EventSet ) ) != PAPI_OK )	/* JT */
		test_fail( __FILE__, __LINE__, "PAPI_cleanup_eventset", retval );

	if ( ( retval = PAPI_unregister_thread(  ) ) != PAPI_OK )
		test_fail( __FILE__, __LINE__, "PAPI_unregister_thread", retval );

	return ( ( void * ) SUCCESS );
}

int
case1( void )
{
	int retval;

	PAPI_events_len = 2;
	init_papi_pthreads( PAPI_events, &PAPI_events_len );

	retval = do_pthreads( case1_pthreads );

	PAPI_shutdown(  );

	return ( retval );
}

int
case2( void )
{
	int retval;

	PAPI_events_len = 2;
	init_papi_pthreads( PAPI_events, &PAPI_events_len );

	retval = do_pthreads( case2_pthreads );

	PAPI_shutdown(  );

	return ( retval );
}

int
case3( void )
{
	int retval;

	PAPI_events_len = 2;
	init_papi_pthreads( PAPI_events, &PAPI_events_len );

	retval = do_pthreads( case3_pthreads );

	PAPI_shutdown(  );

	return ( retval );
}

int
case4( void )
{
	int retval;

	PAPI_events_len = 2;
	init_papi_pthreads( PAPI_events, &PAPI_events_len );

	retval = do_pthreads( case4_pthreads );

	PAPI_shutdown(  );

	return ( retval );
}

int
main( int argc, char **argv )
{
	tests_quiet( argc, argv );	/* Set TESTS_QUIET variable */

	printf( "%s: Using %d threads\n\n", argv[0], NUM_THREADS );

	printf
		( "case1: Does PAPI_multiplex_init() not break regular operation?\n" );
	if ( case1(  ) != SUCCESS )
		test_fail( __FILE__, __LINE__, "case1", PAPI_ESYS );

	printf( "case2: Does setmpx/add work?\n" );
	if ( case2(  ) != SUCCESS )
		test_fail( __FILE__, __LINE__, "case2", PAPI_ESYS );

	printf( "case3: Does add/setmpx work?\n" );
	if ( case3(  ) != SUCCESS )
		test_fail( __FILE__, __LINE__, "case3", PAPI_ESYS );

	printf( "case4: Does add/setmpx/add work?\n" );
	if ( case4(  ) != SUCCESS )
		test_fail( __FILE__, __LINE__, "case4", PAPI_ESYS );

	PAPI_library_init( PAPI_VER_CURRENT );
	test_pass( __FILE__, NULL, 0 );
	exit( 1 );
}
