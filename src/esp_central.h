/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef H_ESP_CENTRAL_
#define H_ESP_CENTRAL_

#ifdef __cplusplus
extern "C" {
#endif

#define PEER_ADDR_VAL_SIZE                                  6

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
char *addr_str(const void *addr);
void print_uuid(const ble_uuid_t *uuid);
void print_conn_desc(const struct ble_gap_conn_desc *desc);
void print_adv_fields(const struct ble_hs_adv_fields *fields);
// void ext_print_adv_report(const void *param);

#ifdef __cplusplus
}
#endif

#endif