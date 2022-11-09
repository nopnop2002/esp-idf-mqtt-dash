# IR Control   
IR Control using MQTT Dash.   

# Background
I have an DVD player.   
It can be operated with an infrared remote control.   
In addition to the infrared remote control, I wanted to operate it from Android as well.   


# Hardware requirements

- IR Receiver   
 Used for parsing infrared codes.   

- IR Transmitter   
 M5StickC+ and M5Atom(Lite) have built-in infrared transmitter.   
 M5StickC+ : GPIO9   
 M5Atom(Lite) : GPIO12   
 Of course, you can also use other than that.   
 Circuit diagram when using 5V driven IR Transmitter:
 ```
 ESP32 5V  ---------------------+
                                |
                               R1
                                |
                               === IR Transmitter
                                |
                               /
                              /
                             /
 ESP32 GPIO ------ R2 ------|   S8050
                             \
                              \
                               \
                               |
                               |
                               |
 ESP32 GND --------------------+

R1 follows your infrared transmitter specifications. If you don't know, 220 ohms is fine
R2 follows your transistor specifications. If you don't know, 2.2K ohms is fine.
```

# How to get IR code
You can get IR code using [this](https://github.com/nopnop2002/esp-idf-irSend/tree/master/esp-idf-irAnalysis).   
__Note:__   
There are several formats for infrared, but only the NEC format is supported.   

```
git clone https://github.com/nopnop2002/esp-idf-irSend
cd esp-idf-irSend/esp-idf-irAnalysis/
idf.py set-target esp32
idf.py menuconfig
idf.py flash monitor
```

Connect the IR receiver to GPIO19.   
I used the TL1838, but you can use anything as long as it is powered by 3.3V.   
IR transmitter don't use.   
When changing GPIO19, you can change it to any GPIO using menuconfig.   

```
 IR Receiver (TL1838)               ESP Board
+--------------------+       +-------------------+
|                  RX+-------+GPIO19             |
|                    |       |                   |
|                 3V3+-------+3V3                |
|                    |       |                   |
|                 GND+-------+GND                |
+--------------------+       +-------------------+
```

Build the project and flash it to the board, then run monitor tool to view serial output.   
When you press a button of the remote control, you will find there output:   
```
I (86070) main: Scan Code  --- addr: 0xff00 cmd: 0xe718
I (86120) main: Scan Code (repeat) --- addr: 0xff00 cmd: 0xe718
```
addr and cmd is displayed as below:   
addr: 0xff00 --> {0xff-addr} << 8 + addr   
cmd: 0xe718 --> {0xff-cmd} << 8 + cmd   

This shows addr as 0x00 and cmd as 0x18.

This is my mp3 player's IR code.
||addr|cmd|
|:-:|:-:|:-:|
|power|0x00|0x45|
|home|0x00|0x47|
|play|0x00|0x18|
|stop|0x00|0x1c|
|prev|0x00|0x08|
|next|0x00|0x5a|
|plus|0x00|0x4a|
|minus|0x00|0x42|

# Download Android App   
You need MQTT Dash (IoT, Smart Home).   
You can download from [Google Play](https://play.google.com/store/apps/details?id=net.routix.mqttdash&gl=US).   

# Setup Dash board   

## Setup Broker
I use broker.emqx.io as broker.   
This is a Public Broker that anyone can use.   

![broker](https://user-images.githubusercontent.com/6020549/188301371-a28bae76-7422-49ce-98bb-3ef4ac151246.jpg)


## Add power button   
- Name:power
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/power
- Payload(on):00/45
- Payload(off):00/45
- Icon Image:Choose your favorite one
- Icon Color:Choose your favorite one

This says to fire infrared with ir addr at 0x00 and ir cmd at 0x45.   

![power](https://user-images.githubusercontent.com/6020549/188301398-cbc95476-a779-46d2-8d01-daab443364a0.jpg)


## Add home button   
- Name:home
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/home
- Payload(on):00/47
- Payload(off):00/47

You can clone from power button.   


## Add volume up button   
- Name:volume up
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/vol-up
- Payload(on):00/4a
- Payload(off):00/4a

You can clone from power button.   

## Add volume down button   
- Name:volume down
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/vol-down
- Payload(on):00/42
- Payload(off):00/42

You can clone from power button.   

## Add play button   
- Name:prev
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/play
- Payload(on):00/18
- Payload(off):00/18

You can clone from power button.   

## Add stop button   
- Name:prev
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/stop
- Payload(on):00/1c
- Payload(off):00/1c

You can clone from power button.   

## Add prev button   
- Name:prev
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/prev
- Payload(on):00/08
- Payload(off):00/08

You can clone from power button.   

## Add next button   
- Name:prev
- Topic(sub):(empty)
- Topic(pub):/esp32/remote/next
- Payload(on):00/5a
- Payload(off):00/5a

You can clone from power button.   

## Final Design   
Your Dash board should look like this.   

![dash-board](https://user-images.githubusercontent.com/6020549/188301626-243ecf9d-23b8-462a-95ae-ec3e50309dbc.jpg)


## Test Dash Board   
Test your Dash board with mosquitto_sub.   

![dash-board-test](https://user-images.githubusercontent.com/6020549/188301633-756732d4-44f9-4a84-bfd4-674891ec4583.jpg)


# Build ESP32 firmware
```
git clone https://github.com/nopnop2002/esp-idf-mqtt-dash
cd esp-idf-mqtt-dash/ir-control
idf.py set-target {esp32/esp32s2/esp32s3/esp32c3}
idf.py menuconfig
idf.py flash
```

![config-main](https://user-images.githubusercontent.com/6020549/188301665-95b883c8-cb30-41e1-a6a1-1b8673e0c7b4.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/188301770-58d8a735-a6b7-4775-9b8c-20dab8010561.jpg)

- WiFi Setting   

![config-wifi](https://user-images.githubusercontent.com/6020549/188301680-5108856e-c6d8-42ff-afbd-840caa3ed544.jpg)

- Broker Setting   

![config-broker](https://user-images.githubusercontent.com/6020549/188301695-26c42b4e-977e-47f9-aee9-13a02a71fd95.jpg)


- IR Transmitter Setting   

![config-transmitter](https://user-images.githubusercontent.com/6020549/188301703-ce0c2d42-7a05-4612-a5d3-a6cb14758d5b.jpg)

# Using Dash board   
Press button what you want.


# Change topic after confirming work fine   
broker.emqx.io is Public broker.   
So, many use this broker.   
This example uses /esp32/remote/# as the topic.   
This topic may also be used by others.   
If you continue to use this broker, you have to change the topic to a unique topic.   
Example:   
- /esp32/remote/# --> /your_name/your_address/remote/#
- /esp32/remote/play --> /your_name/your_address/remote/play

![config-topic](https://user-images.githubusercontent.com/6020549/188301829-82d93011-6599-4309-9041-081840d5a3a6.jpg)

As an alternative, you can use a local broker.   
However, if you use a local broker, you won't be able to connect from outside your home.    


