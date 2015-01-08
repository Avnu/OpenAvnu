# Setup AVB tools for listener / talker example

_Note:_ this tutorial targets a Linux OS.

Fetch the Open-AVB repository. To ease the further steps, enter your local copy and export its path via
```bash
export AVB_PATH=$(pwd)
```
Then, you can use a terminal multiplexer like _byobu_ to start all needed AVB tools in different windows.

We assume the following PC setup:
  * PC1: ethernet eth0 for AVB connection
  * PC2: ethernet eth2 for AVB connection
  * Both ethernet cards are using the `igb_avb` driver.
    Check via `ethtool -i eth0`.
    If not, launch `sudo ./startup.sh eth0` in `kmod/igb` to load the required driver for the provided interface.
  * Both PCs can ping each other through the AVB network cards.

## PC 1: start AVB environment + talker application

Start AVB environment:

```bash
# GPTP time synchronization daemon
cd "${AVB_PATH}/daemons/gptp/linux/build/obj"
sudo ./daemon_cl eth0 -R 1

# Stream reservation daemon (m: MMRP, v: MVRP, s: MSRP)
cd "${AVB_PATH}/daemons/mrpd"
sudo ./mrpd -mvs -i eth0
```

Start AVB talker example:

```bash
cd "${AVB_PATH}/examples/simple_talker"
sudo ./simple_talker -i eth0 -t 2
```

## PC 2: start AVB environment + listener application

Start AVB environment similar to PC 1:

```bash
# GPTP time synchronization daemon (will synchronize on PC 1)
cd "${AVB_PATH}/daemons/gptp/linux/build/obj"
sudo ./daemon_cl eth2

# Stream reservation daemon (m: MMRP, v: MVRP, s: MSRP)
cd "${AVB_PATH}/daemons/mrpd"
sudo ./mrpd -mvs -i eth2
```

Start AVB listener example:

```bash
cd "${AVB_PATH}/examples/simple_listener"
sudo ./simple_listener -i eth2 -f out.wav
```

:tada:
