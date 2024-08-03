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
extern "C" {
#include "btc/btc_ble_storage.h"
}
#define SCAN 1

// uncomment to print all devices that were seen during a scan
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

static bool PairFlg1 = false; 
BTKeyboard *BLE_KyBrd = nullptr;
char msgbuf[256];


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

/*new*/
esp_err_t BTKeyboard::init_low_level(uint8_t mode)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    bt_cfg.mode = mode;
#endif
#if CONFIG_BT_HID_HOST_ENABLED
    if (mode & ESP_BT_MODE_CLASSIC_BT) {
        bt_cfg.bt_max_acl_conn = 3;
        bt_cfg.bt_max_sync_conn = 3;
    } else
#endif
    {
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret) {
            ESP_LOGE(TAG, "esp_bt_controller_mem_release failed: %d", ret);
            return ret;
        }
    }
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %d", ret);
        return ret;
    }

    ret = esp_bt_controller_enable((esp_bt_mode_t)mode);
    if (ret) {
        ESP_LOGE(TAG, "esp_bt_controller_enable failed: %d", ret);
        return ret;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %d", ret);
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %d", ret);
        return ret;
    }
#if CONFIG_BT_HID_HOST_ENABLED
    if (mode & ESP_BT_MODE_CLASSIC_BT) {
        ret = init_bt_gap();
        if (ret) {
            return ret;
        }
    }
#endif
#if CONFIG_BT_BLE_ENABLED
    if (mode & ESP_BT_MODE_BLE) {
        ret = init_ble_gap();
        if (ret) {
            return ret;
        }
    }
#endif /* CONFIG_BT_BLE_ENABLED */
    return ret;
}


esp_err_t BTKeyboard::esp_hid_gap_init(uint8_t mode)
{
    esp_err_t ret;
    if (!mode || mode > ESP_BT_MODE_BTDM) {
        ESP_LOGE(TAG, "Invalid mode given!");
        return ESP_FAIL;
    }

    if (bt_hidh_cb_semaphore != NULL) {
        ESP_LOGE(TAG, "Already initialised");
        return ESP_FAIL;
    }

    bt_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (bt_hidh_cb_semaphore == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
        return ESP_FAIL;
    }

    ble_hidh_cb_semaphore = xSemaphoreCreateBinary();
    if (ble_hidh_cb_semaphore == NULL) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex failed!");
        vSemaphoreDelete(bt_hidh_cb_semaphore);
        bt_hidh_cb_semaphore = NULL;
        return ESP_FAIL;
    }

    ret = init_low_level(mode);
    if (ret != ESP_OK) {
        vSemaphoreDelete(bt_hidh_cb_semaphore);
        bt_hidh_cb_semaphore = NULL;
        vSemaphoreDelete(ble_hidh_cb_semaphore);
        ble_hidh_cb_semaphore = NULL;
        return ret;
    }

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
#if HID_HOST_MODE != HIDH_BLE_MODE//HIDH_IDLE_MODE
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

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK( esp_hid_gap_init(HID_HOST_MODE) );
#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK( esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler) );
#endif /* CONFIG_BT_BLE_ENABLED */
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = nullptr,
    };
    ESP_ERROR_CHECK( esp_hidh_init(&config) );
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
    if (ret) {
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

}//end setup()

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
  //printf("bt_gap_event_handler Start\n"); //JMH Diagnosstic testing
  switch (event)
  {
  case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
  {
    ESP_LOGV(TAG, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP");
    // if(pDFault->DeBug){
    //sprintf(msgbuf, "BT GAP DISC_STATE %s", (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) ? "START" : "STOP"); //JMH Diagnosstic testing
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
    //ESP_LOGV(TAG, "BT GAP KEY_NOTIF passkey:%d", param->key_notif.passkey); // JMH changed %lu to %d 
    ESP_LOGV(TAG, "BT GAP KEY_NOTIF passkey:%lu", param->key_notif.passkey); // JMH changed for WINDOWS 10 version
    //   if(pDFault->DeBug){
    //sprintf(msgbuf, "BT GAP KEY_NOTIF passkey:%lu", param->key_notif.passkey); //JMH Diagnosstic testing
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }
    if (BLE_KyBrd->pairing_handler != nullptr)
      (*BLE_KyBrd->pairing_handler)(param->key_notif.passkey);
    break;
  case ESP_BT_GAP_MODE_CHG_EVT:
    ESP_LOGV(TAG, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode);
    // if(pDFault->DeBug){
    //sprintf(msgbuf, "BT GAP MODE_CHG_EVT mode:%d", param->mode_chg.mode); //JMH Diagnosstic testing
    //   pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    // }

    if((param->mode_chg.mode == 0) && !BLE_KyBrd->OpnEvntFlg && adcON){
      ESP_LOGI(TAG1, "adc_continuous_stop");
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
  //printf("bt_gap_event_handler: %s EXIT\n",msgbuf); //JMH Diagnosstic testing
}
#endif /*CONFIG_BT_HID_HOST_ENABLED*/

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
  /*JMH Will use this function to see if the 'calling' BLE device address matches a previously 'paired'(registered) Keyboard */
  // bt_status_t _btc_storage_get_ble_bonding_key(bt_bdaddr_t *remote_bd_addr,
  //                                           uint8_t key_type,
  //                                           char *key_value,
  //                                           int key_length)

  
  uint8_t key_type = BTM_LE_KEY_PID; 
  char key_value;
  int key_length = 30;
 
  bt_bdaddr_t remote_bda;
  memcpy(&remote_bda, &param->scan_rst.bda, sizeof(bt_bdaddr_t));
  bt_status_t stat =  btc_storage_get_ble_bonding_key(&remote_bda, key_type, &key_value, key_length);
  if (stat == BT_STATUS_SUCCESS)
  {/*we found a stored record for this BLE Keyboard/address*/
    ESP_LOGI(TAG, "Previously PAIRED");
    sprintf(msgbuf, "Found Paired Keybrd\n Standby... while CW Machine Re-Connects...");
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
    sprintf(msgbuf,ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(curHexAddr));
    ESP_LOGI(TAG, "Address '%s' does not match a 'Known' Device ", msgbuf );
  }
  
#endif
}

void BTKeyboard::ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  const char *TAG = "ble_gap_event_handler";
  //printf("ble_gap_event_handler\n"); //JMH Diagnosstic testing
  switch (event)
  {

    // SCAN

  case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
  {
    
    ESP_LOGV(TAG, "BLE GAP EVENT SCAN_PARAM_SET_COMPLETE");
    if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP EVENT SCAN_PARAM_SET_COMPLETE\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
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
      //handle_ble_device_result(&scan_result->scan_rst);
      //handle_ble_device_result(param);
      break;
    }
    case ESP_GAP_SEARCH_INQ_CMPL_EVT:
      ESP_LOGV(TAG, "BLE GAP EVENT SCAN DONE: %d", param->scan_rst.num_resps);
      if(BLE_KyBrd->pDFault->DeBug){
        sprintf(msgbuf, "%s: BLE GAP EVENT SCAN DONE: %d\n", TAG, param->scan_rst.num_resps);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
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
    if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP EVENT SCAN CANCELED\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    }
    break;
  }

    // ADVERTISEMENT

  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    ESP_LOGV(TAG, "BLE GAP ADV_DATA_SET_COMPLETE");
    if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP ADV_DATA_SET_COMPLETE\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    }
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    ESP_LOGV(TAG, "BLE GAP ADV_START_COMPLETE");
    if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP ADV_START_COMPLETE", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
    }
    break;

    // AUTHENTICATION

  case ESP_GAP_BLE_AUTH_CMPL_EVT:
    if (!param->ble_security.auth_cmpl.success)
    {
      ESP_LOGE(TAG, "BLE GAP AUTH ERROR: 0x%x", param->ble_security.auth_cmpl.fail_reason);
      if(BLE_KyBrd->pDFault->DeBug){
        sprintf(msgbuf, "%s: BLE GAP AUTH ERROR: 0x%x", TAG, param->ble_security.auth_cmpl.fail_reason);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_RED);
      }
    }
    else
    {
      ESP_LOGV(TAG, "BLE GAP AUTH SUCCESS");
      if(BLE_KyBrd->pDFault->DeBug){
        sprintf(msgbuf, "%s: BLE GAP AUTH SUCCESS\n", TAG);
        BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_GREEN);
      }
      break;

    case ESP_GAP_BLE_KEY_EVT: // shows the ble key info share with peer device to the user.
      ESP_LOGV(TAG, "BLE GAP KEY type = %s", ble_key_type_str(param->ble_security.ble_key.key_type));
      if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP KEY type = %s\n", TAG, ble_key_type_str(param->ble_security.ble_key.key_type));
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
      }
      break;

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // ESP_IO_CAP_OUT
      // The app will receive this evt when the IO has Output capability and the peer device IO has Input capability.
      // Show the passkey number to the user to input it in the peer device.
      //ESP_LOGV(TAG, "BLE GAP PASSKEY_NOTIF passkey:%d", param->ble_security.key_notif.passkey); // JMH changed %d to %lu
      ESP_LOGV(TAG, "BLE GAP PASSKEY_NOTIF passkey:%lu", param->ble_security.key_notif.passkey); // JMH changed %d to %lu for Windows 10 Version
      if(BLE_KyBrd->pDFault->DeBug){
         sprintf(msgbuf, "%s: \nBLE GAP PASSKEY_NOTIF passkey: %lu\n\nENTER NOW\n", TAG, param->ble_security.key_notif.passkey);
         BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
          sprintf(msgbuf, "%s: %lu\n", TAG, param->ble_security.key_notif.passkey);
        BLE_KyBrd->pmsgbx->dispStat(msgbuf,TFT_WHITE); //this has to be syncronized with display updates
      }
      if (BLE_KyBrd->pairing_handler != nullptr)
        (*BLE_KyBrd->pairing_handler)(param->ble_security.key_notif.passkey);
      break;

    case ESP_GAP_BLE_NC_REQ_EVT: // ESP_IO_CAP_IO
      // The app will receive this event when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
      // show the passkey number to the user to confirm it with the number displayed by peer device.
      //ESP_LOGV(TAG, "BLE GAP NC_REQ passkey:%d", param->ble_security.key_notif.passkey); // JMH changed %d to %lu
      ESP_LOGV(TAG, "BLE GAP NC_REQ passkey:%lu", param->ble_security.key_notif.passkey); // JMH changed %d to %lu for Windows 10 version
      if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP NC_REQ passkey:%lu\n", TAG, param->ble_security.key_notif.passkey);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
      }
      if(esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true) == ESP_OK)
      {
        /*with esp32s3 BLE this never happens*/
        PairFlg1 = true;
        ESP_LOGI(TAG, "hidh_dev_open complete");
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
      if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP PASSKEY_REQ\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
      }
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 1234);
      break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
      ESP_LOGV(TAG, "BLE GAP SEC_REQ");
      if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP SEC_REQ\n", TAG);
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
      }
      // Send the positive(true) security response to the peer device to accept the security request.
      // If not accept the security request, should send the security response with negative(false) accept value.
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;

    default:
      ESP_LOGV(TAG, "BLE GAP EVENT %s", ble_gap_evt_str(event));
      if(BLE_KyBrd->pDFault->DeBug){
      sprintf(msgbuf, "%s: BLE GAP EVENT %s\n", TAG, ble_gap_evt_str(event));
      BLE_KyBrd->pmsgbx->dispKeyBrdTxt(msgbuf,TFT_WHITE);
      }
      break;
    }
  }
  //printf("ble_gap_event_handler EXIT\n"); //JMH Diagnosstic testing
} // jhm added this bracket

