/****************************/
/* THIS IS OPEN SOURCE CODE */
/****************************/

/* 
* File:    linux-memory.c
* Author:  Kevin London
*          london@cs.utk.edu
* Mods:    Dan Terpstra
*          terpstra@eecs.utk.edu
*          cache and TLB info exported to a separate file
*          which is not OS or driver dependent
* Mods:    Vince Weaver
*          vweaver1@eecs.utk.edu
*          Merge all of the various copies of linux-related
*          memory detection info this file.
*/

#include <string.h>
#include <errno.h>

#include "papi.h"
#include "papi_internal.h"
#include "papi_memory.h" /* papi_calloc() */

#include SUBSTRATE

extern int x86_cache_info( PAPI_mh_info_t * mh_info );


/* 2.6.19 has this:
VmPeak:     4588 kB
VmSize:     4584 kB
VmLck:         0 kB
VmHWM:      1548 kB
VmRSS:      1548 kB
VmData:      312 kB
VmStk:        88 kB
VmExe:       684 kB
VmLib:      1360 kB
VmPTE:        20 kB
*/


int
_linux_get_dmem_info( PAPI_dmem_info_t * d )
{
	char fn[PATH_MAX], tmp[PATH_MAX];
	FILE *f;
	int ret;
	long long sz = 0, lck = 0, res = 0, shr = 0, stk = 0, txt = 0, dat =
		0, dum = 0, lib = 0, hwm = 0;

	sprintf( fn, "/proc/%ld/status", ( long ) getpid(  ) );
	f = fopen( fn, "r" );
	if ( f == NULL ) {
		PAPIERROR( "fopen(%s): %s\n", fn, strerror( errno ) );
		return PAPI_ESBSTR;
	}
	while ( 1 ) {
		if ( fgets( tmp, PATH_MAX, f ) == NULL )
			break;
		if ( strspn( tmp, "VmSize:" ) == strlen( "VmSize:" ) ) {
			sscanf( tmp + strlen( "VmSize:" ), "%lld", &sz );
			d->size = sz;
			continue;
		}
		if ( strspn( tmp, "VmHWM:" ) == strlen( "VmHWM:" ) ) {
			sscanf( tmp + strlen( "VmHWM:" ), "%lld", &hwm );
			d->high_water_mark = hwm;
			continue;
		}
		if ( strspn( tmp, "VmLck:" ) == strlen( "VmLck:" ) ) {
			sscanf( tmp + strlen( "VmLck:" ), "%lld", &lck );
			d->locked = lck;
			continue;
		}
		if ( strspn( tmp, "VmRSS:" ) == strlen( "VmRSS:" ) ) {
			sscanf( tmp + strlen( "VmRSS:" ), "%lld", &res );
			d->resident = res;
			continue;
		}
		if ( strspn( tmp, "VmData:" ) == strlen( "VmData:" ) ) {
			sscanf( tmp + strlen( "VmData:" ), "%lld", &dat );
			d->heap = dat;
			continue;
		}
		if ( strspn( tmp, "VmStk:" ) == strlen( "VmStk:" ) ) {
			sscanf( tmp + strlen( "VmStk:" ), "%lld", &stk );
			d->stack = stk;
			continue;
		}
		if ( strspn( tmp, "VmExe:" ) == strlen( "VmExe:" ) ) {
			sscanf( tmp + strlen( "VmExe:" ), "%lld", &txt );
			d->text = txt;
			continue;
		}
		if ( strspn( tmp, "VmLib:" ) == strlen( "VmLib:" ) ) {
			sscanf( tmp + strlen( "VmLib:" ), "%lld", &lib );
			d->library = lib;
			continue;
		}
	}
	fclose( f );

	sprintf( fn, "/proc/%ld/statm", ( long ) getpid(  ) );
	f = fopen( fn, "r" );
	if ( f == NULL ) {
		PAPIERROR( "fopen(%s): %s\n", fn, strerror( errno ) );
		return PAPI_ESBSTR;
	}
	ret =
		fscanf( f, "%lld %lld %lld %lld %lld %lld %lld", &dum, &dum, &shr, &dum,
				&dum, &dat, &dum );
	if ( ret != 7 ) {
		PAPIERROR( "fscanf(7 items): %d\n", ret );
		return PAPI_ESBSTR;
	}
	d->pagesize = getpagesize(  );
	d->shared = ( shr * d->pagesize ) / 1024;
	fclose( f );

	return PAPI_OK;
}

