# LED Control   
PWM Control using MQTT Dash.   

# Download Android App   
You need MQTT Dash (IoT, Smart Home).   
You can download from [Google Play](https://play.google.com/store/apps/details?id=net.routix.mqttdash&gl=US).   

# Setup Dash board   

## Setup Broker
I use broker.emqx.io as broker.   
This is a Public Broker that anyone can use.   

![broker](https://user-images.githubusercontent.com/6020549/187845958-3291dd75-006e-4d28-b31e-a9eae6b4f6c9.jpg)



## Add GPIO12 button   
- Name:gpio12
- Topic(sub):/esp32/led/12
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

This shows that this app control GPIO12.   

![gpio12](https://user-images.githubusercontent.com/6020549/187846042-d8c1cd4c-b7d0-445d-9a63-928de1aaf0a3.jpg)


## Add GPIO13 button   
- Name:gpio13
- Topic(sub):/esp32/led/13
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app control GPIO13.   

## Add GPIO14 button   
- Name:gpio14
- Topic(sub):/esp32/led/14
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app control GPIO14.   

## Add GPIO15 button   
- Name:gpio15
- Topic(sub):/esp32/led/15
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app control GPIO15.   


## Final Design   
Your Dash board should look like this.   

![dash-board](https://user-images.githubusercontent.com/6020549/187845864-16537ad9-669a-4fa0-85b1-6133b8dc09ae.jpg)


## Test Dash Board   
Test your Dash board with mosquitto_sub.   

![dash-board-test](https://user-images.githubusercontent.com/6020549/187846233-8f7f7433-84f3-46bc-861d-db00a1db904a.jpg)


# Build ESP32 firmware
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-dash
cd esp-idf-mqtt-dash/led-control
idf.py set-target {esp32/esp32s2/esp32s3/esp32c3}
idf.py menuconfig
idf.py flash
```

![config-main](https://user-images.githubusercontent.com/6020549/187846304-0a2ee09a-d5dd-4086-b4e5-e22a5d747174.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/187846313-4eb9f68d-00a3-4e8b-b5fb-1bb8e9460491.jpg)

- WiFi Setting   

![config-wifi](https://user-images.githubusercontent.com/6020549/187846367-0d1c3c9f-8e81-42bd-8c23-c5730e61ee13.jpg)

- Broker Setting   

![config-broker](https://user-images.githubusercontent.com/6020549/187846400-67ccfa50-8ea2-4166-9834-870c903a27c0.jpg)


# Using Dash board   
Press GPIO12 and Set value.   
The brightness of the LED connected to GPIO12 changes.   

# Change topic after confirming work fine   
broker.emqx.io is Public broker.   
So, many use this broker.   
This example uses /esp32/led/# as the topic.   
This topic may also be used by others.   
If you continue to use this broker, you have to change the topic to a unique topic.   
Example:   
- /esp32/led/12 --> /your_name/your_address/led/12

![config-topic](https://user-images.githubusercontent.com/6020549/187846732-8f621ae5-bced-4f29-ac7a-9050cb353bf0.jpg)

As an alternative, you can use a local broker.   
However, if you use a local broker, you won't be able to connect from outside your home.    

