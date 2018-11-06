Air firmware

===========
== FILES ==
===========
user/user_html.c: 	Set of functions to extract usefull data from a page request (GET/POST, page name, query param) 
					Set of functions to return a page after a request

user/user_led.c:	Driver for red, green et blue leds

user/user_main.c: 	Entry point. Set esp8266 in Station and softAP mode.
					Start main state machine
					Start TCP Server
					Start a task to capture data from SDS011
					Start a task to scan wifi networks
					
user/user_mqtt.c:	To send mqtt messages to our server
					
user/user_pages.c:	Each function handles a web page for a given protocol (GET, POST or both)

user/user_queue.c:	To create queues which are used for inter tasks communication

user/user_spiffs.c: Spiffs init
										
user/user_sds011.c:	Driver for SDS011 (PM2.5 & PM10 sensor)
					Use GPIO4 (RX) and GPIO5 (TX) to connect to SDS011 uart port
					
user/user_tcp_client.c:	TCP client, used to send device registration to our server

user/user_tcp_server.c:	TCP server + table to map functions and web page

user/user_test.c:	Unit tests

user/user_wifi.c:	Task for wifi scan and setuping wifi

library/utf8.c:		To convert UTF8 char to ASCII char (used when converting password with special characters)

=================
== COMPILATION ==
=================
= Env variables =
export BIN_PATH=/path/to/binary/output 	(already set in Eclipse project configuration)
export SDK_PATH=/path/to/esp8266/sdk	(already set in .bashrc for me) (tag v2.x.x)

= Compiler =
You need xtensa-lx106-elf cross compilation tools (version xtensa-lx106-elf-gcc (crosstool-NG 1.20.0) 4.8.2) (available on espressif web site)
Or:
Get it from esp-open-sdk:  https://github.com/pfalcon/esp-open-sdk 
Clone in recursive mode. Install the required packages. Compile not in standalone mode.

= To compile =
make BOOT=none APP=0 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=0

==============
== FLASHING ==
==============
To be called from ${BIN_PATH}
esptool.py --port /dev/ttyUSB0 -b 460800 write_flash 0x00000 eagle.flash.bin 0x20000 eagle.irom0text.bin 0x3fc000 /home/gautier/git/ESP8266_RTOS_SDK/bin/esp_init_data_default.bin 0x3fe000 /home/gautier/git/ESP8266_RTOS_SDK/bin/blank.bin

=============
==  DEBUG  ==
=============
To be called from ${BIN_PATH}
gdbcmds should be copied from esp-gdbstub to ${BIN_PATH}
Uncomment call to gdbstub_init() in user_main.c before launching it.
~/git/esp-open-sdk/xtensa-lx106-elf/bin/xtensa-lx106-elf-gdb -x gdbcmds -tui -b 115200 ~/workspace/air/.output/eagle/DEBUG/image/eagle.app.v6.out
Or launch it from Eclipse (do not forget to put a breakpoint before launching it)


==================
== ARCHITECTURE ==
==================
See UML file air in plan folder

## HTML ##
The pages are stored in a spiffs image.
The spiffs image is flashed to the board at address 0x300000 with command esptool.py --port /dev/ttyUSB0 -b 460800 write_flash 0x300000 spiffs-image.bin
The pages are stored in the folder spiffs/data. Spiffs image is created with command ~/git/mkspiffs/mkspiffs -p 256 -b 8192 -s 0xfa000 -c data/ spiffs-image.bin 
The http header is added by a call to html_add_header()


## Wifi scan ##
A task (task_wifi_scan())is dedicated for scanning wifi networks. A scan is done by calling wifi_station_scan().
When scan is done, scan_done() is called.
Data are then read and for each network a message is queued.

## Main ##
See UML file air in plan folder																														Goto (2)				Goto (3)

===============
== WEB PAGES ==
===============
/wifi.html: Page used to display all wifi network and let user select the one he wants to connect to
/connect.html: To retrieve the ssid name and password of the wifi ssid the user wants to connect to
													
===========
== TO DO ==
===========
See github issues