/*
 * Architecture-specific cache detection code 
 */


#if defined(__i386__)||defined(__x86_64__)
static int
x86_get_memory_info( PAPI_hw_info_t * hw_info )
{
	int retval = PAPI_OK;

	extern int x86_cache_info( PAPI_mh_info_t * mh_info );

	switch ( hw_info->vendor ) {
	case PAPI_VENDOR_AMD:
	case PAPI_VENDOR_INTEL:
		retval = x86_cache_info( &hw_info->mem_hierarchy );
		break;
	default:
		PAPIERROR( "Unknown vendor in memory information call for x86." );
		return PAPI_ESBSTR;
	}
	return retval;
}
#endif

#if defined(__ia64__)
static int
get_number( char *buf )
{
	char numbers[] = "0123456789";
	int num;
	char *tmp, *end;

	tmp = strpbrk( buf, numbers );
	if ( tmp != NULL ) {
		end = tmp;
		while ( isdigit( *end ) )
			end++;
		*end = '\0';
		num = atoi( tmp );
		return num;
	}

	PAPIERROR( "Number could not be parsed from %s", buf );
	return -1;
}

static void
fline( FILE * fp, char *rline )
{
	char *tmp, *end, c;

	tmp = rline;
	end = &rline[1023];

	memset( rline, '\0', 1024 );

	do {
		if ( feof( fp ) )
			return;
		c = getc( fp );
	}
	while ( isspace( c ) || c == '\n' || c == '\r' );

	ungetc( c, fp );

	for ( ;; ) {
		if ( feof( fp ) ) {
			return;
		}
		c = getc( fp );
		if ( c == '\n' || c == '\r' )
			break;
		*tmp++ = c;
		if ( tmp == end ) {
			*tmp = '\0';
			return;
		}
	}
	return;
}

