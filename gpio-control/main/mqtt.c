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
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

#include "mqtt.h"

static const char *TAG = "MQTT";

extern QueueHandle_t xQueueSubscribe;

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

	MQTT_t mqttBuf;
	switch ((esp_mqtt_event_id_t)event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			mqttBuf.topic_type = MQTT_CONNECT;
			if (xQueueSendFromISR(xQueueSubscribe, &mqttBuf, NULL) != pdPASS) {
				ESP_LOGE(TAG, "xQueueSend Fail");
			}
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			mqttBuf.topic_type = MQTT_DISCONNECT;
			if (xQueueSendFromISR(xQueueSubscribe, &mqttBuf, NULL) != pdPASS) {
				ESP_LOGE(TAG, "xQueueSend Fail");
			}
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
			//ESP_LOGI(TAG, "DATA=%.*s\r", event->data_len, event->data);

			//MQTT_t mqttBuf;
			mqttBuf.topic_type = MQTT_SUB;
			mqttBuf.topic_len = event->topic_len;
			for(int i=0;i<event->topic_len;i++) {
				mqttBuf.topic[i] = event->topic[i];
				mqttBuf.topic[i+1] = 0;
			}
			mqttBuf.data_len = event->data_len;
			for(int i=0;i<event->data_len;i++) {
				mqttBuf.data[i] = event->data[i];
				mqttBuf.data[i+1] = 0;
			}
			if (xQueueSendFromISR(xQueueSubscribe, &mqttBuf, NULL) != pdPASS) {
				ESP_LOGE(TAG, "xQueueSend Fail");
			}
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			mqttBuf.topic_type = MQTT_ERROR;
			if (xQueueSendFromISR(xQueueSubscribe, &mqttBuf, NULL) != pdPASS) {
				ESP_LOGE(TAG, "xQueueSend Fail");
			}
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
}

typedef struct {
	uint16_t gpio;
	uint16_t value;
} GPIO_t;


esp_err_t build_table(char *buffer, GPIO_t **tables, int16_t *ntable)
{
	ESP_LOGI(__FUNCTION__, "buffer=[%s]", buffer);
	int _ntable = 0;
	for(int index=0;index<strlen(buffer);index++) {
		if (buffer[index] == '/') _ntable++;
	}
	ESP_LOGI(__FUNCTION__, "_ntable=%d", _ntable);
	
	*tables = calloc(_ntable, sizeof(GPIO_t));
	if (*tables == NULL) {
		ESP_LOGE(__FUNCTION__, "Error allocating memory for topic");
		return ESP_ERR_NO_MEM;
	}

	int index = 0;
	char *start = buffer;
	char *find;
	while(1) {
		ESP_LOGD(TAG, "start=%p [%s]", start, start);
		find = strstr(start, "/");
		ESP_LOGD(TAG, "find=%p", find);
		if (find == 0) break;
		ESP_LOGD(TAG, "find=%p [%s]", find, find);

		char item[16];
		memset(item, 0, sizeof(item));
		strncpy(item, start, find-start);
		ESP_LOGI(TAG, "item=[%s]",item);
		int value = 0;
		bool valid = true;
		for (int i=0;i<strlen(item);i++) {
			int c1 = item[i];
			if (c1 == 0x3D) { // =
				item[i] = 0;
				char wk[10];
				strcpy(wk, &item[i+1]);
				ESP_LOGI(TAG, "wk=[%s]", wk);
				value = atoi(wk);
			} else {
				if (isdigit(c1) == 0) valid = false;
			}
		}
		ESP_LOGI(TAG, "valid=%d item=[%s] value=%d",valid, item, value);
		if (valid) {
			(*tables+index)->gpio = atoi(item);
			(*tables+index)->value = value;
			index++;
		}
		start = find + 1;
	}
	*ntable = index;
	return ESP_OK;
}

void dump_table(GPIO_t *table, int16_t ntable)
{
	for(int i=0;i<ntable;i++) {
		ESP_LOGI(__FUNCTION__, "table[%d] gpio=%d value=%d",
		i, (table+i)->gpio, (table+i)->value);
	}
}

