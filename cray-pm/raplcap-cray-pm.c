/**
 * Implementation that uses Cray Power Monitor
 *
 * @author Connor Imes
 * @date 2017-06-30
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raplcap.h"
#define RAPLCAP_IMPL "raplcap-cray-pm"
#include "raplcap-common.h"

#define RAPLCAP_CRAY_PM_FILE_ENV_VAR "RAPLCAP_CRAY_PM_FILE"

typedef enum raplcap_cray_pm_file {
  FILE_POWER_CAP = 0,
  FILE_ACCEL_POWER_CAP
} raplcap_cray_pm_file;

#define SYSFS_DIR "/sys/cray/pm_counters"
#define POWER_CAP "power_cap"
#define ACCEL_POWER_CAP "accel_power_cap"

// indices aligned with raplcap_cray_pm_file
static const char* CRAY_PM_FILES[] = { SYSFS_DIR"/"POWER_CAP, SYSFS_DIR"/"ACCEL_POWER_CAP };

typedef struct raplcap_cray_pm {
  FILE* f;
  raplcap_cray_pm_file type;
} raplcap_cray_pm;

static raplcap rc_default;

static int open_sysfs_file(raplcap_cray_pm* state) {
  assert(state != NULL);
  const char* env_type = getenv(RAPLCAP_CRAY_PM_FILE_ENV_VAR);
  if (env_type == NULL || strncmp(env_type, POWER_CAP, sizeof(POWER_CAP) == 0)) {
    state->type = FILE_POWER_CAP;
  } else if (strncmp(env_type, ACCEL_POWER_CAP, sizeof(ACCEL_POWER_CAP) == 0)) {
    state->type = FILE_ACCEL_POWER_CAP;
  } else {
    raplcap_log(WARN, "Defaulting to '"POWER_CAP"' file, unknown value for environment variable "
                      RAPLCAP_CRAY_PM_FILE_ENV_VAR"=%s\n", env_type);
    state->type = FILE_POWER_CAP;
  }
  state->f = fopen(CRAY_PM_FILES[state->type], "r");
  if (state->f == NULL) {
    raplcap_perror(ERROR, CRAY_PM_FILES[state->type]);
    return -errno;
  }
  return 0;
}

int raplcap_init(raplcap* rc) {
  if (rc == NULL) {
    rc = &rc_default;
  }
  if ((rc->state = (raplcap_cray_pm*) malloc(sizeof(raplcap_cray_pm))) == NULL) {
    raplcap_perror(ERROR, "raplcap_init: malloc");
    return -errno;
  }
  if (open_sysfs_file((raplcap_cray_pm*) rc->state)) {
    raplcap_destroy(rc);
    return -errno;
  }
  // Interface doesn't support multiple sockets
  rc->nsockets = 1;
  raplcap_log(DEBUG, "raplcap_init: Initialized\n");
  return 0;
}

int raplcap_destroy(raplcap* rc) {
  raplcap_cray_pm* state;
  if (rc == NULL) {
    rc = &rc_default;
  }
  int err_save = 0;
  state = (raplcap_cray_pm*) rc->state;
  if (state->f != NULL && fclose(state->f)) {
    err_save = errno;
  }
  free(state);
  rc->state = NULL;
  raplcap_log(DEBUG, "raplcap_destroy: Destroyed\n");
  if (err_save) {
    errno = err_save;
  }
  return err_save ? -errno : 0;
}

uint32_t raplcap_get_num_sockets(const raplcap* rc) {
  return 1;
}

static raplcap_cray_pm* get_state(uint32_t socket, const raplcap* rc) {
  if (rc == NULL) {
    rc = &rc_default;
  }
  if (rc->state == NULL || socket >= rc->nsockets) {
    errno = EINVAL;
    return NULL;
  }
  return (raplcap_cray_pm*) rc->state;
}

int raplcap_is_zone_supported(uint32_t socket, const raplcap* rc, raplcap_zone zone) {
  if (get_state(socket, rc) == NULL) {
    errno = EINVAL;
    return -errno;
  }
  // TODO: only package is supported?
  return zone == RAPLCAP_ZONE_PACKAGE;
}

int raplcap_is_zone_enabled(uint32_t socket, const raplcap* rc, raplcap_zone zone) {
  raplcap_limit l;
  int ret;
  if ((ret = raplcap_get_limits(socket, rc, zone, &l, NULL))) {
    return ret;
  }
  // enabled when power cap is set
  return l.watts > 0;
}

int raplcap_set_zone_enabled(uint32_t socket, const raplcap* rc, raplcap_zone zone, int enabled) {
  // we can disable a zone, but can't enable it without knowing the power cap to apply
  raplcap_limit l;
  if (enabled) {
    raplcap_log(ERROR, "raplcap_set_zone_enabled: Can disable zones, but cannot enable them\n");
    raplcap_log(INFO, "raplcap_set_zone_enabled: Enable by setting a power cap\n");
    errno = ENOSYS;
    return -errno;
  }
  l.watts = 0;
  l.seconds = 0;
  return raplcap_set_limits(socket, rc, zone, &l, NULL);
}

int raplcap_get_limits(uint32_t socket, const raplcap* rc, raplcap_zone zone,
                       raplcap_limit* limit_long, raplcap_limit* limit_short) {
  raplcap_cray_pm* state;
  uint64_t watts = 0;
  if (raplcap_is_zone_supported(socket, rc, zone) <= 0) {
    errno = EINVAL;
    return -errno;
  }
  // will not be NULL if zone is supported
  state = get_state(socket, rc);
  if (state->f == NULL) {
    raplcap_log(ERROR, "raplcap_get_limits: power cap file is NULL!\n");
    errno = EINVAL;
    return -errno;
  }
  rewind(state->f);
  if (fscanf(state->f, "%"PRIu64" J", &watts) != 1) {
    if (!errno) {
      errno = EIO;
    }
    raplcap_perror(ERROR, "raplcap_get_limits: Failed to read power cap file");
    return -errno;
  }
  raplcap_log(DEBUG, "raplcap_get_limits: Got power cap: %"PRIu64"\n", watts);
  // TODO: assuming this is long_term data
  // TODO: What about data we can't collect, like long term seconds or any short term data? Currently setting to 0.
  if (limit_long != NULL) {
    limit_long->watts = (double) watts;
    limit_long->seconds = 0;
  }
  if (limit_short != NULL) {
    limit_short->watts = 0;
    limit_short->seconds = 0;
  }
  return 0;
}

int raplcap_set_limits(uint32_t socket, const raplcap* rc, raplcap_zone zone,
                       const raplcap_limit* limit_long, const raplcap_limit* limit_short) {
  (void) limit_short;
  raplcap_cray_pm* state;
  uint64_t watts;
  if (limit_long == NULL || limit_long->watts < 0) {
    errno = EINVAL;
    return -errno;
  }
  if (raplcap_is_zone_supported(socket, rc, zone) <= 0) {
    errno = EINVAL;
    return -errno;
  }
  // will not be NULL if zone is supported
  state = get_state(socket, rc);
  if (state->f == NULL) {
    raplcap_log(ERROR, "raplcap_set_limits: power cap file is NULL!\n");
    errno = EINVAL;
    return -errno;
  }
  // round value
  watts = (uint64_t) (limit_long->watts + 0.5);
  rewind(state->f);
  if (fwrite(&watts, sizeof(watts), 1, state->f) < 1) {
    if (!errno) {
      errno = EIO;
    }
    raplcap_perror(ERROR, "raplcap_set_limits: Failed to write power cap file");
    return -errno;
  }
  return 0;
}
