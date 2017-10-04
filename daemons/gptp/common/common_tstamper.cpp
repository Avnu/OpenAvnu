/*
 * Copyright (C) 2017 Art and Logic.
 *
 * Implementation for the CommonTimestamper class.
 *
 *
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, 
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright 
 *      notice, this list of conditions and the following disclaimer in the 
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Art and Logic nor the names of its 
 *      contributors may be used to endorse or promote products derived from 
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "common_tstamper.hpp"
#include "avbts_message.hpp"
#include "ether_port.hpp"


 /**
 * @brief Default constructor. Sets version to zero.
 */
CommonTimestamper::CommonTimestamper()
{
	version = 0;
}

 /**
 * @brief Deletes HWtimestamper object
 */
CommonTimestamper::~CommonTimestamper()
{

}

bool CommonTimestamper::HWTimestamper_init(InterfaceLabel *iface_label,
 OSNetworkInterface *iface, EtherPort *port)
{
	return true;
}

/**
 * @brief Reset the hardware timestamp unit
 * @return void
 */
void CommonTimestamper::HWTimestamper_reset(void)
{
}

/**
 * @brief  This method is called before the object is de-allocated.
 * @return void
 */
void CommonTimestamper::HWTimestamper_final(void)
{
}

/**
 * @brief  Adjusts the hardware clock frequency
 * @param  frequency_offset Frequency offset
 * @return false
 */
bool CommonTimestamper::HWTimestamper_adjclockrate(float frequency_offset) const
{
	return false;
}

/**
 * @brief  Adjusts the hardware clock phase
 * @param  phase_adjust Phase offset
 * @return false
 */
bool CommonTimestamper::HWTimestamper_adjclockphase(int64_t phase_adjust)
{
	return false;
}

/**
 * @brief  Gets a string with the error from the hardware timestamp block
 * @param  msg [out] String error
 * @return void
 * @todo There is no current implementation for this method.
 */
void CommonTimestamper::HWTimestamper_get_extderror(char *msg) const
{
	*msg = '\0';
}

/**
 * @brief  Starts the PPS (pulse per second) interface
 * @return false
 */
bool CommonTimestamper::HWTimestamper_PPS_start()
{
	return false;
}

/**
 * @brief  Stops the PPS (pulse per second) interface
 * @return true
 */
bool CommonTimestamper::HWTimestamper_PPS_stop() { return true; };

/**
 * @brief  Gets the HWTimestamper version
 * @return version (signed integer)
 */
int CommonTimestamper::getVersion() const
{
	return version;
}

void CommonTimestamper::MasterOffset(FrequencyRatio offset)
{
	fMasterOffset = offset;
}

FrequencyRatio CommonTimestamper::MasterOffset() const
{
	return fMasterOffset;
}

void CommonTimestamper::AdjustedTime(FrequencyRatio t)
{
	fAdjustedTime = t;
}

FrequencyRatio CommonTimestamper::AdjustedTime() const
{
	return fAdjustedTime;
}
