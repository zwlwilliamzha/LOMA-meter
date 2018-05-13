
#include <string.h>
#include <esp_system.h>
#include <esp_log.h>
#include <cJSON.h>
#include <apps/sntp/sntp.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <tcpip_adapter.h>
#include <lwip/sockets.h>
#include <esp_wifi.h>

#include "esp_wifi_func.h"
#include "esp_platform.h"
#include "esp_afe.h"
#include "esp_lcd.h"


/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_clientmail_enable( uint8_t enable )
{
	meter_config_info.client_mail_enable = enable;

	return ESP_OK;
}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_clientmail_enable( uint8_t * enable )
{
	if( enable != NULL ){
		*enable = meter_config_info.client_mail_enable;
		return ESP_OK;
	}
	else {
		return ESP_FAIL;
	}

}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_clientmail_interval( uint8_t interval )
{
	meter_config_info.client_mail_time_interval = interval;

	return ESP_OK;
}
/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_clientmail_interval( uint8_t * interval )
{
	if( interval != NULL ){
		*interval = meter_config_info.client_mail_time_interval ;
		return ESP_OK;
	}
	else{
		return ESP_FAIL;
	}

}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_clientmail_rcptmailbox( char * pstrmailbox )
{
	sprintf( meter_config_info.client_mail_rcptmailbox , "%s", pstrmailbox );

	return ESP_OK;
}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_clientmail_rcptmailbox( char * pstr_rcptmailbox )
{
	if( pstr_rcptmailbox != NULL )
	{
		memcpy( pstr_rcptmailbox, meter_config_info.client_mail_rcptmailbox, sizeof( meter_config_info.client_mail_rcptmailbox ));
		return ESP_OK;
	}
	else{
		return ESP_FAIL;
	}

}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_servermail_enable( uint8_t enable )
{
	meter_config_info.server_mail_enable = enable;

	return ESP_OK;
}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_servermail_enable( uint8_t * enable )
{
	if( enable != NULL ){
		*enable = meter_config_info.server_mail_enable;
		return ESP_OK;
	}
	else{
		return ESP_FAIL;
	}

}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_servermail_interval(uint8_t interval )
{
	meter_config_info.server_mail_time_interval = interval;
	return ESP_OK;
}
/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_servermail_interval(uint8_t * interval )
{
	if( interval != NULL ) {
		*interval = meter_config_info.server_mail_time_interval ;
		return ESP_OK;
	}
	else {
		return ESP_FAIL;
	}
}

