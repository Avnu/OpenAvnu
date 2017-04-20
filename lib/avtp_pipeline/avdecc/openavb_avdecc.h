/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC - Top level 1722.1 implementation
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Top level 1722.1 implementation
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_AVDECC_H
#define OPENAVB_AVDECC_H 1

#include "openavb_platform.h"

#include "openavb_avdecc_pub.h"
#include "openavb_avdecc_osal.h"
#include "openavb_types.h"
#include "openavb_trace.h"

#ifndef DATA_ALIGNMENT_ADJUSTMENT
// No Data alignment precaution needed.

// Octet based data 2 buffer macros
#define OCT_D2BMEMCP(d, s) {size_t data_size = sizeof(s); memcpy(d, s, data_size); d += data_size;} // Uses data_size variable to avoid compiler warnings
#define OCT_D2BBUFCP(d, s, len) memcpy(d, s, len); d += len;
#define OCT_D2BHTONB(d, s) *(U8 *)(d) = s; d += sizeof(s);
#define OCT_D2BHTONS(d, s) *(U16 *)(d) = htons(s); d += sizeof(s);
#define OCT_D2BHTONL(d, s) *(U32 *)(d) = htonl(s); d += sizeof(s);

// Bit based data 2 buffer macros
#define BIT_D2BHTONB(d, s, shf, inc) *(U8 *)(d) |= s << shf; d += inc;
#define BIT_D2BHTONS(d, s, shf, inc) *(U16 *)(d) |= htons((U16)(s << shf)); d += inc;
#define BIT_D2BHTONL(d, s, shf, inc) *(U32 *)(d) |= htonl((U32)(s << shf)); d += inc;


// Octet based buffer 2 data macros
#define OCT_B2DMEMCP(d, s) {size_t data_size = sizeof(d); memcpy(d, s, data_size); s += data_size;} // Uses data_size variable to avoid compiler warnings
#define OCT_B2DBUFCP(d, s, len) memcpy(d, s, len); s += len;
#define OCT_B2DNTOHB(d, s) d = *(U8 *)(s); s += sizeof(d);
#define OCT_B2DNTOHS(d, s) d = ntohs(*(U16 *)(s)); s += sizeof(d);
#define OCT_B2DNTOHL(d, s) d = ntohl(*(U32 *)(s)); s += sizeof(d);

// Bit based buffer 2 data macros
#define BIT_B2DNTOHB(d, s, msk, shf, inc) d = (*(U8 *)(s) & (U8)msk) >> shf; s += inc;
#define BIT_B2DNTOHS(d, s, msk, shf, inc) d = (ntohs(*(U16 *)(s)) & (U16)msk) >> shf; s += inc;
#define BIT_B2DNTOHL(d, s, msk, shf, inc) d = (ntohl(*(U32 *)(s)) & (U32)msk) >> shf; s += inc;

#else
// Data alignment precaution needed.

// Octet based data 2 buffer macros
#define OCT_D2BMEMCP(d, s) {size_t data_size = sizeof(s); memcpy(d, s, data_size); d += data_size;} // Uses data_size variable to avoid compiler warnings

#define OCT_D2BBUFCP(d, s, len) memcpy(d, s, len); d += len;

//#define OCT_D2BHTONB(d, s) *(U8 *)(d) = s; d += sizeof(s);
#define OCT_D2BHTONB(d, s)						\
{												\
memcpy(d, &s, sizeof(U8));						\
d += sizeof(U8);								\
}

//#define OCT_D2BHTONS(d, s) *(U16 *)(d) = htons(s); d += sizeof(s);
#define OCT_D2BHTONS(d, s)						\
{												\
U16 sAlign16 = s;								\
sAlign16 = htons(sAlign16);						\
memcpy(d, &sAlign16, sizeof(U16));				\
d += sizeof(U16);								\
}

//#define OCT_D2BHTONL(d, s) *(U32 *)(d) = htonl(s); d += sizeof(s);
#define OCT_D2BHTONL(d, s)						\
{												\
U32 sAlign32 = s;								\
sAlign32 = htonl(sAlign32);						\
memcpy(d, &sAlign32, sizeof(U32));				\
d += sizeof(U32);								\
}

