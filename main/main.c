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

#define DEVICE_NAME "HARTMAN_SIGHT"             /* Define the device name */
#define DEVICE_INFO_SERVICE 0x180A              /* Define the device information service UUID */
#define MANUFACTURER_NAME 0x2A29                /* Define the manufacturer name characteristic UUID */
#define DEVICE_BATTERY_SERVICE 0x180F           /* Define the device battery service UUID */
#define BATTERY_LEVEL 0x2A19                    /* Define the battery level characteristic UUID */
#define BATTERY_CLIENT_CONFIG_DESCRIPTOR 0x2902 /* Define the battery client configuration descriptor UUID */
#define BATTERY_INFORMATION 0x2BEC              /* Define the battery information characteristic UUID */

void BLE_app_advertise(void); /* Function prototype for advertising */

uint8_t BLE_Addr_Type;                                   /* Variable to hold the BLE address type */
uint16_t Battery_level_characteristic_attribute_handler; /* Variable to hold the battery level characteristic attribute handler */
uint16_t connection_handler;                             /* Variable to hold the connection handler */
static xTimerHandle Battery_Timer_Handler;               /* Timer handler for the battery level update */

uint8_t Battery_Level = 100; /* Variable to hold the battery level */

/* Configuration array for Client Characteristic Configuration Descriptor (CCCD):
 * 0x01: Enable notifications
 * 0x00: Disable notifications
 *
 * The `static` keyword ensures that the `config` array retains its value between function calls,
 * making it persistent and not re-initialized with every function call.
 * The array is initialized with {0x01, 0x00}, meaning that notifications are enabled by default.
 * @link https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Descriptors/org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
 */
static uint8_t config[2] = {0x01, 0x00};

/**
 * @brief GATT descriptor access callback for battery level notifications
 *
 * This function handles read and write operations to the battery level
 * descriptor. It updates the configuration based on the client's request
 * and starts or stops the battery level timer accordingly.
 *
 * @param conn_handle Connection handle
 * @param attr_handle Attribute handle
 * @param ctxt GATT access context
 * @param arg User-defined argument
 * @return int Returns 0 on success
 */
static int Battery_Level_Descriptor(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) /* Check if the operation is a read descriptor */
    {
        os_mbuf_append(ctxt->om, &config, sizeof(config)); /* Append the configuration to the output buffer */
    }
    else /* If the operation is not read, it must be write */
    {
        memcpy(config, ctxt->om->om_data, ctxt->om->om_len); /* Copy the configuration from the input buffer */
    }

    if (config[0] == 0x01) /* If the configuration is set to notify */
    {
        xTimerStart(Battery_Timer_Handler, 0); /* Start the battery timer */
    }
    else /* If the configuration is set to stop notifying */
    {
        xTimerStop(Battery_Timer_Handler, 0); /* Stop the battery timer */
    }
    return 0; /* Return success */
}

static int Custom_Service(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    printf("Incoming Message :- %.*s \n", ctxt->om->om_len, ctxt->om->om_data); /* Print the incoming message */
    return 0;                                                                   /* Return success */
}

static int Device_Battery_Level(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const uint8_t message = 100;                         /* Battery level message */
    os_mbuf_append(ctxt->om, &message, sizeof(message)); /* Append the message to the output buffer */
    return 0;                                            /* Return success */
}

static int Device_Battery_Information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *message = "NOT CONNECTED";              /* Battery information message */
    os_mbuf_append(ctxt->om, message, strlen(message)); /* Append the message to the output buffer */
    return 0;                                           /* Return success */
}

static int Device_Info(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *message = "PARAS DEFENSE";              /* Device information message */
    os_mbuf_append(ctxt->om, message, strlen(message)); /* Append the message to the output buffer */
    return 0;                                           /* Return success */
}

/**
 * @brief GATT service definitions
 *
 * This array defines the GATT services and characteristics for the BLE application.
 * It includes the Device Information Service, Battery Service, and a Custom Service.
 *
 * - Device Information Service:
 *   - Characteristic: Manufacturer Name (Read-only)
 *
 * - Battery Service:
 *   - Characteristic: Battery Information (Read-only)
 *   - Characteristic: Battery Level (Read and Notify)
 *     - Descriptor: Client Configuration (Read and Write)
 *
 * - Custom Service:
 *   - Characteristic: Custom Characteristic (Write-only)
 *
 * The services are defined as primary services. Each characteristic within a service
 * has a UUID, flags indicating its properties (e.g., read, write, notify), and an
 * access callback function to handle read/write operations.
 */
static const struct ble_gatt_svc_def GATT_Service[] =
    {
        {.type = BLE_GATT_SVC_TYPE_PRIMARY,               /* Primary service */
         .uuid = BLE_UUID16_DECLARE(DEVICE_INFO_SERVICE), /* Device information service UUID */
         .characteristics = (struct ble_gatt_chr_def[]){{
                                                            .uuid = BLE_UUID16_DECLARE(MANUFACTURER_NAME), /* Manufacturer name characteristic UUID */
                                                            .flags = BLE_GATT_CHR_F_READ,                  /* Read flag */
                                                            .access_cb = Device_Info                       /* Access callback for device information */
                                                        },
                                                        {0}}},

        {.type = BLE_GATT_SVC_TYPE_PRIMARY,                  /* Primary service */
         .uuid = BLE_UUID16_DECLARE(DEVICE_BATTERY_SERVICE), /* Battery service UUID */
         .characteristics = (struct ble_gatt_chr_def[]){{
                                                            .uuid = BLE_UUID16_DECLARE(BATTERY_INFORMATION), /* Battery information characteristic UUID */
                                                            .flags = BLE_GATT_CHR_F_READ,                    /* Read flag */
                                                            .access_cb = Device_Battery_Information          /* Access callback for battery information */
                                                        },
                                                        {.uuid = BLE_UUID16_DECLARE(BATTERY_LEVEL),                     /* Battery level characteristic UUID */
                                                         .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,          /* Read and notify flags */
                                                         .access_cb = Device_Battery_Level,                             /* Access callback for battery level */
                                                         .val_handle = &Battery_level_characteristic_attribute_handler, /* Handle for the battery level characteristic */
                                                         .descriptors = (struct ble_gatt_dsc_def[]){{
                                                                                                        .uuid = BLE_UUID16_DECLARE(BATTERY_CLIENT_CONFIG_DESCRIPTOR), /* Client configuration descriptor UUID */
                                                                                                        .att_flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,      /* Read and write flags */
                                                                                                        .access_cb = Battery_Level_Descriptor                         /* Access callback for battery level descriptor */
                                                                                                    },
                                                                                                    {0}}},
                                                        {0}}},

        {.type = BLE_GATT_SVC_TYPE_PRIMARY,                                                                                           /* Primary service */
         .uuid = BLE_UUID128_DECLARE(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff), /* Custom service UUID */
         .characteristics = (struct ble_gatt_chr_def[]){{
                                                            .uuid = BLE_UUID128_DECLARE(0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00), /* Custom characteristic UUID */
                                                            .flags = BLE_GATT_CHR_F_WRITE,                                                                                               /* Write flag */
                                                            .access_cb = Custom_Service                                                                                                  /* Access callback for custom service */
                                                        },
                                                        {0}}},
        {0}};

/**
 * @brief BLE GAP event handler
 *
 * This function handles various GAP events such as connection, disconnection,
 * advertising complete, and subscription. It logs the events and performs
 * appropriate actions based on the event type.
 *
 * @param event Pointer to the BLE GAP event structure.
 * @param arg Pointer to user-defined argument.
 * @return int 0 on success, error code otherwise.
 */
int BLE_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) /* Switch on the type of GAP event */
    {
    case BLE_GAP_EVENT_CONNECT:                                                                    /* Event type: Connection */
        ESP_LOGI("GAP", "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "FAILED"); /* Log the connection event status */
        if (event->connect.status != 0)                                                            /* If the connection failed */
        {
            BLE_app_advertise(); /* Restart advertising */
        }
        connection_handler = event->connect.conn_handle; /* Save the connection handle */
        break;

    case BLE_GAP_EVENT_DISCONNECT:                   /* Event type: Disconnection */
        ESP_LOGI("GAP", "BLE_GAP_EVENT_DISCONNECT"); /* Log the disconnection event */
        xTimerStop(Battery_Timer_Handler, 0);        /* Start the battery timer */
        BLE_app_advertise();                         /* Restart advertising */
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:                   /* Event type: Advertising complete */
        ESP_LOGI("GAP", "BLE_GAP_EVENT_ADV_COMPLETE"); /* Log the advertising complete event */
        BLE_app_advertise();                           /* Restart advertising */
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:                   /* Event type: Subscription */
        ESP_LOGI("GAP", "BLE_GAP_EVENT_SUBSCRIBE"); /* Log the subscribe event */

        if (event->subscribe.attr_handle == Battery_level_characteristic_attribute_handler) /* Check if the subscription is for the battery level characteristic */
        {
            xTimerStart(Battery_Timer_Handler, 0); /* Start the battery timer */
        }
        break;

    default: /* Default case for unhandled events */
        break;
    }

    return 0; /* Return success */
}

