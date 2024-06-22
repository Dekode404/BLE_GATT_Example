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
#include <services/gap/ble_svc_gap.h>    /* This is ESP lib used for initiate the ble service */

#define DEVICE_NAME "HARTMAN_SIGHT"

void BLE_app_advertise(void);

uint8_t ble_addr_type;

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

    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, BLE_gap_event, NULL);
}

void BLE_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    BLE_app_advertise();
}

void Host_task(void *param)
{
    nimble_port_run(); /* Run the nimble port */
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); /* Initialize NVS flash */

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init()); /* Initiate the Nimble BLE Hardware control interference and controller */
    nimble_port_init();                                    /* Initiate the nimble port for the BLE */

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();

    ble_hs_cfg.sync_cb = BLE_app_on_sync; /* Set the synchronization callback */
    nimble_port_freertos_init(Host_task); /* Initialize NimBLE port with FreeRTOS */
}
