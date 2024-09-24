// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.
//
// -----
//
// Original code from the bluetooth/esp_hid_host example of ESP-IDF license:
//
// Copyright 2017-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define __BT_KEYBOARD__ 1
#include "bt_keyboard.h"

#include <cstring>
#include <iostream> //JMH added
#include <stdio.h>  //JMH added
#include <string.h> //JMH added
#include "esp_hidh_private.h"
extern "C"
{
#include "btc/btc_ble_storage.h"
}
#define SCAN 1
/* uncomment the following, to get a detailed listing
 *  of gattc call back events (via USB serial terminal)
 */
//#define gatDebug
//#define gatDebug1
/* uncomment to print all devices that were seen during a scan */
#define GAP_DBG_PRINTF(...) printf(__VA_ARGS__)

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(*a))

#define WAIT_BT_CB() xSemaphoreTake(bt_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BT_CB() xSemaphoreGive(bt_hidh_cb_semaphore)

#define WAIT_BLE_CB() xSemaphoreTake(ble_hidh_cb_semaphore, portMAX_DELAY)
#define SEND_BLE_CB() xSemaphoreGive(ble_hidh_cb_semaphore)
/*JMH 20230205 - replaced the following 2 lines xSemaphoreHandle with SemaphoreHandle_t */

SemaphoreHandle_t BTKeyboard::bt_hidh_cb_semaphore = nullptr;
SemaphoreHandle_t BTKeyboard::ble_hidh_cb_semaphore = nullptr;

const char *BTKeyboard::gap_bt_prop_type_names[] = {"", "BDNAME", "COD", "RSSI", "EIR"};
const char *BTKeyboard::ble_gap_evt_names[] = {"ADV_DATA_SET_COMPLETE", "SCAN_RSP_DATA_SET_COMPLETE", "SCAN_PARAM_SET_COMPLETE", "SCAN_RESULT", "ADV_DATA_RAW_SET_COMPLETE", "SCAN_RSP_DATA_RAW_SET_COMPLETE", "ADV_START_COMPLETE", "SCAN_START_COMPLETE", "AUTH_CMPL", "KEY", "SEC_REQ", "PASSKEY_NOTIF", "PASSKEY_REQ", "OOB_REQ", "LOCAL_IR", "LOCAL_ER", "NC_REQ", "ADV_STOP_COMPLETE", "SCAN_STOP_COMPLETE", "SET_STATIC_RAND_ADDR", "UPDATE_CONN_PARAMS", "SET_PKT_LENGTH_COMPLETE", "SET_LOCAL_PRIVACY_COMPLETE", "REMOVE_BOND_DEV_COMPLETE", "CLEAR_BOND_DEV_COMPLETE", "GET_BOND_DEV_COMPLETE", "READ_RSSI_COMPLETE", "UPDATE_WHITELIST_COMPLETE"};
const char *BTKeyboard::bt_gap_evt_names[] = {"DISC_RES", "DISC_STATE_CHANGED", "RMT_SRVCS", "RMT_SRVC_REC", "AUTH_CMPL", "PIN_REQ", "CFM_REQ", "KEY_NOTIF", "KEY_REQ", "READ_RSSI_DELTA"};
const char *BTKeyboard::ble_addr_type_names[] = {"PUBLIC", "RANDOM", "RPA_PUBLIC", "RPA_RANDOM"};

const char BTKeyboard::shift_trans_dict[] =
    "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ1!2@3#4$5%6^7&8*9(0)"
    "\r\231\033\033\b\b\t\t  -_=+[{]}\\|??;:'\"`~,<.>/?"
    "\200\200"                                          // CAPS LOC
    "\201\201\202\202\203\203\204\204\205\205\206\206"  // F1..F6
    "\207\207\210\210\211\211\212\212\213\213\214\214"  // F7..F12
    "\215\215\216\216\217\217"                          // PrintScreen ScrollLock Pause
    "\220\220\221\221\222\222\177\177"                  // Insert Home PageUp Delete
    "\223\223\224\224\225\225\226\226\227\227\230\230"; // End PageDown Right Left Dow Up

static bool Shwgattcb = false;//used for debugging BLE reconnect  
static bool PairFlg1 = false;
bool PauseFlg = false;
static esp_bd_addr_t Prd_bda;
static esp_ble_addr_type_t Prd_addr_type;
BTKeyboard *BLE_KyBrd = nullptr;
char msgbuf[256];
/*moved this here to also support GATT code*/
static esp_ble_scan_params_t hid_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
};

/*JMH GATT stuff*/
const char *BLE_TAG = "BLE_GATT";
#define PROFILE_BATTERY_SERVICE 0 // 0x180F
// #define PROFILE_HID_SERVICE 1
#define INVALID_HANDLE 0
#define PROFILE_NUM 1
static bool connect_ble = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

struct gattc_profile_inst
{
  esp_gattc_cb_t gattc_cb;
  uint16_t gattc_if;
  uint16_t app_id;
  uint16_t conn_id;
  uint16_t service_start_handle;
  uint16_t service_end_handle;
  uint16_t char_handle;
  esp_bd_addr_t remote_bda;
};
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_battery_service_event_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_BATTERY_SERVICE] = {
        .gattc_cb = gattc_battery_service_event_cb,
        .gattc_if = ESP_GATT_IF_NONE,
    },

    //    [PROFILE_DEVICE_INFO] = {
    //        .gattc_cb = gattc_profile_device_info_event_handler,
    //        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    //    },

    //  [PROFILE_HID_SERVICE] = {
    //      .gattc_cb = esp_hidh_gattc_event_handler,
    //      .gattc_if = ESP_GATT_IF_NONE,
    //  },
};
static esp_bt_uuid_t remote_filter_battery_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_BATTERY_SERVICE_SVC,
    },
};
static esp_bt_uuid_t remote_battery_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_BATTERY_LEVEL,
    },
};
static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
    },
};

static void gattc_battery_service_event_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
  const char *TAG_evnt = "BAT SVC EVNT";
  esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

  // int event1 = 2;
  switch (event)
  {
  case ESP_GATTC_REG_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)REG_EVT");
#endif
    esp_err_t scan_ret = esp_ble_gap_set_scan_params(&hid_scan_params);
    if (scan_ret)
    {
      ESP_LOGE(TAG_evnt, "(battery)set scan params error, error code = %x", scan_ret);
    }
  }
  break;
  case ESP_GATTC_UNREG_EVT:
  {
    ESP_LOGE(TAG_evnt, "ESP_GATTC_UNREG_EVT");
  }
  break;
  case ESP_GATTC_OPEN_EVT:
  {
    if (param->open.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)open failed, status %d", p_data->open.status);
      break;
    }
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_OPEN_EVT Ok");
#endif
  }
  break;
  case ESP_GATTC_READ_CHAR_EVT:
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_READ_CHAR_EVT");
#endif
    if (p_data->read.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)read char failed, error status = %x", p_data->read.status);
      break;
    }
// ESP_LOGI(TAG_evnt, "(battery)read char success ");
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "p_data->read.value_len %d", p_data->read.value_len);
#endif
    if (p_data->read.value_len > 0)
    {
      char *char_pointer = (char *)p_data->read.value;
      if (p_data->read.value_len > 1)
      {

        /*now make sure we have a NULL terminated string*/
        int i = 0;
        while ((char_pointer[i] != 0) && (i < p_data->read.value_len))
        {
          i++;
        }
        if (i == p_data->read.value_len)
          char_pointer[p_data->read.value_len] = 0;
#ifdef gatDebug          
        ESP_LOGI(TAG_evnt, "Battery Info: %s", char_pointer);
#endif
      }
      else
      {
        // battery_level = (int)*char_pointer;
        BLE_KyBrd->str_battery_level((int)*char_pointer);
#ifdef gatDebug
        ESP_LOGI(TAG_evnt, "Battery level: %d%%", BLE_KyBrd->get_battery_level());
#endif
      }
    }

    break;
  case ESP_GATTC_WRITE_CHAR_EVT:
    if (p_data->write.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)write char failed, error status = %x", p_data->write.status);
      break;
    }
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)write char success ");
#endif
    break;
  case ESP_GATTC_CLOSE_EVT:
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_CLOSE_EVT");
#endif
    BLE_KyBrd->str_battery_level(0);
    /* free char_elem_result */
    // free(char_elem_result);
    break;
  case ESP_GATTC_SEARCH_CMPL_EVT:
    if (p_data->search_cmpl.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)search service failed, error status = %x", p_data->search_cmpl.status);
      break;
    }
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_SEARCH_CMPL_EVT");
#endif
    if (get_server)
    {
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery)get_server");
#endif
      uint16_t count = 0;
      esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                              p_data->search_cmpl.conn_id,
                                                              ESP_GATT_DB_CHARACTERISTIC,
                                                              gl_profile_tab[PROFILE_BATTERY_SERVICE].service_start_handle,
                                                              gl_profile_tab[PROFILE_BATTERY_SERVICE].service_end_handle,
                                                              INVALID_HANDLE,
                                                              &count);
      if (status != ESP_GATT_OK)
      {
        ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_get_attr_count error");
      }

      if (count > 0)
      {
        char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
        if (!char_elem_result)
        {
          ESP_LOGE(TAG_evnt, "(battery)gattc no mem");
        }
        else
        {
          status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                  p_data->search_cmpl.conn_id,
                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].service_start_handle,
                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].service_end_handle,
                                                  remote_battery_filter_char_uuid, // remote_filter_char_uuid,
                                                  char_elem_result,
                                                  &count);
          if (status != ESP_GATT_OK)
          {
            ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_get_char_by_uuid error");
          }

          /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
          if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY))
          {
            gl_profile_tab[PROFILE_BATTERY_SERVICE].char_handle = char_elem_result[0].char_handle;
            esp_err_t errorcode = esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_BATTERY_SERVICE].remote_bda, char_elem_result[0].char_handle);

            if (errorcode)
            {
#ifdef gatDebug
              ESP_LOGI(TAG_evnt, "(battery)esp_ble_gattc_register_for_notify error, code: %x", errorcode);
#endif
            }
            else
            {
#ifdef gatDebug
              ESP_LOGI(TAG_evnt, "(battery)esp_ble_gattc_register_for_notify success; %d Attributes", count);
#endif
            }
          }
        }
        /* free char_elem_result */
        free(char_elem_result);
      }
      else
      {
        ESP_LOGE(TAG_evnt, "(battery)no char found");
      }
    }
    else
    {
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery) 'Get server' flag not set");
#endif
    }
    break;
  case ESP_GATTC_SEARCH_RES_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_SEARCH_RES_EVT");
#endif
    esp_gatt_srvc_id_t *srvc_id = (esp_gatt_srvc_id_t *)&p_data->search_res.srvc_id;

#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)srvc_id_len:%d [%x]", srvc_id->id.uuid.len, srvc_id->id.uuid.len);
#endif
    //        for (uint8_t i = 0; i < 15; i++)
    //        {
    //
    // #ifdef gatDebug
    // ESP_LOGI(TAG_evnt, "(battery)srvc_id:%d [%x]", i, srvc_id->id.uuid.uuid.uuid128[i]);
    // #endif
    //        }

    if (srvc_id->id.uuid.len == ESP_UUID_LEN_16 && srvc_id->id.uuid.uuid.uuid16 == ESP_GATT_UUID_BATTERY_SERVICE_SVC)
    {
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery)service found 2");
#endif
      get_server = true;
      gl_profile_tab[PROFILE_BATTERY_SERVICE].service_start_handle = p_data->search_res.start_handle;
      gl_profile_tab[PROFILE_BATTERY_SERVICE].service_end_handle = p_data->search_res.end_handle;
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery)UUID16: %x", srvc_id->id.uuid.uuid.uuid16);
#endif
    }
    break;
  }
  case ESP_GATTC_READ_DESCR_EVT:
  {
    // ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_READ_DESCR_EVT");
    if (p_data->read.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)read descr failed, error status = %x", p_data->write.status);
      break;
    }
    // ESP_LOGI(TAG_evnt, "(battery)read descr success ");

    esp_err_t ret_status = esp_ble_gattc_read_char(gattc_if,
                                                   gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
                                                   gl_profile_tab[PROFILE_BATTERY_SERVICE].char_handle,
                                                   ESP_GATT_AUTH_REQ_NONE);
    if (ret_status)
    {
      ESP_LOGE(TAG_evnt, "(battery) esp_ble_gattc_read_char failed, error code = %x", ret_status);
    }
    // else
    // {
    //   ESP_LOGI(TAG_evnt, "(battery) esp_ble_gattc_read_char success ");
    // }
    break;
  }
  case ESP_GATTC_WRITE_DESCR_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_WRITE_DESCR_EVT");
