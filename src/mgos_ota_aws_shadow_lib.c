/*
 * Copyright (c) 2014-2017 Cesanta Software Limited
 * All rights reserved
 */

#include "mgos.h"
#include "mgos_aws_shadow.h"
#include "mgos_ota_http_client.h"

#define OTA_STATE_FILE_NAME "ota_state.json"
#define OTA_STATE_FILE_FORMAT "{url: %Q}"
#define OTA_COMMIT_TIMEOUT 600

static char *get_old_url(void) {
  size_t size;
  char *old_url = NULL, *content = cs_read_file(OTA_STATE_FILE_NAME, &size);
  if (content != NULL) {
    json_scanf(content, size, OTA_STATE_FILE_FORMAT, &old_url);
    free(content);
  }
  return old_url;
}

static bool is_equal_to_previous_ota_url(const char *new_url) {
  char *old_url = get_old_url();
  bool equal = old_url && new_url && strcmp(new_url, old_url) == 0;
  free(old_url);
  return equal;
}

static void save_ota_url(const char *ota_url) {
  LOG(LL_INFO, ("Saving %s -> %s", ota_url, OTA_STATE_FILE_NAME));
  FILE *fp = fopen(OTA_STATE_FILE_NAME, "w");
  if (fp != NULL) {
    struct json_out out = JSON_OUT_FILE(fp);
    json_printf(&out, OTA_STATE_FILE_FORMAT, ota_url);
    fclose(fp);
  }
}

static bool upd_cb(enum mgos_upd_event ev, const void *ev_arg, void *cb_arg) {
  switch (ev) {
    case MGOS_UPD_EV_INIT: {
      break;
    }
    case MGOS_UPD_EV_BEGIN: {
      mgos_aws_shadow_updatef(0, "{reported:{ota_code: 0, ota_message: %Q}}",
                              "starting update");
      break;
    }
    case MGOS_UPD_EV_PROGRESS: {
      const struct mgos_upd_info *info = (const struct mgos_upd_info *) ev_arg;
      char *buf = NULL;
      mg_asprintf(&buf, 0, "Progress: %s %d of %d", info->current_file.name,
                  info->current_file.processed, info->current_file.size);
      mgos_aws_shadow_updatef(0, "{reported:{ota_message: %Q}}", buf);
      free(buf);
      break;
    }
    case MGOS_UPD_EV_END: {
      struct update_context *ctx = (struct update_context *) ev_arg;
      mgos_aws_shadow_updatef(0, "{reported:{ota_code: %d, ota_message: %Q}}",
                              ctx->result, ctx->status_msg);
      break;
    }
  }
  (void) cb_arg;
  return true;
}

static void state_cb(void *arg, enum mgos_aws_shadow_event ev, uint64_t version,
                     const struct mg_str reported, const struct mg_str desired,
                     const struct mg_str reported_md,
                     const struct mg_str desired_md) {
  switch (ev) {
    case MGOS_AWS_SHADOW_CONNECTED: {
      char *old_url = get_old_url();
      mgos_aws_shadow_updatef(0, "{reported:{ota_url: %Q}}",
                              old_url ? old_url : "");
      free(old_url);

      /* If we're not committed, commit */
      if (!mgos_upd_is_committed()) {
        mgos_aws_shadow_updatef(0, "{reported:{ota_message: %Q}}",
                                "Committing FW update. OTA update finished.");
        mgos_upd_commit();
      }
      break;
    }
    case MGOS_AWS_SHADOW_UPDATE_DELTA: {
      char *ota_url = NULL;
      json_scanf(desired.p, desired.len, "{ota_url: %Q}", &ota_url);
      if (is_equal_to_previous_ota_url(ota_url)) {
        LOG(LL_INFO, ("ota_url not changed, OTA stopped"));
      } else if (ota_url != NULL) {
        save_ota_url(ota_url);
        mgos_aws_shadow_updatef(0, "{reported:{ota_url: %Q}}", ota_url);
        struct update_context *ctx = updater_context_create();
        ctx->fctx.commit_timeout = OTA_COMMIT_TIMEOUT;
        mgos_upd_set_event_cb(upd_cb, NULL);
        mgos_ota_http_start(ctx, ota_url);
      }
      free(ota_url);
      break;
    }
    default:
      break;
  }

  (void) arg;
  (void) reported;
  (void) reported_md;
  (void) desired_md;
  (void) version;
}

bool mgos_ota_aws_shadow_init(void) {
  mgos_aws_shadow_set_state_handler(state_cb, NULL);
  return true;
}