/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_set_servermail_rcptmailbox( char * pstrmailbox )
{
	if( pstrmailbox != NULL && strlen( pstrmailbox ) < MAILBOX_NAME_SIZE )
	{
		sprintf( meter_config_info.server_mail_rcptmailbox, "%s", pstrmailbox );
		return ESP_OK;
	}
	else{
		return ESP_FAIL;
	}
}
/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_platform_get_servermail_rcptmailbox( char *pstrmailbox )
{
	if( pstrmailbox != NULL ){
		memcpy( pstrmailbox, meter_config_info.server_mail_rcptmailbox, sizeof( meter_config_info.server_mail_rcptmailbox));
		return ESP_OK;
	}
	else {
		return ESP_FAIL;
	}
}
/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
static char *esp_wifi_eventToString(uint8_t ev) 
{
	static char temp[100];
	switch (ev) {
	case WIFI_REASON_UNSPECIFIED:
		return "WIFI UNSPECIFIED";
	case WIFI_REASON_AUTH_EXPIRE:
		return "WIFI AUTH EXPIRE";
	case WIFI_REASON_AUTH_LEAVE:
		return "WIFI AUTH LEAVE";
	case WIFI_REASON_ASSOC_EXPIRE:
		return "WIFI ASSOC EXPIRE";
	case WIFI_REASON_ASSOC_TOOMANY:
		return "WIFI ASSOC TOOMANY";
	case WIFI_REASON_NOT_AUTHED:
		return "WIFI NOT AUTHED";
	case WIFI_REASON_NOT_ASSOCED:
		return "WIFI NOT ASSOCED";
	case WIFI_REASON_ASSOC_LEAVE:
		return "WIFI ASSOC LEAVE";
	case WIFI_REASON_ASSOC_NOT_AUTHED:
		return "WIFI ASSOC NOT AUTHED";
	case WIFI_REASON_DISASSOC_PWRCAP_BAD:
		return "WIFI DISASSOC PWRCAP BAD";
	case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
		return "WIFI DISASSOC SUPCHAN BAD";
	case WIFI_REASON_IE_INVALID:
		return "WIFI IE INVALID";
	case WIFI_REASON_MIC_FAILURE:
		return "WIFI MIC FAILURE";
	case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
		return "WIFI 4WAY HANDSHAKE TIMEOUT";
	case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
		return "WIFI GROUP KEY UPDATE TIMEOUT";
	case WIFI_REASON_IE_IN_4WAY_DIFFERS:
		return "WIFI IE IN 4WAY DIFFERS";
	case WIFI_REASON_GROUP_CIPHER_INVALID:
		return "WIFI GROUP CIPHER INVALID";
	case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
		return "WIFI PAIRWISE CIPHER INVALID";
	case WIFI_REASON_AKMP_INVALID:
		return "WIFI AKMP INVALID";
	case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
		return "WIFI UNSUPP RSN IE VERSION";
	case WIFI_REASON_INVALID_RSN_IE_CAP:
		return "WIFI INVALID RSN IE CAP";
	case WIFI_REASON_802_1X_AUTH_FAILED:
		return "WIFI 802 1X AUTH FAILED";
	case WIFI_REASON_CIPHER_SUITE_REJECTED:
		return "WIFI CIPHER SUITE REJECTED";
	case WIFI_REASON_BEACON_TIMEOUT:
		return "WIFI BEACON TIMEOUT";
	case WIFI_REASON_NO_AP_FOUND:
		return "WIFI NO AP FOUND";
	case WIFI_REASON_AUTH_FAIL:
		return "WIFI AUTH FAIL";
	case WIFI_REASON_ASSOC_FAIL:
		return "WIFI ASSOC FAIL";
	case WIFI_REASON_HANDSHAKE_TIMEOUT:
		return "WIFI HANDSHAKE TIMEOUT";
	}
	sprintf(temp, "Unknown event: %d", ev);
	return temp;
}
/*********************************************************************************************
 * @brief  
 *
 *********************************************************************************************/
esp_err_t esp_wifi_parse_req_setwifi(const char * pValue, char * pApssid, char * pPwd)
{
	cJSON * pJson;
    cJSON * pJsonSub;
    cJSON * pJsonSub_request;
    cJSON * pJsonSub_Connect_Station;
    cJSON * pJsonSub_Connect_Softap;
    cJSON * pJsonSub_Sub;

	if( pValue == NULL || pApssid == NULL || pPwd == NULL ){
		return ESP_FAIL;
	}

	pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("esp_wifi_parse_req_setwifi: cJSON_Parse fail.\n");
        return ESP_FAIL;
    }
    
    pJsonSub_request = cJSON_GetObjectItem(pJson, "Request");
    if(pJsonSub_request == NULL) {
        printf("esp_wifi_parse_req_setwifi: Request Item parse Json fail.\n");
        return ESP_FAIL;
    }

    if((pJsonSub = cJSON_GetObjectItem(pJsonSub_request, "Station")) != NULL){

		pJsonSub_Connect_Station= cJSON_GetObjectItem(pJsonSub,"Connect_Station");
        if(NULL == pJsonSub_Connect_Station){
            printf(" esp_wifi_parse_req_setwifi: Connect_Station Json parse fail\n");
            return ESP_FAIL;
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Station,"ssid");
        if(NULL != pJsonSub_Sub){       
            memcpy( pApssid, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
        }

        pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Connect_Station,"password");
        if(NULL != pJsonSub_Sub){
            memcpy( pPwd, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
        }

	}


	cJSON_Delete(pJson);

	return ESP_OK;
}



/*********************************************************************************************************
 * @brief  
 *
 *********************************************************************************************************/
