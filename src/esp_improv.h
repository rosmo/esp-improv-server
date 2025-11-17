/*
 * SPDX-FileCopyrightText: 2025 Taneli Leppä
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _ESP_IMPROV_H
#define _ESP_IMPROV_H

#include "nimble/ble.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"

#include "improv.h"
#include "esp_central.h"

namespace improvserver
{

/* Device Information configuration */
#define GATT_DEVICE_INFO_UUID       0x180A
#define GATT_MANUFACTURER_NAME_UUID 0x2A29
#define GATT_MODEL_NUMBER_UUID      0x2A24

#define ADVERTISE_NAME_EVERY_MSECS 5000
#define ADVERTISE_NAME_FOR_MSECS   1000
#define AFTER_PROVISION_DELAY      2500

typedef esp_err_t (*wifi_provision_fn)(const char *ssid, const char *password, void *args);

class ImprovServer 
{
    protected:
    static const char *TAG;
    static std::string *deviceName;
    static std::string *manufacturerName;
    static std::string *modelName;
    static uint8_t capabilities;

    static bool advertiseName;
    static improv::State state;
    static improv::Error error;
    static bool advertiseOn;
    static bool advertising;

    static uint16_t errorHandle;
    static uint16_t statusHandle;
    static uint16_t rpcResultHandle;  
    static uint16_t capabilitiesHandle;    
    static TaskHandle_t advertiseTaskHandle;

    static struct ble_gatt_svc_def svc;
    static struct ble_gatt_svc_def devSvc;
    static struct ble_gatt_svc_def nullSvc;
    static struct ble_gatt_chr_def statusChr, errorChr, rpcWriteChr, rpcResultChr, capabilitiesChr;
    static struct ble_gatt_chr_def devManufChr, devModelChr;
    static struct ble_gatt_chr_def nullChr;

    static ble_uuid128_t *serviceUuid;
    ble_uuid16_t infoUuid;
    ble_uuid16_t manufUuid;
    ble_uuid16_t modelUuid;

    static esp_err_t gapEvent(struct ble_gap_event *event, void *arg);
    static esp_err_t advertise();
    static void hostTask(void *param);
    static void advertiseTask(void *param);
    static void onSync();
    static void onReset(int reason);

    static int gattSvrChrDeviceInfo(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gatt_svr_chr_test(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gattSvrChrStatus(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gattSvrChrError(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gattSvrChrRpcWrite(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gattSvrChrRpcResult(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
    static int gattSvrChrCapabilities(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

    static int gattSvrChrStatusNotify();
    static int gattSvrChrErrorNotify();

    wifi_provision_fn onProvision;
    void *onProvisionArgs;

    esp_err_t initServer();
    static ble_uuid128_t *strToUuid(const char *uuidStr);
    esp_err_t onWifiProvisioning(const char *ssid, const char *password, void *args);

    public:
    static uint8_t addrType;
    static uint16_t connHandle;

    ImprovServer(const char *btname, const char *manufacturer, const char *model) {
        ImprovServer::manufacturerName = new std::string(manufacturer);
        ImprovServer::modelName = new std::string(model);
        ImprovServer::deviceName = new std::string(btname);
    };
    ~ImprovServer() {
        delete ImprovServer::manufacturerName;
        delete ImprovServer::modelName;
        delete ImprovServer::deviceName;
    }
    esp_err_t Initialize(wifi_provision_fn onProvisionCallback, void *args);
    esp_err_t StopAdvertising();
    esp_err_t StartAdvertising();
};

} 
#endif