/*
 * Copyright (C) 2017 Art and Logic.
 *
 * An address registration api listener socket that listens on port 9001
 * for address api messages.
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
