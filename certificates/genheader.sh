#!/bin/bash

# set ca crt for use in the client
xxd -i penwv1.messaging.internetofthings.ibmcloud.com.pem | sed -e "s/penwv1_messaging_internetofthings_ibmcloud_com_pem/ca_crt/" > ../include/ssl_server_crt.h
