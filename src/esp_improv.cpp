/*
 * SPDX-FileCopyrightText: 2025 Taneli Leppä
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "esp_improv.h"
#include "console/console.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_central.h"

namespace improvserver 
{

const char *ImprovServer::TAG = "ImprovServer";

std::string *ImprovServer::deviceName;
std::string *ImprovServer::manufacturerName;
std::string *ImprovServer::modelName; 

bool ImprovServer::advertiseName = false;
ble_uuid128_t *ImprovServer::serviceUuid = ImprovServer::strToUuid(improv::SERVICE_UUID);
TaskHandle_t ImprovServer::advertiseTaskHandle = NULL;

improv::State ImprovServer::state = improv::STATE_AUTHORIZED;
improv::Error ImprovServer::error = improv::ERROR_NONE;

uint8_t ImprovServer::capabilities = 0;
uint8_t ImprovServer::addrType = 0;
uint16_t ImprovServer::connHandle = 0;    
uint16_t ImprovServer::errorHandle = 0;
uint16_t ImprovServer::statusHandle = 0;
uint16_t ImprovServer::capabilitiesHandle = 0;
uint16_t ImprovServer::rpcResultHandle = 0;
struct ble_gatt_svc_def ImprovServer::svc;
struct ble_gatt_svc_def ImprovServer::devSvc;
struct ble_gatt_svc_def ImprovServer::nullSvc;
struct ble_gatt_chr_def ImprovServer::statusChr;
struct ble_gatt_chr_def ImprovServer::errorChr;
struct ble_gatt_chr_def ImprovServer::rpcWriteChr;
struct ble_gatt_chr_def ImprovServer::rpcResultChr;
struct ble_gatt_chr_def ImprovServer::capabilitiesChr;
struct ble_gatt_chr_def ImprovServer::devManufChr;
struct ble_gatt_chr_def ImprovServer::devModelChr;
struct ble_gatt_chr_def ImprovServer::nullChr;

bool ImprovServer::advertising = false;
bool ImprovServer::advertiseOn = false;

esp_err_t ImprovServer::gapEvent(struct ble_gap_event *event, void *arg)
{
    ESP_LOGD(TAG, "GAP event: %d", event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
        ESP_LOGI(TAG, "connection %s; status=%d",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising */
            ImprovServer::connHandle = 0;
            ImprovServer::state = improv::STATE_AUTHORIZED;
            ImprovServer::error = improv::ERROR_NONE;
            advertise();
        } else {
            ImprovServer::connHandle = event->connect.conn_handle;
            ImprovServer::state = improv::STATE_AUTHORIZED;
            ImprovServer::error = improv::ERROR_NONE;
            advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        ImprovServer::connHandle = 0;
        /* Connection terminated; resume advertising */
        advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertising complete");
        advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe event attr_handle=%d", event->subscribe.attr_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update event; conn_handle=%d mtu=%d\n", event->mtu.conn_handle, event->mtu.value);
        break;

    }
    return ESP_OK;
}

esp_err_t ImprovServer::advertise()
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    ESP_LOGD(TAG, "Advertising...");
    advertising = true;

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // There isn't enough space usually for the name and the UUID at the same time
    if (advertiseName) {
        fields.tx_pwr_lvl_is_present = 1;
        fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
        
        fields.name = (uint8_t *)deviceName->c_str();
        fields.name_len = deviceName->length();
        fields.name_is_complete = 1;
    } else {
        fields.uuids128 = (const ble_uuid128_t *)serviceUuid;
        fields.num_uuids128 = 1; 
        fields.uuids128_is_complete = 0;

        uint8_t service_data[8] = {};
        service_data[0] = 0x77;  // PR
        service_data[1] = 0x46;  // IM
        service_data[2] = static_cast<uint8_t>(state);
        fields.svc_data_uuid16 = (const uint8_t *)&service_data;
        fields.svc_data_uuid16_len = 8;
    }
  
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d\n", rc);
        return ESP_FAIL;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    //adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    //adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(600);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ImprovServer::gapEvent, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d\n", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void ImprovServer::onReset(int reason) 
{
    ESP_LOGW(TAG, "Resetting state; reason=%d\n", reason);
}