static int
ia64_get_memory_info( PAPI_hw_info_t * hw_info )
{
	int retval = 0;
	FILE *f;
	int clevel = 0, cindex = -1;
	char buf[1024];
	int num, i, j;
	PAPI_mh_info_t *meminfo = &hw_info->mem_hierarchy;
	PAPI_mh_level_t *L = hw_info->mem_hierarchy.level;

	f = fopen( "/proc/pal/cpu0/cache_info", "r" );

	if ( !f ) {
		PAPIERROR( "fopen(/proc/pal/cpu0/cache_info) returned < 0" );
		return PAPI_ESYS;
	}

	while ( !feof( f ) ) {
		fline( f, buf );
		if ( buf[0] == '\0' )
			break;
		if ( !strncmp( buf, "Data Cache", 10 ) ) {
			cindex = 1;
			clevel = get_number( buf );
			L[clevel - 1].cache[cindex].type = PAPI_MH_TYPE_DATA;
		} else if ( !strncmp( buf, "Instruction Cache", 17 ) ) {
			cindex = 0;
			clevel = get_number( buf );
			L[clevel - 1].cache[cindex].type = PAPI_MH_TYPE_INST;
		} else if ( !strncmp( buf, "Data/Instruction Cache", 22 ) ) {
			cindex = 0;
			clevel = get_number( buf );
			L[clevel - 1].cache[cindex].type = PAPI_MH_TYPE_UNIFIED;
		} else {
			if ( ( clevel == 0 || clevel > 3 ) && cindex >= 0 ) {
				PAPIERROR
					( "Cache type could not be recognized, please send /proc/pal/cpu0/cache_info" );
				return PAPI_EBUG;
			}

			if ( !strncmp( buf, "Size", 4 ) ) {
				num = get_number( buf );
				L[clevel - 1].cache[cindex].size = num;
			} else if ( !strncmp( buf, "Associativity", 13 ) ) {
				num = get_number( buf );
				L[clevel - 1].cache[cindex].associativity = num;
			} else if ( !strncmp( buf, "Line size", 9 ) ) {
				num = get_number( buf );
				L[clevel - 1].cache[cindex].line_size = num;
				L[clevel - 1].cache[cindex].num_lines =
					L[clevel - 1].cache[cindex].size / num;
			}
		}
	}

	fclose( f );

	f = fopen( "/proc/pal/cpu0/vm_info", "r" );
	/* No errors on fopen as I am not sure this is always on the systems */
	if ( f != NULL ) {
		cindex = -1;
		clevel = 0;
		while ( !feof( f ) ) {
			fline( f, buf );
			if ( buf[0] == '\0' )
				break;
			if ( !strncmp( buf, "Data Translation", 16 ) ) {
				cindex = 1;
				clevel = get_number( buf );
				L[clevel - 1].tlb[cindex].type = PAPI_MH_TYPE_DATA;
			} else if ( !strncmp( buf, "Instruction Translation", 23 ) ) {
				cindex = 0;
				clevel = get_number( buf );
				L[clevel - 1].tlb[cindex].type = PAPI_MH_TYPE_INST;
			} else {
				if ( ( clevel == 0 || clevel > 2 ) && cindex >= 0 ) {
					PAPIERROR
						( "TLB type could not be recognized, send /proc/pal/cpu0/vm_info" );
					return PAPI_EBUG;
				}

				if ( !strncmp( buf, "Number of entries", 17 ) ) {
					num = get_number( buf );
					L[clevel - 1].tlb[cindex].num_entries = num;
				} else if ( !strncmp( buf, "Associativity", 13 ) ) {
					num = get_number( buf );
					L[clevel - 1].tlb[cindex].associativity = num;
				}
			}
		}
		fclose( f );
	}

	/* Compute and store the number of levels of hierarchy actually used */
	for ( i = 0; i < PAPI_MH_MAX_LEVELS; i++ ) {
		for ( j = 0; j < 2; j++ ) {
			if ( L[i].tlb[j].type != PAPI_MH_TYPE_EMPTY ||
				 L[i].cache[j].type != PAPI_MH_TYPE_EMPTY )
				meminfo->levels = i + 1;
		}
	}
	return retval;
}
#endif

#if defined(__powerpc__)

