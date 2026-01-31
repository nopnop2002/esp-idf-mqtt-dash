/*
	MQTT (over TCP) Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"

#include "mqtt.h"

#include "driver/rmt_tx.h"
#include "ir_nec_encoder.h"

#define EXAMPLE_IR_RESOLUTION_HZ 1000000 // 1MHz resolution, 1 tick = 1us

//#define RMT_TX_GPIO_NUM GPIO_NUM_12 /*!< GPIO number for transmitter signal */
#define RMT_TX_GPIO_NUM CONFIG_RMT_TX_GPIO_NUM /*!< GPIO number for transmitter signal */

static const char *TAG = "MQTT";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = event_data;
	MQTT_t *mqttBuf = handler_args;
	ESP_LOGI(TAG, "taskHandle=0x%x", (unsigned int)mqttBuf->taskHandle);
	mqttBuf->event_id = event_id;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			ESP_LOGI(TAG, "TOPIC=[%.*s] DATA=[%.*s]\r", event->topic_len, event->topic, event->data_len, event->data);
			mqttBuf->topic_len = event->topic_len;
			for(int i=0;i<event->topic_len;i++) {
				mqttBuf->topic[i] = event->topic[i];
				mqttBuf->topic[i+1] = 0;
			}
			mqttBuf->data_len = event->data_len;
			for(int i=0;i<event->data_len;i++) {
				mqttBuf->data[i] = event->data[i];
				mqttBuf->data[i+1] = 0;
			}
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			xTaskNotifyGive( mqttBuf->taskHandle );
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
}

void initializeRMT(rmt_channel_handle_t *tx_channel, rmt_encoder_handle_t *nec_encoder, rmt_transmit_config_t *transmit_config) {
	// Setup IR transmitter
	ESP_LOGI(TAG, "create RMT TX channel");
	rmt_tx_channel_config_t _tx_channel_cfg = {
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
		.mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
		.trans_queue_depth = 4,  // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
		.gpio_num = RMT_TX_GPIO_NUM,
	};
	rmt_channel_handle_t _tx_channel = NULL;
	ESP_ERROR_CHECK(rmt_new_tx_channel(&_tx_channel_cfg, &_tx_channel));

	ESP_LOGI(TAG, "modulate carrier to TX channel");
	rmt_carrier_config_t _carrier_cfg = {
		.duty_cycle = 0.33,
		.frequency_hz = 38000, // 38KHz
	};
	ESP_ERROR_CHECK(rmt_apply_carrier(_tx_channel, &_carrier_cfg));

	// this example won't send NEC frames in a loop
	rmt_transmit_config_t _transmit_config = {
		.loop_count = 0, // no loop
	};

	ESP_LOGI(TAG, "install IR NEC encoder");
	ir_nec_encoder_config_t _nec_encoder_cfg = {
		.resolution = EXAMPLE_IR_RESOLUTION_HZ,
	};
	rmt_encoder_handle_t _nec_encoder = NULL;
	ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&_nec_encoder_cfg, &_nec_encoder));

	ESP_LOGI(TAG, "enable RMT TX channels");
	ESP_ERROR_CHECK(rmt_enable(_tx_channel));

	*tx_channel = _tx_channel;
	*nec_encoder = _nec_encoder;
	*transmit_config = _transmit_config;
}

esp_err_t query_mdns_host(const char * host_name, char *ip);
void convert_mdns_host(char * from, char * to);

void mqtt(void *pvParameters)
{
	ESP_LOGI(TAG, "Start Subscribe Broker:%s", CONFIG_MQTT_BROKER);

	// Set client id from mac
	uint8_t mac[8];
	ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	for(int i=0;i<8;i++) {
		ESP_LOGI(TAG, "mac[%d]=%x", i, mac[i]);
	}
	char client_id[64];
	//strcpy(client_id, pcTaskGetName(NULL));
	sprintf(client_id, "esp32-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	ESP_LOGI(TAG, "client_id=[%s]", client_id);

	// Resolve mDNS host name
	char ip[128];
	ESP_LOGI(TAG, "CONFIG_MQTT_BROKER=[%s]", CONFIG_MQTT_BROKER);
	convert_mdns_host(CONFIG_MQTT_BROKER, ip);
	ESP_LOGI(TAG, "ip=[%s]", ip);
	char uri[138];
	sprintf(uri, "mqtt://%s", ip);
	ESP_LOGI(TAG, "uri=[%s]", uri);

	MQTT_t mqttBuf;
	mqttBuf.taskHandle = xTaskGetCurrentTaskHandle();
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = uri,
		.broker.address.port = 1883,
#if CONFIG_BROKER_AUTHENTICATION
		.credentials.username = CONFIG_AUTHENTICATION_USERNAME,
		.credentials.authentication.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.credentials.client_id = client_id
	};

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, &mqttBuf);
	esp_mqtt_client_start(mqtt_client);

	// Setup IR transmitter
	rmt_channel_handle_t tx_channel = NULL;
	rmt_encoder_handle_t nec_encoder = NULL;
	rmt_transmit_config_t transmit_config = {};
	initializeRMT(&tx_channel, &nec_encoder, &transmit_config);
	ESP_LOGI(TAG, "Setup IR transmitter done");

	while (1) {
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		ESP_LOGI(TAG, "event_id=%"PRIi32, mqttBuf.event_id);

		if (mqttBuf.event_id == MQTT_EVENT_CONNECTED) {
			esp_mqtt_client_subscribe(mqtt_client, CONFIG_MQTT_SUB_TOPIC, 0);
			ESP_LOGI(TAG, "Subscribe to MQTT Server");
		} else if (mqttBuf.event_id == MQTT_EVENT_DISCONNECTED) {
			break;
		} else if (mqttBuf.event_id == MQTT_EVENT_DATA) {
			ESP_LOGD(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
			ESP_LOGD(TAG, "DATA=%.*s\r", mqttBuf.data_len, mqttBuf.data);

			// parse data
			// data is addr/cmd
			uint8_t data[2];
			int index = 0;
			char wk[16];
			memset(wk, 0, sizeof(wk));
			for (int i=0;i<strlen(mqttBuf.data);i++) {
				if (mqttBuf.data[i] == '/') {
					index++;
					if (index == 2) break;
					memset(wk, 0, sizeof(wk));
				} else {
					strncat(wk, &mqttBuf.data[i], 1);
					ESP_LOGD(TAG, "wk=[%s]", wk);
					data[index] = strtol(wk, NULL, 16);
					ESP_LOGD(TAG, "data[%d]=%x", index, data[index]);
				}
			}

			uint16_t addr = data[0];
			uint16_t cmd = data[1];
			cmd = ((~cmd) << 8) |  cmd; // Reverse cmd + cmd
			addr = ((~addr) << 8) | addr; // Reverse addr + addr
			ESP_LOGI(TAG, "cmd=0x%x",cmd);
			ESP_LOGI(TAG, "addr=0x%x",addr);

			// transmit IR NEC packets
			const ir_nec_scan_code_t scan_code = {
				.address = addr,
				.command = cmd,
			};
			ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &scan_code, sizeof(scan_code), &transmit_config));
		} else if (mqttBuf.event_id == MQTT_EVENT_ERROR) {
			break;
		}
	} // end while

	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
