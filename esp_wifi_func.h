#ifndef __ESP_WIFI_FUNC_H__
#define __ESP_WIFI_FUNC_H__

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <cJSON.h>

#include "esp_platform.h"

//Wifi network status for mailbox
#define WIFI_NETWORK_STATUS_UNKNOWN   0
#define WIFI_NETWORK_STATUS_DOWN      1
#define WIFI_NETWORK_STATUS_UP        2


typedef struct WifiEventStructure{
	TickType_t xTimeStamp;
	uint32_t ulValue;
	uint8_t reason;   //disconnected use it
}WifiEvent_t;



esp_err_t esp_platform_get_clientmail_rcptmailbox( char * pstr_rcptmailbox );
esp_err_t esp_platform_get_clientmail_enable( uint8_t * enable );
esp_err_t esp_platform_get_clientmail_interval( uint8_t * interval );
esp_err_t esp_platform_set_clientmail_rcptmailbox( char * pstrmailbox );


esp_err_t  esp_wifi_parse_req_setwifi(const char * pValue, char * pApssid, char * pPwd );
esp_err_t  esp_wifi_parse_req_setmail( const char * pValue, smartmeter_config_info_t * pConfigPara );
esp_err_t esp_wifi_handle_http_req_getmail( char * prespbuf );
esp_err_t esp_wifi_handle_http_req_getsnapshotmetrics( char * prespbuf );
esp_err_t  esp_wifi_parse_req_sendmailnow( const char * pValue, char * pMailbox );
esp_err_t esp_wifi_handle_http_req_getwifi( cJSON *pcjson, uint8_t wifi_networkstatus_event, uint8_t reason );
esp_err_t esp_wifi_handle_http_req_getpartitioninfo( cJSON *pcjson, const char *pRunningbuf, const char *pConfiguredbuf );

esp_err_t esp_wifi_get_json_softap( cJSON *pcjson,uint8_t wifi_networkstatus_event );
esp_err_t esp_wifi_get_json_station( cJSON *pcjson,uint8_t wifi_networkstatus_event  );











#endif
