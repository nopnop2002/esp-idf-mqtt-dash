#define	MQTT_CONNECT 100
#define	MQTT_DISCONNECT 200
#define	MQTT_PUB 300
#define	MQTT_SUB 400
#define	MQTT_ERROR 900

typedef struct {
    int topic_type;
    int topic_len;
    char topic[64];
    int data_len;
    char data[64];
} MQTT_t;

