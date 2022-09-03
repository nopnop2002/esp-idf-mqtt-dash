# GPIO Control   
GPIO Control using MQTT Dash.   

# Download Android App   
You need MQTT Dash (IoT, Smart Home).   
You can download from [Google Play](https://play.google.com/store/apps/details?id=net.routix.mqttdash&gl=US).   

# Setup Dash board   

## Setup Broker
I use broker.emqx.io as broker.   
This is a Public Broker that anyone can use.   

![broker](https://user-images.githubusercontent.com/6020549/187803785-c902efc2-b3ee-4db7-bcdc-0c46e144baf0.jpg)


## Add Initialize button   
- Name:initialize
- Topic(sub):(empty)
- Topic(pub):/esp32/gpio/init
- Payload(on):12/13=1/14/15=1
- Payload(off):12/13=1/14/15=1
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

This shows that this app uses GPIO12/13/14/15, and GPIO13/15 sets the initial value to 1.   

![initialize](https://user-images.githubusercontent.com/6020549/187803828-61d3cfb3-8bb5-4339-a5e9-4fb2d1a9b09a.jpg)


## Add Set All button   
- Name:set all
- Topic(sub):/esp32/gpio/set
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

This shows that this app turns on/off GPIO12/13/14/15 at the same time.   

![set-all](https://user-images.githubusercontent.com/6020549/187803860-95e4113d-9449-4ee0-b844-f6db38f09216.jpg)


## Add GPIO12 button   
- Name:gpio12
- Topic(sub):/esp32/gpio/12
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

This shows that this app turns on/off GPIO12.   

![gpio12](https://user-images.githubusercontent.com/6020549/187803891-1d933778-78a9-40a7-8f52-55b714928843.jpg)


## Add GPIO13 button   
- Name:gpio13
- Topic(sub):/esp32/gpio/13
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app turns on/off GPIO13.   

## Add GPIO14 button   
- Name:gpio14
- Topic(sub):/esp32/gpio/14
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app turns on/off GPIO14.   

## Add GPIO15 button   
- Name:gpio15
- Topic(sub):/esp32/gpio/15
- Topic(pub):(empty)
- Payload(on):1
- Payload(off):0
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

You can clone from GPIO12 button.   
This shows that this app turns on/off GPIO15.   


## Final Design   
Your Dash board should look like this.   

![dash-board](https://user-images.githubusercontent.com/6020549/187803926-3b8fe872-5be2-44a1-85d4-e56b8d553d3f.jpg)


## Test Dash Board   
Test your Dash board with mosquitto_sub.   

![dash-board-test](https://user-images.githubusercontent.com/6020549/187803951-91084534-5621-4952-b2e4-11f3e8cd2b1a.jpg)


# Build ESP32 firmware
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-dash
cd esp-idf-mqtt-dash/gpio-control
idf.py set-target {esp32/esp32s2/esp32s3/esp32c3}
idf.py menuconfig
idf.py flash
```

![config-main](https://user-images.githubusercontent.com/6020549/187825850-77d6baf8-2348-4c9a-877b-e4f7cdd35643.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/187825856-b3ab44ff-2dd1-4e9a-afd0-ceca6641549d.jpg)

- WiFi Setting   

![config-wifi](https://user-images.githubusercontent.com/6020549/187825865-573ef57b-4486-4917-8775-72a55713fb38.jpg)

- Broker Setting   

![config-broker](https://user-images.githubusercontent.com/6020549/187825871-486fc80c-ed51-4402-8684-f5b812d37b9a.jpg)


# Using Dash board   
At first, press the Initialize button to tell the ESP32 which GPIO to use.   
GPIO12/14 is turned off.   
GPIO13/15 is turned on.   


# Change topic after confirming work fine   
broker.emqx.io is Public broker.   
So, many use this broker.   
This example uses /esp32/gpio/# as the topic.   
This topic may also be used by others.   
If you continue to use this broker, you have to change the topic to a unique topic.   
Example:   
- /esp32/gpio/# --> /your_name/your_address/gpio/#
- /esp32/gpio/init --> /your_name/your_address/gpio/init
- /esp32/gpio/set --> /your_name/your_address/gpio/set
- /esp32/gpio/12 --> /your_name/your_address/gpio/12

![change_topic](https://user-images.githubusercontent.com/6020549/187810285-767f7e6e-d00b-441b-b2b8-5ef352a51ce8.jpg)

As an alternative, you can use a local broker.   
However, if you use a local broker, you won't be able to connect from outside your home.    


# Control AC/DC
Such products allow you to control AC/DC.   
This product uses ESP32-C3-12F.   

![ESP32-AC-DC-powere](https://user-images.githubusercontent.com/6020549/188252502-a9bc238b-2256-4a4a-b96d-d0387f9cc19f.jpg)
