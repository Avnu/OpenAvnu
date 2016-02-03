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

/**
 * Provides the gptp interface for
 * the iniParser external module
 */
class GptpIniParser
{
    public:

        /**
         * Container with the information to get from the .ini file
         */
        typedef struct
        {
            /*ptp data set*/
            unsigned char priority1;

            /*port data set*/
            unsigned int announceReceiptTimeout;
            unsigned int syncReceiptTimeout;
            unsigned int syncReceiptThresh;		//!< Number of wrong sync messages that will trigger a switch to master
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
         * @brief  Reads the parser Error value
         * @param  void
         * @return Parser Error
         */
        int parserError(void);

        /**
         * @brief  Reads priority1 config value
         * @param  void
         * @return priority1
         */
        unsigned char getPriority1(void)
        {
            return _config.priority1;
        }

        /**
         * @brief  Reads the announceReceiptTimeout configuration value
         * @param  void
         * @return announceRecepitTimeout value from .ini file
         */
        unsigned int getAnnounceReceiptTimeout(void)
        {
            return _config.announceReceiptTimeout;
        }

        /**
         * @brief  Reads the syncRecepitTimeout configuration value
         * @param  void
         * @return syncRecepitTimeout value from the .ini file
         */
        unsigned int getSyncReceiptTimeout(void)
        {
            return _config.syncReceiptTimeout;
        }

        /**
         * @brief  Reads the TX PHY DELAY GB value from the configuration file
         * @param  void
         * @return gb_tx_phy_delay value from the .ini file
         */
        int getPhyDelayGbTx(void)
        {
            return _config.phyDelay.gb_tx_phy_delay;
        }

        /**
         * @brief  Reads the RX PHY DELAY GB value from the configuration file
         * @param  void
         * @return gb_rx_phy_delay value from the .ini file
         */
        int getPhyDelayGbRx(void)
        {
            return _config.phyDelay.gb_rx_phy_delay;
        }

        /**
         * @brief  Reads the TX PHY DELAY MB value from the configuration file
         * @param  void
         * @return mb_tx_phy_delay value from the .ini file
         */
        int getPhyDelayMbTx(void)
        {
            return _config.phyDelay.mb_tx_phy_delay;
        }

        /**
         * @brief  Reads the RX PHY DELAY MB valye from the configuration file
         * @param  void
         * @return mb_rx_phy_delay value from the .ini file
         */
        int getPhyDelayMbRx(void)
        {
            return _config.phyDelay.mb_rx_phy_delay;
        }

        /**
         * @brief  Reads the neighbohr propagation delay threshold from the configuration file
         * @param  void
         * @return neighborPropDelayThresh value from the .ini file
         */
        int64_t getNeighborPropDelayThresh(void)
        {
            return _config.neighborPropDelayThresh;
        }

        /**
         * @brief  Reads the sync receipt threshold from the configuration file
         * @param  getSyncReceiptThresh
         * @return syncRecepitThresh value from the .ini file
         */
        unsigned int getSyncReceiptThresh(void)
        {
            return _config.syncReceiptThresh;
        }

    private:
        int _error;
        gptp_cfg_t _config;

        static int iniCallBack(void *user, const char *section, const char *name, const char *value);
        static bool parseMatch(const char *s1, const char *s2);
};

