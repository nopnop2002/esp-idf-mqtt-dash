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
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "driver/ledc.h"

#include "mqtt.h"

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

typedef struct {
	uint16_t gpio;
	uint16_t value;
} GPIO_t;


bool isNumber(char *str)
{
	for (int i=0;i<strlen(str);i++) {
		int c1 = str[i];
		if (isdigit(c1) == 0) return false;
	}
	return true;
}

// LEDC Stuff
#define LEDC_TIMER			LEDC_TIMER_0
#define LEDC_MODE			LEDC_LOW_SPEED_MODE
//#define LEDC_OUTPUT_IO	(5) // Define the output GPIO
//#define LEDC_CHANNEL		LEDC_CHANNEL_0
#define LEDC_DUTY_RES		LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY			(4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY		(5000) // Frequency in Hertz. Set frequency at 5 kHz

// gpio_num : the LEDC output gpio_num, if you want to use gpio16, gpio_num = 16
// timer_run : The timer source of channel (0 - 3)
// channel : LEDC channel (0 - 7)
static void ledc_init(int gpio, int channel)
{
	ESP_LOGD(__FUNCTION__, "gpio=%d", gpio);
	ESP_LOGD(__FUNCTION__, "channel=%d", channel);

	// Prepare and then apply the LEDC PWM timer configuration
	ledc_timer_config_t ledc_timer = {
		.speed_mode			= LEDC_MODE,
		.timer_num			= LEDC_TIMER,
		.duty_resolution	= LEDC_DUTY_RES,
		.freq_hz			= LEDC_FREQUENCY,  // Set output frequency at 5 kHz
		.clk_cfg			= LEDC_AUTO_CLK
	};
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

	// Prepare and then apply the LEDC PWM channel configuration
	ledc_channel_config_t ledc_channel = {
		.speed_mode			= LEDC_MODE,
		.channel			= channel,
		.timer_sel			= LEDC_TIMER,
		.intr_type			= LEDC_INTR_DISABLE,
		.gpio_num			= gpio,
		.duty				= 0, // Set duty to 0%
		.hpoint				= 0
	};
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void ledc_duty(int channel, int duty)
{
	ESP_LOGD(__FUNCTION__, "channel=%d", channel);
	ESP_LOGD(__FUNCTION__, "duty=%d", duty);
	double maxduty = pow(2, 13) - 1;
	float percent = duty / 100.0;
	ESP_LOGD(__FUNCTION__, "percent=%f", percent);
	uint32_t _duty = maxduty * percent;
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, _duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
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

	int base_topic_len = strlen(CONFIG_MQTT_SUB_TOPIC)-1;
	ESP_LOGI(TAG, "base_topic_len=%d", base_topic_len);
	char base_topic[64];
	strcpy(base_topic, CONFIG_MQTT_SUB_TOPIC);
	base_topic[base_topic_len] = 0;
	ESP_LOGI(TAG, "base_topic=[%s]", base_topic);

	GPIO_t gpios[10];
	int16_t	ngpios=0;

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
			// get bottom topic
			// /esp32/led/12 --> 12
			char bottom_topic[64];
			strcpy(bottom_topic, &mqttBuf.topic[base_topic_len]);
			ESP_LOGI(TAG, "bottom_topic=[%s]", bottom_topic);
			if (isNumber(bottom_topic) == false) {
				ESP_LOGE(TAG, "Bad gpio [%s]", bottom_topic);
			} else {
				int gpio = atoi(bottom_topic);
				int value = atoi(mqttBuf.data);
				ESP_LOGI(TAG, "gpio=%d value=%d", gpio, value);
				int channel = -1;
				for(int i=0;i<ngpios;i++) {
					if (gpio == gpios[i].gpio) channel = i;
				}
				ESP_LOGI(TAG, "channel=%d", channel);
				// Set the LEDC peripheral configuration
				if (channel < 0) {
					if (ngpios == 7) {
						ESP_LOGE(TAG, "The maximum number of LEDC channels is 8 from 0 to 7");
					} else {
						channel = ngpios;
						ESP_LOGI(TAG, "ledc_init gpio=%d channel=%d", gpio, channel);
						gpios[ngpios].gpio = gpio;
						ledc_init(gpios[ngpios].gpio, channel);
						ngpios++;
					}
				}
				// Set duty
				if (channel >= 0) {
					ESP_LOGI(TAG, "ledc_duty channel=%d value=%d", channel, value);
					ledc_duty(channel, value);
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