esp_err_t  esp_wifi_parse_req_setmail( const char * pValue, smartmeter_config_info_t * pConfigPara )
{
	 //server mail config
	 //
	 uint8_t server_mail_enable_val = 0;
	 uint8_t server_mail_interval_val = 0;

	 //client mail config
	 //
	 uint8_t client_mail_enable_val = 0;
	 char client_mail_interval_type[10];
	 uint8_t client_mail_interval_val = 0;
	 char client_mail_rcptmailbox[40];
	 

	cJSON * pJson;
	cJSON * pJsonSub_Mail;
	cJSON * pJsonSub_Sub;
	cJSON * pJsonSub_Client;
	cJSON * pJsonSub_Server;

	if( pValue == NULL || pConfigPara == NULL ){
		printf(" esp_wifi_parse_req_setmail: input parameter NULL.\n");
		return ESP_FAIL;
	}


	pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("mailsetting_set cJSON_Parse fail\n");
        return ESP_FAIL;
    }

	pJsonSub_Mail = cJSON_GetObjectItem( pJson,"Mail" );
	if( pJsonSub_Mail == NULL ){
		printf(" esp_wifi_parse_req_setmail: cJSON_GetObjectItem Mail fail\n");
		return ESP_FAIL;
	}

	pJsonSub_Client = cJSON_GetObjectItem( pJsonSub_Mail,"Client" );
	if( pJsonSub_Client == NULL ){
		printf(" esp_wifi_parse_req_setmail: cJSON_GetObjectItem Client fail\n");
		return ESP_FAIL;
	}

	pJsonSub_Server = cJSON_GetObjectItem( pJsonSub_Mail,"Server" );
	if( pJsonSub_Server == NULL ){
		printf(" esp_wifi_parse_req_setmail: cJSON_GetObjectItem Server fail\n");
		return ESP_FAIL;
	}

	//client mail configuration
	//
	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Client,"enable");
	if( pJsonSub_Sub != NULL ) {
		client_mail_enable_val = atoi( pJsonSub_Sub->valuestring );
		esp_platform_set_clientmail_enable( client_mail_enable_val );
	}

	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Client,"interval");
	if( pJsonSub_Sub != NULL ) {
		memcpy(client_mail_interval_type, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
		client_mail_interval_type[strlen(pJsonSub_Sub->valuestring)] = '\0';

		if( strcmp( client_mail_interval_type, "hour" ) == 0 ) {
			esp_platform_set_clientmail_interval( CLIENT_MAIL_INTERVAL_TYPE_HOUR );
		}
		else if( strcmp( client_mail_interval_type, "day") == 0 ) {
			esp_platform_set_clientmail_interval( CLIENT_MAIL_INTERVAL_TYPE_DAY );
		}
		else if( strcmp( client_mail_interval_type, "month") == 0 ) {
			esp_platform_set_clientmail_interval(CLIENT_MAIL_INTERVAL_TYPE_MONTH);
		}
		else {
			printf("ERROR: unknown client mail cycle type : %s\n", client_mail_interval_type );
		}

		//test
		//printf(" client mail config-> interval type: %s\n", client_mail_interval_type );

	}

	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Client,"email");
	if( pJsonSub_Sub != NULL ) {
		memcpy( client_mail_rcptmailbox, pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring));
		client_mail_rcptmailbox[strlen(pJsonSub_Sub->valuestring)] = '\0';
		esp_platform_set_clientmail_rcptmailbox( client_mail_rcptmailbox );
		//test
		//printf(" client mail config -> email: %s\n", client_mail_rcptmailbox );
	}

	//Server configuration
	//
	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Server,"enable");
	if( pJsonSub_Sub != NULL ) {
		server_mail_enable_val = atoi( pJsonSub_Sub->valuestring );

		esp_platform_set_servermail_enable( server_mail_enable_val );
		//test
		//printf(" server mail config-> enable : %d\n",server_mail_enable_val );
	}

	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Server,"interval");
	if( pJsonSub_Sub != NULL ) {
		server_mail_interval_val = atoi( pJsonSub_Sub->valuestring );

		esp_platform_set_servermail_interval( server_mail_interval_val );
		//test
		//printf(" server mail config-> interval :  %d\n",server_mail_interval_val );
	}



	return ESP_OK;
}


/*********************************************************************************************************
 * @brief  
 *
 *********************************************************************************************************/