#endif
    if (p_data->write.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)write descr failed, error status = %x", p_data->write.status);
      break;
    }
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)write descr success ");
#endif
    /*JMH the following comes from ESP32 idf demo code & is not needed for this application*/
    //  uint8_t write_char_data[25];
    //  for (int i = 0; i < sizeof(write_char_data); ++i)
    //  {
    //      write_char_data[i] = i % 256;
    //  }
    //  esp_ble_gattc_write_char( gattc_if,
    //                            gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
    //                            gl_profile_tab[PROFILE_BATTERY_SERVICE].char_handle,
    //                            sizeof(write_char_data),
    //                            write_char_data,
    //                            ESP_GATT_WRITE_TYPE_RSP,
    //                            ESP_GATT_AUTH_REQ_NONE);
    //printf("(battery)ESP_GATTC_WRITE_DESCR_EVT - JUST KILLING TIME\n");
     break;
  }
  case ESP_GATTC_NOTIFY_EVT:
    if (p_data->notify.is_notify)
    {
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_NOTIFY_EVT, receive notify value:");
#endif
    }
    else
    {
#ifdef gatDebug
      ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_NOTIFY_EVT, receive indicate value:");
#endif
    }
    esp_log_buffer_hex(TAG_evnt, p_data->notify.value, p_data->notify.value_len);
    break;
  case ESP_GATTC_SRVC_CHG_EVT:
  {
    esp_bd_addr_t bda;
    memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
#endif
    esp_log_buffer_hex(TAG_evnt, bda, sizeof(esp_bd_addr_t));
    break;
  }
  case ESP_GATTC_CFG_MTU_EVT:
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, if %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, gattc_if, param->cfg_mtu.conn_id);
#endif
    if (param->cfg_mtu.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)config mtu failed, error status = %x", param->cfg_mtu.status);
    }
    // esp_err_t ret_search_gatt;
    // ret_search_gatt = esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_battery_service_uuid); // with filter
    // // ret_search_gatt = esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, NULL);
    // if (ret_search_gatt)
    // {
    //   ESP_LOGI(TAG_evnt, "(battery)service not found, error code = %x", ret_search_gatt);
    // }
    // else
    // {
    //   ESP_LOGI(TAG_evnt, "(battery)Search for Service Launched");
    // }
     break;
  case ESP_GATTC_REG_FOR_NOTIFY_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_REG_FOR_NOTIFY_EVT");
#endif
    if (p_data->reg_for_notify.status != ESP_GATT_OK)
    {
      ESP_LOGE(TAG_evnt, "(battery)REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
    }
    else
    {
      uint16_t count = 0;
      uint16_t notify_en = 1;
      esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
                                                                  ESP_GATT_DB_DESCRIPTOR,
                                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].service_start_handle,
                                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].service_end_handle,
                                                                  gl_profile_tab[PROFILE_BATTERY_SERVICE].char_handle,
                                                                  &count);
      if (ret_status != ESP_GATT_OK)
      {
        ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_get_attr_count error");
      }
      if (count > 0)
      {
#ifdef gatDebug
        ESP_LOGI(TAG_evnt, "(battery)malloc needs descr elem size %d X count %d", (int)sizeof(esp_gattc_descr_elem_t), count);
#endif
        descr_elem_result = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
        if (!descr_elem_result)
        {
          ESP_LOGE(TAG_evnt, "(battery)malloc error, gattc no mem");
        }
        else
        {
          ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                              gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
                                                              p_data->reg_for_notify.handle,
                                                              notify_descr_uuid,
                                                              descr_elem_result,
                                                              &count);
          if (ret_status != ESP_GATT_OK)
          {
            ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_get_descr_by_char_handle error");
          }

          /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
          if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
          {
            //                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
            //                                                                     gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
            //                                                                     descr_elem_result[0].handle,
            //                                                                     sizeof(notify_en),
            //                                                                     (uint8_t *)&notify_en,
            //                                                                     ESP_GATT_WRITE_TYPE_RSP,
            //                                                                     ESP_GATT_AUTH_REQ_NONE);
            ret_status = (esp_gatt_status_t)esp_ble_gattc_read_char_descr(gattc_if,
                                                                          gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id,
                                                                          descr_elem_result[0].handle,
                                                                          ESP_GATT_AUTH_REQ_NONE);
          }

          if (ret_status != ESP_GATT_OK)
          {
            // ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_write_char_descr error");
            ESP_LOGE(TAG_evnt, "(battery)esp_ble_gattc_read_char_descr error, code: %x", ret_status);
          }

          /* free descr_elem_result */
          free(descr_elem_result);
        }
      }
      else
      {
        ESP_LOGE(TAG_evnt, "(battery)decsr not found");
      }
    }
    break;
  }
  case ESP_GATTC_CONNECT_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_CONNECT_EVT conn_id %d, interface %d", p_data->connect.conn_id, gattc_if);
#endif
    gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id = p_data->connect.conn_id;
    memcpy(gl_profile_tab[PROFILE_BATTERY_SERVICE].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)REMOTE BDA:");
#endif
    esp_log_buffer_hex(TAG_evnt, gl_profile_tab[PROFILE_BATTERY_SERVICE].remote_bda, sizeof(esp_bd_addr_t));
    esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
    if (mtu_ret)
    {
      ESP_LOGE(TAG_evnt, "(battery)config MTU error, error code = %x", mtu_ret);
    }
    else
    {
      ESP_LOGE(TAG_evnt, "(battery) Send MTU request");
    }
    break;
  }
  case ESP_GATTC_DISCONNECT_EVT:
    connect_ble = false;
    get_server = false;
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "(battery)ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
#endif
    break;
  case ESP_GATTC_DIS_SRVC_CMPL_EVT:
  {
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "JMH(battery)ESP_GATTC_DIS_SRVC_CMPL_EVT(46) conn_id %d, interface %d", p_data->connect.conn_id, gattc_if);
#endif
//     esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
//     if (mtu_ret)
//     {
//       ESP_LOGE(TAG_evnt, "JMH(battery)config MTU error, error code = %x", mtu_ret);
//     }
//     else
//     {
// #ifdef gatDebug
//       ESP_LOGI(TAG_evnt, "JMH(battery)config MTU request Launched");
// #endif
//    }
  }
  break;
  default:
#ifdef gatDebug
    ESP_LOGI(TAG_evnt, "unregistered EVT:%d", event);
#endif
    break;
  }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
  const char *TAG_cb = "esp_gattc_cb";
  if(event == ESP_GATTC_WRITE_DESCR_EVT) PauseFlg = false;//true;
  else PauseFlg = false;
#ifdef gatDebug1
  char Evnt_name[35];
  //if(event == ESP_GATTC_CONNECT_EVT || event == ESP_GATTC_DISCONNECT_EVT || event == ESP_GATTC_WRITE_DESCR_EVT) PauseFlg = true;
    
  switch (event)
  {
  case ESP_GATTC_REG_EVT:
    sprintf(Evnt_name, "ESP_GATTC_REG_EVT");
    break;
  case ESP_GATTC_UNREG_EVT:
    sprintf(Evnt_name, "ESP_GATTC_UNREG_EVT");
    break;
  case ESP_GATTC_OPEN_EVT:
    sprintf(Evnt_name, "ESP_GATTC_OPEN_EVT");
    break;
  case ESP_GATTC_READ_CHAR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_READ_CHAR_EVT");
    break;
  case ESP_GATTC_WRITE_CHAR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_WRITE_CHAR_EVT");
    break;
  case ESP_GATTC_CLOSE_EVT:
    sprintf(Evnt_name, "ESP_GATTC_CLOSE_EVT");
    break;
  case ESP_GATTC_SEARCH_CMPL_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SEARCH_CMPL_EVT");
    break;
  case ESP_GATTC_SEARCH_RES_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SEARCH_RES_EVT");
    break;
  case ESP_GATTC_READ_DESCR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_READ_DESCR_EVT");
    break;
  case ESP_GATTC_WRITE_DESCR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_WRITE_DESCR_EVT");
    break;
  case ESP_GATTC_NOTIFY_EVT:
    sprintf(Evnt_name, "ESP_GATTC_NOTIFY_EVT");
    break;
  case ESP_GATTC_PREP_WRITE_EVT:
    sprintf(Evnt_name, "ESP_GATTC_PREP_WRITE_EVT");
    break;
  case ESP_GATTC_EXEC_EVT:
    sprintf(Evnt_name, "ESP_GATTC_EXEC_EVT");
    break;
  case ESP_GATTC_ACL_EVT:
    sprintf(Evnt_name, "ESP_GATTC_ACL_EVT");
    break;
  case ESP_GATTC_CANCEL_OPEN_EVT:
    sprintf(Evnt_name, "ESP_GATTC_CANCEL_OPEN_EVT");
    break;
  case ESP_GATTC_SRVC_CHG_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SRVC_CHG_EVT");
    break;
  case ESP_GATTC_ENC_CMPL_CB_EVT:
    sprintf(Evnt_name, "ESP_GATTC_ENC_CMPL_CB_EVT");
    break;
  case ESP_GATTC_CFG_MTU_EVT:
    sprintf(Evnt_name, "ESP_GATTC_CFG_MTU_EVT");
    break;
  case ESP_GATTC_ADV_DATA_EVT:
    sprintf(Evnt_name, "ESP_GATTC_ADV_DATA_EVT");
    break;
  case ESP_GATTC_MULT_ADV_ENB_EVT:
    sprintf(Evnt_name, "ESP_GATTC_MULT_ADV_ENB_EVT");
    break;
  case ESP_GATTC_MULT_ADV_UPD_EVT:
    sprintf(Evnt_name, "ESP_GATTC_MULT_ADV_UPD_EVT");
    break;
  case ESP_GATTC_MULT_ADV_DATA_EVT:
    sprintf(Evnt_name, "ESP_GATTC_MULT_ADV_DATA_EVT");
    break;
  case ESP_GATTC_MULT_ADV_DIS_EVT:
    sprintf(Evnt_name, "ESP_GATTC_MULT_ADV_DIS_EV");
    break;
  case ESP_GATTC_CONGEST_EVT:
    sprintf(Evnt_name, "ESP_GATTC_CONGEST_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_ENB_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_ENB_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_CFG_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_CFG_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_RD_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_RD_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_THR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_THR_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_PARAM_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_PARAM_EVT");
    break;
  case ESP_GATTC_BTH_SCAN_DIS_EVT:
    sprintf(Evnt_name, "ESP_GATTC_BTH_SCAN_DIS_EVT");
    break;
  case ESP_GATTC_SCAN_FLT_CFG_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SCAN_FLT_CFG_EVT");
    break;
  case ESP_GATTC_SCAN_FLT_PARAM_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SCAN_FLT_PARAM_EVT");
    break;
  case ESP_GATTC_SCAN_FLT_STATUS_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SCAN_FLT_STATUS_EVT");
    break;
  case ESP_GATTC_ADV_VSC_EVT:
    sprintf(Evnt_name, "ESP_GATTC_ADV_VSC_EVT");
    break;
  case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    sprintf(Evnt_name, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
    break;
  case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
    sprintf(Evnt_name, "ESP_GATTC_UNREG_FOR_NOTIFY_EVT");
    break;
  case ESP_GATTC_CONNECT_EVT:
    sprintf(Evnt_name, "ESP_GATTC_CONNECT_EVT");
    break;
  case ESP_GATTC_DISCONNECT_EVT:
    sprintf(Evnt_name, "ESP_GATTC_DISCONNECT_EVT");
    break;
  case ESP_GATTC_READ_MULTIPLE_EVT:
    sprintf(Evnt_name, "ESP_GATTC_READ_MULTIPLE_EVT");
    break;
  case ESP_GATTC_QUEUE_FULL_EVT:
    sprintf(Evnt_name, "ESP_GATTC_QUEUE_FULL_EVT");
    break;
  case ESP_GATTC_SET_ASSOC_EVT:
    sprintf(Evnt_name, "ESP_GATTC_SET_ASSOC_EVT");
    break;
  case ESP_GATTC_GET_ADDR_LIST_EVT:
    sprintf(Evnt_name, "ESP_GATTC_GET_ADDR_LIST_EVT");
    break;
  case ESP_GATTC_DIS_SRVC_CMPL_EVT:
    sprintf(Evnt_name, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
    break;
  case ESP_GATTC_READ_MULTI_VAR_EVT:
    sprintf(Evnt_name, "ESP_GATTC_READ_MULTI_VAR_EVT");
    break;
  default:
    sprintf(Evnt_name, "UNKNOWN_EVT");
    break;
  }
  if(event == ESP_GATTC_DISCONNECT_EVT || event == ESP_GATTC_CLOSE_EVT)
  {
    ESP_LOGI(TAG_cb, "-> CallBk EVNT, app_id %04x, conn_id %d, if %d, event_id %s(%d)", param->reg.app_id, param->connect.conn_id, gattc_if, Evnt_name, event);
  }
#else
  //vTaskDelay(10/ portTICK_PERIOD_MS);
#endif
//if(Shwgattcb) ESP_LOGI(TAG_cb, "-> CallBk EVNT, app_id %04x, conn_id %d, if %d, event_id %s(%d)", param->reg.app_id, param->connect.conn_id, gattc_if, Evnt_name, event);

  if (event == ESP_GATTC_SEARCH_RES_EVT)
  {

    esp_gatt_srvc_id_t srvc_id = (esp_gatt_srvc_id_t)param->search_res.srvc_id;
    if (srvc_id.id.uuid.len == ESP_UUID_LEN_16 && srvc_id.id.uuid.uuid.uuid16 == UUID_SERVCLASS_BATTERY)
    {
      // ESP_LOGI(TAG_cb, "!!BATTERY Service UUID16: %x found!!", srvc_id.id.uuid.uuid.uuid16);
      gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_cb(event, gattc_if, param);
      // return; // default hidh_gattc_event_handler does nothing with this event, so no need to to continue
    }
  }
//   /* If event is register event, store the gattc_if for each profile */
//   if (event == ESP_GATTC_CONNECT_EVT)
//   {
//     // ESP_LOGI(TAG_cb, "!!ESP_GATTC_CONNECT_EVT!!, app_id %04x, interface %d, status %d",
//     //          param->reg.app_id,
//     //          gattc_if,
//     //          param->reg.status);
//     //  gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_cb(event, gattc_if, param);
//     // return; // default hidh_gattc_event_handler does nothing with this event, so no need to to continue
//   }
  if (event == ESP_GATTC_REG_EVT)
  {
    if (param->reg.status == ESP_GATT_OK)
    {
      gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
      ESP_LOGI(TAG_cb, "reg app COMPLETE, app_id %04x, interface %d, status %d",
               param->reg.app_id,
               gattc_if,
               param->reg.status);
      //esp_hidh_gattc_event_handler(event, gattc_if, param);
    }
//     else
//     {
// #ifdef gatDebug1
//       ESP_LOGI(TAG_cb, "reg app failed, app_id %04x, status %d",
//                param->reg.app_id,
//                param->reg.status);
// #endif
//       return;
//     }
  }

  /* If the param->reg.app_id equal to profile[n]app_id, call profile A cb handler,
   * so here call each profile's callback */

  int idx;
  for (idx = 0; idx < PROFILE_NUM; idx++)
  {
    if (gl_profile_tab[idx].app_id == param->reg.app_id && gl_profile_tab[idx].gattc_if != ESP_GATT_IF_NONE)
    {
      // if (gl_profile_tab[idx].gattc_cb)
      // {
      // ESP_LOGI(TAG_cb, "-> Battery app CallBk, app_id %04x, conn_id %d, event_id %d", param->reg.app_id, param->connect.conn_id, event);
#ifndef gatDebug
    if(PauseFlg) ESP_LOGI(TAG_cb, "(batteryA) event ID: % d", event);
    
#endif      
      gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
#ifndef gatDebug

      if(PauseFlg) printf("%d Battery Service CB Complete\n", event);
      
#endif

      
    }
  }

  /*And finally, if this CB event has not been diverted, treat this event as an HID callback,
  using the espidf default hid callback handler*/
  esp_hidh_gattc_event_handler(event, gattc_if, param);
  #ifndef gatDebug

      //ESP_LOGE(TAG_cb, "(batteryB) event ID: % d", event);
      // printf("%d bbbbbbbbbbbbbbbbbbbbbbbbbbb\n", event);
      if(PauseFlg) printf("%d Gattc HIDH CB complete\n", event);
      if(event == ESP_GATTC_OPEN_EVT) PauseFlg = false;
#endif
}
/*END GATT stuff*/
const char *
BTKeyboard::ble_addr_type_str(esp_ble_addr_type_t ble_addr_type)
{
  if (ble_addr_type > BLE_ADDR_TYPE_RPA_RANDOM)
  {
    return "UNKNOWN";
  }
  return ble_addr_type_names[ble_addr_type];
}

const char *
BTKeyboard::ble_gap_evt_str(uint8_t event)
{
  if (event >= SIZEOF_ARRAY(ble_gap_evt_names))
  {
    return "UNKNOWN";
  }
  return ble_gap_evt_names[event];
}

const char *
BTKeyboard::bt_gap_evt_str(uint8_t event)
{
  if (event >= SIZEOF_ARRAY(bt_gap_evt_names))
  {
    return "UNKNOWN";
  }
  return bt_gap_evt_names[event];
}

const char *
BTKeyboard::ble_key_type_str(esp_ble_key_type_t key_type)
{
  const char *key_str = nullptr;
  switch (key_type)
  {
  case ESP_LE_KEY_NONE:
    key_str = "ESP_LE_KEY_NONE";
    break;
  case ESP_LE_KEY_PENC:
    key_str = "ESP_LE_KEY_PENC";
    break;
  case ESP_LE_KEY_PID:
    key_str = "ESP_LE_KEY_PID";
    break;
  case ESP_LE_KEY_PCSRK:
    key_str = "ESP_LE_KEY_PCSRK";
    break;
  case ESP_LE_KEY_PLK:
    key_str = "ESP_LE_KEY_PLK";
    break;
  case ESP_LE_KEY_LLK:
    key_str = "ESP_LE_KEY_LLK";
    break;
  case ESP_LE_KEY_LENC:
    key_str = "ESP_LE_KEY_LENC";
    break;
  case ESP_LE_KEY_LID:
    key_str = "ESP_LE_KEY_LID";
    break;
  case ESP_LE_KEY_LCSRK:
    key_str = "ESP_LE_KEY_LCSRK";
    break;
  default:
    key_str = "INVALID BLE KEY TYPE";
    break;
  }

  return key_str;
}

void BTKeyboard::print_uuid(esp_bt_uuid_t *uuid)
{
  if (uuid->len == ESP_UUID_LEN_16)
  {
    GAP_DBG_PRINTF("UUID16: 0x%04x", uuid->uuid.uuid16);

    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "UUID16: 0x%04x ", uuid->uuid.uuid16);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
  }
  else if (uuid->len == ESP_UUID_LEN_32)
  {
    // GAP_DBG_PRINTF("UUID32: 0x%08x", uuid->uuid.uuid32);
    /*JMH 20230205 changed to solve complie error*/
    GAP_DBG_PRINTF("UUID32: 0x%08x", (unsigned int)uuid->uuid.uuid32);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "UUID32: 0x%08x\n", (unsigned int)uuid->uuid.uuid32);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
  }
  else if (uuid->len == ESP_UUID_LEN_128)
  {
    GAP_DBG_PRINTF("UUID128: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
                   uuid->uuid.uuid128[0],
                   uuid->uuid.uuid128[1], uuid->uuid.uuid128[2], uuid->uuid.uuid128[3],
                   uuid->uuid.uuid128[4], uuid->uuid.uuid128[5], uuid->uuid.uuid128[6],
                   uuid->uuid.uuid128[7], uuid->uuid.uuid128[8], uuid->uuid.uuid128[9],
                   uuid->uuid.uuid128[10], uuid->uuid.uuid128[11], uuid->uuid.uuid128[12],
                   uuid->uuid.uuid128[13], uuid->uuid.uuid128[14], uuid->uuid.uuid128[15]);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "\nUUID128: %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
              uuid->uuid.uuid128[0],
              uuid->uuid.uuid128[1], uuid->uuid.uuid128[2], uuid->uuid.uuid128[3],
              uuid->uuid.uuid128[4], uuid->uuid.uuid128[5], uuid->uuid.uuid128[6],
              uuid->uuid.uuid128[7], uuid->uuid.uuid128[8], uuid->uuid.uuid128[9],
              uuid->uuid.uuid128[10], uuid->uuid.uuid128[11], uuid->uuid.uuid128[12],
              uuid->uuid.uuid128[13], uuid->uuid.uuid128[14], uuid->uuid.uuid128[15]);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
  }
}

/*new runs once during main startup*/
esp_err_t BTKeyboard::init_low_level(uint8_t mode)
{
  esp_err_t ret;
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
  bt_cfg.mode = mode;
#endif
#if CONFIG_BT_HID_HOST_ENABLED
  if (mode & ESP_BT_MODE_CLASSIC_BT)
  {
    bt_cfg.bt_max_acl_conn = 3;
    bt_cfg.bt_max_sync_conn = 3;
  }
  else
#endif
  {
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret)
    {
      ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
      return ret;
    }
  }
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret)
  {
    // ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
    ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
    return ret;
  }

  ret = esp_bt_controller_enable((esp_bt_mode_t)mode);
  if (ret)
  {
    ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
    return ret;
  }

  esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
  bluedroid_cfg.ssp_en = false;
#endif
  ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
  if (ret)
  {
    ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", ret);
    return ret;
  }

  ret = esp_bluedroid_enable();
  if (ret)
  {
    ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", ret);
    return ret;
  }
#if CONFIG_BT_HID_HOST_ENABLED
  if (mode & ESP_BT_MODE_CLASSIC_BT)
  {
    ret = init_bt_gap();
    if (ret)
    {
      return ret;
    }
  }
#endif
#if CONFIG_BT_BLE_ENABLED
  if (mode & ESP_BT_MODE_BLE)
  {
    ret = init_ble_gap();
    if (ret)
    {
      return ret;
    }
  }
#endif /* CONFIG_BT_BLE_ENABLED */
  return ret;
}

/* JMH for this application, the following runs at main startup */
esp_err_t BTKeyboard::esp_hid_gap_init(uint8_t mode)
{
  esp_err_t ret;
  if (!mode || mode > ESP_BT_MODE_BTDM)
  {
    ESP_LOGE(TAG, "Invalid mode given!");
    return ESP_FAIL;
  }

  if (bt_hidh_cb_semaphore != NULL)
  {
    ESP_LOGE(TAG, "Already initialized");
    return ESP_FAIL;
  }

  bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
  if (bt_hidh_cb_semaphore == NULL)
  {
    ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
    return ESP_FAIL;
  }

  ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
  if (ble_hidh_cb_semaphore == NULL)
  {
    ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
    vSemaphoreDelete(bt_hidh_cb_semaphore);
    bt_hidh_cb_semaphore = NULL;
    return ESP_FAIL;
  }

  ret = init_low_level(mode);
  if (ret != ESP_OK)
  {
    vSemaphoreDelete(bt_hidh_cb_semaphore);
    bt_hidh_cb_semaphore = NULL;
    vSemaphoreDelete(ble_hidh_cb_semaphore);
    ble_hidh_cb_semaphore = NULL;
    ESP_LOGE(TAG, "init_low_level(mode) failed!");
    return ret;
  }
#ifdef gatDebug
  ESP_LOGI(TAG, "esp_hid_gap_init() COMPLETE");
#endif
  return ESP_OK;
}

/*end new*/
void BTKeyboard::esp_hid_scan_results_free(esp_hid_scan_result_t *results)
{
  esp_hid_scan_result_t *r = nullptr;
  while (results)
  {
    r = results;
    results = results->next;
    if (r->name != nullptr)
    {
      free((char *)r->name);
    }
    free(r);
  }
}
#if (CONFIG_BT_HID_HOST_ENABLED || CONFIG_BT_BLE_ENABLED)
BTKeyboard::esp_hid_scan_result_t *
BTKeyboard::find_scan_result(esp_bd_addr_t bda, esp_hid_scan_result_t *results)
{
  esp_hid_scan_result_t *r = results;
  while (r)
  {
    if (memcmp(bda, r->bda, sizeof(esp_bd_addr_t)) == 0)
    {
      return r;
    }
    r = r->next;
  }
  return nullptr;
}
#endif /* (CONFIG_BT_HID_HOST_ENABLED || CONFIG_BT_BLE_ENABLED) */
void BTKeyboard::add_bt_scan_result(esp_bd_addr_t bda,
                                    esp_bt_cod_t *cod,
                                    esp_bt_uuid_t *uuid,
                                    uint8_t *name,
                                    uint8_t name_len,
                                    int rssi)
{
  esp_hid_scan_result_t *r = find_scan_result(bda, bt_scan_results);
  if (r)
  {
    // Some info may come later
    if (r->name == nullptr && name && name_len)
    {
      char *name_s = (char *)malloc(name_len + 1);
      if (name_s == nullptr)
      {
        ESP_LOGE(TAG, "Malloc result name failed!");
        return;
      }
      memcpy(name_s, name, name_len);
      name_s[name_len] = 0;
      r->name = (const char *)name_s;
    }
    if (r->bt.uuid.len == 0 && uuid->len)
    {
      memcpy(&r->bt.uuid, uuid, sizeof(esp_bt_uuid_t));
    }
    if (rssi != 0)
    {
      r->rssi = rssi;
    }
    return;
  }

  r = (esp_hid_scan_result_t *)malloc(sizeof(esp_hid_scan_result_t));

  if (r == nullptr)
  {
    ESP_LOGE(TAG, "Malloc bt_hidh_scan_result_t failed!");
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "Malloc bt_hidh_scan_result_t failed!\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_RED);
    }
    return;
  }

  r->transport = ESP_HID_TRANSPORT_BT;

  memcpy(r->bda, bda, sizeof(esp_bd_addr_t));
  memcpy(&r->bt.cod, cod, sizeof(esp_bt_cod_t));
  memcpy(&r->bt.uuid, uuid, sizeof(esp_bt_uuid_t));

  r->usage = esp_hid_usage_from_cod((uint32_t)cod);
  r->rssi = rssi;
  r->name = nullptr;

  if (name_len && name)
  {
    char *name_s = (char *)malloc(name_len + 1);
    if (name_s == nullptr)
    {
      free(r);
      ESP_LOGE(TAG, "Malloc result name failed!");
      if (pDFault->DeBug)
      {
        sprintf(msgbuf, "Malloc result name failed!\n");
        pmsgbx->dispKeyBrdTxt(msgbuf, TFT_RED);
      }
      return;
    }
    memcpy(name_s, name, name_len);
    name_s[name_len] = 0;
    r->name = (const char *)name_s;
  }
  r->next = bt_scan_results;
  bt_scan_results = r;
  num_bt_scan_results++;
}

void BTKeyboard::add_ble_scan_result(esp_bd_addr_t bda,
                                     esp_ble_addr_type_t addr_type,
                                     uint16_t appearance,
                                     uint8_t *name,
                                     uint8_t name_len,
                                     int rssi)
{
  if (find_scan_result(bda, ble_scan_results))
  {
    ESP_LOGW(TAG, "Result already exists!");
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "Result already exists!\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
    }
    return;
  }

  esp_hid_scan_result_t *r = (esp_hid_scan_result_t *)malloc(sizeof(esp_hid_scan_result_t));

  if (r == nullptr)
  {
    ESP_LOGE(TAG, "Malloc ble_hidh_scan_result_t failed!");
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "Malloc ble_hidh_scan_result_t failed!\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_RED);
    }
    return;
  }

  r->transport = ESP_HID_TRANSPORT_BLE;

  memcpy(r->bda, bda, sizeof(esp_bd_addr_t));

  r->ble.appearance = appearance;
  r->ble.addr_type = addr_type;
  r->usage = esp_hid_usage_from_appearance(appearance);
  r->rssi = rssi;
  r->name = nullptr;

  if (name_len && name)
  {
    char *name_s = (char *)malloc(name_len + 1);
    if (name_s == nullptr)
    {
      free(r);
      ESP_LOGE(TAG, "Malloc result name failed!");
      if (pDFault->DeBug)
      {
        sprintf(msgbuf, "Malloc result name failed!\n");
        pmsgbx->dispKeyBrdTxt(msgbuf, TFT_RED);
      }
      return;
    }
    memcpy(name_s, name, name_len);
    name_s[name_len] = 0;
    r->name = (const char *)name_s;
  }

  r->next = ble_scan_results;
  ble_scan_results = r;
  num_ble_scan_results++;
}

bool BTKeyboard::setup(pid_handler *handler, BTKeyboard *KBptr)
{
  const char *TAG = "BTKeyboard::setup";
  esp_err_t ret;
#if HID_HOST_MODE != HIDH_BLE_MODE // HIDH_IDLE_MODE
  ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
  return false;
#endif

  const esp_bt_mode_t mode = HID_HOST_MODE;

  if (BLE_KyBrd != nullptr)
  {
    ESP_LOGE(TAG, "Setup called more than once. Only one instance of BTKeyboard is allowed.");
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "Setup called more than once. Only one instance of BTKeyboard is allowed.\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
    }
    return false;
  }
  BLE_KyBrd = KBptr;
  pairing_handler = handler;
#ifdef gatDebug
  ESP_LOGI(TAG, "setting hid gap, mode:%d", mode);
#endif
  ESP_ERROR_CHECK(esp_hid_gap_init(mode));
#if CONFIG_BT_BLE_ENABLED
  // ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
  ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_gattc_cb));

#endif /* CONFIG_BT_BLE_ENABLED */
/*Note: this sets the stack size for the 'esp_ble_hidh_events' TASK*/
  esp_hidh_config_t config = {
      .callback = hidh_callback,
      .event_stack_size = 5120,
      .callback_arg = nullptr,
  };
  ESP_ERROR_CHECK(esp_hidh_init(&config));
  for (int i = 0; i < MAX_KEY_COUNT; i++)
  {
    key_avail[i] = true;
  }
  event_queue = xQueueCreate(10, sizeof(KeyInfo));

#if CONFIG_BT_NIMBLE_ENABLED
  /* XXX Need to have template for store */
  ble_store_config_init();

  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  /* Starting nimble task after gatts is initialized*/
  ret = esp_nimble_enable(ble_hid_host_task);
  if (ret)
  {
    ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
  }
#endif

  return true;
  /*The following is the OLD approach Based mostly claaic Bluetooth*/
  //   BLE_KyBrd = this;

  //   pairing_handler = handler;
  //   event_queue = xQueueCreate(10, sizeof(KeyInfo));

  //   if (HID_HOST_MODE == HIDH_IDLE_MODE)
  //   {
  //     ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
  //     sprintf(msgbuf, "Please turn on BT HID host or BLE!\n");
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  //   bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
  //   if (bt_hidh_cb_semaphore == nullptr)
  //   {
  //     ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
  //     sprintf(msgbuf, "xSemaphoreCreateMutex failed!\n");
  //     pmsgbx->dispStat(msgbuf, TFT_RED);
  //     return false;
  //   }

  //   ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
  //   if (ble_hidh_cb_semaphore == nullptr)
  //   {
  //     ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
  //     sprintf(msgbuf, "xSemaphoreCreateMutex failed!\n");
  //     pmsgbx->dispStat(msgbuf, TFT_RED);
  //     vSemaphoreDelete(bt_hidh_cb_semaphore);
  //     bt_hidh_cb_semaphore = nullptr;
  //     return false;
  //   }

  //   esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  // /*JMH commented out for BLE/5.2 version*/
  //   // bt_cfg.mode = mode;
  //   // bt_cfg.bt_max_acl_conn = 3;
  //   // bt_cfg.bt_max_sync_conn = 3;

  //   if ((ret = esp_bt_controller_init(&bt_cfg)))
  //   {
  //     ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
  //     sprintf(msgbuf, "esp_bt_controller_init failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  //   if ((ret = esp_bt_controller_enable(mode)))
  //   {
  //     ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
  //     sprintf(msgbuf, "esp_bt_controller_enable failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  //   if ((ret = esp_bluedroid_init()))
  //   {
  //     ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", ret);
  //     sprintf(msgbuf, "esp_bluedroid_init failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  //   if ((ret = esp_bluedroid_enable()))
  //   {
  //     ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", ret);
  //     sprintf(msgbuf, "esp_bluedroid_enable failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  // Classic Bluetooth GAP
  // #if CONFIG_BT_HID_HOST_ENABLED
  //   esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
  //   esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
  //   esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

  //   /*
  //    * Set default parameters for Legacy Pairing
  //    * Use fixed pin code
  //    */
  //   esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
  //   esp_bt_pin_code_t pin_code;
  //   pin_code[0] = '1';
  //   pin_code[1] = '2';
  //   pin_code[2] = '3';
  //   pin_code[3] = '4';
  //   esp_bt_gap_set_pin(pin_type, 4, pin_code);

  //   if ((ret = esp_bt_gap_register_callback(bt_gap_event_handler)))
  //   {
  //     ESP_LOGE(TAG, "esp_bt_gap_register_callback failed: %d", ret);
  //     sprintf(msgbuf, "esp_bt_gap_register_callback failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }

  //   // Allow BT devices to connect back to us
  //   if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE)))
  //   {
  //     ESP_LOGE(TAG, "esp_bt_gap_set_scan_mode failed: %d", ret);
  //     sprintf(msgbuf, "esp_bt_gap_set_scan_mode failed: %d\n", ret);
  //     pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //     return false;
  //   }
  // #endif /*CONFIG_BT_HID_HOST_ENABLED*/
  //   // BLE GAP
  //   /*for ESP32s3 changed how the esp_ble_gap_register_callback was done*/
  //   init_ble_gap();
  //   /*for ESP32s3 commented out the following*/
  //   // if ((ret = esp_ble_gap_register_callback(ble_gap_event_handler)))
  //   // {
  //   //   ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", ret);
  //   //   sprintf(msgbuf, "esp_bt_gap_set_scan_mode ffailed: %d\n", ret);
  //   //   pmsgbx->dispStat(msgbuf, TFT_YELLOW);
  //   //   return false;
  //   // }

  //   // //ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
  //   // ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_hidh_gattc_event_handler));
  //   esp_hidh_config_t config = {
  //       .callback = hidh_callback,
  //       .event_stack_size = 4 * 1024, // Required with ESP-IDF 4.4
  //       .callback_arg = nullptr       // idem
  //   };
  //   ESP_ERROR_CHECK(esp_hidh_init(&config));

  //   for (int i = 0; i < MAX_KEY_COUNT; i++)
  //   {
  //     key_avail[i] = true;
  //   }

  //   last_ch = 0;
  //   battery_level = -1;
  //  return true;

} // end setup()

#if CONFIG_BT_HID_HOST_ENABLED
void BTKeyboard::handle_bt_device_result(esp_bt_gap_cb_param_t *param)
{
  GAP_DBG_PRINTF("BT : " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(param->disc_res.bda));
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "BT  %02x:%02x:%02x:%02x:%02x:%02x", ESP_BD_ADDR_HEX(param->disc_res.bda));
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }
  uint32_t codv = 0;
  esp_bt_cod_t *cod = (esp_bt_cod_t *)&codv;
  int8_t rssi = 0;
  uint8_t *name = nullptr;
  uint8_t name_len = 0;
  esp_bt_uuid_t uuid;

  uuid.len = ESP_UUID_LEN_16;
  uuid.uuid.uuid16 = 0;

  for (int i = 0; i < param->disc_res.num_prop; i++)
  {
    esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];
    if (prop->type != ESP_BT_GAP_DEV_PROP_EIR)
    {
      GAP_DBG_PRINTF(", %s: ", gap_bt_prop_type_names[prop->type]);
      // if (pDFault->DeBug)
      // {
      //   sprintf(msgbuf, ", %s: ", gap_bt_prop_type_names[prop->type]);
      //   pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      // }
    }
    if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME)
    {
      name = (uint8_t *)prop->val;
      name_len = strlen((const char *)name);
      GAP_DBG_PRINTF("%s", (const char *)name);
      if (pDFault->DeBug)
      {
        sprintf(msgbuf, "%s", (const char *)name);
        pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      }
    }
    else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI)
    {
      rssi = *((int8_t *)prop->val);
      GAP_DBG_PRINTF("%d", rssi);
      // if (pDFault->DeBug)
      // {
      //   sprintf(msgbuf, "%d", rssi);
      //   pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      // }
    }
    else if (prop->type == ESP_BT_GAP_DEV_PROP_COD)
    {
      memcpy(&codv, prop->val, sizeof(uint32_t));
      GAP_DBG_PRINTF("major: %s, minor: %d, service: 0x%03x", esp_hid_cod_major_str(cod->major), cod->minor, cod->service);
      // if (pDFault->DeBug)
      // {
      //   sprintf(msgbuf, "major: %s, minor: %d, service: 0x%03x", esp_hid_cod_major_str(cod->major), cod->minor, cod->service);
      //   pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      // }
    }
    else if (prop->type == ESP_BT_GAP_DEV_PROP_EIR)
    {
      uint8_t len = 0;
      uint8_t *data = 0;

      data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_16BITS_UUID, &len);

      if (data == nullptr)
      {
        data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID, &len);
      }

      if (data && len == ESP_UUID_LEN_16)
      {
        uuid.len = ESP_UUID_LEN_16;
        uuid.uuid.uuid16 = data[0] + (data[1] << 8);
        GAP_DBG_PRINTF(", ");
        print_uuid(&uuid);
        continue;
      }

      data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_32BITS_UUID, &len);

      if (data == nullptr)
      {
        data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_32BITS_UUID, &len);
      }

      if (data && len == ESP_UUID_LEN_32)
      {
        uuid.len = len;
        memcpy(&uuid.uuid.uuid32, data, sizeof(uint32_t));
        GAP_DBG_PRINTF(", ");
        print_uuid(&uuid);
        continue;
      }

      data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_128BITS_UUID, &len);

      if (data == nullptr)
      {
        data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_INCMPL_128BITS_UUID, &len);
      }

      if (data && len == ESP_UUID_LEN_128)
      {
        uuid.len = len;
        memcpy(uuid.uuid.uuid128, (uint8_t *)data, len);
        GAP_DBG_PRINTF(", ");
        print_uuid(&uuid);
        continue;
      }

      // try to find a name
      if (name == nullptr)
      {
        data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);

        if (data == nullptr)
        {
          data = esp_bt_gap_resolve_eir_data((uint8_t *)prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
        }

        if (data && len)
        {
          name = data;
          name_len = len;
          GAP_DBG_PRINTF(", NAME: ");
          if (pDFault->DeBug)
          {
            sprintf(msgbuf, ", ");
            for (int x = 0; x < len; x++)
            {
              GAP_DBG_PRINTF("%c", (char)data[x]);
              /*inspite of the given lenght, treat every entry as if it were one & terminate the array with LF & 0*/
              msgbuf[2 + x] = (char)data[x];
              // msgbuf[2 + x + 1] = 0xA; // lf
              // msgbuf[2 + x + 2] = 0;
              msgbuf[2 + x + 1] = 0;
            }
            pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
          }
        }
      }
    }
  }
  GAP_DBG_PRINTF("\n");
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "\n");
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }

  if ((cod->major == ESP_BT_COD_MAJOR_DEV_PERIPHERAL) ||
      (find_scan_result(param->disc_res.bda, bt_scan_results) != nullptr))
  {
    add_bt_scan_result(param->disc_res.bda, cod, &uuid, name, name_len, rssi);
  }
}
#endif /*CONFIG_BT_HID_HOST_ENABLED*/
#if CONFIG_BT_HID_HOST_ENABLED
void BTKeyboard::bt_gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
  static const char *TAG1 = "bt_gap_event_handler";
  // printf("bt_gap_event_handler Start\n"); //JMH Diagnosstic testing
  switch (event)
  {
  case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
  {
    ESP_LOGV(TAG, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP");
    // if(pDFault->DeBug){
    // sprintf(msgbuf, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP"); //JMH Diagnosstic testing
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }
    if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
    {
      SEND_BT_CB();
    }
    break;
  }
#if CONFIG_BT_HID_HOST_ENABLED
  case ESP_BT_GAP_DISC_RES_EVT:
  {
    BLE_KyBrd->handle_bt_device_result(param);
    break;
  }
#endif /*CONFIG_BT_HID_HOST_ENABLED*/
  case ESP_BT_GAP_KEY_NOTIF_EVT:
    // ESP_LOGV(TAG, "BT GAP KEY_NOTIF passkey:%d", param->key_notif.passkey); // JMH changed %lu to %d
    ESP_LOGV(TAG, "BT GAP KEY_NOTIF passkey:%lu", param->key_notif.passkey); // JMH changed for WINDOWS 10 version
    //   if(pDFault->DeBug){
    // sprintf(msgbuf, "BT GAP KEY_NOTIF passkey:%lu", param->key_notif.passkey); //JMH Diagnosstic testing
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }
    if (BLE_KyBrd->pairing_handler != nullptr)
      (*BLE_KyBrd->pairing_handler)(param->key_notif.passkey);
    break;
  case ESP_BT_GAP_MODE_CHG_EVT:
    ESP_LOGV(TAG, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode);
    // if(pDFault->DeBug){
    // sprintf(msgbuf, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode); //JMH Diagnosstic testing
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }

    if ((param->mode_chg.mode == 0) && !BLE_KyBrd->OpnEvntFlg && adcON)
    {
#ifdef gatDebug
      ESP_LOGI(TAG1, "adc_continuous_stop");
#endif
      BLE_KyBrd->Adc_Sw = 1;
      vTaskDelay(20);
    }
    break;
  default:
    ESP_LOGV(TAG, "BT GAP EVENT %s", bt_gap_evt_str(event));
    // if(pDFault->DeBug){
    sprintf(msgbuf, "BT GAP EVENT %s", bt_gap_evt_str(event));
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }
    break;
  }
  // printf("bt_gap_event_handler: %s EXIT\n",msgbuf); //JMH Diagnosstic testing
}
#endif /*CONFIG_BT_HID_HOST_ENABLED*/
/*This is the call back event that gets fired during a SCAN for advertising BLE devices*/
void BTKeyboard::handle_ble_device_result(esp_ble_gap_cb_param_t *param)
{
  const char *TAG = "handle_ble_device_result";
  uint16_t uuid = 0;
  uint16_t appearance = 0;
  char name[64] = "";

  uint8_t uuid_len = 0;
  uint8_t *uuid_d = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_16SRV_CMPL, &uuid_len);

  if (uuid_d != nullptr && uuid_len)
  {
    uuid = uuid_d[0] + (uuid_d[1] << 8);
  }

  uint8_t appearance_len = 0;
  uint8_t *appearance_d = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_APPEARANCE, &appearance_len);

  if (appearance_d != nullptr && appearance_len)
  {
    appearance = appearance_d[0] + (appearance_d[1] << 8);
  }

  uint8_t adv_name_len = 0;
  uint8_t *adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

  if (adv_name == nullptr)
  {
    adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &adv_name_len);
  }

  if (adv_name != nullptr && adv_name_len)
  {
    memcpy(name, adv_name, adv_name_len);
    name[adv_name_len] = 0;
  }

  GAP_DBG_PRINTF("BLE: " ESP_BD_ADDR_STR ", ", ESP_BD_ADDR_HEX(param->scan_rst.bda));
  GAP_DBG_PRINTF("RSSI: %d, ", param->scan_rst.rssi);
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "RSSI: %d, ", param->scan_rst.rssi);
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }
  GAP_DBG_PRINTF("UUID: 0x%04x, ", uuid);
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "UUID: 0x%04x, ", uuid);
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }
  GAP_DBG_PRINTF("APPEARANCE: 0x%04x, ", appearance);
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "APPEARANCE: 0x%04x, ", appearance);
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }
  GAP_DBG_PRINTF("ADDR_TYPE: '%s'", ble_addr_type_str(param->scan_rst.ble_addr_type));
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "ADDR_TYPE: '%s'", ble_addr_type_str(param->scan_rst.ble_addr_type));
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }

  if (adv_name_len)
  {
    GAP_DBG_PRINTF(", NAME: '%s'", name);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, ", NAME: '%s'\n", name);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
  }
  GAP_DBG_PRINTF("\n");
  if (pDFault->DeBug)
  {
    sprintf(msgbuf, "\n");
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
  }

#if SCAN
  /*JMH use this function to see if the 'calling' BLE device address matches a previously 'paired'(registered) Keyboard */
  // bt_status_t _btc_storage_get_ble_bonding_key(bt_bdaddr_t *remote_bd_addr,
  //                                           uint8_t key_type,
  //                                           char *key_value,
  //                                           int key_length)

  uint8_t key_type = BTM_LE_KEY_PID;
  char key_value;
  int key_length = 30;

  bt_bdaddr_t remote_bda;
  memcpy(&remote_bda, &param->scan_rst.bda, sizeof(bt_bdaddr_t));
  bt_status_t stat = btc_storage_get_ble_bonding_key(&remote_bda, key_type, &key_value, key_length);
  if (stat == BT_STATUS_SUCCESS)
  { /*we found a stored record for this BLE Keyboard/address*/
    ESP_LOGI(TAG, "Previously PAIRED");
    PairFlg1 = true;
    sprintf(msgbuf, "!!Found Paired Keyboard!!\n Standby... while CW Machine Re-Connects...");
    pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    /*So add it to the scan results list*/
    add_ble_scan_result(param->scan_rst.bda,
                        param->scan_rst.ble_addr_type,
                        appearance, adv_name, adv_name_len,
                        param->scan_rst.rssi);
  }
  else if (uuid == ESP_GATT_UUID_HID_SVC)
  {
    add_ble_scan_result(param->scan_rst.bda,
                        param->scan_rst.ble_addr_type,
                        appearance, adv_name, adv_name_len,
                        param->scan_rst.rssi);
  }
  else
  {
    uint8_t curHexAddr[6];
    memcpy(&curHexAddr, &remote_bda, sizeof(bt_bdaddr_t));
    sprintf(msgbuf, ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(curHexAddr));
    ESP_LOGI(TAG, "Address '%s' does not match a 'Known' Device ", msgbuf);
  }

#endif
}

void BTKeyboard::ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  const char *TAG = "ble_gap_event_handler";
  uint8_t *adv_name = NULL;
  uint8_t adv_name_len = 0;
  switch (event)
  {

    // SCAN

  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
  {

    ESP_LOGV(TAG, "BLE GAP EVENT SCAN_PARAM_SET_COMPLETE");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP EVENT SCAN_PARAM_SET_COMPLETE\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    SEND_BLE_CB();
    break;
  }
  case ESP_GAP_BLE_SCAN_RESULT_EVT:
  {
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
    switch (param->scan_rst.search_evt)
    {
    case ESP_GAP_SEARCH_INQ_RES_EVT:
    {
      BLE_KyBrd->handle_ble_device_result(param);
      adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                          ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
      ESP_LOGI(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: @ Rmt Addr: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));
      // if (PairFlg1)
      // {
      //   PairFlg1 = false;
      //   esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
      //   printf("\nSTART - esp_ble_gattc_open()\n");
      //   esp_err_t ret = esp_ble_gattc_open(gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
      //   if (ret != ESP_OK)
      //   {
      //     ESP_LOGE(TAG, "esp_ble_gattc_open - FAILED");
      //   }
      //   else
      //   {
      //     ESP_LOGI(TAG, "esp_ble_gattc_open - COMPLETE; Remote Addr: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));
      //   }
      // }
      break;
    }
    case ESP_GAP_SEARCH_INQ_CMPL_EVT:
      ESP_LOGV(TAG, "BLE GAP EVENT SCAN DONE: %d", param->scan_rst.num_resps);
      if (BLE_KyBrd->pDFault->DeBug)
      {
        sprintf(msgbuf, "%s: BLE GAP EVENT SCAN DONE: %d\n", TAG, param->scan_rst.num_resps);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      }
      SEND_BLE_CB();
      break;
    default:
      break;
    }
    break;
  }
  case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
  {
    ESP_LOGV(TAG, "BLE GAP EVENT SCAN CANCELED");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP EVENT SCAN CANCELED\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    break;
  }

    // ADVERTISEMENT

  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    ESP_LOGV(TAG, "BLE GAP ADV_DATA_SET_COMPLETE");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP ADV_DATA_SET_COMPLETE\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    ESP_LOGV(TAG, "BLE GAP ADV_START_COMPLETE");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP ADV_START_COMPLETE", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    break;

    // AUTHENTICATION

  case ESP_GAP_BLE_AUTH_CMPL_EVT:
    if (!param->ble_security.auth_cmpl.success)
    {
      ESP_LOGE(TAG, "BLE GAP AUTH ERROR: 0x%x", param->ble_security.auth_cmpl.fail_reason);
      if (BLE_KyBrd->pDFault->DeBug)
      {
        sprintf(msgbuf, "%s: BLE GAP AUTH ERROR: 0x%x", TAG, param->ble_security.auth_cmpl.fail_reason);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_RED);
      }
    }
    else
    {
      ESP_LOGV(TAG, "BLE GAP AUTH SUCCESS");

      if (BLE_KyBrd->pDFault->DeBug)
      {
        sprintf(msgbuf, "%s: BLE GAP AUTH SUCCESS\n", TAG);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_GREEN);
      }
    }
    break;
  case ESP_GAP_BLE_KEY_EVT: // shows the ble key info share with peer device to the user.
    ESP_LOGV(TAG, "BLE GAP KEY type = %s", ble_key_type_str(param->ble_security.ble_key.key_type));
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP KEY type = %s\n", TAG, ble_key_type_str(param->ble_security.ble_key.key_type));
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    break;

  case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // ESP_IO_CAP_OUT
    // The app will receive this evt when the IO has Output capability and the peer device IO has Input capability.
    // Show the passkey number to the user to input it in the peer device.
    // ESP_LOGV(TAG, "BLE GAP PASSKEY_NOTIF passkey:%d", param->ble_security.key_notif.passkey); // JMH changed %d to %lu
    ESP_LOGV(TAG, "BLE GAP PASSKEY_NOTIF passkey:%lu", param->ble_security.key_notif.passkey); // JMH changed %d to %lu for Windows 10 Version
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: \nBLE GAP PASSKEY_NOTIF passkey: %lu\n\nENTER NOW\n", TAG, param->ble_security.key_notif.passkey);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      sprintf(msgbuf, "%s: %lu\n", TAG, param->ble_security.key_notif.passkey);
      BLE_KyBrd->pmsgbx->dispStat(msgbuf, TFT_WHITE); // this has to be syncronized with display updates
    }
    if (BLE_KyBrd->pairing_handler != nullptr)
      (*BLE_KyBrd->pairing_handler)(param->ble_security.key_notif.passkey);
    break;

  case ESP_GAP_BLE_NC_REQ_EVT: // ESP_IO_CAP_IO
    // The app will receive this event when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
    // show the passkey number to the user to confirm it with the number displayed by peer device.
    // ESP_LOGV(TAG, "BLE GAP NC_REQ passkey:%d", param->ble_security.key_notif.passkey); // JMH changed %d to %lu
    ESP_LOGV(TAG, "BLE GAP NC_REQ passkey:%lu", param->ble_security.key_notif.passkey); // JMH changed %d to %lu for Windows 10 version
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP NC_REQ passkey:%lu\n", TAG, param->ble_security.key_notif.passkey);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    if (esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true) == ESP_OK)
    {
      /*with esp32s3 BLE this never happens*/
      PairFlg1 = true;
      ESP_LOGI(TAG, "hidh_dev_open complete(Alt)");
    }
    else
    {
      ESP_LOGI(TAG, "esp_ble_confirm_reply: FAILED");
    }
    break;

  case ESP_GAP_BLE_PASSKEY_REQ_EVT: // ESP_IO_CAP_IN
    // The app will receive this evt when the IO has Input capability and the peer device IO has Output capability.
    // See the passkey number on the peer device and send it back.
    ESP_LOGV(TAG, "BLE GAP PASSKEY_REQ");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP PASSKEY_REQ\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 1234);
    break;

  case ESP_GAP_BLE_SEC_REQ_EVT:
    ESP_LOGV(TAG, "BLE GAP SEC_REQ");
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP SEC_REQ\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    // Send the positive(true) security response to the peer device to accept the security request.
    // If not accept the security request, should send the security response with negative(false) accept value.
    esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    break;

  default:
    ESP_LOGV(TAG, "BLE GAP EVENT %s", ble_gap_evt_str(event));
    if (BLE_KyBrd->pDFault->DeBug)
    {
      sprintf(msgbuf, "%s: BLE GAP EVENT %s\n", TAG, ble_gap_evt_str(event));
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    break;
  }

  // printf("ble_gap_event_handler EXIT\n"); //JMH Diagnosstic testing
} // jhm added this bracket
/*JMH for this application, the following runs once, as part of main startup; keyboard.setup*/
esp_err_t BTKeyboard::init_ble_gap(void)
{
  esp_err_t ret;
  printf("\n");
  ESP_LOGI(BLE_TAG, "BEGIN... 'init_ble_gap()'");
  if ((ret = esp_ble_gap_register_callback(ble_gap_event_handler)) != ESP_OK)
  {
    ESP_LOGE(BLE_TAG, "esp_ble_gap_register_callback failed: %d", ret);
    return ret;
  }
  else
  {
    ESP_LOGI(BLE_TAG, "%s(): esp_ble_gap_register_callback Complete\n", __func__);
  }
  /*added to support battery level status*/
  // printf("\nStart - esp_ble_gattc_app_register(PROFILE_BATTERY_SERVICE)\n");
  // ret = esp_ble_gattc_app_register(PROFILE_BATTERY_SERVICE); //  0x180f
  // if (ret)
  // {
  //   ESP_LOGE(BLE_TAG, "%s(): gattc app register failed, error code = %x\n", __func__, ret);
  //   return ret;
  // }
  // else
  // {
  //   ESP_LOGI(BLE_TAG, "%s(): gattc app register Complete\n", __func__);
  //   vTaskDelay(1500/portTICK_PERIOD_MS);
  // }
  // printf("\nStart - esp_ble_gatt_set_local_mtu(128)\n");
  // esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(128);
  // if (local_mtu_ret)
  // {
  //   ESP_LOGE(BLE_TAG, "%s(): set local  MTU failed, error code = %x", __func__, local_mtu_ret);
  //   return local_mtu_ret;
  // }
  // else
  // {
  //   ESP_LOGI(BLE_TAG, "%s(): gatt set local mtu Complete\n", __func__);
  // }

  /*END Battery status support*/
  return ret;
}

