/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : General header file for the AVB stack
*/

#ifndef AVB_PUB_H
#define AVB_PUB_H 1

#include "openavb_log_pub.h"

/////////////////////////////////////////////////////////
// AVB Core version related macros
// 
// These must NOT be edited for project related work
/////////////////////////////////////////////////////////
#define AVB_CORE_NAME	"AVTP Pipeline"

#define AVB_CORE_VER_MAJOR		(0)
#define AVB_CORE_VER_MINOR		(1)
#define AVB_CORE_VER_REVISION	(0)

#define AVB_CORE_VER_FULL		(AVB_CORE_VER_MAJOR << 16 | AVB_CORE_VER_MINOR << 8 | AVB_CORE_VER_REVISION)

// Standard release designations. Uncomment one AVB_RELEASE_TYPE
#define AVB_CORE_RELEASE_TYPE	"Development"
//#define AVB_CORE_RELEASE_TYPE	"Alpha"
//#define AVB_CORE_RELEASE_TYPE	"Beta"
//#define AVB_CORE_RELEASE_TYPE	"Release"

#define LOG_EAVB_CORE_VERSION() AVB_LOGF_INFO("%s: %i.%i.%i (%s)", AVB_CORE_NAME, AVB_CORE_VER_MAJOR, AVB_CORE_VER_MINOR, AVB_CORE_VER_REVISION, AVB_CORE_RELEASE_TYPE)



/////////////////////////////////////////////////////////
// AVB Project version related macros
// 
// These can be used for project solutions
/////////////////////////////////////////////////////////
#define AVB_PROJECT_NAME	"Sample AVB Solution"

#define AVB_PROJECT_VER_MAJOR		(1)
#define AVB_PROJECT_VER_MINOR		(0)
#define AVB_PROJECT_VER_REVISION	(0)

#define AVB_PROJECT_VER_FULL		(AVB_PROJECT_VER_MAJOR << 16 | AVB_PROJECT_VER_MINOR << 8 | AVB_PROJECT_VER_REVISION)

// Standard release designations. Uncomment one AVB_RELEASE_TYPE
#define AVB_PROJECT_RELEASE_TYPE	"Development"
//#define AVB_PROJECT_RELEASE_TYPE	"Alpha"
//#define AVB_PROJECT_RELEASE_TYPE	"Beta"
//#define AVB_PROJECT_RELEASE_TYPE	"Release"

#define LOG_EAVB_PROJECT_VERSION() AVB_LOGF_INFO("%s: %i.%i.%i (%s)", AVB_PROJECT_NAME, AVB_PROJECT_VER_MAJOR, AVB_PROJECT_VER_MINOR, AVB_PROJECT_VER_REVISION, AVB_PROJECT_RELEASE_TYPE)




#endif // AVB_PUB_H
