/**
 * Implementation that wraps libpowercap with RAPL sysfs interface.
 *
 * @author Connor Imes
 * @date 2016-05-13
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include "raplcap.h"
#define RAPLCAP_IMPL "raplcap-powercap"
#include "raplcap-common.h"
// powercap header
#include <powercap-rapl.h>

static raplcap rc_default;

static void raplcap_limit_to_powercap(const raplcap_limit* l, uint64_t* us, uint64_t* uw) {
  assert(l != NULL);
  assert(us != NULL);
  assert(uw != NULL);
  static const uint64_t ONE_MILLION = 1000000;
  *us = ONE_MILLION * l->seconds;
  *uw = ONE_MILLION * l->watts;
}

static void powercap_to_raplcap(uint64_t us, uint64_t uw, raplcap_limit* l) {
  assert(l != NULL);
  static const double ONE_MILLION = 1000000.0;
  l->seconds = ((double) us) / ONE_MILLION;
  l->watts = ((double) uw) / ONE_MILLION;
}

static int raplcap_zone_to_powercap(raplcap_zone zone, powercap_rapl_zone* z) {
  assert(z != NULL);
  switch (zone) {
    case RAPLCAP_ZONE_PACKAGE:
      *z = POWERCAP_RAPL_ZONE_PACKAGE;
      break;
    case RAPLCAP_ZONE_CORE:
      *z =  POWERCAP_RAPL_ZONE_CORE;
      break;
    case RAPLCAP_ZONE_UNCORE:
      *z =  POWERCAP_RAPL_ZONE_UNCORE;
      break;
    case RAPLCAP_ZONE_DRAM:
      *z =  POWERCAP_RAPL_ZONE_DRAM;
      break;
    case RAPLCAP_ZONE_PSYS:
      *z =  POWERCAP_RAPL_ZONE_PSYS;
      break;
    default:
      return -1;
  }
  return 0;
}

static powercap_rapl_pkg* get_pkg_zone(uint32_t socket, const raplcap* rc, raplcap_zone zone, powercap_rapl_zone* z) {
  assert(z != NULL);
  if (rc == NULL) {
    rc = &rc_default;
  }
  if (socket >= rc->nsockets) {
    raplcap_log(ERROR, "get_pkg_zone: Socket %"PRIu32" not in range [0, %"PRIu32")\n", socket, rc->nsockets);
    errno = EINVAL;
    return NULL;
  }
  if (raplcap_zone_to_powercap(zone, z)) {
    raplcap_log(ERROR, "get_pkg_zone: Unknown zone: %d\n", *z);
    errno = EINVAL;
    return NULL;
  }
  return &((powercap_rapl_pkg*) rc->state)[socket];
}

static uint32_t get_powercap_sockets(void) {
  uint32_t sockets = powercap_rapl_get_num_packages();
  raplcap_log(DEBUG, "get_powercap_sockets: sockets=%"PRIu32"\n", sockets);
  if (sockets == 0) {
    raplcap_perror(ERROR, "get_powercap_sockets: powercap_rapl_get_num_packages");
  }
  return sockets;
}

int raplcap_init(raplcap* rc) {
  powercap_rapl_pkg* pkgs;
  uint32_t i;
  int err_save;
  if (rc == NULL) {
    rc = &rc_default;
  }
  // get the number of packages/sockets
  if ((rc->nsockets = get_powercap_sockets()) == 0) {
    return -1;
  }
  if ((pkgs = malloc(rc->nsockets * sizeof(powercap_rapl_pkg))) == NULL) {
    raplcap_perror(ERROR, "raplcap_init: malloc");
    return -1;
  }
  rc->state = pkgs;
  for (i = 0; i < rc->nsockets; i++) {
    if (powercap_rapl_init(i, &pkgs[i], 0)) {
      raplcap_perror(ERROR, "raplcap_init: powercap_rapl_init");
      err_save = errno;
      raplcap_destroy(rc);
      errno = err_save;
      return -1;
    }
  }
  raplcap_log(DEBUG, "raplcap_init: Initialized\n");
  return 0;
}

int raplcap_destroy(raplcap* rc) {
  uint32_t i;
  int err_save = 0;
  if (rc == NULL) {
    rc = &rc_default;
  }
  if (rc->state != NULL) {
    for (i = 0; i < rc->nsockets; i++) {
      raplcap_log(DEBUG, "raplcap_destroy: destroying powercap context: socket=%"PRIu32"\n", i);
      if (powercap_rapl_destroy(&((powercap_rapl_pkg*) rc->state)[i])) {
        raplcap_perror(ERROR, "raplcap_destroy: powercap_rapl_destroy");
        err_save = errno;
      }
    }
    free(rc->state);
    rc->state = NULL;
    rc->nsockets = 0;
  }
  raplcap_log(DEBUG, "raplcap_destroy: Destroyed\n");
  errno = err_save;
  return err_save ? -1 : 0;
}

uint32_t raplcap_get_num_sockets(const raplcap* rc) {
  if (rc == NULL) {
    rc = &rc_default;
  }
  return rc->nsockets == 0 ? get_powercap_sockets() : rc->nsockets;
}

int raplcap_is_zone_supported(uint32_t socket, const raplcap* rc, raplcap_zone zone) {
  powercap_rapl_zone z;
  const powercap_rapl_pkg* pkg = get_pkg_zone(socket, rc, zone, &z);
  int ret;
  if (pkg == NULL) {
    ret = -1;
  } else if ((ret = powercap_rapl_is_zone_supported(pkg, z)) < 0) {
    raplcap_perror(ERROR, "raplcap_is_zone_supported: powercap_rapl_is_zone_supported");
  }
  raplcap_log(DEBUG, "raplcap_is_zone_supported: socket=%"PRIu32", zone=%d, supported=%d\n", socket, zone, ret);
  return ret;
}

int raplcap_is_zone_enabled(uint32_t socket, const raplcap* rc, raplcap_zone zone) {
  powercap_rapl_zone z;
  const powercap_rapl_pkg* pkg = get_pkg_zone(socket, rc, zone, &z);
  int ret;
  if (pkg == NULL) {
    ret = -1;
  } else if ((ret = powercap_rapl_is_enabled(pkg, z)) < 0) {
    raplcap_perror(ERROR, "raplcap_is_zone_enabled: powercap_rapl_is_enabled");
  }
  raplcap_log(DEBUG, "raplcap_is_zone_enabled: socket=%"PRIu32", zone=%d, enabled=%d\n", socket, zone, ret);
  return ret;
}

int raplcap_set_zone_enabled(uint32_t socket, const raplcap* rc, raplcap_zone zone, int enabled) {
  powercap_rapl_zone z;
  const powercap_rapl_pkg* pkg = get_pkg_zone(socket, rc, zone, &z);
  int ret;
  if (pkg == NULL) {
    ret = -1;
  } else {
    raplcap_log(DEBUG, "raplcap_set_zone_enabled: socket=%"PRIu32", zone=%d, enabled=%d\n", socket, zone, enabled);
    if ((ret = powercap_rapl_set_enabled(pkg, z, enabled)) != 0) {
      raplcap_perror(ERROR, "raplcap_set_zone_enabled: powercap_rapl_set_enabled");
    }
  }
  return ret;
}

int raplcap_get_limits(uint32_t socket, const raplcap* rc, raplcap_zone zone,
                       raplcap_limit* limit_long, raplcap_limit* limit_short) {
  uint64_t time_window, power_limit;
  powercap_rapl_zone z;
  const powercap_rapl_pkg* pkg = get_pkg_zone(socket, rc, zone, &z);
  if (pkg == NULL) {
    return -1;
  }
  if (limit_long != NULL) {
    if (powercap_rapl_get_time_window_us(pkg, z, POWERCAP_RAPL_CONSTRAINT_LONG, &time_window)) {
      raplcap_perror(ERROR, "raplcap_get_limits: powercap_rapl_get_time_window_us (long_term)");
      return -1;
    }
    if (powercap_rapl_get_power_limit_uw(pkg, z, POWERCAP_RAPL_CONSTRAINT_LONG, &power_limit)) {
      raplcap_perror(ERROR, "raplcap_get_limits: powercap_rapl_get_power_limit_uw (long_term)");
      return -1;
    }
    powercap_to_raplcap(time_window, power_limit, limit_long);
    raplcap_log(DEBUG, "raplcap_get_limits: socket=%"PRIu32", zone=%d, long_term:"
                "\n\ttime=%.12f s (%"PRIu64" us)\n\tpower=%.12f W (%"PRIu64" uW)\n",
                socket, zone, limit_long->seconds, time_window, limit_long->watts, power_limit);
  }
  if (limit_short != NULL) {
    if (powercap_rapl_get_time_window_us(pkg, z, POWERCAP_RAPL_CONSTRAINT_SHORT, &time_window)) {
      raplcap_perror(ERROR, "raplcap_get_limits: powercap_rapl_get_time_window_us (short_term)");
      return -1;
    }
    if (powercap_rapl_get_power_limit_uw(pkg, z, POWERCAP_RAPL_CONSTRAINT_SHORT, &power_limit)) {
      raplcap_perror(ERROR, "raplcap_get_limits: powercap_rapl_get_power_limit_uw (short_term)");
      return -1;
    }
    powercap_to_raplcap(time_window, power_limit, limit_short);
    raplcap_log(DEBUG, "raplcap_get_limits: socket=%"PRIu32", zone=%d, short_term:"
                "\n\ttime=%.12f s (%"PRIu64" us)\n\tpower=%.12f W (%"PRIu64" uW)\n",
                socket, zone, limit_short->seconds, time_window, limit_short->watts, power_limit);
  }
  return 0;
}

int raplcap_set_limits(uint32_t socket, const raplcap* rc, raplcap_zone zone,
                       const raplcap_limit* limit_long, const raplcap_limit* limit_short) {
  uint64_t time_window, power_limit;
  powercap_rapl_zone z;
  const powercap_rapl_pkg* pkg = get_pkg_zone(socket, rc, zone, &z);
  if (pkg == NULL) {
    return -1;
  }
  if (limit_long != NULL) {
    raplcap_limit_to_powercap(limit_long, &time_window, &power_limit);
    raplcap_log(DEBUG, "raplcap_set_limits: socket=%"PRIu32", zone=%d, long_term:"
                "\n\ttime=%.12f s (%"PRIu64" us)\n\tpower=%.12f W (%"PRIu64" uW)\n",
                socket, zone, limit_long->seconds, time_window, limit_long->watts, power_limit);
    if (time_window != 0 && powercap_rapl_set_time_window_us(pkg, z, POWERCAP_RAPL_CONSTRAINT_LONG, time_window)) {
      raplcap_perror(ERROR, "raplcap_set_limits: powercap_rapl_set_time_window_us (long_term)");
      return -1;
    }
    if (power_limit != 0 && powercap_rapl_set_power_limit_uw(pkg, z, POWERCAP_RAPL_CONSTRAINT_LONG, power_limit)) {
      raplcap_perror(ERROR, "raplcap_set_limits: powercap_rapl_set_power_limit_uw (long_term)");
      return -1;
    }
  }
  if (limit_short != NULL) {
    raplcap_limit_to_powercap(limit_short, &time_window, &power_limit);
    raplcap_log(DEBUG, "raplcap_set_limits: socket=%"PRIu32", zone=%d, short_term:"
                "\n\ttime=%.12f s (%"PRIu64" us)\n\tpower=%.12f W (%"PRIu64" uW)\n",
                socket, zone, limit_short->seconds, time_window, limit_short->watts, power_limit);
    if (time_window != 0 && powercap_rapl_set_time_window_us(pkg, z, POWERCAP_RAPL_CONSTRAINT_SHORT, time_window)) {
      raplcap_perror(ERROR, "raplcap_set_limits: powercap_rapl_set_time_window_us (short_term)");
      return -1;
    }
    if (power_limit != 0 && powercap_rapl_set_power_limit_uw(pkg, z, POWERCAP_RAPL_CONSTRAINT_SHORT, power_limit)) {
      raplcap_perror(ERROR, "raplcap_set_limits: powercap_rapl_set_power_limit_uw (short_term)");
      return -1;
    }
  }
  return 0;
}