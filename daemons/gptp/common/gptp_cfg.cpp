/*************************************************************************************************************
Copyright (c) 2015, Coveloz Consulting Ltda
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

/**
 * \file
* MODULE SUMMARY : Reads the .ini file and parses it into information
* to be used on daemon_cl
*/

#include <iostream>

/* need Microsoft version for strcasecmp() from GCC strings.h */
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include <errno.h>
#include <stdlib.h>

#include "gptp_cfg.hpp"

GptpIniParser::GptpIniParser(std::string filename)
{
    _error = ini_parse(filename.c_str(), iniCallBack, this);
}

GptpIniParser::~GptpIniParser()
{
}

/****************************************************************************/

int GptpIniParser::parserError(void)
{
    return _error;
}

/****************************************************************************/

int GptpIniParser::iniCallBack(void *user, const char *section, const char *name, const char *value)
{
    GptpIniParser *parser = (GptpIniParser*)user;
    bool valOK = false;

    if( parseMatch(section, "ptp") )
    {
        if( parseMatch(name, "priority1") )
        {
            errno = 0;
            char *pEnd;
            unsigned char p1 = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.priority1 = p1;
            }
        }
    }
    else if( parseMatch(section, "port") )
    {
        if( parseMatch(name, "announceReceiptTimeout") )
        {
            errno = 0;
            char *pEnd;
            unsigned int art = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.announceReceiptTimeout = art;
            }
        }
        else if( parseMatch(name, "syncReceiptTimeout") )
        {
            errno = 0;
            char *pEnd;
            unsigned int srt = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.syncReceiptTimeout = srt;
            }
        }
        else if( parseMatch(name, "neighborPropDelayThresh") )
        {
            errno = 0;
            char *pEnd;
            int64_t nt = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.neighborPropDelayThresh = nt;
            }
        }
        else if( parseMatch(name, "syncReceiptThresh") )
        {
            errno = 0;
            char *pEnd;
            unsigned int st = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.syncReceiptThresh = st;
            }
        }
        else if( parseMatch(name, "seqIdAsCapableThresh") )
        {
            errno = 0;
            char *pEnd;
            unsigned int sidt = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.seqIdAsCapableThresh = sidt;
            }
        }
        else if( parseMatch( name, "lostPdelayRespThresh") )
        {
            errno = 0;
            char *pEnd;
            uint16_t lostpdelayth = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0 ) {
                valOK = true;
                parser->_config.lostPdelayRespThresh = lostpdelayth;
            }
        }
    }
    else if( parseMatch(section, "eth") )
    {
        if( parseMatch(name, "phy_delay_gb_tx") )
        {
            errno = 0;
            char *pEnd;
            int phdly = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.phyDelay.gb_tx_phy_delay = phdly;
            }
        }

        else if( parseMatch(name, "phy_delay_gb_rx") )
        {
            errno = 0;
            char *pEnd;
            int phdly = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.phyDelay.gb_rx_phy_delay = phdly;
            }
        }

        else if( parseMatch(name, "phy_delay_mb_tx") )
        {
            errno = 0;
            char *pEnd;
            int phdly = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.phyDelay.mb_tx_phy_delay = phdly;
            }
        }

        else if( parseMatch(name, "phy_delay_mb_rx") )
        {
            errno = 0;
            char *pEnd;
            int phdly = strtoul(value, &pEnd, 10);
            if( *pEnd == '\0' && errno == 0) {
                valOK = true;
                parser->_config.phyDelay.mb_rx_phy_delay = phdly;
            }
        }
    }

    if(!valOK)
    {
        std::cerr << "Unrecognized configuration item: section=" << section << ", name=" << name << std::endl;
        return 0;
    }

    return 1;
}


/****************************************************************************/

bool GptpIniParser::parseMatch(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2) == 0;
}

