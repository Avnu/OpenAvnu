/*
 * Copyright (C) 2017 Art and Logic.
 *
 * An address registration api listener socket that listens on port 9001
 * for address api messages.
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
#include "addressregisterlistener.hpp"

std::mutex gAddressApiMutex;

OSThreadExitCode runListener(void *arg)
{
	AAddressRegisterListener* listener = static_cast<AAddressRegisterListener*>(arg);
	return listener->Open(gAddressApiMutex) ? osthread_ok : osthread_error;
}

bool AAddressRegisterListener::Start()
{
   bool ok = true;
   fThread = fThreadFactory->create();
   if (!fThread->start(runListener, static_cast<void *>(this)))
   {
      GPTP_LOG_ERROR("Error address register thread");
      ok = false;
   }
	return ok;
}

void AAddressRegisterListener::Stop()
{
	Close();
}

void AAddressRegisterListener::ProcessData(ARawPacket& data)
{
	AAddressMessage msg(data);
	//msg.DebugLog();
	const int kKind = msg.Type();
	if (kKind != AddressApiAdd && kKind != AddressApiDelete)
	{
		std::string msg = "Invalid address api message type "
		 + std::to_string(kKind) + ".";
		//std::cerr << msg << std::endl;
		GPTP_LOG_ERROR(msg.c_str());
		throw(std::runtime_error(msg));
	}

	// Use an associative collection to map integral message types to
	// corresponding class instances. Processing of the data is then
	// delegated to each class instance.
	static std::map<int, std::shared_ptr<AAddressMessage> > mapper = {
		{AddressApiAdd, std::make_shared<AAddAddressMessage>()},
		{AddressApiDelete, std::make_shared<ADeleteAddressMessage>()}
	};
	mapper[kKind]->Process(msg, fPtpPort);
}
