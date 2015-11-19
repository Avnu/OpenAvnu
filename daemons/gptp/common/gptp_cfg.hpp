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

#include <string>

#include "ini.h"
#include "ieee1588.hpp"

class GptpIniParser {
    public:

        /*Public types*/
        typedef struct {
            /*ptp data set*/
            unsigned char priority1;

            /*port data set*/
            unsigned int announceReceiptTimeout;
            unsigned int syncReceiptTimeout;
            int64_t neighborPropDelayThresh;
            PortState port_state;

            /*ethernet adapter data set*/
            std::string ifname;
            struct phy_delay phyDelay;
        } gptp_cfg_t;

        /*public methods*/
        GptpIniParser(std::string ini_path);
        ~GptpIniParser();

        /**
         * @brief
         * @param  void
         * @return
         */
        int parserError(void);

        /**
         * @brief
         * @param  void
         * @return
         */
        unsigned char getPriority1(void)
        {
            return _config.priority1;
        }

        unsigned int getAnnounceReceiptTimeout(void)
        {
            return _config.announceReceiptTimeout;
        }

        unsigned int getSyncReceiptTimeout(void)
        {
            return _config.syncReceiptTimeout;
        }

        int getPhyDelayGbTx(void)
        {
            return _config.phyDelay.gb_tx_phy_delay;
        }

        int getPhyDelayGbRx(void)
        {
            return _config.phyDelay.gb_rx_phy_delay;
        }

        int getPhyDelayMbTx(void)
        {
            return _config.phyDelay.mb_tx_phy_delay;
        }

        int getPhyDelayMbRx(void)
        {
            return _config.phyDelay.mb_rx_phy_delay;
        }

        int64_t getNeighborPropDelayThresh(void)
        {
            return _config.neighborPropDelayThresh;
        }

    private:
        int _error;
        gptp_cfg_t _config;

        /**
         * @brief
         * @param  user
         * @param  section
         * @param  name
         * @param  value
         * @return
         */
        static int iniCallBack(void *user, const char *section, const char *name, const char *value);

        /**
         * @brief
         * @param  s1
         * @param  s2
         * @return
         */
        static bool parseMatch(const char *s1, const char *s2);
};