#if CONFIG_BT_HID_HOST_ENABLED
esp_err_t BTKeyboard::start_bt_scan(uint32_t seconds)
{
  esp_err_t ret = ESP_OK;
  if ((ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, (int)(seconds / 1.28), 0)) != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_bt_gap_start_discovery failed: %d", ret);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "esp_bt_gap_start_discovery failed: %d", ret);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    return ret;
  }
  return ret;
}
#endif /*CONFIG_BT_HID_HOST_ENABLED*/

esp_err_t BTKeyboard::start_ble_scan(uint32_t seconds)
{
  /*shown here just for reference*/
  // static esp_ble_scan_params_t hid_scan_params = {
  //     .scan_type = BLE_SCAN_TYPE_ACTIVE,
  //     .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  //     .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  //     .scan_interval = 0x50,
  //     .scan_window = 0x30,
  //     .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
  // };

  esp_err_t ret = ESP_OK;
  if ((ret = esp_ble_gap_set_scan_params(&hid_scan_params)) != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %d", ret);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "esp_ble_gap_set_scan_params failed: %d", ret);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    return ret;
  }
  WAIT_BLE_CB();
  int64_t alarm = esp_timer_get_time();
  printf("alarm: %llu\n",alarm);
  if ((ret = esp_ble_gap_start_scanning(seconds)) != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ble_gap_start_scanning failed: %d", ret);
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "esp_ble_gap_start_scanning failed: %d", ret);
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    return ret;
  }
  return ret;
}
/*new -esp32s3 CW_machine, not used */
esp_err_t BTKeyboard::esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name)
{

  esp_err_t ret;

  const uint8_t hidd_service_uuid128[] = {
      0xfb,
      0x34,
      0x9b,
      0x5f,
      0x80,
      0x00,
      0x00,
      0x80,
      0x00,
      0x10,
      0x00,
      0x00,
      0x12,
      0x18,
      0x00,
      0x00,
  };

  esp_ble_adv_data_t ble_adv_data = {
      .set_scan_rsp = false,
      .include_name = true,
      .include_txpower = true,
      .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
      .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
      .appearance = appearance,
      .manufacturer_len = 0,
      .p_manufacturer_data = NULL,
      .service_data_len = 0,
      .p_service_data = NULL,
      .service_uuid_len = sizeof(hidd_service_uuid128),
      .p_service_uuid = (uint8_t *)hidd_service_uuid128,
      .flag = 0x6,
  };

  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  // esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;//you have to enter the key on the host
  // esp_ble_io_cap_t iocap = ESP_IO_CAP_IN;//you have to enter the key on the device
  esp_ble_io_cap_t iocap = ESP_IO_CAP_IO; // you have to agree that key matches on both
  // esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;//device is not capable of input or output, unsecure
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t key_size = 16;   // the key size should be 7~16 bytes
  uint32_t passkey = 1234; // ESP_IO_CAP_OUT

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, 1)) != ESP_OK) //
  {
    ESP_LOGE(TAG, "GAP set_security_param AUTHEN_REQ_MODE failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_security_param IOCAP_MODE failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, 1)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_security_param SET_INIT_KEY failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, 1)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_security_param SET_RSP_KEY failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, 1)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_security_param MAX_KEY_SIZE failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t))) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_security_param SET_STATIC_PASSKEY failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_set_device_name(device_name)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP set_device_name failed: %d", ret);
    return ret;
  }

  if ((ret = esp_ble_gap_config_adv_data(&ble_adv_data)) != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP config_adv_data failed: %d", ret);
    return ret;
  }

  return ret;
}
/*end new*/
/*new beleive this is a server function & not used in this application, because this ESP32s3 is acting as a cliant*/
esp_err_t BTKeyboard::esp_hid_ble_gap_adv_start(void)
{
  static esp_ble_adv_params_t hidd_adv_params = {
      .adv_int_min = 0x20,
      .adv_int_max = 0x30,
      .adv_type = ADV_TYPE_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };
  return esp_ble_gap_start_advertising(&hidd_adv_params);
}
/*
Start loooking for adversitizing BLE devices & create a table of found devices
*/
esp_err_t
BTKeyboard::esp_hid_scan(uint32_t seconds, size_t *num_results, esp_hid_scan_result_t **results)
{
  if (num_bt_scan_results || bt_scan_results || num_ble_scan_results || ble_scan_results)
  {
    ESP_LOGE(TAG, "There are old scan results. Free them first!");
    if (pDFault->DeBug)
    {
      sprintf(msgbuf, "There are old scan results. Free them first!");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
    }
    return ESP_FAIL;
  }

  if (start_ble_scan(seconds) == ESP_OK)
  {
    printf("BLE scan - START\n");
    //printf("Free memory: %d bytes\n", (int)esp_get_free_heap_size());
    /*Uncomment next 2 lins for Crash Demo*/
    //int *ptr = NULL;
    //*ptr = 0;
    WAIT_BLE_CB();
    printf("BLE scan - DONE\n");
  }
  else
  {
    return ESP_FAIL;
  }
#if CONFIG_BT_HID_HOST_ENABLED
  if (start_bt_scan(seconds) == ESP_OK)
  {
    WAIT_BT_CB();
  }
  else
  {
    return ESP_FAIL;
  }
#endif /*CONFIG_BT_HID_HOST_ENABLED */
  *num_results = num_bt_scan_results + num_ble_scan_results;
  *results = bt_scan_results;

  if (num_bt_scan_results)
  {
    while (bt_scan_results->next != NULL)
    {
      bt_scan_results = bt_scan_results->next;
    }
    bt_scan_results->next = ble_scan_results;
  }
  else
  {
    *results = ble_scan_results;
  }

  num_bt_scan_results = 0;
  bt_scan_results = NULL;
  num_ble_scan_results = 0;
  ble_scan_results = NULL;

  return ESP_OK;
}
/*
start a scan. which creates a table of advertizing devices
from the table determine if there is an HID device among them,
or a device the been previously 'paired'
Note: if a connection is made (with a paired device), this
function/method normally takes a little over 4 seconds to complete.
If it fails to 'open' the paired device. the process completes in <2 seconds
*/
void BTKeyboard::devices_scan(int seconds_wait_time)
{
  const char *TAG = "devices_scan()";
  size_t results_len = 0;
  esp_hid_scan_result_t *results = NULL;
  // bt_keyboard->OpnEvntFlg = false;
  ESP_LOGI(TAG, "SCAN...");
  sprintf(msgbuf, "Looking for New Keyboard...\n");
  pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);

  // start scan for HID devices

  esp_hid_scan(seconds_wait_time, &results_len, &results);
  ESP_LOGI(TAG, "SCAN: %u results", results_len);
  if (results_len)
  {
    esp_hid_scan_result_t *r = results;
    esp_hid_scan_result_t *cr = NULL;
    int devcnt = 1;
    while (r)
    {
      printf("%d. ", devcnt);
      printf("%s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
      devcnt++;
      printf("RSSI: %d, ", r->rssi);
      printf("USAGE: %s, ", r->usage ? esp_hid_usage_str(r->usage) : "      ");
      if (r->transport != NULL)
        printf("TRANSPORT: %d, ", r->transport ? r->transport : 999);
      else
        printf("TRANSPORT: 'Not available', ");
      if (r->transport == ESP_HID_TRANSPORT_BLE)
      {
        cr = r;
        printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
        printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
      }
      if (r->transport == ESP_HID_TRANSPORT_BT) // with ESP32S3 (BLE bluetooth) this is never 'true'
      {
        cr = r;
        printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
        esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
        printf("] srv 0x%03x, ", r->bt.cod.service);
        ;
        printf(", ");
      }
      printf("NAME: %s ", r->name ? r->name : "Not Available");
      printf("\n");
      if (pDFault->DeBug)
      {
        sprintf(msgbuf, "NAME: %s\n", r->name ? r->name : "Not Available");
        pmsgbx->dispKeyBrdTxt(msgbuf, TFT_WHITE);
      }
      r = r->next;
    }

    if (cr != NULL)
    {
      // open the last result
      if ((int)(cr->ble.addr_type) == 9536)
        BLE_KyBrd->Adc_Sw = 1; // classic bluetooth
      else
      { // ble
        BLE_KyBrd->Adc_Sw = 2;
        BLE_KyBrd->inPrgsFlg = true;
      }
      // ESP_ERROR_CHECK(adc_continuous_stop(adc_handle)); // true; user has pressed Ctl+S key, & wants to configure default settings
      // adcON = false;
      vTaskSuspend(GoertzelTaskHandle);
      ESP_LOGI(TAG, "SUSPEND GoertzelHandler TASK");
      vTaskSuspend(CWDecodeTaskHandle);
      ESP_LOGI(TAG, "SUSPEND CWDecodeTaskHandle TASK");
      vTaskDelay(20);
      uint32_t EvntStart = pdTICKS_TO_MS(xTaskGetTickCount());
      printf("START - esp_hidh_dev_open()\n");
      dev_Opnd = esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type); // Returns immediately w/ BT classic device; But waits for pairing code w/ BLE device
      uint16_t dev_open_intrvl = (uint16_t)(pdTICKS_TO_MS(xTaskGetTickCount()) - EvntStart);
      printf("DONE - esp_hidh_dev_open():  intrvl: %d\n", dev_open_intrvl);
      if (dev_Opnd != NULL && !Shwgattcb)
      {
        vTaskDelay(50 / portTICK_PERIOD_MS); // pause long enough for flag change to take effect
        int lpcnt = 0;
        while(Shwgattcb && lpcnt < 60){
          vTaskDelay(150 / portTICK_PERIOD_MS);
          lpcnt++;
        }
        if(lpcnt<60){
        Shwgattcb = false;
        ESP_LOGI(TAG, "hidh_dev_open complete(Nrml)");
        }
        else
        {
          ESP_LOGE(TAG, "Someting went wronG - Never GOT an HIDH OPEN EVNT");
        }
      }
      
      // if(!Shwgattcb)
      // {
      ESP_LOGI(TAG, "RESUME CWDecodeTaskHandle TASK");
      vTaskResume(CWDecodeTaskHandle);
      ESP_LOGI(TAG, "RESUME GoertzelHandler TASK");
      vTaskResume(GoertzelTaskHandle);
      // }
      if(dev_open_intrvl<2000)
      {
        sprintf(msgbuf, "!!!KeyBoard NOT found!!!\n");
        pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
        vTaskDelay(500/portTICK_PERIOD_MS);
        printf("esp_hidh_dev_close()- START\n");
        esp_hidh_dev_close(dev_Opnd);
        printf("esp_hidh_dev_close()- DONE\n");
        //vTaskDelay(8000/portTICK_PERIOD_MS);
        int lpcnt = 0;
        while(lpcnt < 20)
        {
          vTaskDelay(100/portTICK_PERIOD_MS);
          lpcnt++;
        }
        // free the results
        esp_hid_scan_results_free(results);
        printf("Close BLE dev 'failed Connection' wait period DONE\n");
        //printf("launching 'BLE Scan Task' from %s\n", __func__);
        //xTaskCreatePinnedToCore(BLE_scan_tsk, "BLE Scan Task", 8192, NULL, 1, &BLEscanTask_hndl, 0);
        PairFlg1 = false;
        return;        
      }
      vTaskDelay(20);
    }
    else
    {
      sprintf(msgbuf, "No KeyBoard found...\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
    }
    // free the results
    esp_hid_scan_results_free(results);
  }
  else
  {
    if (BLE_KyBrd->OpnEvntFlg)
    {
      sprintf(msgbuf, "Keyboard Ready...\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
    }
    else
    {
      sprintf(msgbuf, "NONE found...\n");
      pmsgbx->dispKeyBrdTxt(msgbuf, TFT_YELLOW);
    }
  }
}

void BTKeyboard::hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
  static const char *TAG1 = "hidh_callback";
  char temp[250];
  uint16_t clr = 0;
  temp[0] = 255;
  esp_hidh_event_t event = (esp_hidh_event_t)id;
  esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;
  bool talk = false; // true;//false; //set to true for hid callback debugging //JMH Diagnosstic testing
  if (mutex != NULL)
  {
    /* See if we can obtain the semaphore.  If the semaphore is not
    available wait 15 ticks to see if it becomes free. */
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) //(TickType_t)15
    {
      if (talk)
        printf("hidh_callback\n");
      switch (event)
      {
      case ESP_HIDH_OPEN_EVENT:
      {
        sprintf(temp, "ESP_HIDH_OPEN_EVENT\n");
        if(Shwgattcb) printf(temp);
        if (param->open.status == ESP_OK)
        {
          const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
          ESP_LOGI(TAG, "bda Memory %p "ESP_BD_ADDR_STR ,bda , ESP_BD_ADDR_HEX(bda));
          //ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
          /*Now go query the reports for this device/keyboard, & See if the Logictech 'F-Key' config report can be found*/
          /*JMH added the following to ferrit out Logitech keyboards*/
          /*Query the keyboard for reports*/

          if (talk)
          {
            printf("\n\nSTARTING REPORT REQUEST\n");
          }
          bool K380Fnd = false;
          int rptIndx = 0;
          size_t num_reports;
          esp_hid_report_map_t MyMap;
          // printf("ESP_HIDH_OPEN_EVENT: dev info "ESP_BD_ADDR_STR"\n",  ESP_BD_ADDR_HEX(param->open.dev->bda));
          // ESP_ERROR_CHECK(esp_hidh_dev_reports_get(param->open.dev, &num_reports, &MyMap.reports));
          esp_err_t ret = esp_hidh_dev_reports_get(param->open.dev, &num_reports, &MyMap.reports);
          if (ret != ESP_OK)
          {
            // uint8_t *MyRptData = (uint8_t *)malloc(sizeof(uint8_t)+1);
            // param->open.dev->config.report_maps[0].data = MyRptData;
            // param->open.dev->config.report_maps[0].len = 1;
            Shwgattcb = true; // enable gattc cb reporting/logging
            // printf("\nESP_HIDH_OPEN_EVENT: 'get reports' FAILED\n");
            // esp_ble_gattc_cb_param_t *Myparam = (esp_ble_gattc_cb_param_t *)malloc(sizeof(esp_ble_gattc_cb_param_t));
            // if (Myparam == nullptr)
            // {
            //   ESP_LOGE(TAG, "Malloc result Myparam failed!");
            //   return;
            // }
            // memcpy(&Myparam->disconnect.remote_bda, &param->open.dev->bda, sizeof(esp_bd_addr_t));
            // Myparam->disconnect.conn_id = gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id;
            // Myparam->connect.conn_id = gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id;
            // esp_gattc_cb(ESP_GATTC_DISCONNECT_EVT, gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if, Myparam);
            // memcpy(&Myparam->close.remote_bda, &param->open.dev->bda, sizeof(esp_bd_addr_t));
            // Myparam->close.conn_id = gl_profile_tab[PROFILE_BATTERY_SERVICE].conn_id;
            // //printf("simulate ESP_GATTC_CLOSE_EVT:\n dev Addr: " ESP_BD_ADDR_STR ",\n gattc_if: %d\n", ESP_BD_ADDR_HEX(Myparam->close.remote_bda), gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if);
            // esp_gattc_cb(ESP_GATTC_CLOSE_EVT, gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if, Myparam);
            // free(Myparam);
            // int cnt = 0;
            // while(Shwgattcb && cnt <30)
            // {
            //   vTaskDelay(100 / portTICK_PERIOD_MS);
            //   cnt++;
            // }
            //Shwgattcb = false;
            break;
          }
          // esp_err_t ret =1;
          // while(ret != ESP_OK)
          // {
          //   vTaskDelay(50/portTICK_PERIOD_MS);
          //   printf("ESP_HIDH_OPEN_EVENT: dev info "ESP_BD_ADDR_STR"\n",  ESP_BD_ADDR_HEX(param->open.dev->bda));
          //   ret =esp_hidh_dev_reports_get(param->open.dev, &num_reports, &MyMap.reports);
          //   printf("stuck in wait loop\n");
          // }
          MyMap.reports_len = num_reports;
          if (talk)
          {
            sprintf(temp, "REPORT COUNT: %d\n", (int)num_reports);
            printf(temp);
          }
          /*see esp_hid_common.h for code definitions*/
          char typestr[10];
          char modestr[10];
          char usestr[14];
          for (int i = 0; i < MyMap.reports_len; i++)
          {
            if ((MyMap.reports[i].report_type == 0x2) && (MyMap.reports[i].report_id == 0x10 || MyMap.reports[i].report_id == 0x11) && (MyMap.reports[i].usage == 0x40))
            {
              rptIndx = i;
              K380Fnd = true;
            }

            /* for debugging print out reports found on the current device/keyboard*/
            /* decode the values returned*/
            switch (MyMap.reports[i].report_type)
            {
            case 1:
            {
              sprintf(typestr, "1 INPUT  ");
              break;
            }
            case 2:
            {
              sprintf(typestr, "2 OUTPUT ");
              break;
            }
            case 3:
            {
              sprintf(typestr, "3 FEATURE");
              break;
            }
            }
            switch (MyMap.reports[i].protocol_mode)
            {
            case 0:
            {
              sprintf(modestr, "0 BOOT  ");
              break;
            }
            case 1:
            {
              sprintf(modestr, "1 REPORT");
              break;
            }
            }
            switch (MyMap.reports[i].usage)
            {
            case 0:
            {
              sprintf(usestr, " 0 GENERIC ");
              break;
            }
            case 1:
            {
              sprintf(usestr, " 1 KEYBOARD");
              break;
            }
            case 2:
            {
              sprintf(usestr, " 2 MOUSE   ");
              break;
            }
            case 4:
            {
              sprintf(usestr, " 4 JOYSTICK");
              break;
            }
            case 8:
            {
              sprintf(usestr, " 8 GAMEPAD ");
              break;
            }
            case 16:
            {
              sprintf(usestr, "16 TABLET  ");
              break;
            }
            case 32:
            {
              sprintf(usestr, "32 CCONTROL");
              break;
            }
            case 64:
            {
              sprintf(usestr, "64 VENDOR  ");
              break;
            }
            }
            if (talk)
            {
              sprintf(temp, "| Map_Index %02x; Rpt_Id: %02x; Usage:%s; Type:%s; Mode%s; Rpt_len: %02d |\n",
                      MyMap.reports[i].map_index,
                      MyMap.reports[i].report_id,
                      usestr,
                      typestr,
                      modestr,
                      MyMap.reports[i].value_len);
              printf(temp);
            }
          }

          if (K380Fnd) // If true, we found a K380 Keyboard; Send command to reconfigure 'F' Keys
          {
            /*Logitech K380 Key Command*/
            // uint8_t configKBrd[] = {0xff, 0x0b, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            uint8_t configKBrd[] = {0xff, 0x0c, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            // uint8_t configKBrd[] = {0x00, 0x00, 0x2a, 0x19, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};
            if (talk)
            {
              printf("K380 Key command: ");
              for (int i = 0; i < sizeof(configKBrd); i++)
              {
                sprintf(temp, "0x%02x:", configKBrd[i]); //
                printf(temp);
              }
              printf("\n\n");
            }
            ESP_ERROR_CHECK(esp_hidh_dev_output_set(param->open.dev, MyMap.reports[rptIndx].map_index, MyMap.reports[rptIndx].report_id, configKBrd, sizeof(configKBrd)));
            sprintf(temp, "K380 Found-Configured F-keys");
            // BLE_KyBrd->pmsgbx->dispKeyBrdTxt(temp, TFT_BLUE);
            // esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            // esp_err_t ret = esp_ble_gattc_open(gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
            // if (ret != ESP_OK)
            // {
            //   ESP_LOGI(TAG, "esp_ble_gattc_open - FAILED");
            // }
            // else
            // {
            //   ESP_LOGI(TAG, "esp_ble_gattc_open - COMPLETE; Remote Addr: "ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(scan_result->scan_rst.bda));
            // }
          }
          else
            sprintf(temp, "OPEN: %02x:%02x:%02x:%02x:%02x:%02x", ESP_BD_ADDR_HEX(bda));

          clr = TFT_GREEN;
          // if(!BLE_KyBrd->OpnEvntFlg && !adcON)
          if (!BLE_KyBrd->OpnEvntFlg)
          {
            ESP_LOGI(TAG1, "adc_continuous_start");
            BLE_KyBrd->Adc_Sw = 2;
            PairFlg1 = true;
          }
          BLE_KyBrd->OpnEvntFlg = true;
        }
        else
        {
          ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
      }
      case ESP_HIDH_BATTERY_EVENT:
      {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGV(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        sprintf(temp, " BATTERY: %02x:%02x:%02x:%02x:%02x:%02x; %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
        clr = TFT_YELLOW;
        BLE_KyBrd->set_battery_level(param->battery.level);
        break;
      }
      case ESP_HIDH_INPUT_EVENT:
      {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGV("INPUT_EVENT", ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %02x, Len: %d, Data:",
                 ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->input.usage),
                 param->input.map_index,
                 param->input.report_id,
                 param->input.length);
        char databuf[150];
        char databuf1[75];
        int cntr = 0;
        databuf1[0] = 0;
        // const uint8_t indat = param->input.data;
        // unsigned int curdata[2];
        for (int i = 0; i < param->input.length; i++)
        {
          // curdata[0] = (unsigned int )param->input.data[i];
          sprintf(databuf, "%s:%02x", databuf1, param->input.data[i]);
          if (param->input.data[i] == 0x01)
            cntr++;
          else
            cntr = 0;
          for (int p = 0; p < sizeof(databuf1); p++)
          {
            databuf1[p] = databuf[p];
            if (databuf[p] == 0)
              break;
          }
        }
        for (int p = 0; p < sizeof(databuf1); p++)
        {
          databuf1[p] = databuf[p];
          if (databuf[p] == 0)
            break;
        }
        if (talk)
        {
          sprintf(temp, "INPUT_EVENT; ADDR:%02x:%02x:%02x:%02x:%02x:%02x; MAP:%s; NDX:%d; ID:%02x; LEN:%d; DATA%s\n",
                  ESP_BD_ADDR_HEX(bda),
                  esp_hid_usage_str(param->input.usage),
                  param->input.map_index,
                  param->input.report_id,
                  param->input.length,
                  databuf1);
        }
        clr = TFT_BLACK;
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->input.data, param->input.length, ESP_LOG_DEBUG);
        if ((cntr != 6) && !BLE_KyBrd->trapFlg)
        {
          /*Classic BT Keybrds send key press date w/ data len = 8; K380s keybrd has a deta lenght of 7*/
          if (param->input.length == 8)
            BLE_KyBrd->push_key(param->input.data, param->input.length); // normal path when keystroke data is good/usuable
          else if (param->input.length == 7)
          { // looks like K380s data. So add one additional data element (0) to the array
            uint8_t tmpdat[8];
            tmpdat[0] = param->input.data[0];
            tmpdat[1] = 0;
            for (int i = 1; i < param->input.length; i++)
            {
              tmpdat[i + 1] = param->input.data[i];
            }
            BLE_KyBrd->push_key(tmpdat, 8);
          }
        }
        else if (BLE_KyBrd->trapFlg)
        { // path used to test for data corruption recovery
          if (param->input.length == 1)
          {
            BLE_KyBrd->trapFlg = false;

            BLE_KyBrd->pmsgbx->dispStat("KEYBOARD READY", TFT_GREEN);
            xSemaphoreGive(mutex);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            xSemaphoreTake(mutex, portMAX_DELAY);
          }
          break;
        }
        else
        {
          /*Keybrd data is corrupt. So sit and wait while send buffer contents is completeS*/
          BLE_KyBrd->trapFlg = true;
          BLE_KyBrd->pmsgbx->dispStat("!!! KEYBOARD BLOCKED !!!", TFT_RED);
          if (talk)
          {
            printf("!!! DATA BLOCKED !!!\n");
          }
          xSemaphoreGive(mutex); // we're likely going to be stuck for awhile. So let the other parts of the program conitnue (i.e. send whats in the cw buffer)
          // while(clrbuf){
          vTaskDelay(200 / portTICK_PERIOD_MS);
          //}
          if (talk)
            printf("WAITING FOR SOMETHING GOOD TO HAPPEN\n");
          xSemaphoreTake(mutex, portMAX_DELAY);
        }
        break;
      }
      case ESP_HIDH_FEATURE_EVENT:
      {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGV("FEATURE_EVENT", ESP_BD_ADDR_STR " FEATURE: %8s MAP: %2u, ID: %3u, Len: %d",
                 ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage),
                 param->feature.map_index,
                 param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX_LEVEL("FEATURE_EVENT", param->feature.data, param->feature.length, ESP_LOG_DEBUG);
        sprintf(temp, " FEATURE: %02x:%02x:%02x:%02x:%02x:%02x, MAP: %s, ID: %3u\n",
                ESP_BD_ADDR_HEX(bda),
                esp_hid_usage_str(param->feature.usage),
                param->feature.report_id); //,
                                           //(int)param->feature.length);
        // BLE_KyBrd->pmsgbx->dispKeyBrdTxt(temp, TFT_ORANGE);
        // temp[0] = 255;
        break;
      }
      case ESP_HIDH_CLOSE_EVENT:
      {
        ESP_LOGI(TAG, "%s() ESP_HIDH_CLOSE_EVENT", __func__);
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGV(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        sprintf(temp, " CLOSE: %02x:%02x:%02x:%02x:%02x:%02x", ESP_BD_ADDR_HEX(bda));
        clr = TFT_RED;
        PairFlg1 = false;
        int lpcnt = 0;
        while(lpcnt < 40)
        {
          vTaskDelay(50/portTICK_PERIOD_MS);
          lpcnt++;
        }
        if(!Shwgattcb)
        {
          printf("launching 'BLE Scan Task' from %s\n", __func__);
          xTaskCreatePinnedToCore(BLE_scan_tsk, "BLE Scan Task", 8192, NULL, 1, &BLEscanTask_hndl, 0);
        }
        else
        {
          /*
            * Added the following to prevent crashing during hidh close event
            * specifically at esp_hidh.c:671: 
            *    free((void *)dev->config.report_maps[d].data);
            */
          param->close.dev->config.report_maps_len = 0;
          printf("skip starting 'BLE Scan Task' from %s\n", __func__);
        }
        break;
      }
      default:
        ESP_LOGV(TAG, "**** UnHandled EVENT: %d\n", event);
        sprintf(temp, "UNHANDLED EVENT: %d\n", event);
        break;
      }
      if (temp[0] != 255)
      {
        /*took the post to display status line out because it could induce an unnecessary crash out because */
        if (clr != TFT_BLACK)
          BLE_KyBrd->pmsgbx->dispStat(temp, clr);
        if (talk)
          printf("hidh_callback EXIT: %s\n", temp); // JMH Diagnostic testing
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }

      xSemaphoreGive(mutex);
    }
  }
}

void BTKeyboard::push_key(uint8_t *keys, uint8_t size)
{

  KeyInfo inf;
  inf.modifier = (KeyModifier)keys[0];

  uint8_t max = (size > MAX_KEY_COUNT + 2) ? MAX_KEY_COUNT : size - 2;
  inf.keys[0] = inf.keys[1] = inf.keys[2] = 0;
  for (int i = 0; i < max; i++)
  {
    inf.keys[i] = keys[i + 2];
  }

  xQueueSend(event_queue, &inf, 0);
}

char BTKeyboard::wait_for_ascii_char(bool forever)
{
  KeyInfo inf;
  last_ch = 0;

  // printf( "**  wait_for_ascii_cha\n");//for testing/debugging
  while (true)
  {
    repeat_period = pdMS_TO_TICKS(120);
    if (!wait_for_low_event(inf, (last_ch == 0) ? (forever ? portMAX_DELAY : 0) : repeat_period))
    {
      repeat_period = pdMS_TO_TICKS(120);
      return last_ch;
    }
    // printf( "**\n");
    int k = -1;
    for (int i = 0; i < MAX_KEY_COUNT; i++)
    {
      if ((k < 0) && key_avail[i])
        k = i;
      key_avail[i] = inf.keys[i] == 0;
    }

    if (k < 0)
    {
      continue;
    }

    char ch = inf.keys[k];
    /*JMH: uncomment the following to expose key entry values*/
    // char buf[20];
    // if((inf.keys[0] != (uint8_t)0) || (inf.keys[1] != (uint8_t)0) || (inf.keys[0] != (uint8_t)0) || ((uint8_t)inf.modifier != (uint8_t)0)){
    //    sprintf(buf, "%02x; %02x; %02x; %02x\n", inf.keys[0], inf.keys[1], inf.keys[2], (uint8_t)inf.modifier);
    //    printf(buf);//print to computer
    //    pmsgbx->dispKeyBrdTxt(buf, TFT_GOLD); //print to LCD Display
    // }
    /* special test for TAB */
    if (inf.keys[0] == 43 && ((uint8_t)inf.modifier == 0))
    {
      return last_ch = 0x97; // replace with up arrow
    }
    /* special test for Shift+TAB */
    if (inf.keys[0] == 43 && ((uint8_t)inf.modifier == 2))
    {
      return last_ch = 0x98; // replace with down arrow
    }
    /* special test for ctr+Enter */
    if ((inf.keys[1] == 0x28 || inf.keys[0] == 0x28) && ((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16))
    {
      return last_ch = 0x9A;
    }
    /* special test for ctr+'T' */
    if (((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16) && (inf.keys[1] == 23 || inf.keys[0] == 0x17))
    {
      return last_ch = 0x9B;
    }
    /* special test for ctr+'S' */
    if (((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16) && (inf.keys[1] == 22 || inf.keys[0] == 0x16))
    {
      return last_ch = 0x9C;
    }
    /* special test for ctr+'f' */
    if (((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 9) || (inf.keys[1] == 9)))
    {
      return last_ch = 0x9D;
    }
    /* special test for Left ctr+'d' */
    if (((uint8_t)inf.modifier == 1) && ((inf.keys[0] == 7) || (inf.keys[1] == 7)))
    {
      return last_ch = 0x9E;
    }
    /* special test for Left or Right ctr+'g' */
    if (((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 10) || (inf.keys[1] == 10)))
    {
      return last_ch = 0xA1;
    }
    /* special test for Right ctr+'g' */
    if (((uint8_t)inf.modifier == 16) && (inf.keys[0] == 10))
    {
      return last_ch = 0xA2;
    }
    /* special test for ctr+'p' */
    if (((uint8_t)inf.modifier == 1 || (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 0x13) || (inf.keys[1] == 0x13)))
    {
      return last_ch = 0x9F;
    }
    /* special test for Right ctr+'d' */
    if (((uint8_t)inf.modifier == 16) && (inf.keys[0] == 7))
    {
      return last_ch = 0xA0;
    }
    /* special test for 'del' */
    if (inf.keys[0] == 76 && ((uint8_t)inf.modifier == 0))
    {
      return last_ch = 0x2A;
    }
    if (ch >= 4)
    {
      if ((uint8_t)inf.modifier & CTRL_MASK)
      {
        if (ch < (3 + 26))
        {
          repeat_period = pdMS_TO_TICKS(500);
          return last_ch = (ch - 3);
        }
      }
      else if (ch <= 0x52)
      {
        // ESP_LOGI(TAG, "Scan code: %d", ch);
        if (ch == KEY_CAPS_LOCK)
          ch = 0; // JMH Don't need or want "Caps lock" feature on CW keyboard
        // if (ch == KEY_CAPS_LOCK)
        // caps_lock = !caps_lock; //JMH Don't need or want "Caps lock" feature on CW keyboard
        if ((uint8_t)inf.modifier & SHIFT_MASK)
        {
          if (caps_lock)
          {
            repeat_period = pdMS_TO_TICKS(500);
            return last_ch = shift_trans_dict[(ch - 4) << 1];
          }
          else
          {
            repeat_period = pdMS_TO_TICKS(500);
            return last_ch = shift_trans_dict[((ch - 4) << 1) + 1];
          }
        }
        else
        {
          if (caps_lock)
          {
            repeat_period = pdMS_TO_TICKS(500);
            return last_ch = shift_trans_dict[((ch - 4) << 1) + 1];
          }
          else
          {
            repeat_period = pdMS_TO_TICKS(500);
            return last_ch = shift_trans_dict[(ch - 4) << 1];
          }
        }
      }
    }

    last_ch = 0;
  }
}
bool BTKeyboard::GetPairFlg1(void)
{
  if (PairFlg1)
  {
    // /*added to support battery level status*/
    // printf("\nStart - esp_ble_gattc_app_register(PROFILE_BATTERY_SERVICE)\n");
    // esp_err_t ret = esp_ble_gattc_app_register(PROFILE_BATTERY_SERVICE); // 0x180f
    // if (ret)
    // {
    //   ESP_LOGE(BLE_TAG, "%s(): gattc app register failed, error code = %x\n", __func__, ret);
    //   // return ret;
    // }
    // else
    // {
    //   ESP_LOGI(BLE_TAG, "%s(): gattc app register Complete\n", __func__);
    //   vTaskDelay(1500 / portTICK_PERIOD_MS);
    // }
  }

  return PairFlg1;
};

void BTKeyboard::BatValueRpt(void)
{
  const char *TAG = "BTKeyboard::BatValueRpt";
  printf("\nStart - esp_ble_gatt_set_local_mtu(128)\n");
  esp_err_t ret = esp_ble_gatt_set_local_mtu(128);
  if (ret)
  {
    ESP_LOGE(TAG, "%s(): set local  MTU failed, error code = %x", __func__, ret);
    return;
  }
  else
  {
    ESP_LOGI(TAG, "%s(): gatt set local mtu Complete\n", __func__);
  }
  printf("\nStart - BatValueRpt(void)\n");
  ESP_LOGI(TAG, "Remote Dev Addr: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(Prd_bda));
  ret = esp_ble_gattc_open(gl_profile_tab[PROFILE_BATTERY_SERVICE].gattc_if, Prd_bda, Prd_addr_type, true);
  if (ret)
  {
    ESP_LOGE(TAG, "%s(): Open Battery Profile, error code = %x", __func__, ret);
  }
  else
  {
    ESP_LOGI(TAG, "%s(): Open Battery Profile Complete\n", __func__);
  }
  return;
};