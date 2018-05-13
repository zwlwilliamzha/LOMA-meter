#ifndef __ESP_PLATFORM_H__
#define __ESP_PLATFORM_H__


#define METER_SN_SIZE   32
#define MAILBOX_NAME_SIZE    32


#define CLIENT_MAIL_INTERVAL_TYPE_HOUR  10
#define CLIENT_MAIL_INTERVAL_TYPE_DAY   11
#define CLIENT_MAIL_INTERVAL_TYPE_MONTH 12
#define CLIENT_MAIL_INTERVAL_TYPE_MINUTE 13

	
#define KEY_CONNECTION_INFO "connectionInfo" // Key used in NVS for connection info
#define BOOTWIFI_NAMESPACE "bootwifi" // Namespace in NVS for bootwifi
#define SSID_SIZE (32) // Maximum SSID size
#define PASSWORD_SIZE (64) // Maximum password size

typedef struct {
	char ssid[SSID_SIZE];
	char password[PASSWORD_SIZE];
	tcpip_adapter_ip_info_t ipInfo; // Optional static IP information
} connection_info_t;



typedef struct {
	char meterSN[METER_SN_SIZE];
	//server mail config
	uint8_t server_mail_enable;
	uint8_t server_mail_time_interval;
	char server_mail_rcptmailbox[MAILBOX_NAME_SIZE];
	//client mail config
	uint8_t client_mail_enable;
	uint8_t client_mail_time_interval;
	char client_mail_rcptmailbox[MAILBOX_NAME_SIZE];


}smartmeter_config_info_t;


//smartmeter config paramter
extern smartmeter_config_info_t  meter_config_info;


void  nvs_save_WifiConnectionInfo(  connection_info_t *pConnectionInfo);
int nvs_get_WifiConnectionInfo( connection_info_t *pConnectionInfo );




#endif