PAPI_mh_info_t sys_mem_info[4] = {
	{2,						 // 970 begin
	 {
	  {						 // level 1 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_UNIFIED, 1024, 4, 0}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_INST, 65536, 128, 512, 1}
		,
		{PAPI_MH_TYPE_DATA, 32768, 128, 256, 2}
		}
	   }
	  ,
	  {						 // level 2 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_UNIFIED, 524288, 128, 4096, 8}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1, -1}
		}
	   }
	  ,
	  }
	 }
	,						 // 970 end
	{3,
	 {
	  {						 // level 1 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_UNIFIED, 1024, 4, 0}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_INST, 65536, 128, 512, 2}
		,
		{PAPI_MH_TYPE_DATA, 32768, 128, 256, 4}
		}
	   }
	  ,
	  {						 // level 2 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_UNIFIED, 1966080, 128, 15360, 10}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1, -1}
		}
	   }
	  ,
	  {						 // level 3 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_UNIFIED, 37748736, 256, 147456, 12}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1, -1}
		}
	   }
	  ,
	  }
	 }
	,						 // POWER5 end
	{3,
	 {
	  {						 // level 1 begins
	   {					 // tlb's begin
		/// POWER6 has an ERAT (Effective to Real Address
		/// Translation) instead of a TLB.  For the purposes of this
		/// data, we will treat it like a TLB.
		{PAPI_MH_TYPE_INST, 128, 2, 0}
		,
		{PAPI_MH_TYPE_DATA, 128, 128, 0}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_INST, 65536, 128, 512, 4}
		,
		{PAPI_MH_TYPE_DATA, 65536, 128, 512, 8}
		}
	   }
	  ,
	  {						 // level 2 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		{PAPI_MH_TYPE_UNIFIED, 4194304, 128, 16384, 8}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1, -1}
		}
	   }
	  ,
	  {						 // level 3 begins
	   {					 // tlb's begin
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1}
		}
	   ,
	   {					 // caches begin
		/// POWER6 has a 2 slice L3 cache.  Each slice is 16MB, so
		/// combined they are 32MB and usable by each core.  For
		/// this reason, we will treat it as a single 32MB cache.
		{PAPI_MH_TYPE_UNIFIED, 33554432, 128, 262144, 16}
		,
		{PAPI_MH_TYPE_EMPTY, -1, -1, -1, -1}
		}
	   }
	  ,
	  }
	 }
	,						 // POWER6 end
	{3,
	 {
	  [0] = { // level 1 begins
		.tlb = {
		/// POWER7 has an ERAT (Effective to Real Address
		/// Translation) instead of a TLB.  For the purposes of this
		/// data, we will treat it like a TLB.
		[0] = { .type = PAPI_MH_TYPE_INST,
				.num_entries = 64, .page_size = 0, .associativity = 2 }
		,
		[1] = { .type = PAPI_MH_TYPE_DATA,
				.num_entries = 64, .page_size = 0,
				.associativity = SHRT_MAX }
		}
		,
		.cache = { // level 1 caches begin
		[0] = { .type = PAPI_MH_TYPE_INST | PAPI_MH_TYPE_PSEUDO_LRU,
				.size = 32768, .line_size = 128, .num_lines = 64,
				.associativity = 4 }
		,
		[1] = { .type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_WT | PAPI_MH_TYPE_LRU,
				.size = 32768, .line_size = 128, .num_lines = 32,
				.associativity = 8 }
		}
	   }
	  ,
	  [1] = { // level 2 begins
		.tlb = {
		[0] = { .type = PAPI_MH_TYPE_EMPTY, .num_entries = -1,
			.page_size = -1, .associativity = -1 }
		,
		[1] = { .type = PAPI_MH_TYPE_EMPTY, .num_entries = -1,
			.page_size = -1, .associativity = -1 }
		}
		,
		.cache = {
		[0] = { .type = PAPI_MH_TYPE_UNIFIED | PAPI_MH_TYPE_PSEUDO_LRU,
				.size = 262144, .line_size = 128, .num_lines = 256,
				.associativity = 8 }
		,
		[1] = { .type = PAPI_MH_TYPE_EMPTY, .size = -1, .line_size = -1,
				.num_lines = -1, .associativity = -1 }
		}
	   }
	  ,
	  [2] = { // level 3 begins
		.tlb = {
		[0] = { .type = PAPI_MH_TYPE_EMPTY, .num_entries = -1,
			.page_size = -1, .associativity = -1 }
		,
		[1] = { .type = PAPI_MH_TYPE_EMPTY, .num_entries = -1,
			.page_size = -1, .associativity = -1 }
		}
		,
		.cache = {
		[0] = { .type = PAPI_MH_TYPE_UNIFIED | PAPI_MH_TYPE_PSEUDO_LRU,
				.size = 4194304, .line_size = 128, .num_lines = 4096,
				.associativity = 8 }
		,
		[1] = { .type = PAPI_MH_TYPE_EMPTY, .size = -1, .line_size = -1,
				.num_lines = -1, .associativity = -1 }
		}
	   }
	  ,
	  }
	 }						 // POWER7 end
};

#define SPRN_PVR 0x11F		 /* Processor Version Register */
#define PVR_PROCESSOR_SHIFT 16
static unsigned int
mfpvr( void )
{
	unsigned long pvr;

  asm( "mfspr          %0,%1": "=r"( pvr ):"i"( SPRN_PVR ) );
	return pvr;

}

