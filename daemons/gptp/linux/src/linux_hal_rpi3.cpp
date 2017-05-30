/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <linux_hal_generic.hpp>
#include <linux_hal_generic_tsprivate.hpp>
#include <errno.h>


#include <ctime>
#include <chrono>
#include <wiringPi.h>

class AGPioPinger
{
   public:
      enum PulseRate
      {
         kOnePerSecond,
         kTenPerSecond,
         kOneHundredPerSecond,
         kOneThousandPerSecond
      };

   public:
      AGPioPinger(int piPinNumber) :
       fKeepGoing(true),
       fPinNumber(piPinNumber)
      {
         wiringPiSetup();
         pinMode(piPinNumber, OUTPUT);
      }

      ~AGPioPinger()
      {
         // Ensure we set the pin low
         digitalWrite(fPinNumber, LOW) ;
      }

      bool Start(time_t timeToStart, PulseRate rate = kOnePerSecond)
      {
         using std::chrono::system_clock;
         time_t nowTs = system_clock::to_time_t(system_clock::now());
         while (nowTs < timeToStart)
         {
            delay(1000);
            nowTs = system_clock::to_time_t(system_clock::now());
         }
         fKeepGoing = true;
         return StartGPIOLoop(rate);
      }

      void Stop()
      {
         fKeepGoing = false;
      }

   private:
      bool StartGPIOLoop(PulseRate rate)
      {
         int interval = 500;
         switch (rate)
         {
            case kOnePerSecond:
            interval = 500000;
            break;

            case kTenPerSecond:
            interval = 50000;
            break;

            case kOneHundredPerSecond:
            interval = 5000;
            break;

            case kOneThousandPerSecond:
            interval = 500;
            break;

            default:
            interval = 500000;
            break;
         }

         while (fKeepGoing)
         {
            // Set the GPIO pin high for interval microseconds
            digitalWrite(fPinNumber, HIGH);
            delayMicroseconds(interval);

            // Set the GPIO pin low for interval microseconds
            digitalWrite(fPinNumber, LOW) ;
            delayMicroseconds(interval);
         }
         return true;
      }

   private:
      bool fKeepGoing;
      int fPinNumber;
};

// Create a rpi GPIO pinger for GPIO physical pin 15
// NOTE: physical_pin_number_15 == GPIO22 == wiringPi_pin_3
static AGPioPinger sPinger(3);

OSThreadExitCode runPinger(void *arg)
{
   using std::chrono::system_clock;

   AGPioPinger *p = static_cast<AGPioPinger *>(arg);

   system_clock::time_point now = system_clock::now();
 
   // Edges must be second aligned
   return p->Start(system_clock::to_time_t(now), AGPioPinger::kOnePerSecond)
    ? osthread_ok : osthread_error;
}

bool LinuxTimestamperGeneric::HWTimestamper_PPS_start() 
{
   bool ok = true;
   fPulseThread = fPulseThreadFactory->create();
   if (!fPulseThread->start(runPinger, static_cast<void *>(&sPinger)))
   {
      GPTP_LOG_ERROR("Error creating port link thread");
      ok = false;
   }
	return ok;
}

bool LinuxTimestamperGeneric::HWTimestamper_PPS_stop() 
{
   sPinger.Stop();
	return true;
}