esp_err_t esp_wifi_handle_http_req_getmail( char * prespbuf )
{
	char *pchar = NULL;
	//server mail config buffer
	uint8_t server_mail_enable;
	uint8_t server_mail_time_interval;
	char buff[128];

	//client mail config buffer
	uint8_t client_mail_enable;
	uint8_t client_mail_time_interval;
	char client_mail_rcptmailbox[MAILBOX_NAME_SIZE];

	if( prespbuf == NULL ){
		return ESP_FAIL;
	}

	//get client mail config from memory
	esp_platform_get_clientmail_enable( &client_mail_enable );
	esp_platform_get_clientmail_interval( &client_mail_time_interval );
	esp_platform_get_clientmail_rcptmailbox( client_mail_rcptmailbox );

	//get server mail config from memory
	esp_platform_get_servermail_enable( &server_mail_enable );
	esp_platform_get_servermail_interval( &server_mail_time_interval );

	//root node
	cJSON *pcjson = cJSON_CreateObject();
	if( pcjson == NULL ){
		printf(" esp_wifi_handle_http_req_getmail: create json object fail.\n");
		return ESP_FAIL;
	}

	//Mail node
	cJSON * pSubJson_Mail = cJSON_CreateObject();
    if(NULL == pSubJson_Mail ){
        printf("esp_wifi_handle_http_req_getmail: create json object for mail fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Mail", pSubJson_Mail);

	//Client node
	cJSON * pSubJson_Client = cJSON_CreateObject();
	if( NULL == pSubJson_Client ) {
		printf("esp_wifi_handle_http_req_getmail: create json object for client fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
	}
	cJSON_AddItemToObject( pSubJson_Mail, "Client", pSubJson_Client );

	//Server node
	cJSON * pSubJson_Server = cJSON_CreateObject();
	if( NULL == pSubJson_Server ) {
		printf("esp_wifi_handle_http_req_getmail: create json object for server fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
	}
	cJSON_AddItemToObject( pSubJson_Mail, "Server", pSubJson_Server );

	//construct client mail children node
	sprintf( buff, "%d", client_mail_enable );
	cJSON_AddStringToObject( pSubJson_Client, "enable", buff );

	if( client_mail_time_interval == CLIENT_MAIL_INTERVAL_TYPE_HOUR ){
		sprintf( buff, "hour");
	}
	else if( client_mail_time_interval == CLIENT_MAIL_INTERVAL_TYPE_DAY ){
		sprintf( buff, "day");
	}
	else if( client_mail_time_interval == CLIENT_MAIL_INTERVAL_TYPE_MONTH ) {
		sprintf( buff, "month");
	}
	else {
		sprintf( buff, "unknown");
	}
	cJSON_AddStringToObject( pSubJson_Client, "interval", buff );

	cJSON_AddStringToObject( pSubJson_Client, "email", client_mail_rcptmailbox );

	//server mail config from memory
	sprintf( buff, "%d", server_mail_enable );
	cJSON_AddStringToObject( pSubJson_Server, "enable", buff  );

	sprintf( buff, "%d", server_mail_time_interval );
	cJSON_AddStringToObject( pSubJson_Server, "interval", buff  );


	//print json struct
	if( pchar == NULL ){
		pchar = cJSON_Print(pcjson);
		//copy into buffer
		memcpy( prespbuf, pchar, strlen( pchar ));		
		//printf(" getmail JSON string: %s\n", prespbuf );
	}

	cJSON_Delete(pcjson);
	if( pchar ){
		free(pchar);
		pchar=NULL;
	}
	return ESP_OK;
}

/*********************************************************************************************************
 * @brief  
 *
 *********************************************************************************************************/
esp_err_t esp_wifi_handle_http_req_getsnapshotmetrics(  char * prespbuf )
{
	int len;
	char *pchar = NULL;
	char measureValueStr[64];
	cJSON *pcjson;

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[32];
	esp_err_t err;

	//root node
    pcjson=cJSON_CreateObject();
    if(NULL == pcjson){
        printf("esp_wifi_handle_http_req_getsnapshotmetrics: create json object fail.\n");
        return ESP_FAIL;
    }

	//MeterSN node
	if( strlen( meterSnapshotDb.meterSN ) > 0 ){
		cJSON_AddStringToObject(pcjson, "MeterSN", meterSnapshotDb.meterSN );
	}
	else {
		cJSON_AddStringToObject(pcjson, "MeterSN", "00000000" );
	}
	
	//Time node baokai test
    if( sntp_enabled() == 1 ) {
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
			cJSON_AddStringToObject(pcjson, "Time", strftime_buf );
		}
		else{
			cJSON_AddStringToObject(pcjson, "Time", "00/00/0 00:00:00" );
		}
	}
	else{
		cJSON_AddStringToObject(pcjson, "Time", "00/00/0 00:00:00" );
	}

	//PhaseA node
	cJSON * pSubJson_phaseA = cJSON_CreateObject();
    if(NULL == pSubJson_phaseA ){
        printf("esp_wifi_handle_http_req_getsnapshotmetrics: create json object for phaseA fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "PhaseA", pSubJson_phaseA);
    /*
	//PhaseB node
	cJSON * pSubJson_phaseB = cJSON_CreateObject();
    if(NULL == pSubJson_phaseB ){
        printf("esp_wifi_handle_http_req_getsnapshotmetrics: create json object for phaseB fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "PhaseB", pSubJson_phaseB);

	//PhaseC node
	cJSON * pSubJson_phaseC = cJSON_CreateObject();
    if(NULL == pSubJson_phaseC ){
        printf("esp_wifi_handle_http_req_getsnapshotmetrics: create json object for phaseC fail.\n");
		cJSON_Delete(pcjson);
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "PhaseC", pSubJson_phaseC);
    */

	//PhaseA metric node
	sprintf(measureValueStr, "%s", "HH"  );
	cJSON_AddStringToObject(pSubJson_phaseA, "Status", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Vr.measurementValue  );
	cJSON_AddStringToObject(pSubJson_phaseA, "Voltage", measureValueStr);
    //lcd_test(meterSnapshotDb.phaseA.Vr.measurementValue);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Ir.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "Current", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Pf.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "PowerFactor", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Tc.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "Temperature", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Ae.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ActiveEnergyF", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseAe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ActiveEnergyR", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Ap.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ActivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Ad.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseA, "ActiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Re.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ReactiveEnergyC", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseRe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ReactiveEnergyI", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Rp.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "ReactivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Rd.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseA, "ReactiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseA.Hz.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseA, "Frequency", measureValueStr);
    /*
	//PhaseB metric node
	sprintf(measureValueStr, "%s", "HH"  );
	cJSON_AddStringToObject(pSubJson_phaseB, "Status", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Vr.measurementValue  );
	cJSON_AddStringToObject(pSubJson_phaseB, "Voltage", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Ir.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "Current", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Pf.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "PowerFactor", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Tc.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "Temperature", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Ae.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ActiveEnergyF", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseAe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ActiveEnergyR", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Ap.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ActivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Ad.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseB, "ActiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Re.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ReactiveEnergyC", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseRe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ReactiveEnergyI", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Rp.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "ReactivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Rd.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseB, "ReactiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseB.Hz.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseB, "Frequency", measureValueStr);


	//PhaseC metric node
	sprintf(measureValueStr, "%s", "HH"  );
	cJSON_AddStringToObject(pSubJson_phaseC, "Status", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Vr.measurementValue  );
	cJSON_AddStringToObject(pSubJson_phaseC, "Voltage", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Ir.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "Current", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Pf.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "PowerFactor", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Tc.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "Temperature", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Ae.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ActiveEnergyF", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseAe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ActiveEnergyR", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Ap.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ActivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Ad.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseC, "ActiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Re.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ReactiveEnergyC", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseRe.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ReactiveEnergyI", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Rp.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "ReactivePower", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Rd.measurementValue*1000.0 );
	cJSON_AddStringToObject(pSubJson_phaseC, "ReactiveDemand", measureValueStr);

	sprintf(measureValueStr, "%f", meterSnapshotDb.phaseC.Hz.measurementValue );
	cJSON_AddStringToObject(pSubJson_phaseC, "Frequency", measureValueStr);
    */
	//print json struct
	if( pchar == NULL ){
		pchar = cJSON_Print(pcjson);
		//copy into buffer
		memcpy( prespbuf, pchar, strlen( pchar ));		
		printf(" getmail JSON string: %s, len: %d\n", prespbuf, strlen(prespbuf) );
	}

	cJSON_Delete(pcjson);
	if( pchar ){
		free(pchar);
		pchar=NULL;
	}


	return ESP_OK;
}


/*********************************************************************************************************
 * @brief  
 * {\"Mail\": {\"email\":\"634276301@qq.com\"}}
 *********************************************************************************************************/
esp_err_t  esp_wifi_parse_req_sendmailnow( const char * pValue, char * pMailbox )
{
	cJSON * pJson;
	cJSON * pJsonSub_Mail;
	cJSON * pJsonSub_Sub;
	char mailbuf[128];

	memset( mailbuf, 0,sizeof(mailbuf));

	pJson = cJSON_Parse(pValue);
    if(NULL == pJson){
        printf("esp_wifi_parse_req_sendmailnow: parse root json object fail.\n");
        return ESP_FAIL;
    }

	pJsonSub_Mail = cJSON_GetObjectItem( pJson,"Mail" );
	if( pJsonSub_Mail == NULL ){
		printf(" esp_wifi_parse_req_sendmailnow: get Mail node fail.\n");
		return ESP_FAIL;
	}

	pJsonSub_Sub = cJSON_GetObjectItem(pJsonSub_Mail,"email");
	if( pJsonSub_Sub != NULL ) {
		memcpy( mailbuf , pJsonSub_Sub->valuestring, strlen(pJsonSub_Sub->valuestring) );
		mailbuf[ strlen(pJsonSub_Sub->valuestring)] = '\0';

		//printf(" sendmailnow mailbox is : %s\n", mailbuf );
		sprintf( pMailbox, "%s", mailbuf );
	}
	else {
		printf(" esp_wifi_parse_req_sendmailnow: get email node fail.\n");
		return ESP_FAIL;
	}


	cJSON_Delete(pJson);

	return ESP_OK;
}

/*********************************************************************************************************
 * @brief  
 * 
 *********************************************************************************************************/
esp_err_t esp_wifi_get_json_softap( cJSON *pcjson,uint8_t wifi_networkstatus_event  )
{
	char buff[64];
    bzero(buff, sizeof(buff));

	wifi_config_t ap_conf;
	memset((void*)&ap_conf, 0 , sizeof(wifi_config_t) );
    
    cJSON * pSubJson_Connect_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Connect_Softap){
        printf("esp_wifi_get_json_softap: Connect_Softap json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Connect_Softap", pSubJson_Connect_Softap);

    cJSON * pSubJson_Ipinfo_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Ipinfo_Softap){
        printf("esp_wifi_get_json_softap: Ipinfo_Softap json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Ipinfo_Softap", pSubJson_Ipinfo_Softap);
	
	sprintf(buff, "%s","192.168.4.1");
	cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"ip",buff);
	sprintf(buff, "%s","255.255.255.0");
	cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"mask",buff);
	sprintf(buff, "%s", "192.168.4.1");
	cJSON_AddStringToObject(pSubJson_Ipinfo_Softap,"gw",buff);

	//get ap config 
	if( esp_wifi_get_config( WIFI_IF_AP, &ap_conf ) == ESP_OK ) {
		wifi_ap_config_t * pApconf = &ap_conf.ap;
		sprintf(buff, "%s",pApconf->ssid );
		cJSON_AddStringToObject(pSubJson_Connect_Softap, "ssid", buff);
		sprintf(buff, "%s", pApconf->password );
		cJSON_AddStringToObject(pSubJson_Connect_Softap, "password", buff);
		cJSON_AddNumberToObject(pSubJson_Connect_Softap, "channel", pApconf->channel);

		switch (pApconf->authmode) {
			case WIFI_AUTH_OPEN:
				cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "OPEN");
				break;
			case WIFI_AUTH_WEP:
				cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WEP");
				break;
			case WIFI_AUTH_WPA_PSK:
				cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPAPSK");
				break;
			case WIFI_AUTH_WPA2_PSK:
				cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPA2PSK");
				break;
			case WIFI_AUTH_WPA_WPA2_PSK:
				cJSON_AddStringToObject(pSubJson_Connect_Softap, "authmode", "WPAPSK/WPA2PSK");
				break;
			default :
				cJSON_AddNumberToObject(pSubJson_Connect_Softap, "authmode",  pApconf->authmode);
				break;
		}
	}
	else {
		//if get ap config fail
		//
		printf("esp_wifi_get_json_softap : esp_wifi_get_config for AP fail.\n");
		return ESP_FAIL;
	}

	return ESP_OK;
}
/*********************************************************************************************************
 * @brief  
 * 
 *********************************************************************************************************/
esp_err_t esp_wifi_get_json_station( cJSON *pcjson,uint8_t wifi_networkstatus_event )
{
	char buff[64];
    bzero(buff, sizeof(buff));

	tcpip_adapter_ip_info_t ip_info; 
	wifi_ap_record_t ap_conf;
	wifi_config_t sta_conf;

    cJSON * pSubJson_Connect_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Connect_Station){
        printf("esp_wifi_get_json_station:  Connect_Station json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Connect_Station", pSubJson_Connect_Station);

    cJSON * pSubJson_Ipinfo_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Ipinfo_Station){
        printf("esp_wifi_get_json_station: IpInfo_Station json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Ipinfo_Station", pSubJson_Ipinfo_Station);

	//if networkstatus is up 
	//
	if( wifi_networkstatus_event == WIFI_NETWORK_STATUS_UP ){
		//if networkstatus is up, we can get ipinfo
		//
		ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info)); 
 		sprintf(buff,"%s", ip4addr_ntoa(&ip_info.ip)); 
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"ip",buff);

 		sprintf(buff, "%s", ip4addr_ntoa(&ip_info.netmask)); 
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"mask",buff);

 		sprintf(buff, "%s", ip4addr_ntoa(&ip_info.gw));
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"gw",buff);

		//get wifi ap when connected
		//
		if( esp_wifi_sta_get_ap_info( &ap_conf ) == ESP_OK ){
			sprintf(buff,"%s", ap_conf.ssid );
			cJSON_AddStringToObject(pSubJson_Connect_Station, "ssid", buff);

			cJSON_AddStringToObject(pSubJson_Connect_Station, "password","");
		}
		else{
			printf(" esp_wifi_get_json_station: esp_wifi_sta_get_ap_info fail.\n");
			return ESP_FAIL;
		}


	}//if networkstatus is down
	else if( wifi_networkstatus_event == WIFI_NETWORK_STATUS_DOWN ||
			wifi_networkstatus_event == WIFI_NETWORK_STATUS_UNKNOWN )
	{
		sprintf(buff,"%s", "0.0.0.0"); 
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"ip",buff);

 		sprintf(buff, "%s", "0.0.0.0"); 
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"mask",buff);

 		sprintf(buff, "%s", "0.0.0.0");
		cJSON_AddStringToObject(pSubJson_Ipinfo_Station,"gw",buff);

		//get wifi config
		if( esp_wifi_get_config( WIFI_IF_STA, &sta_conf ) == ESP_OK ){
			wifi_sta_config_t *pSta_conf = &sta_conf.sta;
			sprintf(buff, "%s", pSta_conf->ssid );
			cJSON_AddStringToObject(pSubJson_Connect_Station, "ssid", buff  );
			sprintf(buff, "%s", pSta_conf->password );
			cJSON_AddStringToObject(pSubJson_Connect_Station, "password", buff);
		}
		else{
			printf(" esp_wifi_get_config error for STATION config.\n");
			cJSON_AddStringToObject(pSubJson_Connect_Station, "ssid", "");
			cJSON_AddStringToObject(pSubJson_Connect_Station, "password","");
		}

	}
	else {
		printf("esp_wifi_get_json_station: unknown network status event?\n");
		return ESP_FAIL;
	}


	return ESP_OK;
}


