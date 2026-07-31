/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include "key_counter_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Counters - total persists to flash, daily resets on boot */
static uint32_t total_count = 0;
static uint32_t daily_count = 0;
static uint32_t last_saved_total = 0;

/* Settings subtree name */
#define KCS_SUBTREE   "zmk/kcs"
#define KCS_KEY_TOTAL "total"

/* Save every N keystrokes + after idle delay to avoid flash wear */
#define SAVE_THRESHOLD 50
#define SAVE_IDLE_MS   10000

static bool handler_registered;

/* ---- number formatting ---- */
static void format_with_commas(uint32_t num, char *buf, size_t bufsize)
{
    if (bufsize < 2) {
        return;
    }
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    char temp[16];
    int len = 0;
    int digit_count = 0;

    while (num > 0 && len < (int)sizeof(temp)) {
        if (digit_count > 0 && (digit_count % 3) == 0) {
            temp[len++] = ',';
        }
        temp[len++] = '0' + (num % 10);
        num /= 10;
        digit_count++;
    }

    int i;
    for (i = 0; i < len && i < (int)bufsize - 1; i++) {
        buf[i] = temp[len - 1 - i];
    }
    buf[i] = '\0';
}

/* ---- settings persistence ---- */
static int kcs_settings_set(const char *name, size_t len,
                            settings_read_cb read_cb, void *cb_arg)
{
    const char *next;
    int rc;

    if (settings_name_steq(name, KCS_KEY_TOTAL, &next) && !next) {
        if (len != sizeof(total_count)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &total_count, sizeof(total_count));
        if (rc > 0) {
            last_saved_total = total_count;
            LOG_DBG("Restored total count: %u", total_count);
            return 0;
        }
        return -ENODATA;
    }

    return -ENOENT;
}

static struct settings_handler kcs_handler = {
    .name = KCS_SUBTREE,
    .h_set = kcs_settings_set,
};

static void do_save(void)
{
    int rc = settings_save_one(KCS_SUBTREE "/" KCS_KEY_TOTAL,
                               &total_count, sizeof(total_count));
    if (rc == 0) {
        last_saved_total = total_count;
    } else {
        LOG_WRN("Failed to save total count (err %d)", rc);
    }
}

static void save_work_handler(struct k_work *work)
{
    do_save();
}

static K_WORK_DELAYABLE_DEFINE(save_work, save_work_handler);

static void schedule_save(void)
{
    uint32_t delta = total_count - last_saved_total;

    if (delta >= SAVE_THRESHOLD) {
        /* Threshold crossed - save now, cancel any pending idle save */
        k_work_cancel_delayable(&save_work);
        do_save();
    } else {
        /* Defer save until user stops typing for SAVE_IDLE_MS */
        k_work_reschedule(&save_work, K_MSEC(SAVE_IDLE_MS));
    }
}

static void ensure_settings_loaded(void)
{
    if (!handler_registered) {
        settings_register(&kcs_handler);
        settings_load_subtree(KCS_SUBTREE);
        handler_registered = true;
    }
}

/* ---- display update ---- */
struct key_counter_state {
    uint32_t total;
    uint32_t daily;
};

static struct key_counter_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_keycode_state_changed *ev =
        as_zmk_keycode_state_changed(_eh);

    if (ev && ev->state) {
        total_count++;
        daily_count++;
        schedule_save();
    }

    return (struct key_counter_state){
        .total = total_count,
        .daily = daily_count,
    };
}

static void set_key_counts(struct zmk_widget_key_counter_status *widget,
                           struct key_counter_state state)
{
    char buf[24];

    snprintf(buf, sizeof(buf), "T ");
    size_t prefix_len = strlen(buf);
    format_with_commas(state.total, buf + prefix_len, sizeof(buf) - prefix_len);
    lv_label_set_text(widget->total_label, buf);

    snprintf(buf, sizeof(buf), "D ");
    prefix_len = strlen(buf);
    format_with_commas(state.daily, buf + prefix_len, sizeof(buf) - prefix_len);
    lv_label_set_text(widget->daily_label, buf);
}

static void key_counter_update_cb(struct key_counter_state state)
{
    struct zmk_widget_key_counter_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_key_counts(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_key_counter, struct key_counter_state,
                            key_counter_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_key_counter, zmk_keycode_state_changed);

/* ---- public API ---- */
int zmk_widget_key_counter_status_init(struct zmk_widget_key_counter_status *widget,
                                       lv_obj_t *parent)
{
    ensure_settings_loaded();

    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    widget->total_label = lv_label_create(widget->obj);
    lv_obj_align(widget->total_label, LV_ALIGN_TOP_LEFT, 0, 0);

    widget->daily_label = lv_label_create(widget->obj);
    lv_obj_align_to(widget->daily_label, widget->total_label,
                    LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    /* Show current values (loaded from settings or 0) */
    char buf[24];
    snprintf(buf, sizeof(buf), "T ");
    size_t prefix_len = strlen(buf);
    format_with_commas(total_count, buf + prefix_len, sizeof(buf) - prefix_len);
    lv_label_set_text(widget->total_label, buf);

    snprintf(buf, sizeof(buf), "D ");
    prefix_len = strlen(buf);
    format_with_commas(daily_count, buf + prefix_len, sizeof(buf) - prefix_len);
    lv_label_set_text(widget->daily_label, buf);

    sys_slist_append(&widgets, &widget->node);

    widget_key_counter_init();
    return 0;
}

lv_obj_t *zmk_widget_key_counter_status_obj(struct zmk_widget_key_counter_status *widget)
{
    return widget->obj;
}

/* ---- public reset API (used by &daily_reset / &total_reset behaviors) ---- */

static void update_all_widgets(void)
{
    struct key_counter_state state = {
        .total = total_count,
        .daily = daily_count,
    };
    struct zmk_widget_key_counter_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_key_counts(widget, state);
    }
}

void zmk_key_counter_reset_daily(void)
{
    daily_count = 0;
    update_all_widgets();
}

void zmk_key_counter_reset_total(void)
{
    total_count = 0;
    schedule_save();
    update_all_widgets();
}
