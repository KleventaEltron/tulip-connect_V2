#ifndef _WINC_SOFT_AP_H
#define _WINC_SOFT_AP_H


#if (defined __SAMD21J18A__)
#define WLAN_SSID               "MicrochipDemoAp"
#define WLAN_CHANNEL            1

#define WLAN_AUTH_OPEN
#define WLAN_WEB_KEY_INDEX      1
#define WLAN_WEB_KEY            "1234567890"

#define TCP_LISTEN_PORT         6666
#define TCP_BUFFER_SIZE         1460

#define WLAN_DHCP_SRV_ADDR      "192.168.1.1"
#define WLAN_DHCP_SRV_NETMASK   "255.255.255.0"
#endif
#if (defined __SAME54P20A__)

#define WLAN_SSID               "MicrochipDemoAp"
#define WLAN_CHANNEL            1

#define WLAN_AUTH_OPEN

#define TCP_LISTEN_PORT         6666
#define TCP_BUFFER_SIZE         1460

#define WLAN_DHCP_SRV_ADDR      "192.168.1.1"
#define WLAN_DHCP_SRV_NETMASK   "255.255.255.0"	
#endif

#endif /* _EXAMPLE_CONF_H */

bool getActivateWifiAp();
void setActivateWifiAp(bool value);