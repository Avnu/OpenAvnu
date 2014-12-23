# Setup AVB tools for listener / talker example

We assume the following PC setup:
  * PC1: ethernet eth0 for AVB connection
  * PC2: ethernet eth2 for AVB connection
  * Both ethernet cards are using the driver `igb_avb`.
    Check via `ethtool -i eth0`.
  * Both PCs can ping each other through the AVB network cards.

## PC 1: start AVB environment + talker application

Start AVB environment:

```bash
# GPTP time synchronization daemon
sudo ./daemon_cl eth0 -R 1

# Stream reservation daemon (m: MMRP, v: MVRP, s: MSRP)
sudo ./mrpd -mvs -i eth0
```

Start AVB talker example:

```bash
sudo ./simple_talker -i eth0 -t 2
```

## PC 2: start AVB environment + listener application

Start AVB environment similar to PC 1:

```bash
# GPTP time synchronization daemon (will synchronize on PC 1)
sudo ./daemon_cl eth2

# Stream reservation daemon (m: MMRP, v: MVRP, s: MSRP)
sudo ./mrpd -mvs -i eth2
```

Start AVB listener example:

```bash
sudo ./simple_listener -i eth2 -f out.wav
```

:tada:
