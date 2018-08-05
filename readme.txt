Air firmware

===========
== FILES ==
===========
user/user_html.c: 	Generate the web page to display sensors data
					display_xxx_page() is called by tcp server in order to display a page

user/user_led.c:	Driver for red, green et blue leds

user/user_main.c: 	Entry point. Set esp8266 in Station and softAP mode.
					Start main state machine
					Start TCP Server
					Start a task to capture data from SDS011
					Start a task to scan wifi networks

user/user_queue.c:	To create queues which are used for inter tasks communication
										
user/user_sds011.c:	Driver for SDS011 (PM2.5 & PM10 sensor)
					Use GPIO4 (RX) and GPIO5 (TX) to connect to SDS011 uart port
					
user/user_tcp_server.c:	TCP server

user/user_wifi.c:	Task for wifi scan and setuping wifi

library/utf8.c:		To convert UTF8 char to ASCII char (used when converting password with special characters)

=================
== COMPILATION ==
=================
export BIN_PATH=/path/to/binary/output
export SDK_PATH=/path/to/esp8266/sdk	(already set in .bashrc for me)
You need xtensa-lx106-elf cross compilation tools (version xtensa-lx106-elf-gcc (crosstool-NG 1.20.0) 4.8.2) (available on espressif web site)

To compile:
make BOOT=none APP=0 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=0

==============
== FLASHING ==
==============
esptool.py --port /dev/ttyUSB0 -b 460800 write_flash 0x00000 eagle.flash.bin 0x20000 eagle.irom0text.bin 0x3fc000 /home/gautier/git/ESP8266_RTOS_SDK/bin/esp_init_data_default.bin 0x3fe000 /home/gautier/git/ESP8266_RTOS_SDK/bin/blank.bin

=============
==  DEBUG  ==
=============
To be called from ${SDK_PATH}
~/git/esp-open-sdk/xtensa-lx106-elf/bin/xtensa-lx106-elf-gdb -x gdbcmds -tui -b 115200 ~/workspace/air/.output/eagle/debug/image/eagle.app.v6.out

==================
== ARCHITECTURE ==
==================
## HTML ##
The page is splited in several parts:
 - http header
 - html header
 - body header
 - body content
 - body footer

## Wifi scan ##
A task (task_wifi_scan())is dedicated for scanning wifi networks. A scan is done by calling wifi_station_scan().
When scan is done, scan_done() is called.
Data are then read and for each network a message is queued.

## Main ##
= Power on
= Configure as station + AP
= Do we have connection information?
				YES												NO (1)														NONE
Connect to internet	+ led blue blinking				Scan wifi nearby + led BLUE blinking								Scan wifi nearby + led BLUE blinking
Can connect?										 						
	YES					NO							Start TCP server + led BLUE blinking								Start TCP server + led BLUE blinking
Start TCP server	Goto to (1)						Wait for user to connect to AP										Wait for user to connect to AP
Display data + led									(3) Display all wifi user can connect to or none if none			Wifi available ?
													User select a network or none										NO						YES
																														Goto (2)				Goto (3)

===============
== WEB PAGES ==
===============
/index.html or /: main page 

/wifi.html: Page used to connect to a wifi station
	
													
===========
== TO DO ==
===========

2- Display measurements
3- Send measurements to server