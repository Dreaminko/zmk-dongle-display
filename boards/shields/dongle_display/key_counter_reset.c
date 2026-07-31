/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "widgets/key_counter_status.h"

/* ---- &daily_reset ---- */

static int daily_reset_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event)
{
    zmk_key_counter_reset_daily();
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api daily_reset_api = {
    .binding_pressed = daily_reset_binding_pressed,
};

#define DT_DRV_COMPAT zmk_behavior_daily_reset

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
                      APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &daily_reset_api);

/* ---- &total_reset ---- */

static int total_reset_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event)
{
    zmk_key_counter_reset_total();
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api total_reset_api = {
    .binding_pressed = total_reset_binding_pressed,
};

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT zmk_behavior_total_reset

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
                      APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &total_reset_api);
