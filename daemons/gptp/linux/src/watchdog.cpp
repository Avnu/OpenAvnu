#include "watchdog.hpp"
#include "avbts_osthread.hpp"
#include "gptp_log.hpp"
#include <systemd/sd-daemon.h>


OSThreadExitCode watchdogUpdateThreadFunction(void *arg)
{
	SystemdWatchdogHandler *watchdog = (SystemdWatchdogHandler*) arg;
	watchdog->run_update();
	return osthread_ok;
}


SystemdWatchdogHandler::SystemdWatchdogHandler()
{
	GPTP_LOG_INFO("Creating Systemd watchdog handler.");
	LinuxTimerFactory timer_factory = LinuxTimerFactory();
	timer = timer_factory.createTimer();
}

SystemdWatchdogHandler::~SystemdWatchdogHandler()
{
	//Do nothing
}

long unsigned int
SystemdWatchdogHandler::getSystemdWatchdogInterval(int *result)
{
	long unsigned int watchdog_interval; //in microseconds
	*result = sd_watchdog_enabled(0, &watchdog_interval);
	return watchdog_interval;
}

void SystemdWatchdogHandler::run_update()
{
	while(1)
	{
		GPTP_LOG_DEBUG("NOTIFYING WATCHDOG.");
		sd_notify(0, "WATCHDOG=1");
		GPTP_LOG_DEBUG("GOING TO SLEEP %lld", update_interval);
		timer->sleep(update_interval);
		GPTP_LOG_DEBUG("WATCHDOG WAKE UP");
	}
}

