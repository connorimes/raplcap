# RAPLCap - Cray Power Manager

This implementation of the `raplcap` interface uses Cray's Power Manager.

This is designed for and tested on the [NERSC Cori](http://www.nersc.gov/users/computational-systems/cori/), a Cray XC40 system.

Specifically, this implementation supports reading the following files in `/sys/cray/pm_counters`:

* `power_cap` - XC30 and XC40 systems.
* `accel_power_cap` - XC30 and XC40 systems with accelerators.

By default, the `power_cap` file is used.
To override, set the environment variable `RAPLCAP_CRAY_PM_FILE=accel_power_cap`.

The implementation assumes that the power caps are for the Package zone, i.e. `RAPLCAP_ZONE_PACKAGE`.
All other zones are unsupported, as is setting time windows.
The `raplcap_set_zone_enabled` function can disable power capping but not enable it - setting a positive power cap using `raplcap_set_limits` automatically enables power capping at the desired level.

The implementation also assumes only 1 socket per node.
If for some reason there is more than one, it is expected that the power manager allocates the power cap appropriately between the multiple sockets (perhaps sharing it evenly).

## Prerequisites

You must be running on a Cray system that supports the power monitoring interfaces listed above.

The `perftools-base` module must be loaded:

```sh
module load perftools-base
```