/**
 * @brief Start BLE advertising
 *
 * This function sets up the BLE advertising fields and parameters, and then
 * starts the advertising process. It configures the advertising data with
 * the device name and TX power level, and sets the advertising parameters
 * such as connection mode and discovery mode.
 */
void BLE_app_advertise(void)
{
    struct ble_hs_adv_fields fields;    /* Declare advertising fields */
    memset(&fields, 0, sizeof(fields)); /* Clear the advertising fields structure */

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_DISC_LTD; /* Set the advertising flags for general and limited discoverability */
    fields.tx_pwr_lvl_is_present = true;                          /* Indicate that the TX power level is present */
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;               /* Set the TX power level to auto */

    fields.name = (uint8_t *)ble_svc_gap_device_name();  /* Set the device name for advertising */
    fields.name_len = strlen(ble_svc_gap_device_name()); /* Set the length of the device name */
    fields.name_is_complete = true;                      /* Indicate that the device name is complete */

    ble_gap_adv_set_fields(&fields); /* Set the advertising fields */

    struct ble_gap_adv_params adv_params;         /* Declare advertising parameters */
    memset(&adv_params, 0, sizeof(adv_params));   /* Clear the advertising parameters structure */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; /* Set the connection mode to undirected */
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; /* Set the discovery mode to general */

    ble_gap_adv_start(BLE_Addr_Type, NULL, BLE_HS_FOREVER, &adv_params, BLE_gap_event, NULL); /* Start advertising */
}

/**
 * @brief BLE synchronization callback
 *
 * This function is called when the BLE stack has completed synchronization.
 * It infers the BLE address type automatically and starts the advertising
 * process.
 */
void BLE_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &BLE_Addr_Type); /* Infer the BLE address type automatically */
    BLE_app_advertise();                     /* Start advertising */
}

/**
 * @brief Task function to run the NimBLE port
 *
 * This function is the main task that runs the NimBLE port. It is intended
 * to be called within a FreeRTOS task to manage the BLE stack's execution.
 *
 * @param param Pointer to the task's parameter (unused)
 */
void Host_task(void *param)
{
    nimble_port_run(); /* Run the NimBLE port */
}

/**
 * @brief Timer callback function to update battery level
 *
 * This function is called periodically by a timer to simulate the battery
 * level update. It decrements the battery level, resets it to 100 when it
 * reaches 0, prints the current battery level, and sends a notification
 * to the connected client with the updated battery level.
 */
void Update_Battery_Timer()
{
    if (Battery_Level-- == 0) /* Decrement the battery level, reset if it reaches 0 */
    {
        Battery_Level = 100; /* Reset battery level to 100 */
    }

    printf("Reporting battery level %d\n", Battery_Level); /* Print the battery level */

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&Battery_Level, sizeof(Battery_Level)); /* Create an mbuf from the battery level */

    ble_gattc_notify_custom(connection_handler, Battery_level_characteristic_attribute_handler, om); /* Notify the client with the battery level */
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); /* Initialize NVS flash */

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init()); /* Initialize the NimBLE HCI and controller */
    nimble_port_init();                                    /* Initialize the NimBLE port */

    ble_svc_gap_device_name_set(DEVICE_NAME); /* Set the device name */
    ble_svc_gap_init();                       /* Initialize the GAP service */

    ble_svc_gatt_init();               /* Initialize the GATT service */
    ble_gatts_count_cfg(GATT_Service); /* Count the GATT services */
    ble_gatts_add_svcs(GATT_Service);  /* Add the GATT services */

    ble_hs_cfg.sync_cb = BLE_app_on_sync; /* Set the synchronization callback */

    Battery_Timer_Handler = xTimerCreate("Update_Battery_Timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, Update_Battery_Timer); /* Create the battery timer */

    nimble_port_freertos_init(Host_task); /* Initialize NimBLE port with FreeRTOS */
}
