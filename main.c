#include "crc16speed.h"
#include "crc64speed.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <sys\timeb.h> 
#include <Windows.h>
#include <stdint.h> 
#include <intrin.h>

int gettimeofday(struct timeval* tp, struct timezone* tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}

static inline uint64_t rdtsc() {
    return __rdtsc();
}



#define ftello _ftelli64
#elif __linux__
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

/* GCC 4.8 on Linux is dumb */
#ifndef ftello
extern off_t ftello(FILE* stream);
#endif

static inline uint64_t rdtsc() {
    unsigned int lo = 0, hi = 0;

    /* ask for something that can't be executed out-of-order
     * to force the next rdtsc to not get re-ordered. */
    __sync_synchronize();
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}


#elif __ANDROID__

#elif __APPLE__

#endif

#ifdef _MSC_VER             // Compiler: Microsoft Visual Studio

#ifdef _M_IX86                      // Processor: x86

inline uint64_t clockCycleCount()
{
	uint64_t c;
	__asm {
		cpuid       // serialize processor
		rdtsc       // read time stamp counter
		mov dword ptr[c + 0], eax
		mov dword ptr[c + 4], edx
	}
	return c;
}

#elif defined(_M_X64)               // Processor: x64

extern unsigned __int64 __rdtsc();
#pragma intrinsic(__rdtsc)
inline uint64_t clockCycleCount()
{
	return __rdtsc();
}

#endif

#endif


typedef uint64_t (*fns64)(uint64_t, const void *, const uint64_t);
typedef uint16_t (*fns16)(uint16_t crc, const void* in_data, uint64_t len);

static long long ustime(void) {
	struct timeval tv;
	long long ust;

	gettimeofday(&tv, NULL);
	ust = ((long long)tv.tv_sec) * 1e6;
	ust += tv.tv_usec;
	return ust;
}



int main(int argc, char *argv[]) {
    crc64speed_init();
    crc16speed_init();

    char li[] =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
        "enim ad minim veniam, quis nostrud exercitation ullamco laboris "
        "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat "
        "nulla pariatur. Excepteur sint occaecat cupidatat non proident, "
        "sunt in culpa qui officia deserunt mollit anim id est laborum.";

    if (argc == 1) {
        /* REGULAR CHECK VALUES */
        printf("[64calcu]: e9c6d914c4b8d9ca == %016" PRIx64 "\n",
               (uint64_t)crc64(0, "123456789", 9));
        printf("[64looku]: e9c6d914c4b8d9ca == %016" PRIx64 "\n",
               (uint64_t)crc64_lookup(0, "123456789", 9));
        printf("[64speed]: e9c6d914c4b8d9ca == %016" PRIx64 "\n",
               (uint64_t)crc64speed(0, "123456789", 9));
        printf("[16calcu]: 31c3 == %04" PRIx64 "\n",
               (uint64_t)crc16(0, "123456789", 9));
        printf("[16looku]: 31c3 == %04" PRIx64 "\n",
               (uint64_t)crc16_lookup(0, "123456789", 9));
        printf("[16speed]: 31c3 == %04" PRIx64 "\n",
               (uint64_t)crc16speed(0, "123456789", 9));

        /* LOREM IPSUM */
        printf("[64calcu]: c7794709e69683b3 == %016" PRIx64 "\n",
               (uint64_t)crc64(0, li, sizeof(li)));
        printf("[64looku]: c7794709e69683b3 == %016" PRIx64 "\n",
               (uint64_t)crc64_lookup(0, li, sizeof(li)));
        printf("[64speed]: c7794709e69683b3 == %016" PRIx64 "\n",
               (uint64_t)crc64speed(0, li, sizeof(li)));
        printf("[16calcu]: 4b20 == %04" PRIx64 "\n",
               (uint64_t)crc16(0, li, sizeof(li)));
        printf("[16looku]: 4b20 == %04" PRIx64 "\n",
               (uint64_t)crc16_lookup(0, li, sizeof(li)));
        printf("[16speed]: 4b20 == %04" PRIx64 "\n",
               (uint64_t)crc16speed(0, li, sizeof(li)));

        return 0;
    }

    char *filename = argv[1];
    FILE* fp = NULL;
    fopen_s(&fp, filename, "r");

    if (fp == NULL)
    {
        perror("Can't open file");
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("Can't find file length");
        return 1;
    }

    long long sz = ftello(fp);
    rewind(fp);
    char *contents = malloc(sz); /* potentially very big */

    if (fread(contents, sz, 1, fp) != 1) {
        free(contents);
        perror("Could not read entire file");
        return 1;
    }

    fclose(fp);

    fns64 compares64[] = { crc64,      crc64_lookup,      crc64speed };
    fns16 compares16[] = { crc16,      crc16_lookup,      crc16speed };
 
    size_t cc64 = sizeof(compares64) / sizeof(*compares64);
    size_t cc16 = sizeof(compares16) / sizeof(*compares16); /* compare count */
    char *names[] = {"crc64 (no table)", "crc64 (lookup table)", "crc64speed",
                     "crc16 (no table)", "crc16 (lookup table)", "crc16speed"};
    bool is16[] = {false, false, false, true, true, true};

    double size_mb = sz / 1024.0 / 1024.0;
    printf("Comparing CRCs against %0.2lf MB file...\n\n", size_mb);

    bool error = false;
    uint64_t accum_result = 0;
    for (size_t i = 0; i < cc64 + cc16; i++) {
        if (is16[i]) {
            crc16speed_cache_table();
			/* prime the code path with a dummy untimed call */
			compares16[i - cc64](0, li, sizeof(li));
        } else {
            crc64speed_cache_table();
			/* prime the code path with a dummy untimed call */
			compares16[i](0, li, sizeof(li));
        }

        

        long long start = ustime();
        uint64_t start_c = rdtsc();
        uint64_t result = 0;
        if (is16[i]) {
			result = compares16[i - cc64](0, contents, sz);
        } else {
			result = compares64[i](0, contents, sz);
        }

        uint64_t stop_c = rdtsc();
        long long end = ustime();

        /* Our test type returns 64 bits, but CRC16 only returns
         * 16 bits, so let's ignore any upper-bit garbage. */
        if (is16[i]) {
            result &= 0xffff;
        }

        double total_time_seconds = (end - start) / 1e6;
        double speed = size_mb / total_time_seconds; /* MB per second */
        double cycles = (double)(stop_c - start_c) / sz;

        if (argc > 2) { /* easier parsing for comparisons */
            printf("%016" PRIx64 ":%lf\n", result, speed);
        } else { /* boring human readable results */
            printf("%s\n", names[i]);
            printf("CRC = %016" PRIx64 "\n", result);
            printf("%lf seconds at %0.2f MB/s (%0.2f CPU cycles per byte)\n",
                   total_time_seconds, speed, cycles);
        }

        /* We test outputs in pairs, so compare every 3 results for equality. */
        if (i % 3 == 0) {
            accum_result = result;
        } else if (accum_result != result) {
            printf("ERROR: CRC results don't match! (%016" PRIx64
                   " vs. %016" PRIx64 ")\n",
                   accum_result, result);
            error = true;
        }

        printf("\n");
        fflush(stdout);
    }

    free(contents);

    return error;
}
