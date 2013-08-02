#include <Winsock2.h>
#include <stdint.h>

errno_t PLAT_strncpy( char *dest, const char *src, rsize_t max ) {
	return strncpy_s( dest, max+1, src, _TRUNCATE );
}

uint16_t PLAT_htons( uint16_t s ) {
	return htons( s );
}

uint32_t PLAT_htonl( uint32_t l ) {
	return htonl( l );
}

uint16_t PLAT_ntohs( uint16_t s ) {
	return ntohs( s );
}

uint32_t PLAT_ntohl( uint32_t l ) {
	return ntohl( l );
}