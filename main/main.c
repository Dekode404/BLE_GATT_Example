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

#define URL_SITE "parasdefence"

void BLE_app_on_sync(void)
{
}

void Host_task(void *param)
{
    nimble_port_run(); /* Run the nimble port */
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); /* Initialize NVS flash */

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init()); /* Initiate the Nimble BLE Hardware control interference and controller */

    nimble_port_init(); /* Initiate the nimble port for the BLE */

    ble_hs_cfg.sync_cb = BLE_app_on_sync; /* Set the synchronization callback */
    nimble_port_freertos_init(Host_task); /* Initialize NimBLE port with FreeRTOS */
}
