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

#include <iostream>
#include <linux_hal_generic.hpp>
#include <linux_hal_generic_tsprivate.hpp>
#include <errno.h>


#include <ctime>
#include <chrono>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// For debug logging to a file
#include <fstream>

using namespace std::chrono;

#include <unistd.h>
#include <fcntl.h>

class AGpio
{
   public:
      enum Direction {kIn, kOut};
      enum Binary {kLow = 0, kHigh};

   public:
      AGpio(int gpioPinNumber, Direction inout) :
       fGpioPinNumber(gpioPinNumber),
       fDirection(inout),
       fIsSetup(false)
      {
      }

      ~AGpio()
      {
         if (fIsSetup)
         {
            int unexportfd = open("/sys/class/gpio/unexport", O_WRONLY|O_SYNC);
            if (unexportfd < 0)
            {
               GPTP_LOG_ERROR("Can't open GPIO unexport device.");
            }

            std::string toWrite = std::to_string(fGpioPinNumber);
            if (write(unexportfd, toWrite.c_str(), toWrite.length()) < 0)
            {
               GPTP_LOG_ERROR("Can't write to GPIO unexport device.");
            }

            if (close(unexportfd) < 0)
            {
               GPTP_LOG_ERROR("Can't close GPIO unexport device.");
            }
         }
      }

   public:
      void Output(Binary value)
      {
         if (!fIsSetup)
         {
            Setup();
         }
         std::string valueDevice = std::string("/sys/class/gpio/gpio") +
          std::to_string(fGpioPinNumber) + "/value";
         int valuefd = open(valueDevice.c_str(), O_WRONLY|O_SYNC);
         if (valuefd < 0)
         {
            GPTP_LOG_ERROR("Can't open the GPIO value device.");
         }
         else
         {
            std::string toWrite = std::to_string(value);
            if (write(valuefd, toWrite.c_str(), toWrite.length()) < 0)
            {
               GPTP_LOG_ERROR("Can't write to the GPIO value device.");
            }

            if (close(valuefd) < 0)
            {
               GPTP_LOG_ERROR("Can't close the GPIO value device.");
            }
         }
      }

   private:
      void Setup()
      {
         std::string toWrite;
         int exportfd = open("/sys/class/gpio/export",  O_WRONLY|O_SYNC);
         if (exportfd < 0)
         {
            GPTP_LOG_ERROR("Can't open the GPIO export device.");
         }
         else
         {
            bool writeOk = false;
            toWrite = std::to_string(fGpioPinNumber);
            if (write(exportfd, toWrite.c_str(), toWrite.length()) < 0)
            {
               GPTP_LOG_ERROR("Can't write to the GPIO export device.");
            }
            else
            {
               writeOk = true;
            }

            if (close(exportfd) < 0)
            {
               GPTP_LOG_ERROR("Can't close the GPIO export device.");
            }

            if (writeOk)
            {
               std::string directionDevicePath ="/sys/class/gpio/gpio" +
                std::to_string(fGpioPinNumber) + "/direction";
               int directionfd = open(directionDevicePath.c_str(), O_WRONLY|O_SYNC);
               if (directionfd < 0)
               {
                  GPTP_LOG_ERROR("Can't open the GPIO direction device.");
               }
               else
               {
                  toWrite = kIn == fDirection ? "in" : "out";
                  if (write(directionfd, toWrite.c_str(), toWrite.length()) < 0)
                  {
                     GPTP_LOG_ERROR("Can't write to the GPIO direction device.");
                  }
                  else
                  {
                     fIsSetup = true;
                  }
               }

               if (close(directionfd) < 0)
               {
                  GPTP_LOG_ERROR("Can't close GPIO direction device.");
               }
            }
         }
      }