esp_err_t  BTKeyboard::init_ble_gap(void)
{
    esp_err_t ret;

    if ((ret = esp_ble_gap_register_callback(ble_gap_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %d", ret);
        return ret;
    }
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
  static esp_ble_scan_params_t hid_scan_params = {
      .scan_type = BLE_SCAN_TYPE_ACTIVE,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
      .scan_interval = 0x50,
      .scan_window = 0x30,
      .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
  };

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
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
    };

    esp_ble_adv_data_t ble_adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
        .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
        .appearance = appearance,
        .manufacturer_len = 0,
        .p_manufacturer_data =  NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(hidd_service_uuid128),
        .p_service_uuid = (uint8_t *)hidd_service_uuid128,
        .flag = 0x6,
    };

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    //esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;//you have to enter the key on the host
    //esp_ble_io_cap_t iocap = ESP_IO_CAP_IN;//you have to enter the key on the device
    esp_ble_io_cap_t iocap = ESP_IO_CAP_IO;//you have to agree that key matches on both
    //esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;//device is not capable of input or output, unsecure
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t key_size = 16; //the key size should be 7~16 bytes
    uint32_t passkey = 1234;//ESP_IO_CAP_OUT

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, 1)) != ESP_OK)// 
    {
        ESP_LOGE(TAG, "GAP set_security_param AUTHEN_REQ_MODE failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_security_param IOCAP_MODE failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, 1)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_security_param SET_INIT_KEY failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, 1)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_security_param SET_RSP_KEY failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, 1)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_security_param MAX_KEY_SIZE failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t))) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_security_param SET_STATIC_PASSKEY failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_set_device_name(device_name)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP set_device_name failed: %d", ret);
        return ret;
    }

    if ((ret = esp_ble_gap_config_adv_data(&ble_adv_data)) != ESP_OK) {
        ESP_LOGE(TAG, "GAP config_adv_data failed: %d", ret);
        return ret;
    }

    return ret;
}
/*end new*/
/*new*/
esp_err_t BTKeyboard::esp_hid_ble_gap_adv_start(void)
{
    static esp_ble_adv_params_t hidd_adv_params = {
        .adv_int_min        = 0x20,
        .adv_int_max        = 0x30,
        .adv_type           = ADV_TYPE_IND,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    return esp_ble_gap_start_advertising(&hidd_adv_params);
}




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
    WAIT_BLE_CB();
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
void BTKeyboard::devices_scan(int seconds_wait_time)
{
  const char *TAG = "devices_scan()";
  size_t results_len = 0;
  esp_hid_scan_result_t *results = NULL;
  // bt_keyboard->OpnEvntFlg = false;
  ESP_LOGV(TAG, "SCAN...");
  sprintf(msgbuf, "LooKing for New Keyboard...\n");
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
        print_uuid(&r->bt.uuid);
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
      vTaskDelay(50 / portTICK_PERIOD_MS); // pause long enough for flag change to take effect
      // sprintf(msgbuf, "Enter PAIRING code for %s\n", cr->name);
      // pmsgbx->dispKeyBrdTxt(msgbuf, TFT_GREEN);
      ESP_LOGI(TAG, "cr->ble.addr_type: %d\n", (int)cr->ble.addr_type);
      esp_hidh_dev_t *dev_Opnd;
      printf("esp_hidh_dev_open(" ESP_BD_ADDR_STR ")\n", ESP_BD_ADDR_HEX(cr->bda));
      dev_Opnd = esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type); // Returns immediately w/ BT classic device; But waits for pairing code w/ BLE device
      if (dev_Opnd != NULL)
        PairFlg1 = true;
      ESP_LOGI(TAG, "hidh_dev_open complete");
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
  bool talk = false; //true;//false; //set to true for hid callback debugging //JMH Diagnosstic testing
  if (mutex != NULL)
  {
    /* See if we can obtain the semaphore.  If the semaphore is not
    available wait 15 ticks to see if it becomes free. */
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) //(TickType_t)15
    {
      if(talk) printf("hidh_callback\n"); 
      switch (event)
      {
      case ESP_HIDH_OPEN_EVENT:
      {
        sprintf(temp, "ESP_HIDH_OPEN_EVENT\n");
        if (param->open.status == ESP_OK)
        {
          const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
        //  ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
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

          ESP_ERROR_CHECK(esp_hidh_dev_reports_get(param->open.dev, &num_reports, &MyMap.reports));
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
            uint8_t configKBrd[] = {0xff, 0x0b, 0x1e, 0x00, 0x00, 0x00};
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
          }
          else
            sprintf(temp, "OPEN: %02x:%02x:%02x:%02x:%02x:%02x", ESP_BD_ADDR_HEX(bda));

          clr = TFT_GREEN;
          //if(!BLE_KyBrd->OpnEvntFlg && !adcON)
          if(!BLE_KyBrd->OpnEvntFlg)
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
        ESP_LOGV("INPUT_EVENT", ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:",
                 ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->input.usage),
                 param->input.map_index,
                 param->input.report_id,
                 param->input.length);
        char databuf[150];
        char databuf1[75];
        int cntr = 0;
        databuf1[0] =0;
        //const uint8_t indat = param->input.data;
        //unsigned int curdata[2];
        for(int i = 0; i<param->input.length; i++){
          //curdata[0] = (unsigned int )param->input.data[i];
          sprintf(databuf, "%s:%02x", databuf1, param->input.data[i]);
          if( param->input.data[i] == 0x01) cntr++;
          else cntr = 0;
          for(int p = 0; p<sizeof(databuf1); p++){
            databuf1[p] = databuf[p];
            if(databuf[p]==0) break;
          }
        }
        for(int p = 0; p<sizeof(databuf1); p++){
          databuf1[p] = databuf[p];
          if(databuf[p]==0) break;
        }
        if (talk){
          sprintf(temp, "INPUT_EVENT; ADDR:%02x:%02x:%02x:%02x:%02x:%02x; MAP:%s; NDX:%d; ID:%3u; LEN:%d; DATA%s\n",
                   ESP_BD_ADDR_HEX(bda),
                   esp_hid_usage_str(param->input.usage),
                   param->input.map_index,
                   param->input.report_id,
                   param->input.length,
                   databuf1);
        }
        clr = TFT_BLACK;                  
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->input.data, param->input.length, ESP_LOG_DEBUG);
        if((cntr != 6) && !BLE_KyBrd->trapFlg){
          /*Classic BT Keybrds send key press date w/ data len = 8; K380s keybrd has a deta lenght of 7*/
          if(param->input.length == 8) BLE_KyBrd->push_key(param->input.data, param->input.length); // normal path when keystroke data is good/usuable
          else if(param->input.length == 7){// looks like K380s data. So add one additional data element (0) to the array
            uint8_t tmpdat[8];
            tmpdat[0] = param->input.data[0];
            tmpdat[1] = 0;
            for(int i =1; i< param->input.length; i++){
              tmpdat[i+1] = param->input.data[i];
            }
            BLE_KyBrd->push_key(tmpdat, 8);
          } 
        }
        else if(BLE_KyBrd->trapFlg){// path used to test for data corruption recovery 
          if(param->input.length == 1){
            BLE_KyBrd->trapFlg = false;
            
            BLE_KyBrd->pmsgbx->dispStat("KEYBOARD READY", TFT_GREEN);
            xSemaphoreGive(mutex);
            vTaskDelay(200/portTICK_PERIOD_MS);
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
          xSemaphoreGive(mutex);//we're likely going to be stuck for awhile. So let the other parts of the program conitnue (i.e. send whats in the cw buffer)
          //while(clrbuf){
            vTaskDelay(200/portTICK_PERIOD_MS);
          //}
          if(talk) printf("WAITING FOR SOMETHING GOOD TO HAPPEN\n");
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
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGV(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        sprintf(temp, " CLOSE: %02x:%02x:%02x:%02x:%02x:%02x", ESP_BD_ADDR_HEX(bda));
        clr = TFT_RED;
        PairFlg1 = false;
        //vTaskResume(BLEscanTask_hndl);
        xTaskCreatePinnedToCore(BLE_scan_tsk, "BLE Scan Task", 8192, NULL, 1, &BLEscanTask_hndl, 0);
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
        if(clr != TFT_BLACK) BLE_KyBrd->pmsgbx->dispStat(temp, clr);
        if(talk) printf("hidh_callback EXIT: %s\n", temp); //JMH Diagnostic testing
        vTaskDelay(500/portTICK_PERIOD_MS);
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
    if ((inf.keys[1] == 0x28 || inf.keys[0] == 0x28) && ((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16))
    {
      return last_ch = 0x9A;
    }
    /* special test for ctr+'T' */
    if (((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16) && (inf.keys[1] == 23 || inf.keys[0] == 0x17))
    {
      return last_ch = 0x9B;
    }
    /* special test for ctr+'S' */
    if (((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16) && (inf.keys[1] == 22 || inf.keys[0] == 0x16))
    {
      return last_ch = 0x9C;
    }
    /* special test for ctr+'f' */
    if (((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 9) || (inf.keys[1] == 9)))
    {
      return last_ch = 0x9D;
    }
    /* special test for Left ctr+'d' */
    if (((uint8_t)inf.modifier == 1) && ((inf.keys[0] == 7) || (inf.keys[1] == 7)))
    {
      return last_ch = 0x9E;
    }
     /* special test for Left or Right ctr+'g' */
    if (((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 10) || (inf.keys[1] == 10)))
    {
      return last_ch = 0xA1;
    }
    /* special test for Right ctr+'g' */
    if (((uint8_t)inf.modifier == 16) && (inf.keys[0] == 10))
    {
      return last_ch = 0xA2;
    }
    /* special test for ctr+'p' */
    if (((uint8_t)inf.modifier == 1|| (uint8_t)inf.modifier == 16) && ((inf.keys[0] == 0x13) || (inf.keys[1] == 0x13)))
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
        if (ch == KEY_CAPS_LOCK) ch =0; //JMH Don't need or want "Caps lock" feature on CW keyboard
        // if (ch == KEY_CAPS_LOCK)
          //caps_lock = !caps_lock; //JMH Don't need or want "Caps lock" feature on CW keyboard
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
  return PairFlg1;
};