#ifndef MAAP_PLATFORM_H
#define MAAP_PLATFORM_H

#if defined(__linux__)

#include <endian.h>
#include <time.h>

#define OS_TIME_TYPE struct timespec

#elif defined(__APPLE__)

#include <libkern/OSByteOrder.h>
#include <sys/time.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)

#define ETH_ALEN 6

#define OS_TIME_TYPE struct timeval

#elif defined(_WIN32)

#include <Winsock2.h>

#define htobe16(x) htons(x)
#define be16toh(x) ntohs(x)
#define htobe64(x) ((htonl(1) == 1) ? x : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define be64toh(x) ((ntohl(1) == 1) ? x : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

#define OS_TIME_TYPE struct timeval

#else

#error Please create the platform support definitions for this platform

#endif


#define HTOBE16(x) htobe16(x)
#define BE16TOH(x) be16toh(x)
#define HTOBE64(x) htobe64(x)
#define BE64TOH(x) be64toh(x)


#endif
