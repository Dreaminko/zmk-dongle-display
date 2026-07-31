/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_key_counter_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *total_label;
    lv_obj_t *daily_label;
};

int zmk_widget_key_counter_status_init(struct zmk_widget_key_counter_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_key_counter_status_obj(struct zmk_widget_key_counter_status *widget);
