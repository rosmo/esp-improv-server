/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "esp_central.h"

/**
 * Utility function to log an array of bytes.
 */
void
print_bytes(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        ESP_LOGI("DEBUG", "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

char *
addr_str(const void *addr)
{
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = (const uint8_t *)addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}

void
print_uuid(const ble_uuid_t *uuid)
{
    char buf[BLE_UUID_STR_LEN];

    ESP_LOGI("DEBUG", "%s", ble_uuid_to_str(uuid, buf));
}

/**
 * Logs information about a connection to the console.
 */
void
print_conn_desc(const struct ble_gap_conn_desc *desc)
{
    ESP_LOGI("DEBUG", "handle=%d our_ota_addr_type=%d our_ota_addr=%s ",
                desc->conn_handle, desc->our_ota_addr.type,
                addr_str(desc->our_ota_addr.val));
    ESP_LOGI("DEBUG", "our_id_addr_type=%d our_id_addr=%s ",
                desc->our_id_addr.type, addr_str(desc->our_id_addr.val));
    ESP_LOGI("DEBUG", "peer_ota_addr_type=%d peer_ota_addr=%s ",
                desc->peer_ota_addr.type, addr_str(desc->peer_ota_addr.val));
    ESP_LOGI("DEBUG", "peer_id_addr_type=%d peer_id_addr=%s ",
                desc->peer_id_addr.type, addr_str(desc->peer_id_addr.val));
    ESP_LOGI("DEBUG", "conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                "encrypted=%d authenticated=%d bonded=%d",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

void
print_addr(const void *addr, const char *name)
{
    const uint8_t *u8p;
    u8p = (const uint8_t *)addr;
    ESP_LOGI("DEBUG", "%s = %02x:%02x:%02x:%02x:%02x:%02x",
                name, u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

#if 0
void
ext_print_adv_report(const void *param)
{
    const struct ble_gap_ext_disc_desc *disc = (struct ble_gap_ext_disc_desc *)param;
    ESP_LOGI("DEBUG", "props=%d data_status=%d legacy_event_type=%d", disc->props, disc->data_status, disc->legacy_event_type);
    print_addr(disc->addr.val, "address");
    ESP_LOGI("DEBUG", "rssi=%d tx_power=%d", disc->rssi, disc->tx_power);
    ESP_LOGI("DEBUG", "sid=%d prim_phy=%d sec_phy=%d", disc->sid, disc->prim_phy, disc->sec_phy);
    ESP_LOGI("DEBUG", "periodic_adv_itvl=%d length_data=%d", disc->periodic_adv_itvl, disc->length_data);
    print_addr(disc->direct_addr.val, "direct address");
}
#endif

void
print_adv_fields(const struct ble_hs_adv_fields *fields)
{
    char s[BLE_HS_ADV_MAX_SZ];
    const uint8_t *u8p;
    int i;

    if (fields->flags != 0) {
        ESP_LOGI("DEBUG", "    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        ESP_LOGI("DEBUG", "    uuids16(%scomplete)=",
                    fields->uuids16_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids16; i++) {
            print_uuid(&fields->uuids16[i].u);
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->uuids32 != NULL) {
        ESP_LOGI("DEBUG", "    uuids32(%scomplete)=",
                    fields->uuids32_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids32; i++) {
            print_uuid(&fields->uuids32[i].u);
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->uuids128 != NULL) {
        ESP_LOGI("DEBUG", "    uuids128(%scomplete)=",
                    fields->uuids128_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids128; i++) {
            print_uuid(&fields->uuids128[i].u);
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->name != NULL) {
        assert(fields->name_len < sizeof s - 1);
        memcpy(s, fields->name, fields->name_len);
        s[fields->name_len] = '\0';
        ESP_LOGI("DEBUG", "    name(%scomplete)=%s\n",
                    fields->name_is_complete ? "" : "in", s);
    }

    if (fields->tx_pwr_lvl_is_present) {
        ESP_LOGI("DEBUG", "    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->slave_itvl_range != NULL) {
        ESP_LOGI("DEBUG", "    slave_itvl_range=");
        print_bytes(fields->slave_itvl_range, BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->sm_tk_value_is_present) {
        ESP_LOGI("DEBUG", "    sm_tk_value=");
        print_bytes(fields->sm_tk_value, 16);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->sm_oob_flag_is_present) {
        ESP_LOGI("DEBUG", "    sm_oob_flag=%d\n", fields->sm_oob_flag);
    }

    if (fields->sol_uuids16 != NULL) {
        ESP_LOGI("DEBUG", "    sol_uuids16=");
        for (i = 0; i < fields->sol_num_uuids16; i++) {
            print_uuid(&fields->sol_uuids16[i].u);
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->sol_uuids32 != NULL) {
        ESP_LOGI("DEBUG", "    sol_uuids32=");
        for (i = 0; i < fields->sol_num_uuids32; i++) {
            print_uuid(&fields->sol_uuids32[i].u);
            ESP_LOGI("DEBUG", "\n");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->sol_uuids128 != NULL) {
        ESP_LOGI("DEBUG", "    sol_uuids128=");
        for (i = 0; i < fields->sol_num_uuids128; i++) {
            print_uuid(&fields->sol_uuids128[i].u);
            ESP_LOGI("DEBUG", " ");
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        ESP_LOGI("DEBUG", "    svc_data_uuid16=");
        print_bytes(fields->svc_data_uuid16, fields->svc_data_uuid16_len);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->public_tgt_addr != NULL) {
        ESP_LOGI("DEBUG", "    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            ESP_LOGI("DEBUG", "public_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->random_tgt_addr != NULL) {
        ESP_LOGI("DEBUG", "    random_tgt_addr=");
        u8p = fields->random_tgt_addr;
        for (i = 0; i < fields->num_random_tgt_addrs; i++) {
            ESP_LOGI("DEBUG", "random_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->appearance_is_present) {
        ESP_LOGI("DEBUG", "    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        ESP_LOGI("DEBUG", "    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->device_addr_is_present) {
        ESP_LOGI("DEBUG", "    device_addr=");
	u8p = fields->device_addr;
	ESP_LOGI("DEBUG", "%s ", addr_str(u8p));

	u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
	ESP_LOGI("DEBUG", "addr_type %d ", *u8p);
    }

    if (fields->le_role_is_present) {
        ESP_LOGI("DEBUG", "    le_role=%d\n", fields->le_role);
    }

    if (fields->svc_data_uuid32 != NULL) {
        ESP_LOGI("DEBUG", "    svc_data_uuid32=");
        print_bytes(fields->svc_data_uuid32, fields->svc_data_uuid32_len);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        ESP_LOGI("DEBUG", "    svc_data_uuid128=");
        print_bytes(fields->svc_data_uuid128, fields->svc_data_uuid128_len);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->uri != NULL) {
        ESP_LOGI("DEBUG", "    uri=");
        print_bytes(fields->uri, fields->uri_len);
        ESP_LOGI("DEBUG", "\n");
    }

    if (fields->mfg_data != NULL) {
        ESP_LOGI("DEBUG", "    mfg_data=");
        print_bytes(fields->mfg_data, fields->mfg_data_len);
        ESP_LOGI("DEBUG", "\n");
    }
}
