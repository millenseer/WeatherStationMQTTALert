# WeatherStationMQTTALert

Based upon
* OLED and fancy UI: https://github.com/ThingPulse/esp8266-weather-station/tree/master/examples/WeatherStationDemoExtended
* MQTT: https://github.com/knolleary/pubsubclient/tree/master/examples/mqtt_esp8266
* WiFiManager: https://github.com/tzapu/WiFiManager/


## Purpose: 
* Show weather stuff
* connect to MQTT and listen for incoming topics
** display alerts on OLED display
** visible and audible (todo!) alerts
* come back to weather stuff after alerts

* Have local MQTT server running 
** Raspberry Pi: sudo apt-get install -y mosquitto mosquitto-clients

* Have local dashboard for MQTT messaging running
** Raspberry Pi: Apache with PHP. Source code in pager/index.php

## Setup:
* ESP-Settings
** SDA/SDC connection pins for your OLED
** WIFI is setup by OTA by Wifimanager (see: https://github.com/tzapu/WiFiManager)
*** First start will either connect to last setup or start Access Point to let you connect to this ESP and setup WIFI server. These settings will be saved.
** MQTT-Server and -port: so far hard coded setup to local MQTT server. 
** TODO: MQTT-Settings might be stored in file system config.json (See: https://github.com/tzapu/WiFiManager/tree/master/examples/AutoConnectWithFSParameters)

* MQTT-Server settings
* I installed mosquitto server and clients on my home server, Raspberry Pi

sudo apt-get install -y mosquitto mosquitto-clients

This installs and starts the MQTT Server. Can be tested using a subscriber listening to the topic "test"
mosquitto_sub -h localhost -v -t test

Using a second (!) connection to your Raspberry, send a message (publish) to the same topic:
mosquitto_pub -h localhost -t test -m "Hello world, Mosquitto"

The subscriber should see the text message now.

* Apache PHP
** Install pager/index.php anywhere on your webserver. I installed it on my Raspberry Pi in /var/www/html/pager/index.php
** Assuming the MQTT-Server is on the same server, the PHP code connects to localhost.
** Use a web browser, preferably your mobile, tablet, smartphone and start a message 
** The PHP code sends the text to the MQTT-Server
** the ESP runs a MQTT-client and listens on the topics "LIGHTS" and "TEXT"

# MQTT "protocol"
* Topic LIGHTS with payload "LIGHT-1:ON" will enable the LED
* Topic LIGHTS with payload "LIGHT-1:OFF" will disable the LED
* Topic LIGHTS with payload "LIGHT-1:BLINK" will alert (blink) the LED and the OLED will blink, too
* Topic TEXT with payload "any text" will enable the LED, print the text on OLED and blink. The text will be displayed in the 4th frame of the standard frame animation


