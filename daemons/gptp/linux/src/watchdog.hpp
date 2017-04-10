#ifndef SYSTEMDWATCHDOGHANDLER_H
#define SYSTEMDWATCHDOGHANDLER_H
#include <linux_hal_common.hpp>
#include <avbts_ostimer.hpp>


OSThreadExitCode watchdogUpdateThreadFunction(void *arg);

class SystemdWatchdogHandler
{
public:
	long unsigned int update_interval;
	long unsigned int getSystemdWatchdogInterval(int *result);
	void run_update();
	SystemdWatchdogHandler();
	virtual ~SystemdWatchdogHandler();
private:
	OSTimer *timer;
};

#endif // SYSTEMDWATCHDOGHANDLER_H
