/* 
* File:    multiplex.c
* CVS:     $Id: multiplex1.c,v 1.53 2010/03/12 22:48:03 vweaver1 Exp $
* Author:  Philip Mucci
*          mucci@cs.utk.edu
* Mods:    <your name here>
*          <your email address>
*/

/* This file tests the multiplex functionality, originally developed by 
   John May of LLNL. */

#include "papi_test.h"

/* Event to use in all cases; initialized in init_papi() */

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
init_papi( int *out_events, int *len )
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

/* Tests that PAPI_multiplex_init does not mess with normal operation. */

int
case1(  )
{
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	PAPI_events_len = 2;
	init_papi( PAPI_events, &PAPI_events_len );

	retval = PAPI_create_eventset( &EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_create_eventset", retval );

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

	if ( PAPI_start( EventSet ) != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_start", retval );

	do_stuff(  );

	retval = PAPI_stop( EventSet, values );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		test_print_event_header( "case1:", EventSet );
		printf( TAB2, "case1:", values[0], values[1] );
	}
	retval = PAPI_cleanup_eventset( EventSet );	/* JT */
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_cleanup_eventset", retval );

	PAPI_shutdown(  );
	return ( SUCCESS );
}

/* Tests that PAPI_set_multiplex() works before adding events */

int
case2(  )
{
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	PAPI_events_len = 2;
	init_papi( PAPI_events, &PAPI_events_len );

	retval = PAPI_create_eventset( &EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_create_eventset", retval );

	/* In Component PAPI, EventSets must be assigned a component index
	   before you can fiddle with their internals.
	   0 is always the cpu component */
	retval = PAPI_assign_eventset_component( EventSet, 0 );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_assign_eventset_component", retval );

	retval = PAPI_set_multiplex( EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_set_multiplex", retval );

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

	if ( PAPI_start( EventSet ) != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_start", retval );

	do_stuff(  );

	retval = PAPI_stop( EventSet, values );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		test_print_event_header( "case2:", EventSet );
		printf( TAB2, "case2:", values[0], values[1] );
	}

	retval = PAPI_cleanup_eventset( EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_cleanup_eventset", retval );

	PAPI_shutdown(  );
	return ( SUCCESS );
}

/* Tests that PAPI_set_multiplex() works after adding events */

int
case3(  )
{
	int retval, i, EventSet = PAPI_NULL;
	long long values[2];

	PAPI_events_len = 2;
	init_papi( PAPI_events, &PAPI_events_len );

	retval = PAPI_create_eventset( &EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_create_eventset", retval );

	for ( i = 0; i < PAPI_events_len; i++ ) {
		char out[PAPI_MAX_STR_LEN];

		retval = PAPI_add_event( EventSet, PAPI_events[i] );
		if ( retval != PAPI_OK )
			CPP_TEST_FAIL( "PAPI_add_event", retval );
		PAPI_event_code_to_name( PAPI_events[i], out );
		if ( !TESTS_QUIET )
			printf( "Added %s\n", out );
	}

	retval = PAPI_set_multiplex( EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_set_multiplex", retval );

	do_stuff(  );

	if ( PAPI_start( EventSet ) != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_start", retval );

	do_stuff(  );

	retval = PAPI_stop( EventSet, values );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		test_print_event_header( "case3:", EventSet );
		printf( TAB2, "case3:", values[0], values[1] );
	}

	retval = PAPI_cleanup_eventset( EventSet );	/* JT */
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_cleanup_eventset", retval );

	PAPI_shutdown(  );
	return ( SUCCESS );
}

/* Tests that PAPI_set_multiplex() works before adding events */

/* Tests that PAPI_add_event() works after
   PAPI_add_event()/PAPI_set_multiplex() */

int
case4(  )
{
	int retval, i, EventSet = PAPI_NULL;
	long long values[4];
	char out[PAPI_MAX_STR_LEN];

	PAPI_events_len = 2;
	init_papi( PAPI_events, &PAPI_events_len );

	retval = PAPI_create_eventset( &EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_create_eventset", retval );

	i = 0;
	retval = PAPI_add_event( EventSet, PAPI_events[i] );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_add_event", retval );
	PAPI_event_code_to_name( PAPI_events[i], out );
	printf( "Added %s\n", out );

	retval = PAPI_set_multiplex( EventSet );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_set_multiplex", retval );

	i = 1;
	retval = PAPI_add_event( EventSet, PAPI_events[i] );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_add_event", retval );
	PAPI_event_code_to_name( PAPI_events[i], out );
	printf( "Added %s\n", out );

	do_stuff(  );

	if ( PAPI_start( EventSet ) != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_start", retval );

	do_stuff(  );

	retval = PAPI_stop( EventSet, values );
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_stop", retval );

	if ( !TESTS_QUIET ) {
		test_print_event_header( "case4:", EventSet );
		printf( TAB2, "case4:", values[0], values[1] );
	}

	retval = PAPI_cleanup_eventset( EventSet );	/* JT */
	if ( retval != PAPI_OK )
		CPP_TEST_FAIL( "PAPI_cleanup_eventset", retval );

	PAPI_shutdown(  );
	return ( SUCCESS );
}

int
main( int argc, char **argv )
{

	tests_quiet( argc, argv );	/* Set TESTS_QUIET variable */

	printf
		( "case1: Does PAPI_multiplex_init() not break regular operation?\n" );
	case1(  );

	printf( "\ncase2: Does setmpx/add work?\n" );
	case2(  );

	printf( "\ncase3: Does add/setmpx work?\n" );
	case3(  );

	printf( "\ncase4: Does add/setmpx/add work?\n" );
	case4(  );

	test_pass( __FILE__, NULL, 0 );
	exit( 0 );
}
