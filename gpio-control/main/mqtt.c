/* MQTT (over TCP) Example

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
#include "driver/gpio.h"

#include "mqtt.h"

static const char *TAG = "MQTT";

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
#endif
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_event_handle_t event = event_data;
	MQTT_t *mqttBuf = handler_args;
#else
	MQTT_t *mqttBuf = event->user_context;
#endif
	ESP_LOGI(TAG, "taskHandle=0x%x", (unsigned int)mqttBuf->taskHandle);
	mqttBuf->event_id = event->event_id;
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
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
	return ESP_OK;
#endif
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
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = uri,
        .broker.address.port = 1883,
#if CONFIG_BROKER_AUTHENTICATION
        .credentials.username = CONFIG_AUTHENTICATION_USERNAME,
        .credentials.authentication.password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.credentials.client_id = client_id
	};
#else
	esp_mqtt_client_config_t mqtt_cfg = {
		.user_context = &mqttBuf,
		.uri = uri,
		.port = 1883,
		.event_handle = mqtt_event_handler,
#if CONFIG_BROKER_AUTHENTICATION
        .username = CONFIG_AUTHENTICATION_USERNAME,
        .password = CONFIG_AUTHENTICATION_PASSWORD,
#endif
		.client_id = client_id
	};
#endif

	esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
	esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, &mqttBuf);
#endif

	esp_mqtt_client_start(mqtt_client);

	int base_topic_len = strlen(CONFIG_MQTT_SUB_TOPIC)-1;
	ESP_LOGI(TAG, "base_topic_len=%d", base_topic_len);
	char base_topic[64];
	strcpy(base_topic, CONFIG_MQTT_SUB_TOPIC);
	base_topic[base_topic_len] = 0;
	ESP_LOGI(TAG, "base_topic=[%s]", base_topic);

	GPIO_t *gpios;
	int16_t	ngpios;

	while (1) {
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		ESP_LOGI(TAG, "event_id=%"PRIi32, mqttBuf.event_id);

		if (mqttBuf.event_id == MQTT_EVENT_CONNECTED) {
			esp_mqtt_client_subscribe(mqtt_client, CONFIG_MQTT_SUB_TOPIC, 0);
			ESP_LOGI(TAG, "Subscribe to MQTT Server");
		} else if (mqttBuf.event_id == MQTT_EVENT_DISCONNECTED) {
			break;
		} else if (mqttBuf.event_id == MQTT_EVENT_DATA) {
			ESP_LOGI(TAG, "TOPIC=%.*s\r", mqttBuf.topic_len, mqttBuf.topic);
			ESP_LOGI(TAG, "DATA=%.*s\r", mqttBuf.data_len, mqttBuf.data);
			// get bottom topic
			// /esp32/gpio/init --> init
			// /esp32/gpio/set --> set
			// /esp32/gpio/13 --> 13
			char bottom_topic[64];
			strcpy(bottom_topic, &mqttBuf.topic[base_topic_len]);
			ESP_LOGI(TAG, "bottom_topic=[%s]", bottom_topic);
			if (strcmp(bottom_topic, "init") == 0) {
				ESP_LOGI(TAG, "init ngpios=%d", ngpios);
				//if (ngpios != 0) continue;
				if (ngpios != 0) {
					free(gpios);
					ngpios = 0;
				}

				// Initialize GPIO
				esp_err_t ret = build_table(mqttBuf.data, &gpios, &ngpios);
				if (ret != ESP_OK) continue;
				dump_table(gpios, ngpios);
				for(int i=0;i<ngpios;i++) {
					ESP_LOGI(TAG, "gpio=%d value=%d", gpios[i].gpio, gpios[i].value);
					gpio_reset_pin(gpios[i].gpio);
					gpio_set_direction(gpios[i].gpio, GPIO_MODE_OUTPUT );
					gpio_set_level(gpios[i].gpio, gpios[i].value);
					sprintf(mqttBuf.topic, "%s%02d", base_topic, gpios[i].gpio);
					sprintf(mqttBuf.data, "%d", gpios[i].value);
					esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, 0, 1, 0);
					vTaskDelay(10); // emqx is not so fast
				}
			} else if (strcmp(bottom_topic, "set") == 0) {
				ESP_LOGI(TAG, "set ngpios=%d", ngpios);
				if (ngpios == 0) continue;
				int value = atoi(mqttBuf.data);
				for(int i=0;i<ngpios;i++) {
					ESP_LOGD(TAG, "gpio=%d value=%d", gpios[i].gpio, gpios[i].value);
					if (value == gpios[i].value) continue;
					gpios[i].value = value;
					gpio_set_level(gpios[i].gpio, gpios[i].value);
					ESP_LOGI(TAG, "gpio %d set to %d", gpios[i].gpio, gpios[i].value);
					sprintf(mqttBuf.topic, "%s%02d", base_topic, gpios[i].gpio);
					sprintf(mqttBuf.data, "%d", gpios[i].value);
					esp_mqtt_client_publish(mqtt_client, mqttBuf.topic, mqttBuf.data, 0, 1, 0);
					vTaskDelay(10); // emqx is not so fast
				}
			} else {
				ESP_LOGI(TAG, "gpio ngpios=%d", ngpios);
				if (ngpios == 0) continue;
				if (isNumber(bottom_topic) == false) {
					ESP_LOGE(TAG, "Bad gpio [%s]", bottom_topic);
				} else {
					int gpio = atoi(bottom_topic);
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
		} else if (mqttBuf.event_id == MQTT_EVENT_ERROR) {
			break;
		}
	} // end while

	ESP_LOGI(TAG, "Task Delete");
	esp_mqtt_client_stop(mqtt_client);
	vTaskDelete(NULL);

}
