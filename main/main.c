/* BLE GATT example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>                       /* This is the standard C lib used for the printf statement in this project */
#include <nvs_flash.h>                   /* This is ESP lib used to initiate the NVS flsh used for the bluetooth application */
#include <esp_log.h>                     /* This is ESP lib used to loig the error */
#include <esp_nimble_hci.h>              /* This is ESP lib used for the HOST and CONTROLLER interface */
#include <nimble/nimble_port.h>          /* This is ESP lib used for initiate the nimbale port for the bluetooth application */
#include <nimble/nimble_port_freertos.h> /* This is ESP lib used for create the task for the nimble bluetooth application */
#include <host/ble_hs.h>                 /* This is ESP lib used for the ble host controller */
#include <services/gap/ble_svc_gap.h>    /* This is ESP lib used for initiate the ble GAP service */
#include "services/gatt/ble_svc_gatt.h"  /* This is ESP lib used for initiate the ble GATT service */

#define DEVICE_NAME "HARTMAN_SIGHT"

#define DEVICE_INFO_SERVICE 0x180A
#define MANUFACTURER_NAME 0x2A29

#define DEVICE_BATTERY_SERVICE 0x180F
#define BATTERY_LEVEL 0x2A19
#define BATTERY_CLIENT_CONFIG_DESCRIPTOR 0x2902
#define BATTERY_INFORMATION 0x2BEC

void BLE_app_advertise(void);

uint8_t BLE_Addr_Type;

uint8_t Battery_Level = 100;

uint16_t Battery_level_characteristic_attribute_handler;
uint16_t connection_handler;

static xTimerHandle Battery_Timer_Handler;

// see https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Descriptors/org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
static uint8_t config[2] = {0x01, 0x00};

static int Battery_Level_Descriptor(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        os_mbuf_append(ctxt->om, &config, sizeof(config));
    }
    else
    {
        memcpy(config, ctxt->om->om_data, ctxt->om->om_len);
    }

    if (config[0] == 0x01)
    {
        xTimerStart(Battery_Timer_Handler, 0);
    }
    else
    {
        xTimerStop(Battery_Timer_Handler, 0);
    }
    return 0;
}

static int Custom_Service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Incoming Message :- %.*s \n", ctxt->om->om_len, ctxt->om->om_data);
    return 0;
}

static int Device_Battery_Level(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const uint8_t message = 100;
    os_mbuf_append(ctxt->om, &message, sizeof(message));
    return 0;
}

static int Device_Battery_Information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *message = "NOT CONNECTED";
    os_mbuf_append(ctxt->om, message, strlen(message));
    return 0;
}

static int Device_Info(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *message = "PARAS DEFENSE";
    os_mbuf_append(ctxt->om, message, strlen(message));
    return 0;
}

static const struct ble_gatt_svc_def GATT_Service[] =
    {
        {.type = BLE_GATT_SVC_TYPE_PRIMARY,
         .uuid = BLE_UUID16_DECLARE(DEVICE_INFO_SERVICE),
         .characteristics = (struct ble_gatt_chr_def[]){
             {.uuid = BLE_UUID16_DECLARE(MANUFACTURER_NAME),
              .flags = BLE_GATT_CHR_F_READ,
              .access_cb = Device_Info},
             {0}}},

        {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID16_DECLARE(DEVICE_BATTERY_SERVICE), .characteristics = (struct ble_gatt_chr_def[]){{.uuid = BLE_UUID16_DECLARE(BATTERY_INFORMATION), .flags = BLE_GATT_CHR_F_READ, .access_cb = Device_Battery_Information}, {.uuid = BLE_UUID16_DECLARE(BATTERY_LEVEL), .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, .access_cb = Device_Battery_Level, .val_handle = &Battery_level_characteristic_attribute_handler, .descriptors = (struct ble_gatt_dsc_def[]){{.uuid = BLE_UUID16_DECLARE(BATTERY_CLIENT_CONFIG_DESCRIPTOR), .att_flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE, .access_cb = Battery_Level_Descriptor}, {0}}}}},

        {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = BLE_UUID128_DECLARE(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff), .characteristics = (struct ble_gatt_chr_def[]){{.uuid = BLE_UUID128_DECLARE(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00), .flags = BLE_GATT_CHR_F_WRITE, .access_cb = Custom_Service}, {0}}},
        {0}};

int BLE_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "FAILED");
        if (event->connect.status != 0)
        {
            BLE_app_advertise();
        }
        connection_handler = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_DISCONNECT");
        BLE_app_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_ADV_COMPLETE");
        BLE_app_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_SUBSCRIBE");

        if (event->subscribe.attr_handle == Battery_level_characteristic_attribute_handler)
        {
            xTimerStart(Battery_Timer_Handler, 0);
        }
        break;

    default:
        break;
    }

    return 0;
}

void BLE_app_advertise(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_DISC_LTD;
    fields.tx_pwr_lvl_is_present = true;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen(ble_svc_gap_device_name());
    fields.name_is_complete = true;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_Addr_Type, NULL, BLE_HS_FOREVER, &adv_params, BLE_gap_event, NULL);
}

void BLE_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &BLE_Addr_Type);
    BLE_app_advertise();
}

void Host_task(void *param)
{
    nimble_port_run(); /* Run the nimble port */
}

void Update_Battery_Timer()
{
    if (Battery_Level-- == 0)
    {
        Battery_Level = 100;
    }

    printf("Reporting battery level %d\n", Battery_Level);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&Battery_Level, sizeof(Battery_Level));

    ble_gattc_notify_custom(connection_handler, Battery_level_characteristic_attribute_handler, om);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); /* Initialize NVS flash */

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init()); /* Initiate the Nimble BLE Hardware control interference and controller */
    nimble_port_init();                                    /* Initiate the nimble port for the BLE */

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();

    ble_svc_gatt_init();
    ble_gatts_count_cfg(GATT_Service);
    ble_gatts_add_svcs(GATT_Service);

    ble_hs_cfg.sync_cb = BLE_app_on_sync; /* Set the synchronization callback */

    Battery_Timer_Handler = xTimerCreate("Update_Battery_Timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, Update_Battery_Timer);

    nimble_port_freertos_init(Host_task); /* Initialize NimBLE port with FreeRTOS */
}