void ImprovServer::onSync()
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &addrType);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(addrType, addr_val, NULL);
    ESP_LOGI(TAG, "Device address (type %d): %02x:%02x:%02x:%02x:%02x:%02x", addrType, addr_val[5], addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);
    ESP_LOGI(TAG, "On sync completed, signaling advertise task to start.");
    // Notify the task that advertising has started
    xTaskNotifyGive(advertiseTaskHandle);
}

void ImprovServer::hostTask(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task: started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t ImprovServer::StopAdvertising() 
{
    advertiseOn = false;
    return ESP_OK;
}

esp_err_t ImprovServer::StartAdvertising()
{
    advertiseOn = true;
    return ESP_OK;
}

void ImprovServer::advertiseTask(void *param)
{
    int rc = 0;

    ESP_LOGI(TAG, "BLE Advertise Task: waiting to start...");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (true) {
        if (advertising && !advertiseOn) {
            ESP_LOGI(TAG, "Stopping advertising.");
            rc = ble_gap_adv_stop();
            if (rc != 0) {
                ESP_LOGE(TAG, "BLE Advertise Task: failed to stop advertising!");
            }
            advertising = false;
            continue;
        }
        if (state == improv::STATE_PROVISIONED) {
            ESP_LOGI(TAG, "Just provisioned, waiting and resetting state...");
            vTaskDelay(pdMS_TO_TICKS(AFTER_PROVISION_DELAY));
            
            if (connHandle != 0) {
                ESP_LOGI(TAG, "Disconnecting client, handle=%d", connHandle);
                rc = ble_gap_terminate(connHandle, 0);
                if (rc != 0) {
                    ESP_LOGW(TAG, "Failed to disconnect client, rc=%d", rc);
                }
            }
            state = improv::STATE_AUTHORIZED;
        }
        if (advertiseOn && !advertising) {
            ESP_LOGI(TAG, "Starting advertising.");
            advertise();
            advertising = true;
        } else if (advertiseOn && advertising) {
            vTaskDelay(pdMS_TO_TICKS(ADVERTISE_NAME_EVERY_MSECS));
            ESP_LOGD(TAG, "BLE Advertise Task: starting to advertise name.");
            advertiseName = true;
            rc = ble_gap_adv_stop();
            if (rc != 0) {
                ESP_LOGE(TAG, "BLE Advertise Task: failed to stop advertising!");
                continue;
            }
            advertise();
            vTaskDelay(pdMS_TO_TICKS(ADVERTISE_NAME_FOR_MSECS));
            ESP_LOGD(TAG, "BLE Advertise Task: starting to advertise service and service data.");
            rc = ble_gap_adv_stop();
            if (rc != 0) {
                ESP_LOGE(TAG, "BLE Advertise Task: failed to stop advertising!");
                continue;
            }
            advertiseName = false;
            advertise();
            advertising = true;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t ImprovServer::Initialize(wifi_provision_fn onProvisionCallback, void *args)
{
    esp_err_t err;

    err = nimble_port_init();
	if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed!");
        return err;
	}

    /* Initialize the NimBLE host configuration */
    ble_hs_cfg.sync_cb = ImprovServer::onSync;
    ble_hs_cfg.reset_cb = ImprovServer::onReset;

    onProvision = onProvisionCallback;
    onProvisionArgs = args;

    err = initServer();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Improv server initialization failed!");
        return err;
    }

    int rc = ble_svc_gap_device_name_set(ImprovServer::deviceName->c_str());
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed!");
        return ESP_FAIL;
    }
    
    xTaskCreate(ImprovServer::advertiseTask, "ble_advertise_task", 4096, (void *)this, 1, &advertiseTaskHandle);
    xTaskCreate(ImprovServer::hostTask, "ble_host_task", 4096, (void *)this, 1, NULL);

    return ESP_OK;
}

ble_uuid128_t *ImprovServer::strToUuid(const char *_uuidStr) 
{
    size_t i, si = 0;
    size_t ulen = strlen(_uuidStr);
    char *uuidStr = strdup(_uuidStr);
    ble_uuid128_t *uuid;

    uuid = (ble_uuid128_t *)malloc(sizeof(ble_uuid128_t));
    if (uuid == NULL) {
        free(uuidStr);
        return NULL;
    }
    memset(uuid, 0, sizeof(ble_uuid128_t));
    uuid->u.type = BLE_UUID_TYPE_128;
    si = 0;
    for (i = sizeof(uuid->value) - 1; si < ulen; i--) {
        while (uuidStr[si] == '-') si++;
        sscanf((char *)(uuidStr + si), "%2hhx", &uuid->value[i]);
        si += 2;
    }
    free(uuidStr);
    return uuid;
}

int ImprovServer::gattSvrChrDeviceInfo(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid;
    int rc;

    uuid = ble_uuid_u16(ctxt->chr->uuid);
    if (uuid == GATT_MODEL_NUMBER_UUID) {
        rc = os_mbuf_append(ctxt->om, ImprovServer::manufacturerName->c_str(), ImprovServer::manufacturerName->length());
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        rc = os_mbuf_append(ctxt->om, ImprovServer::modelName->c_str(), ImprovServer::modelName->length());
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

int ImprovServer::gattSvrChrStatus(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    rc = os_mbuf_append(ctxt->om, &ImprovServer::state, sizeof(ImprovServer::state));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int ImprovServer::gattSvrChrStatusNotify()
{
    int rc = 0;
    struct os_mbuf *om;
    if (connHandle != 0) {
        om = ble_hs_mbuf_from_flat((uint8_t *)&state, sizeof(state));
        rc = ble_gatts_notify_custom(connHandle, statusHandle, om);
    }
    return rc;
}

int ImprovServer::gattSvrChrErrorNotify()
{
    int rc = 0;
    struct os_mbuf *om;
    if (connHandle != 0) {
        om = ble_hs_mbuf_from_flat((uint8_t *)&error, sizeof(error));
        rc = ble_gatts_notify_custom(connHandle, errorHandle, om);
    }
    return rc;
}

int ImprovServer::gattSvrChrError(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    rc = os_mbuf_append(ctxt->om, &error, sizeof(error));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int ImprovServer::gattSvrChrRpcWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ImprovServer *s = (ImprovServer *)arg;

    int rc = 0;
    int len; 
    uint16_t copied_len;
    uint8_t *le_phy_val;

    len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > 0) {
        le_phy_val = (uint8_t *)malloc(len * sizeof(uint8_t));
        if (le_phy_val) {
            rc = ble_hs_mbuf_to_flat(ctxt->om, le_phy_val, len, &copied_len);
            if (rc == 0) {
                improv::ImprovCommand cmd = improv::parse_improv_data(le_phy_val, copied_len, true);
                free(le_phy_val);
                ESP_LOGI(TAG, "Provisioning wifi: %s, %s", cmd.ssid.c_str(), cmd.password.c_str());

                state = improv::STATE_PROVISIONING;
                gattSvrChrStatusNotify();

                esp_err_t err = s->onWifiProvisioning(cmd.ssid.c_str(), cmd.password.c_str(), s->onProvisionArgs);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to provision WiFi, rc=%d", err);
                    error = improv::ERROR_UNABLE_TO_CONNECT;
                    gattSvrChrErrorNotify();
                } else {
                    state = improv::STATE_PROVISIONED;
                    gattSvrChrStatusNotify();
                }
                return 0;
            } else {
                error = improv::ERROR_INVALID_RPC;
                gattSvrChrErrorNotify();
                ESP_LOGE(TAG, "Failed to receive Improv command, rc=%d", rc);
            }
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for RPC command!");
        }
    }
    return rc == 0 ? 0 : BLE_ATT_ERR_UNLIKELY;
}

int ImprovServer::gattSvrChrRpcResult(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    ESP_LOGW(TAG, "RPC read result not supported!");

    uint8_t zero = 0;
    rc = os_mbuf_append(ctxt->om, &zero, sizeof(uint8_t));
    if (rc == 0) {
        rc = os_mbuf_append(ctxt->om, &zero, sizeof(uint8_t));
    }
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int ImprovServer::gattSvrChrCapabilities(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    rc = os_mbuf_append(ctxt->om, &capabilities, sizeof(uint8_t));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

esp_err_t ImprovServer::onWifiProvisioning(const char *ssid, const char *password, void *args) 
{
    esp_err_t err = onProvision(ssid, password, args);
    return err;
}

esp_err_t ImprovServer::initServer()
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    memset(&svc, 0, sizeof(struct ble_gatt_svc_def));
    memset(&nullSvc, 0, sizeof(struct ble_gatt_svc_def));
    memset(&statusChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&errorChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&rpcWriteChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&rpcResultChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&capabilitiesChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&nullChr, 0, sizeof(struct ble_gatt_svc_def));

    svc.type = BLE_GATT_SVC_TYPE_PRIMARY;
    svc.uuid = &serviceUuid->u;

    ble_uuid128_t *statusUuid = strToUuid(improv::STATUS_UUID);
    statusChr.uuid = &statusUuid->u;
    statusChr.access_cb = gattSvrChrStatus;
    statusChr.arg = (void *)this;
    statusChr.val_handle = &statusHandle;
    statusChr.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;

    ble_uuid128_t *errorUuid = strToUuid(improv::ERROR_UUID); 
    errorChr.uuid = &errorUuid->u;
    errorChr.access_cb = gattSvrChrError;
    errorChr.arg = (void *)this;
    errorChr.val_handle = &errorHandle;
    errorChr.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;

    ble_uuid128_t *rpcWriteUuid = strToUuid(improv::RPC_COMMAND_UUID);
    rpcWriteChr.uuid = &rpcWriteUuid->u;
    rpcWriteChr.access_cb = gattSvrChrRpcWrite;
    rpcWriteChr.arg = (void *)this;
    rpcWriteChr.val_handle = NULL;
    rpcWriteChr.flags = BLE_GATT_CHR_F_WRITE;

    ble_uuid128_t *rpcResultUuid = strToUuid(improv::RPC_RESULT_UUID);
    rpcResultChr.uuid = &rpcResultUuid->u;
    rpcResultChr.access_cb = gattSvrChrRpcResult;
    rpcResultChr.arg = (void *)this;
    rpcResultChr.val_handle = &rpcResultHandle;
    rpcResultChr.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;

    ble_uuid128_t *capabilitiesUuid = strToUuid(improv::CAPABILITIES_UUID);
    capabilitiesChr.uuid = &capabilitiesUuid->u;
    capabilitiesChr.access_cb = gattSvrChrCapabilities;
    capabilitiesChr.arg = (void *)this;
    capabilitiesChr.val_handle = &capabilitiesHandle;
    capabilitiesChr.flags = BLE_GATT_CHR_F_READ;

    svc.characteristics = new struct ble_gatt_chr_def[] {
        statusChr,
        errorChr,
        rpcWriteChr,
        rpcResultChr,
        capabilitiesChr,
        nullChr,
    };

    // NimBLE does not support manually adding 2902 descriptors as they are automatically 
    // added when the characteristic has notifications or indications enabled.
    memset(&devSvc, 0, sizeof(struct ble_gatt_svc_def));
    memset(&devManufChr, 0, sizeof(struct ble_gatt_chr_def));
    memset(&devModelChr, 0, sizeof(struct ble_gatt_chr_def));

    infoUuid = BLE_UUID16_INIT(GATT_DEVICE_INFO_UUID);
    devSvc.type = BLE_GATT_SVC_TYPE_PRIMARY;
    devSvc.uuid = &infoUuid.u;

    manufUuid = BLE_UUID16_INIT(GATT_MANUFACTURER_NAME_UUID);
    devManufChr.uuid = &manufUuid.u;
    devManufChr.access_cb = gattSvrChrDeviceInfo;
    devManufChr.arg = (void *)this;
    devManufChr.flags = BLE_GATT_CHR_F_READ;

    modelUuid = BLE_UUID16_INIT(GATT_MODEL_NUMBER_UUID);
    devModelChr.uuid = &modelUuid.u;
    devModelChr.access_cb = gattSvrChrDeviceInfo;
    devModelChr.arg = (void *)this;
    devModelChr.flags = BLE_GATT_CHR_F_READ;

    devSvc.characteristics = new struct ble_gatt_chr_def[] {
        devManufChr,
        devModelChr,
        nullChr,
    };

    auto svcs = new struct ble_gatt_svc_def[] {
        svc,
        devSvc,
        nullSvc,
    };
    int rc;

    rc = ble_gatts_count_cfg(svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed, rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed, rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

}