/*********************************************************************************************************
 * @brief  
 * 
 *********************************************************************************************************/
esp_err_t esp_wifi_handle_http_req_getwifi( cJSON *pcjson,uint8_t wifi_networkstatus_event , uint8_t reason )
{
	char buff[64];
    bzero(buff, sizeof(buff));

	//Response node
	cJSON * pSubJson_Response= cJSON_CreateObject();
    if(NULL == pSubJson_Response){
        printf("esp_wifi_handle_http_req_getwifi: Response json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJson_Response);

	//status node
	cJSON * pSubJson_status = cJSON_CreateObject();
	if(NULL == pSubJson_status){
        printf("esp_wifi_handle_http_req_getwifi: status json object create fail.\n");
        return ESP_FAIL;
    }
	cJSON_AddItemToObject(pSubJson_Response, "Status", pSubJson_status);
	cJSON_AddStringToObject( pSubJson_status, "status", "ok" );

	//Station node
	 cJSON * pSubJson_Station= cJSON_CreateObject();
    if(NULL == pSubJson_Station){
        printf("esp_wifi_handle_http_req_getwifi:Station json object create fail.\n");
        return -1;
    }
    cJSON_AddItemToObject(pSubJson_Response, "Station", pSubJson_Station);
    
	//Softap node
    cJSON * pSubJson_Softap= cJSON_CreateObject();
    if(NULL == pSubJson_Softap){
        printf("esp_wifi_handle_http_req_getwifi: Softap json object create fail.\n");
        return -1;
    }
    cJSON_AddItemToObject(pSubJson_Response, "Softap", pSubJson_Softap);

	//WifiNetworkStatus node
	cJSON * pSubJson_WifiNetworkStatus= cJSON_CreateObject();
    if(NULL == pSubJson_WifiNetworkStatus){
        printf("esp_wifi_handle_http_req_getwifi: WifiNetworkStatus json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pSubJson_Response, "WifiNetworkStatus", pSubJson_WifiNetworkStatus);

	//if up status 
	if( wifi_networkstatus_event == WIFI_NETWORK_STATUS_UP ){
		cJSON_AddStringToObject( pSubJson_WifiNetworkStatus, "NetworkStatus", "up" );
		cJSON_AddStringToObject( pSubJson_WifiNetworkStatus, "Reason", "");

	}//if networkstatus is down
	else if( wifi_networkstatus_event == WIFI_NETWORK_STATUS_DOWN ||
			wifi_networkstatus_event == WIFI_NETWORK_STATUS_UNKNOWN )
	{
		cJSON_AddStringToObject( pSubJson_WifiNetworkStatus, "NetworkStatus", "down" );
		if( wifi_networkstatus_event == WIFI_NETWORK_STATUS_DOWN ) {
			sprintf( buff, "%s", esp_wifi_eventToString( reason ));
			cJSON_AddStringToObject( pSubJson_WifiNetworkStatus, "Reason", buff );
		}
		else {
			cJSON_AddStringToObject( pSubJson_WifiNetworkStatus, "Reason","" );
		}
	}
	else {
		//error happen
		printf(" esp_wifi_handle_http_req_getwifi: wifi network status event is error.\n");
		return ESP_FAIL;

	}

	//create station and softap json
	if( esp_wifi_get_json_station( pSubJson_Station, wifi_networkstatus_event ) == ESP_FAIL ){
		printf(" esp_wifi_get_json_station fail.\n");
		return ESP_FAIL;
	}
	if( esp_wifi_get_json_softap( pSubJson_Softap, wifi_networkstatus_event ) == ESP_FAIL ) {
		printf("esp_wifi_get_json_softap fail.\n");
		return ESP_FAIL;
	}


	return ESP_OK;
}


/*********************************************************************************************************
 * @brief  
 * 
 *********************************************************************************************************/
esp_err_t esp_wifi_handle_http_req_getpartitioninfo( cJSON *pcjson, const char *pRunningbuf, const char *pConfiguredbuf )
{
	//Response node
	cJSON * pSubJson_Response= cJSON_CreateObject();
    if(NULL == pSubJson_Response){
        printf("esp_wifi_handle_http_req_getpartitioninfo: Response json object create fail.\n");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(pcjson, "Response", pSubJson_Response);

	
	cJSON_AddStringToObject( pSubJson_Response, "ConfiguredPartition", pConfiguredbuf );

	cJSON_AddStringToObject( pSubJson_Response, "RunningPartition", pRunningbuf );

	return ESP_OK;

}













