int
ppc64_get_memory_info( PAPI_hw_info_t * hw_info )
{
	unsigned int pvr = mfpvr(  ) >> PVR_PROCESSOR_SHIFT;

	int index;
	switch ( pvr ) {
	case 0x39:				 /* PPC970 */
	case 0x3C:				 /* PPC970FX */
	case 0x44:				 /* PPC970MP */
	case 0x45:				 /* PPC970GX */
		index = 0;
		break;
	case 0x3A:				 /* POWER5 */
	case 0x3B:				 /* POWER5+ */
		index = 1;
		break;
	case 0x3E:				 /* POWER6 */
		index = 2;
		break;
	case 0x3F:				 /* POWER7 */
		index = 3;
		break;
	default:
		index = -1;
		break;
	}

	if ( index != -1 ) {
		int cache_level;
		PAPI_mh_info_t sys_mh_inf = sys_mem_info[index];
		PAPI_mh_info_t *mh_inf = &hw_info->mem_hierarchy;
		mh_inf->levels = sys_mh_inf.levels;
		PAPI_mh_level_t *level = mh_inf->level;
		PAPI_mh_level_t sys_mh_level;
		for ( cache_level = 0; cache_level < sys_mh_inf.levels; cache_level++ ) {
			sys_mh_level = sys_mh_inf.level[cache_level];
			int cache_idx;
			for ( cache_idx = 0; cache_idx < 2; cache_idx++ ) {
				// process TLB info
				PAPI_mh_tlb_info_t curr_tlb = sys_mh_level.tlb[cache_idx];
				int type = curr_tlb.type;
				if ( type != PAPI_MH_TYPE_EMPTY ) {
					level[cache_level].tlb[cache_idx].type = type;
					level[cache_level].tlb[cache_idx].associativity =
						curr_tlb.associativity;
					level[cache_level].tlb[cache_idx].num_entries =
						curr_tlb.num_entries;
				}
			}
			for ( cache_idx = 0; cache_idx < 2; cache_idx++ ) {
				// process cache info
				PAPI_mh_cache_info_t curr_cache = sys_mh_level.cache[cache_idx];
				int type = curr_cache.type;
				if ( type != PAPI_MH_TYPE_EMPTY ) {
					level[cache_level].cache[cache_idx].type = type;
					level[cache_level].cache[cache_idx].associativity =
						curr_cache.associativity;
					level[cache_level].cache[cache_idx].size = curr_cache.size;
					level[cache_level].cache[cache_idx].line_size =
						curr_cache.line_size;
					level[cache_level].cache[cache_idx].num_lines =
						curr_cache.num_lines;
				}
			}
		}
	}
	return 0;
}
#endif

#if defined(__sparc__)
static int
sparc_sysfs_cpu_attr( char *name, char **result )
{
	const char *path_base = "/sys/devices/system/cpu/";
	char path_buf[PATH_MAX];
	char val_buf[32];
	DIR *sys_cpu;

	sys_cpu = opendir( path_base );
	if ( sys_cpu ) {
		struct dirent *cpu;

		while ( ( cpu = readdir( sys_cpu ) ) != NULL ) {
			int fd;

			if ( strncmp( "cpu", cpu->d_name, 3 ) )
				continue;
			strcpy( path_buf, path_base );
			strcat( path_buf, cpu->d_name );
			strcat( path_buf, "/" );
			strcat( path_buf, name );

			fd = open( path_buf, O_RDONLY );
			if ( fd < 0 )
				continue;

			if ( read( fd, val_buf, 32 ) < 0 )
				continue;
			close( fd );

			*result = strdup( val_buf );
			return 0;
		}
	}
	return -1;
}

static int
sparc_cpu_attr( char *name, unsigned long long *val )
{
	char *buf;
	int r;

	r = sparc_sysfs_cpu_attr( name, &buf );
	if ( r == -1 )
		return -1;

	sscanf( buf, "%llu", val );

	free( buf );

	return 0;
}