// Bit based data 2 buffer macros
//#define BIT_D2BHTONB(d, s, shf, inc) *(U8 *)(d) |= s << shf; d += inc;
#define BIT_D2BHTONB(d, s, shf, inc)			\
{												\
U8 sAlign8 = s << shf;							\
U8 dAlign8;										\
memcpy(&dAlign8, d, sizeof(U8));				\
dAlign8 |= sAlign8;								\
memcpy(d, &dAlign8, sizeof(U8));				\
d += inc;										\
}

//#define BIT_D2BHTONS(d, s, shf, inc) *(U16 *)(d) |= htons((U16)(s << shf)); d += inc;
#define BIT_D2BHTONS(d, s, shf, inc)			\
{												\
U16 sAlign16 = htons((U16)(s << shf));			\
U16 dAlign16;									\
memcpy(&dAlign16, d, sizeof(U16));				\
dAlign16 |= sAlign16;							\
memcpy(d, &dAlign16, sizeof(U16));				\
d += inc;										\
}

//#define BIT_D2BHTONL(d, s, shf, inc) *(U32 *)(d) |= htonl((U32)(s << shf)); d += inc;
#define BIT_D2BHTONL(d, s, shf, inc)			\
{												\
U32 sAlign32 = htonl((U32)(s << shf));			\
U32 dAlign32;									\
memcpy(&dAlign32, d, sizeof(U32));				\
dAlign32 |= sAlign32;							\
memcpy(d, &dAlign32, sizeof(U32));				\
d += inc;										\
}


// Octet based buffer 2 data macros
#define OCT_B2DMEMCP(d, s) {size_t data_size = sizeof(d); memcpy(d, s, data_size); s += data_size;} // Uses data_size variable to avoid compiler warnings

#define OCT_B2DBUFCP(d, s, len) memcpy(d, s, len); s += len;

//#define OCT_B2DNTOHB(d, s) d = *(U8 *)(s); s += sizeof(d);
#define OCT_B2DNTOHB(d, s)						\
{												\
memcpy(&d, s, sizeof(U8));						\
s += sizeof(U8);								\
}

//#define OCT_B2DNTOHS(d, s) d = ntohs(*(U16 *)(s)); s += sizeof(d);
#define OCT_B2DNTOHS(d, s)						\
{												\
U16 sAlign16;									\
memcpy(&sAlign16, s, sizeof(U16));				\
sAlign16 = ntohs(sAlign16);						\
memcpy(&d, &sAlign16, sizeof(U16));				\
s += sizeof(U16);								\
}

//#define OCT_B2DNTOHL(d, s) d = ntohl(*(U32 *)(s)); s += sizeof(d);
#define OCT_B2DNTOHL(d, s)						\
{												\
U32 sAlign32;									\
memcpy(&sAlign32, s, sizeof(U32));				\
sAlign32 = ntohl(sAlign32);						\
memcpy(&d, &sAlign32, sizeof(U32));				\
s += sizeof(U32);								\
}

// Bit based buffer 2 data macros
//#define BIT_B2DNTOHB(d, s, msk, shf, inc) d = (*(U8 *)(s) & (U8)msk) >> shf; s += inc;
#define BIT_B2DNTOHB(d, s, msk, shf, inc)		\
{												\
U8 sAlign8;										\
memcpy(&sAlign8, s, sizeof(U8));				\
sAlign8 = (sAlign8 & (U8)msk) >>shf;			\
memcpy(&d, &sAlign8, sizeof(U8));				\
s += inc;										\
}

//#define BIT_B2DNTOHS(d, s, msk, shf, inc) d = (ntohs(*(U16 *)(s)) & (U16)msk) >> shf; s += inc;
#define BIT_B2DNTOHS(d, s, msk, shf, inc)		\
{												\
U16 sAlign16;									\
memcpy(&sAlign16, s, sizeof(U16));				\
sAlign16 = (ntohs(sAlign16) & (U16)msk) >> shf;	\
memcpy(&d, &sAlign16, sizeof(U16));				\
s += inc;										\
}

//#define BIT_B2DNTOHL(d, s, msk, shf, inc) d = (ntohl(*(U32 *)(s)) & (U32)msk) >> shf; s += inc;
#define BIT_B2DNTOHL(d, s, msk, shf, inc)		\
{												\
U32 sAlign32;									\
memcpy(&sAlign32, s, sizeof(U32));				\
sAlign32 = (ntohs(sAlign32) & (U32)msk) >> shf;	\
memcpy(&d, &sAlign32, sizeof(U32));				\
s += inc;										\
}

#endif 

#endif // OPENAVB_AVDECC_H