bool isNumber(char *str)
{
	for (int i=0;i<strlen(str);i++) {
		int c1 = str[i];
		if (isdigit(c1) == 0) return false;
	}
	return true;
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
	esp_mqtt_client_config_t mqtt_cfg = {
		.uri = CONFIG_BROKER_URL,
		.client_id = client_id
	};

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(mqtt_client);

	//esp_mqtt_client_subscribe(mqtt_client, "/topic/test", 0);
	//esp_mqtt_client_subscribe(mqtt_client, CONFIG_MQTT_SUB_TOPIC, 0);
	//ESP_LOGI(TAG, "Subscribe to MQTT Server");

	MQTT_t mqttBuf;
	//int gpio_table[64];
	//int gpio_count = 0;
	GPIO_t *gpios;
	int16_t	ngpios;
	while (1) {
		xQueueReceive(xQueueSubscribe, &mqttBuf, portMAX_DELAY);
		ESP_LOGI(TAG, "xQueueReceive type=%d", mqttBuf.topic_type);

		if (mqttBuf.topic_type == MQTT_CONNECT) {
			esp_mqtt_client_subscribe(mqtt_client, CONFIG_MQTT_SUB_TOPIC, 0);
			ESP_LOGI(TAG, "Subscribe to MQTT Server");
		} else if (mqttBuf.topic_type == MQTT_DISCONNECT) {
			break;
		} else if (mqttBuf.topic_type == MQTT_SUB) {
			ESP_LOGI(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
			ESP_LOGI(TAG, "DATA=%.*s\r", mqttBuf.data_len, mqttBuf.data);
			if (strcmp(mqttBuf.topic, "/esp32/gpio/init") == 0) {
				ESP_LOGI(TAG, "init ngpios=%d", ngpios);
				if (ngpios != 0) continue;

				// Initialize GPIO
				esp_err_t ret = build_table(mqttBuf.data, &gpios, &ngpios);
				if (ret != ESP_OK) continue;
				dump_table(gpios, ngpios);
				for(int i=0;i<ngpios;i++) {
					ESP_LOGI(TAG, "gpio=%d value=%d", gpios[i].gpio, gpios[i].value);
					gpio_reset_pin(gpios[i].gpio);
					gpio_set_direction(gpios[i].gpio, GPIO_MODE_OUTPUT );
					gpio_set_level(gpios[i].gpio, gpios[i].value);
					sprintf(mqttBuf.topic, "/esp32/gpio/%02d", gpios[i].gpio);
					sprintf(mqttBuf.data, "%d", gpios[i].value);
					esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, 0, 1, 0);
					vTaskDelay(10);
				}
			} else if (strcmp(mqttBuf.topic, "/esp32/gpio/set") == 0) {
				ESP_LOGI(TAG, "set ngpios=%d", ngpios);
				if (ngpios == 0) continue;
				int value = atoi(mqttBuf.data);
				for(int i=0;i<ngpios;i++) {
					ESP_LOGD(TAG, "gpio=%d value=%d", gpios[i].gpio, gpios[i].value);
					if (value == gpios[i].value) continue;
					gpios[i].value = value;
					gpio_set_level(gpios[i].gpio, gpios[i].value);
					ESP_LOGI(TAG, "gpio %d set to %d", gpios[i].gpio, gpios[i].value);
					sprintf(mqttBuf.topic, "/esp32/gpio/%02d", gpios[i].gpio);
					sprintf(mqttBuf.data, "%d", gpios[i].value);
					esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, 0, 1, 0);
					vTaskDelay(10);
				}
			} else {
				ESP_LOGI(TAG, "gpio ngpios=%d", ngpios);
				if (ngpios == 0) continue;
				char wk[16];
				strcpy(wk, &mqttBuf.topic[12]);
				ESP_LOGD(TAG, "gpio wk=[%s]", wk);
				if (isNumber(wk) == false) {
					ESP_LOGE(TAG, "Bad gpio [%s]", wk);
				} else {
					int gpio = atoi(wk);
					int value = atoi(mqttBuf.data);
					ESP_LOGI(TAG, "gpio gpio=%d value=%d", gpio, value);
					for(int i=0;i<ngpios;i++) {
						ESP_LOGD(TAG, "gpio=%d value=%d", gpios[i].gpio, gpios[i].value);
						if (gpio != gpios[i].gpio) continue;
						if (value == gpios[i].value) continue;
						gpios[i].value = value;
						gpio_set_level(gpios[i].gpio, gpios[i].value);
						ESP_LOGI(TAG, "gpio %d set to %d", gpios[i].gpio, gpios[i].value);
					}
				}
			}
		} else if (mqttBuf.topic_type == MQTT_ERROR) {
			break;
		}
	} // end while

	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