static char *
search_cpu_info( FILE * f, char *search_str, char *line )
{
  /* This code courtesy of our friends in Germany. Thanks Rudolph Berrend\
     orf! */
  /* See the home page for the German version of PAPI. */

  char *s;

  while ( fgets( line, 256, f ) != NULL ) {
    if ( strstr( line, search_str ) != NULL ) {
      /* ignore all characters in line up to : */
      for ( s = line; *s && ( *s != ':' ); ++s );
      if ( *s )
	return s;
    }
  }
  return NULL;

  /* End stolen code */
}


static int
sparc_get_memory_info( PAPI_hw_info_t * hw_info )
{
	unsigned long long cache_size, cache_line_size;
	unsigned long long cycles_per_second;
	char maxargs[PAPI_HUGE_STR_LEN];
	PAPI_mh_tlb_info_t *tlb;
	PAPI_mh_level_t *level;
	char *s, *t;
	FILE *f;

	/* First, fix up the cpu vendor/model/etc. values */
	strcpy( hw_info->vendor_string, "Sun" );
	hw_info->vendor = PAPI_VENDOR_SUN;

	f = fopen( "/proc/cpuinfo", "r" );
	if ( !f )
		return PAPI_ESYS;

	rewind( f );
	s = search_cpu_info( f, "cpu", maxargs );
	if ( !s ) {
		fclose( f );
		return PAPI_ESYS;
	}

	t = strchr( s + 2, '\n' );
	if ( !t ) {
		fclose( f );
		return PAPI_ESYS;
	}

	*t = '\0';
	strcpy( hw_info->model_string, s + 2 );

	fclose( f );

	if ( sparc_sysfs_cpu_attr( "clock_tick", &s ) == -1 )
		return PAPI_ESYS;

	sscanf( s, "%llu", &cycles_per_second );
	free( s );

	hw_info->mhz = cycles_per_second / 1000000;
	hw_info->clock_mhz = hw_info->mhz;

	/* Now fetch the cache info */
	hw_info->mem_hierarchy.levels = 3;

	level = &hw_info->mem_hierarchy.level[0];

	sparc_cpu_attr( "l1_icache_size", &cache_size );
	sparc_cpu_attr( "l1_icache_line_size", &cache_line_size );
	level[0].cache[0].type = PAPI_MH_TYPE_INST;
	level[0].cache[0].size = cache_size;
	level[0].cache[0].line_size = cache_line_size;
	level[0].cache[0].num_lines = cache_size / cache_line_size;
	level[0].cache[0].associativity = 1;

	sparc_cpu_attr( "l1_dcache_size", &cache_size );
	sparc_cpu_attr( "l1_dcache_line_size", &cache_line_size );
	level[0].cache[1].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_WT;
	level[0].cache[1].size = cache_size;
	level[0].cache[1].line_size = cache_line_size;
	level[0].cache[1].num_lines = cache_size / cache_line_size;
	level[0].cache[1].associativity = 1;

	sparc_cpu_attr( "l2_cache_size", &cache_size );
	sparc_cpu_attr( "l2_cache_line_size", &cache_line_size );
	level[1].cache[0].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_WB;
	level[1].cache[0].size = cache_size;
	level[1].cache[0].line_size = cache_line_size;
	level[1].cache[0].num_lines = cache_size / cache_line_size;
	level[1].cache[0].associativity = 1;

	tlb = &hw_info->mem_hierarchy.level[0].tlb[0];
	switch ( _perfmon2_pfm_pmu_type ) {
	case PFMLIB_SPARC_ULTRA12_PMU:
		tlb[0].type = PAPI_MH_TYPE_INST | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[0].num_entries = 64;
		tlb[0].associativity = SHRT_MAX;
		tlb[1].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[1].num_entries = 64;
		tlb[1].associativity = SHRT_MAX;
		break;

	case PFMLIB_SPARC_ULTRA3_PMU:
	case PFMLIB_SPARC_ULTRA3I_PMU:
	case PFMLIB_SPARC_ULTRA3PLUS_PMU:
	case PFMLIB_SPARC_ULTRA4PLUS_PMU:
		level[0].cache[0].associativity = 4;
		level[0].cache[1].associativity = 4;
		level[1].cache[0].associativity = 4;

		tlb[0].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[0].num_entries = 16;
		tlb[0].associativity = SHRT_MAX;
		tlb[1].type = PAPI_MH_TYPE_INST | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[1].num_entries = 16;
		tlb[1].associativity = SHRT_MAX;
		tlb[2].type = PAPI_MH_TYPE_DATA;
		tlb[2].num_entries = 1024;
		tlb[2].associativity = 2;
		tlb[3].type = PAPI_MH_TYPE_INST;
		tlb[3].num_entries = 128;
		tlb[3].associativity = 2;
		break;

	case PFMLIB_SPARC_NIAGARA1:
		level[0].cache[0].associativity = 4;
		level[0].cache[1].associativity = 4;
		level[1].cache[0].associativity = 12;

		tlb[0].type = PAPI_MH_TYPE_INST | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[0].num_entries = 64;
		tlb[0].associativity = SHRT_MAX;
		tlb[1].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[1].num_entries = 64;
		tlb[1].associativity = SHRT_MAX;
		break;

	case PFMLIB_SPARC_NIAGARA2:
		level[0].cache[0].associativity = 8;
		level[0].cache[1].associativity = 4;
		level[1].cache[0].associativity = 16;

		tlb[0].type = PAPI_MH_TYPE_INST | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[0].num_entries = 64;
		tlb[0].associativity = SHRT_MAX;
		tlb[1].type = PAPI_MH_TYPE_DATA | PAPI_MH_TYPE_PSEUDO_LRU;
		tlb[1].num_entries = 128;
		tlb[1].associativity = SHRT_MAX;
		break;
	}

	return 0;
}
#endif