   private:
      int fGpioPinNumber;
      Direction fDirection;
      bool fIsSetup;
};

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
       fPinNumber(piPinNumber),
       fInterval(500),
       fTimestamper(nullptr),
       fLastTimestamp(0),
       fRate(kOnePerSecond),
       fGpioPin(piPinNumber, AGpio::kOut)
      {
         fDebugLogFile.open("gpiopinger.txt", std::ofstream::out);
      }

      ~AGPioPinger()
      {
         fDebugLogFile.close();
      }

      void TimeStamper(LinuxTimestamperGeneric* ts)
      {
         fTimestamper = ts;
      }

      bool Start(time_t timeToStart, PulseRate rate = kOnePerSecond)
      {
         using std::chrono::system_clock;
         time_t nowTs = system_clock::to_time_t(system_clock::now());
         while (nowTs < timeToStart)
         {
            usleep(1000000);
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
         fRate = rate;
         switch (rate)
         {
            case kOnePerSecond:
            fInterval = 500000;
            break;

            case kTenPerSecond:
            fInterval = 50000;
            break;

            case kOneHundredPerSecond:
            fInterval = 5000;
            break;

            case kOneThousandPerSecond:
            fInterval = 500;
            break;

            default:
            fInterval = 500;
            break;
         }

         while (fKeepGoing)
         {
            // Adjust the pulse rate to be aliggned with the MC adjusted timestamp
            AdjustToTimestamp();

            // Adjust the pulse rate to be aligned with the system clock
            //AdjustToSystemClock();

            ActivatePin();
         }

         // Ensure we set the pin low
         fGpioPin.Output(AGpio::kLow);

         return true;
      }

   void ActivatePin()
   {
      // high_resolution_clock::time_point now = high_resolution_clock::now();
      // auto delta = duration_cast<nanoseconds>(now - fLastActivate);
      // fDebugLogFile << duration_cast<nanoseconds>(now.time_since_epoch()).count() << "\t"
      //  << duration_cast<nanoseconds>(fLastActivate.time_since_epoch()).count() << "\t"
      //  << delta.count() << std::endl;
      // fLastActivate = now;

      // Set the GPIO pin high for interval microseconds
      fGpioPin.Output(AGpio::kHigh);
      usleep(fInterval);

      // Set the GPIO pin low for interval microseconds
      fGpioPin.Output(AGpio::kLow);
      //usleep(fInterval);
   }

   int64_t CurrentUnit() const
   {
      int64_t unit = kOneSecNano;
      switch (fRate)
      {
         case kOnePerSecond:
         unit = kOneSecNano;
         break;

         case kTenPerSecond:
         unit = kTenthSecNano;
         break;

         case kOneHundredPerSecond:
         unit = kHundrethSecNano;
         break;

         case kOneThousandPerSecond:
         unit = kThousandthSecNano;
         break;
      }
      return unit;
   }

   int64_t CalculateNextInterval()
   {
      using std::chrono::duration_cast;
      using std::chrono::high_resolution_clock;
      using std::chrono::nanoseconds;

      time_point<high_resolution_clock> now = high_resolution_clock::now();
      int64_t timestamp = duration_cast<nanoseconds>(now.time_since_epoch()).count();
      //timestamp += fTimestamper->MasterOffset();
      return CalculateNextInterval(timestamp);
   }

   int64_t CalculateNextInterval(int64_t timeStamp)
   {
      int64_t unit = CurrentUnit();
      fNextInterval = ((timeStamp / unit) + 1) * unit;
      return fNextInterval;
   }

   int64_t CalculateSleepInterval(int64_t timeStamp) const
   {
      // !!! This will need to be adjusted when dealing with fInterval that are
      // less than kMicroSlopFactor
      int64_t delta = fNextInterval < timeStamp
       ? fInterval - kMicroSlopFactor : (fNextInterval - timeStamp) / 1000;
      // std::cout << "**********************timeStamp:" << timeStamp
      //  << "   nextInterval:" << nextInterval
      //  << "   delta:" << delta << std::endl;
      return delta;
   }

   bool AtIntervalBoundary(int64_t timeStamp)
   {
      int64_t delta = CalculateSleepInterval(timeStamp);
      if (delta > fLastDelta || 0 == delta || delta < 0)
      {
         GPTP_LOG_INFO("-------------------------AtIntervalBoundary  timeStamp %" PRIu64 "  delta %" PRIu64 "  fLastDelta %" PRIu64, timeStamp, delta, fLastDelta);
      }
      bool atBoundary = delta <= 0 || fNextInterval < timeStamp;
      fLastDelta = delta;
      return atBoundary;
   }

   int64_t ConvertToLocal(int64_t masterTimeToConvert)
   {
      int64_t localTime = 0;
      IEEE1588Port *port = fTimestamper->Port();
      if (port != nullptr)
      {
         localTime = port->ConvertToLocalTime(masterTimeToConvert);
      }
      else
      {
         GPTP_LOG_ERROR("AGPioPinger  port is null.");
      }
      return localTime;
   }

   void SleepWithLocalClock()
   {
      using std::chrono::duration_cast;
      using std::chrono::high_resolution_clock;
      using std::chrono::nanoseconds;

      time_point<high_resolution_clock> now = high_resolution_clock::now();
      int64_t timestamp = duration_cast<nanoseconds>(now.time_since_epoch()).count();

      CalculateNextInterval();
      int64_t sleepInterval = CalculateSleepInterval(timestamp);
      usleep(sleepInterval > 0 ? sleepInterval : fInterval);
   }

   void AdjustToTimestamp()
   {
      IEEE1588Port *port = fTimestamper->Port();
      if (fTimestamper != nullptr && port != nullptr)
      {
         PTPMessageFollowUp fup = port->getLastFollowUp();
         //int64_t timeStamp = int64_t(fTimestamper->AdjustedTime());
         int64_t timeStamp = TIMESTAMP_TO_NS(fup.getPreciseOriginTimestamp());
         GPTP_LOG_INFO("-------------------------timeStamp:             %" PRIu64, timeStamp);
         GPTP_LOG_INFO("-------------------------fLastTimestamp:        %" PRIu64, fLastTimestamp);
         if (timeStamp == fLastTimestamp)
         {
            //std::cout << "**********************timeStamp:" << timeStamp << "   fLastTimestamp:" << fLastTimestamp << std::endl;
            //usleep(fInterval);
            // Calculate the next interval with respect to local time
            SleepWithLocalClock();
         }
         else
         {
            //usleep(CalculateSleepInterval(timeStamp));

            // Calculate when the next interval(second, 1/10 s, etc) should
            // occur in master time - it sets member fNextInterval
            CalculateNextInterval(timeStamp);

            // Convert fNextInterval to local time and subtract a slop factor - this
            // is how long we sleep
            int64_t adjustedNextSecond = ConvertToLocal(fNextInterval);

            GPTP_LOG_INFO("-------------------------nextInterval:          %" PRIu64, fNextInterval);
            GPTP_LOG_INFO("-------------------------adjustedNextSecond:    %" PRIu64, adjustedNextSecond);
            if (0 == adjustedNextSecond)
            {
               SleepWithLocalClock();
            }
            else
            {
               fNextInterval = adjustedNextSecond - kNanoSlopFactor;

               //int64_t sleepInterval = adjustedNextSecond - int64_t(100000000);
               // FrequencyRatio offsetFromMaster = fTimestamper->MasterOffset();
               // int64_t adjustedNextSecond = int64_t(fNextInterval + offsetFromMaster);
               //adjustedNextSecond -= int64_t(100000000);
               int64_t sleepInterval = CalculateSleepInterval(ConvertToLocal(timeStamp));

               high_resolution_clock::time_point now = high_resolution_clock::now();

               //GPTP_LOG_INFO("-------------------------offsetFromMaster: %Le", offsetFromMaster);
               //GPTP_LOG_INFO("-------------------------timeStamp: %" PRIu64, timeStamp);
               GPTP_LOG_INFO("---------------------ConvertToLocal(timeStamp): %" PRIu64, ConvertToLocal(timeStamp));
               GPTP_LOG_INFO("-------------------------nextInterval:          %" PRIu64, fNextInterval);
               GPTP_LOG_INFO("-------------------------now(1):                %" PRIu64, duration_cast<nanoseconds>(now.time_since_epoch()).count());
               GPTP_LOG_INFO("-------------------------sleepInterval:         %" PRIu64, sleepInterval);
               //GPTP_LOG_INFO("-------------------------OldSleepInterval: %" PRIu64, CalculateSleepInterval(timeStamp));

               // sleep until the the calculated wake up time
               if (sleepInterval > 0)
               {
                  usleep(sleepInterval);
               }

               GPTP_LOG_INFO("-------------------------WAKE UP");
               // Poll the master time and see when the time wraps on the interval
               // (second) boundary
               //timeStamp = int64_t(fTimestamper->AdjustedTime());
               now = high_resolution_clock::now();

               fNextInterval = adjustedNextSecond;

               //fDebugLogFile << duration_cast<nanoseconds>(now.time_since_epoch()).count()

               while (!AtIntervalBoundary(duration_cast<nanoseconds>(now.time_since_epoch()).count()))
               {
                  //timeStamp = int64_t(fTimestamper->AdjustedTime());
                  now = high_resolution_clock::now();
               }
               GPTP_LOG_INFO("-------------------------timeStamp:       %" PRIu64, timeStamp);
               GPTP_LOG_INFO("-------------------------now(2):          %" PRIu64, duration_cast<nanoseconds>(now.time_since_epoch()).count());

               // raise the GPIO.
               // then sleep for short duration
               // lower the GPIO
               // repeat.
            }
         }

         fLastTimestamp = timeStamp;
      }
      else
      {
         std::cout << "Warning PPS has no timestamper." << std::endl;

         // So that the led will be "off" for fInterval of time
         usleep(fInterval);
      }
   }

   void AdjustToSystemClock()
   {
      using std::chrono::duration_cast;
      using std::chrono::high_resolution_clock;
      using std::chrono::seconds;
      using std::chrono::microseconds;
      using std::chrono::time_point;

      time_point<high_resolution_clock> last = high_resolution_clock::now();
      time_point<high_resolution_clock> cur;

      while (duration_cast<microseconds>(cur - last).count() < fInterval)
      {
         cur = high_resolution_clock::now();
      }
   }

   private:
      bool fKeepGoing;
      int fPinNumber;
      int fInterval;
      LinuxTimestamperGeneric* fTimestamper;
      int64_t fLastTimestamp;
      PulseRate fRate;
      int64_t fLastDelta;
      std::ofstream fDebugLogFile;
      std::chrono::high_resolution_clock::time_point fLastActivate;
      int64_t fNextInterval;
      AGpio fGpioPin;

      const int64_t kOneSecNano = 1000000000;
      const int64_t kTenthSecNano = 100000000;
      const int64_t kHundrethSecNano = 10000000;
      const int64_t kThousandthSecNano = 1000000;

      const int64_t kNanoSlopFactor = 125000000;
      const int64_t kMicroSlopFactor = kNanoSlopFactor / 1000;
};

// Create a rpi GPIO pinger for GPIO physical pin 11
// NOTE: physical_pin_number_11 == GPIO17 == wiringPi_pin_0
static AGPioPinger sPinger(17);

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
   sPinger.TimeStamper(this);
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
