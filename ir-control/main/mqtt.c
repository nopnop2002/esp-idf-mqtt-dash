/* MQTT (over TCP) Example

	 This example code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "driver/rmt.h"

#include "mqtt.h"
#include "ir_tools.h"

#define RMT_TX_CHANNEL 1 /*!< RMT channel for transmitter */
//#define RMT_TX_GPIO_NUM GPIO_NUM_12 /*!< GPIO number for transmitter signal */
#define RMT_TX_GPIO_NUM CONFIG_RMT_TX_GPIO_NUM /*!< GPIO number for transmitter signal */

static const char *TAG = "MQTT";

/*
 * @brief Event handler registered to receive MQTT events
 *
 *	This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	esp_mqtt_event_handle_t event = event_data;

    MQTT_t *mqttBuf = event->user_context;
    ESP_LOGI(TAG, "taskHandle=%x", mqttBuf->taskHandle);
    mqttBuf->event_id = event_id;
	switch ((esp_mqtt_event_id_t)event_id) {
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

void mqtt(void *pvParameters)
{
	ESP_LOGI(TAG, "Start");
	ESP_LOGI(TAG, "CONFIG_BROKER_URL=[%s]", CONFIG_BROKER_URL);

	uint8_t mac[8];
	ESP_ERROR_CHECK(esp_base_mac_addr_get(mac));
	for(int i=0;i<8;i++) {
		ESP_LOGI(TAG, "mac[%d]=%x", i, mac[i]);
	}
	char client_id[64];
	//strcpy(client_id, pcTaskGetName(NULL));
	sprintf(client_id, "esp32-%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	ESP_LOGI(TAG, "client_id=[%s]", client_id);

    MQTT_t mqttBuf;
    mqttBuf.taskHandle = xTaskGetCurrentTaskHandle();
	esp_mqtt_client_config_t mqtt_cfg = {
		.user_context = &mqttBuf,
		.uri = CONFIG_BROKER_URL,
		.client_id = client_id
	};

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(mqtt_client);

	// Setup IR transmitter
	ESP_LOGI(TAG, "RMT_TX_GPIO_NUM=%d", RMT_TX_GPIO_NUM);
	rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(RMT_TX_GPIO_NUM, RMT_TX_CHANNEL);
	rmt_tx_config.tx_config.carrier_en = true;
	rmt_config(&rmt_tx_config);
	rmt_driver_install(RMT_TX_CHANNEL, 0, 0);
	ir_builder_config_t ir_builder_config = IR_BUILDER_DEFAULT_CONFIG((ir_dev_t)RMT_TX_CHANNEL);
	ir_builder_config.flags |= IR_TOOLS_FLAGS_PROTO_EXT;
	ir_builder_t *ir_builder = NULL;
	ir_builder = ir_builder_rmt_new_nec(&ir_builder_config);
	ESP_LOGI(TAG, "Setup IR transmitter done");

	while (1) {
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		ESP_LOGI(TAG, "event_id=%d", mqttBuf.event_id);

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

            // Send new key code
   			rmt_item32_t *items = NULL;
		    size_t length = 0;
            ESP_ERROR_CHECK(ir_builder->build_frame(ir_builder, addr, cmd));
            ESP_ERROR_CHECK(ir_builder->get_result(ir_builder, &items, &length));
            // To send data according to the waveform items.
            rmt_write_items(RMT_TX_CHANNEL, items, length, false);
            // Send repeat code
            vTaskDelay(pdMS_TO_TICKS(ir_builder->repeat_period_ms));
            ESP_ERROR_CHECK(ir_builder->build_repeat_frame(ir_builder));
            ESP_ERROR_CHECK(ir_builder->get_result(ir_builder, &items, &length));
            rmt_write_items(RMT_TX_CHANNEL, items, length, false);
		} else if (mqttBuf.event_id == MQTT_EVENT_ERROR) {
			break;
		}
	} // end while

	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