int
_linux_get_memory_info( PAPI_hw_info_t * hwinfo, int cpu_type )
{
	( void ) cpu_type;		 /*unused */
	int retval = PAPI_OK;

#if defined(__i386__)||defined(__x86_64__)
	x86_get_memory_info( hwinfo );
#elif defined(__ia64__)
	ia64_get_memory_info( hwinfo );
#elif defined(__powerpc__)
	ppc64_get_memory_info( hwinfo );
#elif defined(__sparc__)
	sparc_get_memory_info( hwinfo );
#else
#error "No support for this architecture. Please modify linux-memory.c"
#endif

	return retval;
}

int
_linux_update_shlib_info( papi_mdi_t *mdi )
{

	char fname[PAPI_HUGE_STR_LEN];
	unsigned long t_index = 0, d_index = 0, b_index = 0, counting = 1;
	char buf[PAPI_HUGE_STR_LEN + PAPI_HUGE_STR_LEN], perm[5], dev[16];
	char mapname[PAPI_HUGE_STR_LEN], lastmapname[PAPI_HUGE_STR_LEN];
	unsigned long begin = 0, end = 0, size = 0, inode = 0, foo = 0;
	PAPI_address_map_t *tmp = NULL;
	FILE *f;

	memset( fname, 0x0, sizeof ( fname ) );
	memset( buf, 0x0, sizeof ( buf ) );
	memset( perm, 0x0, sizeof ( perm ) );
	memset( dev, 0x0, sizeof ( dev ) );
	memset( mapname, 0x0, sizeof ( mapname ) );
	memset( lastmapname, 0x0, sizeof ( lastmapname ) );

	sprintf( fname, "/proc/%ld/maps", ( long ) mdi->pid );
	f = fopen( fname, "r" );

	if ( !f ) {
		PAPIERROR( "fopen(%s) returned < 0", fname );
		return PAPI_OK;
	}

  again:
	while ( !feof( f ) ) {
		begin = end = size = inode = foo = 0;
		if ( fgets( buf, sizeof ( buf ), f ) == 0 )
			break;
		/* If mapname is null in the string to be scanned, we need to detect that */
		if ( strlen( mapname ) )
			strcpy( lastmapname, mapname );
		else
			lastmapname[0] = '\0';
		/* If mapname is null in the string to be scanned, we need to detect that */
		mapname[0] = '\0';
		sscanf( buf, "%lx-%lx %4s %lx %s %ld %s", &begin, &end, perm, &foo, dev,
				&inode, mapname );
		size = end - begin;

		/* the permission string looks like "rwxp", where each character can
		 * be either the letter, or a hyphen.  The final character is either
		 * p for private or s for shared. */

		if ( counting ) {
			if ( ( perm[2] == 'x' ) && ( perm[0] == 'r' ) && ( inode != 0 ) ) {
				if ( strcmp( mdi->exe_info.fullname, mapname )
					 == 0 ) {
					mdi->exe_info.address_info.text_start =
						( caddr_t ) begin;
					mdi->exe_info.address_info.text_end =
						( caddr_t ) ( begin + size );
				}
				t_index++;
			} else if ( ( perm[0] == 'r' ) && ( perm[1] == 'w' ) &&
						( inode != 0 )
						&&
						( strcmp
						  ( mdi->exe_info.fullname,
							mapname ) == 0 ) ) {
				mdi->exe_info.address_info.data_start =
					( caddr_t ) begin;
				mdi->exe_info.address_info.data_end =
					( caddr_t ) ( begin + size );
				d_index++;
			} else if ( ( perm[0] == 'r' ) && ( perm[1] == 'w' ) &&
						( inode == 0 )
						&&
						( strcmp
						  ( mdi->exe_info.fullname,
							lastmapname ) == 0 ) ) {
				mdi->exe_info.address_info.bss_start =
					( caddr_t ) begin;
				mdi->exe_info.address_info.bss_end =
					( caddr_t ) ( begin + size );
				b_index++;
			}
		} else if ( !counting ) {
			if ( ( perm[2] == 'x' ) && ( perm[0] == 'r' ) && ( inode != 0 ) ) {
				if ( strcmp( mdi->exe_info.fullname, mapname )
					 != 0 ) {
					t_index++;
					tmp[t_index - 1].text_start = ( caddr_t ) begin;
					tmp[t_index - 1].text_end = ( caddr_t ) ( begin + size );
					strncpy( tmp[t_index - 1].name, mapname, PAPI_MAX_STR_LEN );
				}
			} else if ( ( perm[0] == 'r' ) && ( perm[1] == 'w' ) &&
						( inode != 0 ) ) {
				if ( ( strcmp
					   ( mdi->exe_info.fullname,
						 mapname ) != 0 )
					 && ( t_index > 0 ) &&
					 ( tmp[t_index - 1].data_start == 0 ) ) {
					tmp[t_index - 1].data_start = ( caddr_t ) begin;
					tmp[t_index - 1].data_end = ( caddr_t ) ( begin + size );
				}
			} else if ( ( perm[0] == 'r' ) && ( perm[1] == 'w' ) &&
						( inode == 0 ) ) {
				if ( ( t_index > 0 ) && ( tmp[t_index - 1].bss_start == 0 ) ) {
					tmp[t_index - 1].bss_start = ( caddr_t ) begin;
					tmp[t_index - 1].bss_end = ( caddr_t ) ( begin + size );
				}
			}
		}
	}

	if ( counting ) {
		/* When we get here, we have counted the number of entries in the map
		   for us to allocate */

		tmp =
			( PAPI_address_map_t * ) papi_calloc( t_index,
												  sizeof
												  ( PAPI_address_map_t ) );
		if ( tmp == NULL ) {
			PAPIERROR( "Error allocating shared library address map" );
			return PAPI_ENOMEM;
		}
		t_index = 0;
		rewind( f );
		counting = 0;
		goto again;
	} else {
		if ( mdi->shlib_info.map )
			papi_free( mdi->shlib_info.map );
		mdi->shlib_info.map = tmp;
		mdi->shlib_info.count = t_index;

		fclose( f );
	}

	return PAPI_OK;
}
