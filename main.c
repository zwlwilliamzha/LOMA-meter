
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <tcpip_adapter.h>
#include <lwip/sockets.h>
#include <mongoose.h>
#include <cJSON.h>
#include <apps/sntp/sntp.h>
#include <lwip/err.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "driver/uart.h"
#include "esp_wifi_func.h"
#include "esp_afe.h"
#include "esp_mail.h"
#include "esp_platform.h"
#include "esp_lcd.h"
#define KEY_VERSION "wificonnversion"
uint32_t g_version=0x0100;

static int g_mongooseStarted = 0; // Has the mongoose server started?
static int g_mongooseStopRequest = 0; // Request to stop the mongoose server.

static char tag[] = "bootwifi";
static struct mg_serve_http_opts s_http_server_opts;


#define KEY_CALIBRATION_INFO "calibrationpara" // Key used in NVS for calibration
#define CALIBRATION_NAMESPACE "calibration" // Namespace in NVS for calibration

#define KEY_METER_CONFIG_INFO "meterconfig"  //key used in NVS for meter config
#define METERCONFIG_NAMESPACE "meterconfigns" //Namespace in NVS for meter config

#define KEY_METER_ENERGY_INFO "meterenergy"   //key used in NVS for meter energy
#define METER_ENERGY_NAMESPACE "meterenergyns"  //Namespace in NVS for meter energy


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
SemaphoreHandle_t wifi_event_group_mux;           /* wifi event group mutex*/

//A wifi mailbox is a queue, so its handle is stored in a variable of type QueueHandle_t.
QueueHandle_t xWifiEventMailbox;
WifiEvent_t mgWifiEventMail;


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT     = BIT0;
const int DISCONNECTED_BIT  = BIT1;

//whether sntp started?
int sntp_started = 0;
time_t sntp_time;



//smartmeter config parameter
smartmeter_config_info_t  meter_config_info;

//ota function used
static const char *TAG1 = "ota";
#define OTA_SERVER_IP   "157.22.33.37"
#define OTA_SERVER_PORT "8080"

#define OTA_FILENAME  "app-smartmeter.bin"


#define OTA_BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[OTA_BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[OTA_BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;
static char http_request[64] = {0};

//if i am AP now ,set this flag
int selfAccessPointOk = 0;


//http test response
static const char *reply_fmt_ok =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Connect AP successful! %s\n";


static const char *reply_fmt_err =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Connect AP failed! %s\n";


/*******************************************************************************
*  Global define
*
********************************************************************************/
/*
esp_err_t esp32_wifi_eventHandler(void *ctx, system_event_t *event) {
	// Your event handling code here...
	switch(event->event_id) {
		// When we have started being an access point, then start being a web server.
		//
		case SYSTEM_EVENT_AP_START: { // Handle the AP start event
			tcpip_adapter_ip_info_t ip_info;
			tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);

			printf(" AP START event!!!\n");



			break;
		} //

		default: // Ignore the other event types
			break;

	}

	return ESP_OK;
}
*/


/*******************************************************************************
*  Convert a Mongoose event type to a string.  Used for debugging.
*
********************************************************************************/
static char *mongoose_eventToString(int ev) {
	static char temp[100];
	switch (ev) {
	case MG_EV_CONNECT:
		return "MG_EV_CONNECT";
	case MG_EV_ACCEPT:
		return "MG_EV_ACCEPT";
	case MG_EV_CLOSE:
		return "MG_EV_CLOSE";
	case MG_EV_SEND:
		return "MG_EV_SEND";
	case MG_EV_RECV:
		return "MG_EV_RECV";
	case MG_EV_HTTP_REQUEST:
		return "MG_EV_HTTP_REQUEST";
	case MG_EV_MQTT_CONNACK:
		return "MG_EV_MQTT_CONNACK";
	case MG_EV_MQTT_CONNACK_ACCEPTED:
		return "MG_EV_MQTT_CONNACK";
	case MG_EV_MQTT_CONNECT:
		return "MG_EV_MQTT_CONNECT";
	case MG_EV_MQTT_DISCONNECT:
		return "MG_EV_MQTT_DISCONNECT";
	case MG_EV_MQTT_PINGREQ:
		return "MG_EV_MQTT_PINGREQ";
	case MG_EV_MQTT_PINGRESP:
		return "MG_EV_MQTT_PINGRESP";
	case MG_EV_MQTT_PUBACK:
		return "MG_EV_MQTT_PUBACK";
	case MG_EV_MQTT_PUBCOMP:
		return "MG_EV_MQTT_PUBCOMP";
	case MG_EV_MQTT_PUBLISH:
		return "MG_EV_MQTT_PUBLISH";
	case MG_EV_MQTT_PUBREC:
		return "MG_EV_MQTT_PUBREC";
	case MG_EV_MQTT_PUBREL:
		return "MG_EV_MQTT_PUBREL";
	case MG_EV_MQTT_SUBACK:
		return "MG_EV_MQTT_SUBACK";
	case MG_EV_MQTT_SUBSCRIBE:
		return "MG_EV_MQTT_SUBSCRIBE";
	case MG_EV_MQTT_UNSUBACK:
		return "MG_EV_MQTT_UNSUBACK";
	case MG_EV_MQTT_UNSUBSCRIBE:
		return "MG_EV_MQTT_UNSUBSCRIBE";
	case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
		return "MG_EV_WEBSOCKET_HANDSHAKE_REQUEST";
	case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
		return "MG_EV_WEBSOCKET_HANDSHAKE_DONE";
	case MG_EV_WEBSOCKET_FRAME:
		return "MG_EV_WEBSOCKET_FRAME";
	}
	sprintf(temp, "Unknown event: %d", ev);
	return temp;
} //eventToString

/*******************************************************************************
*  Convert a Mongoose string type to a string.
*
********************************************************************************/
static char *mgStrToStr(struct mg_str mgStr) {
	char *retStr = (char *) malloc(mgStr.len + 1);
	memcpy(retStr, mgStr.p, mgStr.len);
	retStr[mgStr.len] = 0;
	return retStr;
} // mgStrToStr

/*******************************************************************************
*  Global define
*
********************************************************************************/
static void handle_sum_call(struct mg_connection *nc, struct http_message *hm) {

	int len;
	char *pbuf = (char*)malloc(256);
	int i,j;
	float fVal[10];
	char measureValueStr[64];
	cJSON *pcjson;

    pcjson=cJSON_CreateObject();
    if(NULL == pcjson){
        printf(" ERROR! cJSON_CreateObject fail!\n");
        return;
    }

	//generate random values
	//
  //int k = rand()%(222-218+1) + 218;
  //printf("-----------> random k : %d\n", k );

	for(i=0;i<10;i++ )
	{
		fVal[i] = rand()/(float)(RAND_MAX)*4+3;
		printf("--------------->random : %f\n", fVal[i]);
	}

	cJSON_AddStringToObject(pcjson, "MeterSN", "LOMA0000001" );
	cJSON_AddStringToObject(pcjson, "Time", "" );

	sprintf(measureValueStr, "%f", fVal[0] );
	cJSON_AddStringToObject(pcjson, "Voltage", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[1] );
	cJSON_AddStringToObject(pcjson, "Current", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[2] );
	cJSON_AddStringToObject(pcjson, "ActivePower", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[3] );
	cJSON_AddStringToObject(pcjson, "ReactivePower", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[4] );
	cJSON_AddStringToObject(pcjson, "ActiveEnergy", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[5] );
	cJSON_AddStringToObject(pcjson, "ReactiveEnergy", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[6] );
	cJSON_AddStringToObject(pcjson, "PowerFactor", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[7] );
	cJSON_AddStringToObject(pcjson, "Frequency", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[8] );
	cJSON_AddStringToObject(pcjson, "Temperature", measureValueStr);

	sprintf(measureValueStr, "%f", fVal[9] );
	cJSON_AddStringToObject(pcjson, "ActiveDemand", measureValueStr);


	pbuf = cJSON_Print(pcjson);

  /* Send headers */
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  //mg_printf_http_chunk(nc, "{ \"MeterSN\":\"LOMA00001\",\"Voltage\":\"220.00\" }" );
  mg_printf_http_chunk(nc, pbuf );

  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

  cJSON_Delete(pcjson);
  if( pbuf ){
	  free( pbuf);
	  pbuf = NULL;
  }
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
static void handle_sum_call_ext(struct mg_connection *nc, struct http_message *hm)
{
	int len;
	char *pbuf = (char*)malloc(512);
	int i,j;
	float fVal[3][10];
	char measureValueStr[64];
	cJSON *pcjson;

    pcjson=cJSON_CreateObject();
    if(NULL == pcjson){
        printf(" ERROR! cJSON_CreateObject fail!\n");
        return;
    }

	//generate random values
	//
	for(i=0;i<3;i++) {  //A/B/C three phase
		for( j=0;j<10;j++ ) {   //ten metrics

			if( j==0 ) {  //voltage(109-111)
				fVal[i][j] = rand()/(float)(RAND_MAX)*2+109;
			}
			else if( j== 1 ) { //current(0.1-0.3)
				fVal[i][j] = (rand()/(float)(RAND_MAX)*2+1) * 0.1;
			}
			else if( j==6 ) {//PowerFactor
				fVal[i][j]  = 1.00;
			}
			else if( j==7 ) {//Frequency(59-61)
				fVal[i][j] = rand()/(float)(RAND_MAX)*2+59;
			}
			else if( j==8 ) {//Temperature(36-38)
				fVal[i][j] = rand()/(float)(RAND_MAX)*2+36;
			}
			else if( j==9 ) {//ActiveDemand
				fVal[i][j]  = 0.00;
			}
			else {
				//do nothing
			}

			//calculate others by use of them
			//
			if( j ==2 ) {//ActivePower
				fVal[i][j] = fVal[i][0] * fVal[i][1];
			}
			else if( j==3 ) {//ReactivePower
				fVal[i][j] = 0.00;
			}
			else if( j==4 ) {//ActiveEnergy(1.3-1.5)
				fVal[i][j] = (rand()/(float)(RAND_MAX)*2+13) * 0.1;
			}
			else if( j==5 ) {//ReactiveEnergy(1.4-1.5)
				fVal[i][j] = (rand()/(float)(RAND_MAX)*1+14) * 0.1;
			}
			else {
				//do nothing
			}


		}

	}//for



	cJSON_AddStringToObject(pcjson, "MeterSN", "LOMA0000001" );
	cJSON_AddStringToObject(pcjson, "Time", "" );

	//Phase process
	//
	cJSON * pSubJson_Phase = NULL;
    lcd_init();
	for(i=0;i<3;i++) {  //A/B/C three phase

		pSubJson_Phase = cJSON_CreateObject();
		if( i==0 ){
			cJSON_AddItemToObject(pcjson, "PhaseA", pSubJson_Phase);
		}
		else if( i==1 ){
			cJSON_AddItemToObject(pcjson, "PhaseB", pSubJson_Phase);
		}
		else if( i==2 ) {
			cJSON_AddItemToObject(pcjson, "PhaseC", pSubJson_Phase);
		}
		else {
			//do nothing
		}

		sprintf(measureValueStr, "%.2f", fVal[i][0] );
		cJSON_AddStringToObject(pSubJson_Phase, "Voltage", measureValueStr);
        //if( i==0 )lcd_test(fVal[i][0]);

		sprintf(measureValueStr, "%f", fVal[i][1] );
		cJSON_AddStringToObject(pSubJson_Phase, "Current", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][2] );
		cJSON_AddStringToObject(pSubJson_Phase, "ActivePower", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][3] );
		cJSON_AddStringToObject(pSubJson_Phase, "ReactivePower", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][4] );
		cJSON_AddStringToObject(pSubJson_Phase, "ActiveEnergy", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][5] );
		cJSON_AddStringToObject(pSubJson_Phase, "ReactiveEnergy", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][6] );
		cJSON_AddStringToObject(pSubJson_Phase, "PowerFactor", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][7] );
		cJSON_AddStringToObject(pSubJson_Phase, "Frequency", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][8] );
		cJSON_AddStringToObject(pSubJson_Phase, "Temperature", measureValueStr);

		sprintf(measureValueStr, "%f", fVal[i][9] );
		cJSON_AddStringToObject(pSubJson_Phase, "ActiveDemand", measureValueStr);

	}

	pbuf = cJSON_Print(pcjson);

  /* Send headers */
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  //mg_printf_http_chunk(nc, "{ \"MeterSN\":\"LOMA00001\",\"Voltage\":\"220.00\" }" );
  mg_printf_http_chunk(nc, pbuf );

  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

  cJSON_Delete(pcjson);
  if( pbuf ){
	  free( pbuf);
	  pbuf = NULL;
  }

}

/*******************************************************************************
*  Global define
*
********************************************************************************/
void  nvs_save_WifiConnectionInfo(  connection_info_t *pConnectionInfo)  {
	nvs_handle handle;
	ESP_ERROR_CHECK(nvs_open(BOOTWIFI_NAMESPACE, NVS_READWRITE, &handle));
	ESP_ERROR_CHECK(nvs_set_blob(handle, KEY_CONNECTION_INFO, pConnectionInfo,
			sizeof(connection_info_t)));
	ESP_ERROR_CHECK(nvs_set_u32(handle, KEY_VERSION, g_version));
	ESP_ERROR_CHECK(nvs_commit(handle));
	nvs_close(handle);
}
/*******************************************************************************
*  Global define
*
********************************************************************************/
int nvs_get_WifiConnectionInfo( connection_info_t *pConnectionInfo ) {
	nvs_handle handle;
	size_t size;
	esp_err_t err;
	uint32_t version;

	err = nvs_open(BOOTWIFI_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(tag, "nvs_open: %x", err);
		nvs_close(handle);
		return -1;
	}

	// Get the version that the data was saved against.
	//
	err = nvs_get_u32(handle, KEY_VERSION, &version);
	if (err != ESP_OK) {
		ESP_LOGD(tag, "No version record found (%d).", err);
		nvs_close(handle);
		return -1;
	}

	// Check the versions match
	//
	if ((version & 0xff00) != (g_version & 0xff00)) {
		ESP_LOGD(tag, "Incompatible versions ... current is %x, found is %x", version, g_version);
		nvs_close(handle);
		return -1;
	}

	size = sizeof(connection_info_t);
	err = nvs_get_blob(handle, KEY_CONNECTION_INFO, pConnectionInfo, &size);
	if (err != ESP_OK) {
		ESP_LOGD(tag, "No connection record found (%d).", err);
		nvs_close(handle);
		return -1;
	}


	// Cleanup nvs
	//
	nvs_close(handle);

	// Do a sanity check on the SSID
	//
	if (strlen(pConnectionInfo->ssid) == 0) {
		ESP_LOGD(tag, "NULL ssid detected");
		return -1;
	}
	return 0;

}
/*******************************************************************************
*  Global define
*
********************************************************************************/
void  nvs_save_MeterConfigInfo(  smartmeter_config_info_t * pMeterConfigInfo  )  {

	nvs_handle handle;
	ESP_ERROR_CHECK(nvs_open(METERCONFIG_NAMESPACE, NVS_READWRITE, &handle));
	ESP_ERROR_CHECK(nvs_set_blob(handle, KEY_METER_CONFIG_INFO, pMeterConfigInfo,
			sizeof( smartmeter_config_info_t )));
	ESP_ERROR_CHECK(nvs_commit(handle));
	nvs_close(handle);
	printf(" nvs_save_MeterConfigInfo end.\n");
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
int nvs_get_MeterConfigInfo( smartmeter_config_info_t * pMeterConfigInfo  ){

	nvs_handle handle;
	size_t size;
	esp_err_t err;

	err = nvs_open(METERCONFIG_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "nvs_open: %x", err);
		nvs_close(handle);
		return -1;
	}

	size = sizeof( smartmeter_config_info_t );
	err = nvs_get_blob(handle, KEY_METER_CONFIG_INFO, pMeterConfigInfo, &size);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "No record found (%d).", err);
		nvs_close(handle);
		return -1;
	}
	else {
		ESP_LOGI(tag," find MeterConfigInfo in nvs.");
	}

	// Cleanup nvs
	//
	nvs_close(handle);

	return 0;

}

/*******************************************************************************
*  Global define
*
********************************************************************************/
int nvs_get_MeterCalibrationPara( MeterCalibrationPara * pCalibrationPara  ) {

	nvs_handle handle;
	size_t size;
	esp_err_t err;
	uint32_t version;

	err = nvs_open(CALIBRATION_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "nvs_open: %x", err);
		nvs_close(handle);
		return -1;
	}

	size = sizeof( MeterCalibrationPara );
	err = nvs_get_blob(handle, KEY_CALIBRATION_INFO, pCalibrationPara, &size);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "No record found (%d).", err);
		nvs_close(handle);
		return -1;
	}
	else {
		ESP_LOGI(tag," find calibration para in nvs.");
	}

	// Cleanup nvs handle
	//
	nvs_close(handle);

	return 0;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
int nvs_get_MeterActiveEnergy(   )
{
	nvs_handle handle;
	size_t size;
	esp_err_t err;
	uint32_t version;

	//max EPC for phaseABC
	int idx=0;
	char strIdx[20];
	int32_t tmpCounter = 0;
	//PhaseA
	int32_t phaseAMaxEPC = 0, maxReversePhaseAEPC=0, maxCapacitiveRePhaseAEPC = 0,maxInductiveRePhaseAEPC = 0;
	//PhaseB
	int32_t phaseBMaxEPC = 0, maxReversePhaseBEPC=0, maxCapacitiveRePhaseBEPC = 0,maxInductiveRePhaseBEPC = 0;
	//PhaseC
	int32_t phaseCMaxEPC = 0, maxReversePhaseCEPC=0, maxCapacitiveRePhaseCEPC = 0,maxInductiveRePhaseCEPC = 0;


	err = nvs_open(METER_ENERGY_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "nvs_open: %x for meter energy", err);
		nvs_close(handle);
		return -1;
	}
	else{
		printf("nvs_open meter energy Done\n");
        // Read Max Energy Pulse Counter here
		//
        printf("Reading max Energy Pulse Counter from NVS ... ");

		/***********************************************************
		*                  Phase A Max                             *
		***********************************************************/

		//PhaseA
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "Ae%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > phaseAMaxEPC ) {
					//printf("-----------PhaseA: maxEPC is set value %d \n", tmpCounter );
					phaseAMaxEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for PhaseA!\n", err);
			}
		}

		//ReverseAe
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "ReverseAe%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxReversePhaseAEPC ) {
					maxReversePhaseAEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for ReverseAe!\n", err);
			}
		}

		//CapReA
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "CapReA%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxCapacitiveRePhaseAEPC ) {
					maxCapacitiveRePhaseAEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for CapReA!\n", err);
			}
		}

		//IndReA
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "IndReA%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxInductiveRePhaseAEPC ) {
					maxInductiveRePhaseAEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for IndReA!\n", err);
			}
		}


		/***********************************************************
		*                  Phase B   Max                           *
		***********************************************************/

		//PhaseB
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "Be%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > phaseBMaxEPC ) {
					//printf("-----------PhaseB: maxEPC is set value %d \n", tmpCounter );
					phaseBMaxEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for PhaseB!\n", err);
			}
		}

		//ReverseBe
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "ReverseBe%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxReversePhaseBEPC ) {
					maxReversePhaseBEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for ReverseBe!\n", err);
			}
		}

		//CapReB
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "CapReB%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxCapacitiveRePhaseBEPC ) {
					maxCapacitiveRePhaseBEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for CapReB!\n", err);
			}
		}

		//IndReB
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "IndReB%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxInductiveRePhaseBEPC ) {
					maxInductiveRePhaseBEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for IndReB!\n", err);
			}
		}


		/***********************************************************
		*                  Phase C   Max                           *
		***********************************************************/
		//PhaseC
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "Ce%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > phaseCMaxEPC ) {
					//printf("-----------PhaseC: maxEPC is set value %d \n", tmpCounter );
					phaseCMaxEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for PhaseC!\n", err);
			}
		}

		//ReverseCe
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "ReverseCe%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxReversePhaseCEPC ) {
					maxReversePhaseCEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for ReverseCe!\n", err);
			}
		}

		//CapReC
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "CapReC%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxCapacitiveRePhaseCEPC ) {
					maxCapacitiveRePhaseCEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for CapReC!\n", err);
			}
		}

		//IndReC
		tmpCounter = 0;
		for( idx=0; idx<288; idx++ ){
			sprintf( strIdx, "IndReC%d", idx );
			err = nvs_get_i32(handle, strIdx, &tmpCounter);
			if ( err == ESP_OK ) {
				if( tmpCounter > maxInductiveRePhaseCEPC ) {
					maxInductiveRePhaseCEPC = tmpCounter;
				}
			}
			else if( err == ESP_ERR_NVS_NOT_FOUND )
			{
				tmpCounter = 0;
				continue;
	        }
			else {
				printf("Error (%d) reading EPC for IndReC!\n", err);
			}
		}


		//update energy pulse counter and active energy for phaseA/B/C
		meterEPC.PhaseAEPC = phaseAMaxEPC;
		meterEPC.ReversePhaseAEPC = maxReversePhaseAEPC;
		meterEPC.CapacitiveRePhaseAEPC = maxCapacitiveRePhaseAEPC;
		meterEPC.InductiveRePhaseAEPC = maxInductiveRePhaseAEPC;

		meterEPC.PhaseBEPC = phaseBMaxEPC;
		meterEPC.ReversePhaseBEPC = maxReversePhaseBEPC;
		meterEPC.CapacitiveRePhaseBEPC = maxCapacitiveRePhaseBEPC;
		meterEPC.InductiveRePhaseBEPC = maxInductiveRePhaseBEPC;

		meterEPC.PhaseCEPC = phaseCMaxEPC;
		meterEPC.ReversePhaseCEPC = maxReversePhaseCEPC;
		meterEPC.CapacitiveRePhaseCEPC = maxCapacitiveRePhaseCEPC;
		meterEPC.InductiveRePhaseCEPC = maxInductiveRePhaseCEPC;

		printf(" Recovery meter EPC end, AeEPC:%d, BeEPC:%d, CeEPC:%d\n", meterEPC.PhaseAEPC, meterEPC.PhaseBEPC , meterEPC.PhaseCEPC );

	}

	// Cleanup nvs handle
	//
	nvs_close(handle);


	return 0;
}


/*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t esp_wifi_set_ap_ssid( wifi_config_t *pAp_conf )
{
	char buff[64];
    bzero(buff, sizeof(buff));


	if( pAp_conf != NULL ){
		//it first have already load meter configuration and meter calibration parameter from NVS
		//so we can use it
		//
		if( strlen( meterCalibrationPara.meterSN ) > 0 ){
			sprintf( meter_config_info.meterSN, "%s", meterCalibrationPara.meterSN );

			sprintf( buff, "%s", meter_config_info.meterSN );
			strcat( buff,"AP" );
			//max ssid length is 32
			//
			if( strlen(buff) < 32 ) {
				printf(" New AP for meter is : %s\n", buff );
				memcpy( pAp_conf->ap.ssid, buff, 32 );
			}
		}
		else{
			printf("esp_wifi_set_ap_ssid: meter configuration have not set meterSN.\n");
			return ESP_FAIL;
		}
	}
	else{
		printf(" esp_wifi_set_ap_ssid : parameter error.\n");
		return ESP_FAIL;
	}

	return ESP_OK;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
static void initialize_sntp(void)
{
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
	//sntp server name
	//
    sntp_setservername(0, "pool.ntp.org");

	//sntp server IPaddr
	//
	//sntp_setserver(0,"163.172.177.158");
    sntp_init();
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
static void test_sntp_api_function()
{
	time_t now;
    struct tm timeinfo;
    time(&now);

	//test sntp function
	//
	time(&now);
	char strftime_buf[64];

	// Set timezone to Eastern Standard Time and print local time
	setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(tag, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8CDT-9,M4.2.0/2,M9.2.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(tag, "The current date/time in Shanghai is: %s", strftime_buf);


}
/*******************************************************************************
*  Global define
*
********************************************************************************/
void updateWifiEventMailbox( uint32_t ulNewValue ,uint8_t reason)
{
	WifiEvent_t xWifiEventData;

	xWifiEventData.ulValue = ulNewValue;
	xWifiEventData.xTimeStamp = xTaskGetTickCount();
	xWifiEventData.reason = reason;

	//Send the structure to the mailbox - overwriting any data that is already in the
	//mailbox.
    xQueueOverwrite( xWifiEventMailbox, &xWifiEventData );

}
/*******************************************************************************
*  Global define
*
********************************************************************************/
BaseType_t readWifiEventMailbox( WifiEvent_t *pxData )
{
	TickType_t xPreviousTimeStamp;
	BaseType_t xDataUpdated = pdFALSE;

	// This function updates an WifiEvent_t structure with the latest value received
	//from the mailbox. Record the time stamp already contained in *pxData before it
	//gets overwritten by the new data.
	//
	xPreviousTimeStamp = pxData->xTimeStamp;


	//if a block time is specified, so the calling task will be placed in the Blocked
	//state to wait for the mailbox to contain data should the mailbox be empty.But
	//i donot want to block here, so set a block time to zero
	//
	//xQueuePeek( xMailbox, pxData, portMAX_DELAY );
	if( xQueuePeek( xWifiEventMailbox, pxData, 0 ) ) {
		// Return pdTRUE if the value read from the mailbox has been updated since this
		//function was last called. Otherwise return pdFALSE.
		//
		if( pxData->xTimeStamp > xPreviousTimeStamp )
		{
			xDataUpdated = pdTRUE;
		}
		else
		{
			xDataUpdated = pdFALSE;
		}
	}
	else {
		printf(" WifiEvent Mailbox is empty.\n");
		xDataUpdated = pdFALSE;
	}
	return xDataUpdated;

}

/*******************************************************************************
*  Global define
*
********************************************************************************/
/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, OTA_BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);

            esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG1, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            } else {
                ESP_LOGI(TAG1, "esp_ota_write header OK");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}

bool connect_to_http_server()
{
    ESP_LOGI(TAG1, "Server IP: %s Server Port:%s", OTA_SERVER_IP, OTA_SERVER_PORT);
    sprintf(http_request, "GET /Upload/uploadFiles/%s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", OTA_FILENAME, OTA_SERVER_IP, OTA_SERVER_PORT);
	ESP_LOGI(TAG1, "%s", http_request );

    int  http_connect_flag = -1;
    struct sockaddr_in sock_info;

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG1, "Create socket failed!");
        return false;
    }

    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(OTA_SERVER_IP);
    sock_info.sin_port = htons(atoi(OTA_SERVER_PORT));

    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG1, "Connect to server failed! errno=%d", errno);
        close(socket_id);
        return false;
    } else {
        ESP_LOGI(TAG1, "Connected to server");
        return true;
    }
    return false;
}

void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(TAG1, "Exiting task due to fatal error...");
    close(socket_id);
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}



/*******************************************************************************
*  Global define
*
********************************************************************************/
void ota_task(void *pvParameter)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG1, "Starting OTA task........");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    assert(configured == running); /* fresh from reset, should be running from configured boot partition */
    ESP_LOGI(TAG1, "Running partition type %d subtype %d (offset 0x%08x)",
             configured->type, configured->subtype, configured->address);



    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG1, "Connected to http server");
    } else {
        ESP_LOGE(TAG1, "Connect to http server failed!");
        task_fatal_error();
    }

    int res = -1;
    /*send GET request to http server*/
    res = send(socket_id, http_request, strlen(http_request), 0);
    if (res == -1) {
        ESP_LOGE(TAG1, "Send GET request to server failed");
        task_fatal_error();
    } else {
        ESP_LOGI(TAG1, "Send GET request to server succeeded");
    }

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG1, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);


    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG1, "esp_ota_begin failed, error=%d", err);
        task_fatal_error();
    }
    ESP_LOGI(TAG1, "esp_ota_begin succeeded");

    bool resp_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(ota_write_data, 0, OTA_BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG1, "Error: receive data error! errno=%d", errno);
            task_fatal_error();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            memcpy(ota_write_data, text, buff_len);
            resp_body_start = read_past_http_header(text, buff_len, update_handle);
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            memcpy(ota_write_data, text, buff_len);

            err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG1, "Error: esp_ota_write failed! err=0x%x", err);
                task_fatal_error();
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG1, "Have written image length %d", binary_file_length);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG1, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG1, "Unexpected recv result");
        }
    }

    ESP_LOGI(TAG1, "Total Write binary data length : %d", binary_file_length);

    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG1, "esp_ota_end failed!");
        task_fatal_error();
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG1, "esp_ota_set_boot_partition failed! err=0x%x", err);
        task_fatal_error();
    }
    ESP_LOGI(TAG1, "Prepare to restart system!");
    esp_restart();
    return ;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
static void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData) {
	//http request body will not exceed max 1024!
	//
	char strbuf[1024] = {0};
	char apssid[64] = {0};
	char password[32] = {0};
	char addr[32];

	char *pchar = NULL;
    cJSON *pcjson = NULL;
    int bufLen = 0;

	EventBits_t uxBits;
	//10 secs
	//
    const TickType_t xTicksToWait = 20000 / portTICK_PERIOD_MS;

	ESP_LOGD(tag, "- Event: %s", mongoose_eventToString(ev));
	switch (ev) {
		case MG_EV_HTTP_REQUEST: {

			//test url
			/*
			struct http_message *message = (struct http_message *) evData;
			char *uri = mgStrToStr(message->uri);
			ESP_LOGD(tag, " - uri: %s", uri);
			*/

			struct http_message *hm = (struct http_message *) evData;
			if (mg_vcmp(&hm->uri, "/getmetersnapshot") == 0) {
				//handle_sum_call(nc, hm); /* Handle RESTful call */
				handle_sum_call_ext(nc, hm); /* Handle RESTful call */
			} else if (mg_vcmp(&hm->uri, "/printcontent") == 0) {
				//I want to test http request body here
				//
				char buf[200] = {0};
				memcpy(buf, hm->body.p,
					sizeof(buf) - 1 < hm->body.len ? sizeof(buf) - 1 : hm->body.len);
				printf("%s\n", buf);

			}
			else if( mg_vcmp(&hm->uri, "/wifi/setwifi") == 0) {

				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//body process
				memcpy(strbuf, hm->body.p,
					sizeof(strbuf) - 1 < hm->body.len ? sizeof(strbuf) - 1 : hm->body.len);
				//printf("%s\n", strbuf);

				//parse set wifi request and get parameters
				//
				if( esp_wifi_parse_req_setwifi( strbuf, apssid, password ) == ESP_OK ) {
					printf("==> apssid : %s,len =%d, pwd : %s, len=%d, \n", apssid,strlen(apssid), password, strlen(password) );

					//clear eventgroup for wait event happen
					//
					xEventGroupClearBits( wifi_event_group,  CONNECTED_BIT | DISCONNECTED_BIT );

					//check if we have already got ip from DHCP server before ,if we got ip , disconnect from the current AP
					//
					tcpip_adapter_ip_info_t current_ip_info;
					ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &current_ip_info));
					if (!(ip4_addr_isany_val(current_ip_info.ip) || ip4_addr_isany_val(current_ip_info.netmask) || ip4_addr_isany_val(current_ip_info.gw))) {
						printf("We have got IP address from DHCP Server! So we disconnect it !\n");
						//disconnect from the current AP
						//
						ESP_ERROR_CHECK(esp_wifi_disconnect());

						xEventGroupWaitBits(wifi_event_group,
												DISCONNECTED_BIT,
												pdTRUE,    //DISCONNECTED_BIT should be cleared before returning
												pdFALSE,
												portMAX_DELAY);
						printf("have already got DISCONNECT_BIT event and processed.\n");
					}
					else{
						printf(" We have not got IP address from DHCP Server,so continue....\n");
					}

					//set STATION config
					//
					wifi_config_t sta_config ;
					memcpy((char *)sta_config.sta.ssid, (char *)apssid, strlen(apssid));
					sta_config.sta.ssid[ strlen(apssid) ] = '\0';

					memcpy((char *)sta_config.sta.password, (char *)password, strlen(password));
					sta_config.sta.password[ strlen(password) ] = '\0';

					sta_config.sta.bssid_set = 0;

					/*
						wifi_config_t sta_config = {
								.sta = {
										.ssid      = AP_TARGET_SSID,
										.password  = AP_TARGET_PASSWORD,
										.bssid_set = 0
										}
						};
					*/

					//config and connect AP , wait connect event happen in callback
					//
					ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

					ESP_ERROR_CHECK(esp_wifi_connect());

					//block and wait for connection for test
					//
					printf("Main task: waiting for connection to the wifi network... ");

					//First choice: We can block and wait indefinitely (without a timeout),so we can do
					//
					//xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

					//Second choice: We can block and wait ten seconds,after timeout
					//
					//this task will block here waitting for event group now
					//uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

					// Wait a maximum of 20s for CONNECTED_BIT to be set within the event group. Donot clear the bit before exiting.
					//
					//uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, xTicksToWait);
					uxBits = xEventGroupWaitBits(wifi_event_group,
												CONNECTED_BIT | DISCONNECTED_BIT,
												pdFALSE,
												pdFALSE,
												portMAX_DELAY);


					if( ( uxBits & CONNECTED_BIT ) != 0 ) {
						printf("connected to AP !\n");
 						// print the local IP address
						//
 						tcpip_adapter_ip_info_t ip_info;
 						ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
 						printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
 						printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
 						printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));

						//save wifi connection info to nvs
						//
						connection_info_t wifiConnectionInfo;
						sprintf(wifiConnectionInfo.ssid, "%s", apssid );
						sprintf(wifiConnectionInfo.password, "%s", password );

						memcpy((void*)&wifiConnectionInfo.ipInfo, (void*)&ip_info, sizeof( tcpip_adapter_ip_info_t));
						//save them to nvs
						//
						nvs_save_WifiConnectionInfo( (connection_info_t*)&wifiConnectionInfo );


						//init sntp call
						//
						//initialize_sntp();

						// wait for time to be set for test
						//
						time_t now = 0;
						struct tm timeinfo = { 0 };
						int retry = 0;
						const int retry_count = 10;
						while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
							ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
							vTaskDelay(2000 / portTICK_PERIOD_MS);
							time(&now);
							localtime_r(&now, &timeinfo);
						}

						//test sntp function
						//
						time(&now);
						localtime_r(&now, &timeinfo);
						if (timeinfo.tm_year < (2016 - 1900)) {
							ESP_LOGI(tag, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
						}
						else {
							char strftime_buf[64];

							//test time zone API
							//
							// Set timezone to Eastern Standard Time and print local time
							//setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
							setenv("TZ", "GMT+8GMT+7,M3.2.0/2,M11.1.0/2", 1);
							tzset();
							localtime_r(&now, &timeinfo);
							strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
							ESP_LOGI(tag, "The current date/time in PST is: %s", strftime_buf);


							/*
							// Set timezone to China Standard Time
							setenv("TZ", "CST-8CDT-9,M4.2.0/2,M9.2.0/3", 1);
							tzset();
							localtime_r(&now, &timeinfo);
							strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
							ESP_LOGI(tag, "The current date/time in Shanghai is: %s", strftime_buf);
							*/


							//if successful, so set sntp started flag
							//
							if( sntp_started == 0 ){
								sntp_started = 1;
								//need start one second timer for esp32 local time here??
								sntp_time = now;
							}


						}

						/*
						//it is the first usage for http response
						//
						//mg_printf(nc, reply_fmt_ok, addr);
						//nc->flags |= MG_F_SEND_AND_CLOSE;
						*/

						//Now we connect to AP successful, send response to client
						//
						//create json root node
						pcjson=cJSON_CreateObject();
						if(NULL == pcjson) {
							printf(" getwifi req process: create root node fail!\n");
						}
						esp_wifi_handle_http_req_getwifi( pcjson, WIFI_NETWORK_STATUS_UP, WIFI_REASON_UNSPECIFIED );
						//send response to client
						pchar = cJSON_Print(pcjson);
						bufLen = strlen(pchar);
						//printf("recv WIFI Get Request, send JSON Response is :   %s, len is : %d\n", pchar, bufLen);

						mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
						mg_printf_http_chunk(nc, "%s", pchar);
						mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

						//free buffer
						if(pcjson){
							cJSON_Delete(pcjson);
						}
						if(pchar){
							free(pchar);
							pchar=NULL;
						}


					}
					else if( ( uxBits & DISCONNECTED_BIT ) != 0 )
					{
						//xEventGroupWaitBits() returned because xTicksToWait ticks passed without CONNECTED_BIT becoming set.
						//
						printf(" Connected to AP failed !\n");

						//if an infinite block time is used,  it is not necessary to check the value returned
						//from xQueuePeek(), as xQueuePeek() will only return when data is available.
						//Here specifying 0 means return immediately,i donot want to block anytime for mailbox
						//
						BaseType_t xMailUpdated = pdFALSE;
						xMailUpdated =  readWifiEventMailbox( &mgWifiEventMail );
						if( xMailUpdated == pdTRUE ){
							if( mgWifiEventMail.ulValue == WIFI_NETWORK_STATUS_DOWN ) {
								printf(" event in mailbox is : %d\n", mgWifiEventMail.ulValue );

							}
							else {
								printf(" mg recv error event mailbox for connection to AP, event in mailbox is: %d\n",mgWifiEventMail.ulValue  );
							}

							//create json root node
							pcjson=cJSON_CreateObject();
							if(NULL == pcjson) {
								printf(" getwifi req process: create root node fail!\n");
							}
							//create json struct for wifi failure
							esp_wifi_handle_http_req_getwifi( pcjson, WIFI_NETWORK_STATUS_DOWN, mgWifiEventMail.reason );
							//send response to client
							pchar = cJSON_Print(pcjson);
							bufLen = strlen(pchar);

							mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
							mg_printf_http_chunk(nc, "%s", pchar);
							mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

							//free buffer
							if(pcjson){
								cJSON_Delete(pcjson);
							}
							if(pchar){
								free(pchar);
								pchar=NULL;
							}

						}
						else {
							printf(" wifievent in mailbox is not updated .\n");
						}
					}
					else {
						printf(" event error in eventgroup .\n");
					}
				}
				else {
					//do sth
					printf(" parse wifi set req error.\n");

				}

			}
			else if( mg_vcmp(&hm->uri, "/wifi/getwifi") == 0) {
				//first get mail event for wifi networkstatus
				//
				WifiEvent_t currentWifiEvent;
				memset((void*)&currentWifiEvent, 0, sizeof(WifiEvent_t));

				//create json root node
				pcjson=cJSON_CreateObject();
				if(NULL == pcjson) {
					printf(" getwifi req process: create root node fail!\n");
				}
				//peek wifiEventMailbox
				if( xQueuePeek( xWifiEventMailbox, &currentWifiEvent, 0 )  ) {
					if( currentWifiEvent.ulValue == WIFI_NETWORK_STATUS_DOWN ) {
						//if network status is not ok
						//
						printf(" Wifi networkstatus is bad.....\n");
						esp_wifi_handle_http_req_getwifi( pcjson, WIFI_NETWORK_STATUS_DOWN,  currentWifiEvent.reason );
					}
					else if( currentWifiEvent.ulValue == WIFI_NETWORK_STATUS_UP ) {
						//if network status is ok
						//
						printf(" Wifi networkstatus is ok......\n");
						esp_wifi_handle_http_req_getwifi( pcjson, WIFI_NETWORK_STATUS_UP, WIFI_REASON_UNSPECIFIED );

					}
					else {//unknown status for network
						printf(" Wifi networkstatus is error........\n");
					}
				}
				else {
					//mail event queue may be empty when init
					//
					printf(" WifiEventMailbox is empty.....\n");
					esp_wifi_handle_http_req_getwifi( pcjson, WIFI_NETWORK_STATUS_UNKNOWN, WIFI_REASON_UNSPECIFIED );

				}

				//send response to client
				pchar = cJSON_Print(pcjson);
				bufLen = strlen(pchar);
				//printf("recv WIFI Get Request, send JSON Response is :   %s, len is : %d\n", pchar, bufLen);

				mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
				mg_printf_http_chunk(nc, "%s", pchar);
				mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

				if(pcjson){
					cJSON_Delete(pcjson);
				}
				if(pchar){
					free(pchar);
					pchar=NULL;
				}


			}
			else if( mg_vcmp(&hm->uri, "/mail/setmail") == 0) {

				char respbuf[200];
				int len = 0;

				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//body process
				memcpy(strbuf, hm->body.p,
					sizeof(strbuf) - 1 < hm->body.len ? sizeof(strbuf) - 1 : hm->body.len);
				printf("%s\n", strbuf);

				//update smartmeter config parameter in memory and save them to NVS
				if( esp_wifi_parse_req_setmail( strbuf,  &meter_config_info ) == ESP_OK ){
					//save config into to NVS
					nvs_save_MeterConfigInfo( &meter_config_info );

					//send http response
					len = sprintf(respbuf, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"ok\" \n }\n }\n  }\n");
					// Send headers first
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", respbuf);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}
				else {
					//parse http request for setmail failed
					printf(" HTTP request for setwifi can not be parsed!\n");
					//send http response
					len = sprintf(respbuf, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"error\" \n }\n }\n  }\n");
					// Send headers first
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", respbuf);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}

			}
			else if( mg_vcmp(&hm->uri, "/mail/getmail") == 0) {

				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//get response and send it
				char response[256];
				int len = 0;
				memset(response, 0, sizeof(response));

				if( esp_wifi_handle_http_req_getmail( response ) == ESP_OK ){
					//send response for getmail req
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", response);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}
				else{
					//handle response fail for getmail req
					len = sprintf(response, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"error\" \n }\n }\n  }\n");
					// Send headers first
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", response);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}

			}
			else if( mg_vcmp(&hm->uri, "/mail/sendmailnow") == 0) {

				char mailboxbuf[100];
				char mailresponse[256];
				int len = 0;
				memset(mailresponse, 0, sizeof(mailresponse));

				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//body process
				memcpy(strbuf, hm->body.p,
					sizeof(strbuf) - 1 < hm->body.len ? sizeof(strbuf) - 1 : hm->body.len);
				printf("%s\n", strbuf);

				memset(mailboxbuf, 0, 100 );
				if( esp_wifi_parse_req_sendmailnow( strbuf, mailboxbuf ) == ESP_OK ){
					//printf(" sendmailnow : mailbox is : %s\n", mailboxbuf );
					if( mail_send_mail( mailboxbuf ) == ESP_OK ){
						len = sprintf(mailresponse, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"ok\" \n }\n }\n  }\n");
						mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
						mg_printf_http_chunk(nc, "%s", mailresponse);
						mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
					}
					else {
						len = sprintf(mailresponse, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"error\" \n }\n }\n  }\n");
						mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
						mg_printf_http_chunk(nc, "%s", mailresponse);
						mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
					}
				}
				else{
					len = sprintf(mailresponse, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"parse error\" \n }\n }\n  }\n");
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", mailresponse);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}


			}
			else if( mg_vcmp(&hm->uri, "/metric/getsnapshotmetrics") == 0) {

				char *pbuf=(char*)malloc(2048);
				memset( pbuf, 0, 2048);

				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//get snapshot metrics from db
				if( esp_wifi_handle_http_req_getsnapshotmetrics( pbuf ) == ESP_OK ){
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", pbuf);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}
				else {
					//send error response
				}

				if( pbuf ){
					free( pbuf );
					pbuf = NULL;
				}


			}
			else if( mg_vcmp(&hm->uri, "/ota/startota") == 0) {
				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP OTA request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//body process
				memcpy(strbuf, hm->body.p,
					sizeof(strbuf) - 1 < hm->body.len ? sizeof(strbuf) - 1 : hm->body.len);
				//printf("%s\n", strbuf);

				//check if we have already got ip from DHCP server before ,if we got ip , disconnect from the current AP
				//
				tcpip_adapter_ip_info_t current_ip_info;
				ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &current_ip_info));
				if (!(ip4_addr_isany_val(current_ip_info.ip) || ip4_addr_isany_val(current_ip_info.netmask) || ip4_addr_isany_val(current_ip_info.gw))) {
						printf("We have got IP address from DHCP Server! So we can begin to ota now !\n");
						//create task for OTA process
						xTaskCreate(&ota_task, "ota_task", 8192, NULL, 9, NULL);
				}

				char respbuf[200];
				int len = 0;

				//send http response
				len = sprintf(respbuf, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"ok\" \n }\n }\n  }\n");
				// Send headers first
				mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
				mg_printf_http_chunk(nc, "%s", respbuf);
				mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */


			}
			else if( mg_vcmp(&hm->uri, "/ota/getpartitioninfo") == 0) {
				//get addr from http client
				mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                         MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
				printf("HTTP Get request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
						hm->method.p, (int) hm->uri.len, hm->uri.p);

				//body process
				memcpy(strbuf, hm->body.p,
					sizeof(strbuf) - 1 < hm->body.len ? sizeof(strbuf) - 1 : hm->body.len);
				//printf("%s\n", strbuf);

				const esp_partition_t *configured = esp_ota_get_boot_partition();
				const esp_partition_t *running = esp_ota_get_running_partition();

				char runningRespbuf[200];
				char configuredRespbuf[200];
				int len = 0;
				char response[200];

				//create json root node
				pcjson=cJSON_CreateObject();
				if(NULL == pcjson) {
					printf(" getpartitioninfo req process: create root node fail!\n");
				}


				sprintf(configuredRespbuf, "Configured partition type %d subtype %d (offset 0x%08x)",
						configured->type, configured->subtype, configured->address);

				sprintf(runningRespbuf, "Running partition type %d subtype %d (offset 0x%08x)",
						running->type, running->subtype, running->address);

				//package get partition info and send it
				if( esp_wifi_handle_http_req_getpartitioninfo( pcjson, runningRespbuf, configuredRespbuf ) == ESP_OK ){
						//send response to client
						pchar = cJSON_Print(pcjson);
						bufLen = strlen(pchar);
						//printf("recv WIFI Get Request, send JSON Response is :   %s, len is : %d\n", pchar, bufLen);

						mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
						mg_printf_http_chunk(nc, "%s", pchar);
						mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */

						//free buffer
						if(pcjson){
							cJSON_Delete(pcjson);
						}
						if(pchar){
							free(pchar);
							pchar=NULL;
						}
				}
				else {
					len = sprintf(response, "{\n \"Response\": {\n \"Status\": {\n \"status\": \"get partition info error\" \n }\n }\n  }\n");
					mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/json\r\nTransfer-Encoding: chunked\r\n\r\n");
					mg_printf_http_chunk(nc, "%s", response);
					mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
				}


			}
			else {
				//mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
			}
			break;

		}// MG_EV_HTTP_REQUEST

	}
}

/*******************************************************************************
*  FreeRTOS task to start Mongoose
*
********************************************************************************/

static void mongooseTask(void *data) {
	struct mg_mgr mgr;
	struct mg_connection *connection;
	struct mg_bind_opts bind_opts;


	ESP_LOGD(tag, ">> mongooseTask");
	g_mongooseStopRequest = 0; // Unset the stop request since we are being asked to start.

	mg_mgr_init(&mgr, NULL);

	connection = mg_bind(&mgr, ":80", mongoose_event_handler);

	if (connection == NULL) {
		ESP_LOGE(tag, "No connection from the mg_bind().");
		mg_mgr_free(&mgr);
		ESP_LOGD(tag, "<< mongooseTask");
		vTaskDelete(NULL);
		return;
	}
	mg_set_protocol_http_websocket(connection);

	// Keep processing until we are flagged that there is a stop request
	//
	while (!g_mongooseStopRequest) {
		mg_mgr_poll(&mgr, 1000);
	}

	// We have received a stop request, so stop being a web server
	//
	mg_mgr_free(&mgr);
	g_mongooseStarted = 0;


	ESP_LOGD(tag, "<< mongooseTask");
	vTaskDelete(NULL);
	return;
} // mongooseTask


/*******************************************************************************
*  FreeRTOS task to start wifiMonitor
*
********************************************************************************/
static void wifiMonitorTask(void *data)
{
	uint8_t waitRetryCnt = 0;
	WifiEvent_t wifiEvent;
	memset((void*)&wifiEvent, 0 ,sizeof(WifiEvent_t));
	wifi_config_t sta_conf;
	char apSSID[32];
	TickType_t xPreviousTimeStamp = 0;

	while(1){
		if( xQueuePeek( xWifiEventMailbox, &wifiEvent, 0 ) ) {
			printf("wifiMonitorTask: find wifievent in mailbox,event is :%d\n",wifiEvent.ulValue);
			if( wifiEvent.ulValue == WIFI_NETWORK_STATUS_DOWN ){
				if( xPreviousTimeStamp == 0 || wifiEvent.xTimeStamp > xPreviousTimeStamp )  {
					xPreviousTimeStamp = wifiEvent.xTimeStamp;
					waitRetryCnt = 0;
					printf("first or new wifi event,so reset retry count for next reconnection.\n");
				}
				else {
					waitRetryCnt++;
					//after 30secs, reconnect according to wifi station config
					if( waitRetryCnt > 6 ){
						waitRetryCnt = 0;
						if( esp_wifi_get_config( WIFI_IF_STA, &sta_conf ) == ESP_OK ){
							wifi_sta_config_t *pSta_conf = &sta_conf.sta;
							printf("try to reconnect , ssid :%s, password:%s\n", pSta_conf->ssid, pSta_conf->password );

							sprintf( apSSID, "%s",  pSta_conf->ssid );
							if( strlen( apSSID ) > 0 ){
								//reconnect AP
								ESP_ERROR_CHECK(esp_wifi_connect());
							}
						}
					}
				}
			}
			else{//UP event
				if( waitRetryCnt > 0 )
					waitRetryCnt = 0;
			}
		}
		//printf("__________ wifiMonitorTask delay 5secs_______________\n");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

}


/**
 * An ESP32 WiFi event handler.
 * The types of events that can be received here are:
 *
 * SYSTEM_EVENT_AP_PROBEREQRECVED
 * SYSTEM_EVENT_AP_STACONNECTED
 * SYSTEM_EVENT_AP_STADISCONNECTED
 * SYSTEM_EVENT_AP_START
 * SYSTEM_EVENT_AP_STOP
 * SYSTEM_EVENT_SCAN_DONE
 * SYSTEM_EVENT_STA_AUTHMODE_CHANGE
 * SYSTEM_EVENT_STA_CONNECTED
 * SYSTEM_EVENT_STA_DISCONNECTED
 * SYSTEM_EVENT_STA_GOT_IP
 * SYSTEM_EVENT_STA_START
 * SYSTEM_EVENT_STA_STOP
 * SYSTEM_EVENT_WIFI_READY
 */
/*******************************************************************************
*  Global define
*
********************************************************************************/
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
// Your event handling code here...
	switch(event->event_id) {
		// When we have started being an access point, then start being a web server.
		//
		case SYSTEM_EVENT_AP_START: { // Handle the AP start event

			tcpip_adapter_ip_info_t ip_info;
			tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
			ESP_LOGD(tag, "**********************************************");
			ESP_LOGD(tag, "* We are now an access point and you can point")
			ESP_LOGD(tag, "* your browser to http://" IPSTR, IP2STR(&ip_info.ip));
			ESP_LOGD(tag, "**********************************************");
			// Start Mongoose as webserver
			//
			if (!g_mongooseStarted)
			{
				g_mongooseStarted = 1;
				//task priority is set 9
				//
				xTaskCreatePinnedToCore(&mongooseTask, "bootwifi_mongoose_task", 8000, NULL, 9, NULL, 0);
			}

			//set selfAccesPointOk flag true
			//
			if( !selfAccessPointOk )
			{
				selfAccessPointOk = 1;
			}

			break;
		} // SYSTEM_EVENT_AP_START

		case  SYSTEM_EVENT_AP_STOP : { //Handle the Ap stop event
			ESP_LOGD(tag, "We receive AP_STOP event, so we are now not as an access point.");

			// Stop mongoose (if it is running).
			//
			g_mongooseStopRequest = 1;

			//reset selfAccessPointOk flag to false
			//
			selfAccessPointOk = 0;

			break;
			}

		case SYSTEM_EVENT_STA_START : {
			ESP_LOGD(tag, "Station connection to AP started.");
			break;

			}

		case SYSTEM_EVENT_STA_STOP: {
			ESP_LOGD(tag, "Station connection to AP stoped.");
			break;
			}


		//if we connect to an access point as a station
		//
		case SYSTEM_EVENT_STA_CONNECTED : {
			ESP_LOGD(tag, "Station connect to AP successful.");

			break;
			}

		// If we fail to connect to an access point as a station
	    //
		case SYSTEM_EVENT_STA_DISCONNECTED: {
			ESP_LOGD(tag, "Station disconnected .");

			//send mailbox for disconnected event
			//
			system_event_sta_disconnected_t *disconnected = &event->event_info.disconnected;
			updateWifiEventMailbox( WIFI_NETWORK_STATUS_DOWN, disconnected->reason );


			//We lost connection to AP,so clear wifi event group for connection
			//
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

			//set DISCONNECTED flag in eventgroup
			//
			xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);


			//I need use other task to monitor network status and reconnect or change other AP
			//so current wifi networkstatus can be read from WifiEventMailbox
			//
			/*
			//reconnect
			//
			ESP_LOGI(tag, "Wifi disconnected, try to connect ...");
			esp_wifi_connect();
			*/

			break;
		}

		// If we connected as a station
		//
		case SYSTEM_EVENT_STA_GOT_IP: {
			ESP_LOGD(tag, "********************************************");
			ESP_LOGD(tag, "* We are now connected and ready to do work!")
			ESP_LOGD(tag, "* - Our IP address is: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
			ESP_LOGD(tag, "********************************************");

			//initialize sntp for time
			//
			if( sntp_enabled() == 0){
				ESP_LOGD(tag, "start SNTP for time....");
				initialize_sntp();
			}
			else {
				ESP_LOGD(tag, "SNTP have already started,so wait....");
			}


			//send mailbox for connected event
			//
			updateWifiEventMailbox( WIFI_NETWORK_STATUS_UP, WIFI_REASON_UNSPECIFIED );

			//clear DISCONNECTED flag in eventgroup
			//
			xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
			//We got IP,so set wifi event group bit for connection
			//
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

			//can we need to reconnect here ?How esp32 do reconnection???
			//


			break;
		} // SYSTEM_EVENT_STA_GOTIP

		case SYSTEM_EVENT_STA_AUTHMODE_CHANGE: {
			ESP_LOGD(tag, "Station connection to AP authmode changed.");
			break;
		}

		case SYSTEM_EVENT_WIFI_READY: {
			ESP_LOGD(tag, "WIFI ready.");
			break;
		}

		case SYSTEM_EVENT_SCAN_DONE: {

			printf("Number of access points found: %d\n",event->event_info.scan_done.number);
			uint16_t apCount = event->event_info.scan_done.number;
			if (apCount == 0) {
				return ESP_OK;
			}
			wifi_ap_record_t *list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
			ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
			int i;
			for (i=0; i<apCount; i++) {
				char *authmode;
				switch(list[i].authmode) {
					case WIFI_AUTH_OPEN:
						authmode = "WIFI_AUTH_OPEN";
						break;

					case WIFI_AUTH_WEP:
						authmode = "WIFI_AUTH_WEP";
						break;

					case WIFI_AUTH_WPA_PSK:
						authmode = "WIFI_AUTH_WPA_PSK";
						break;

					case WIFI_AUTH_WPA2_PSK:
						authmode = "WIFI_AUTH_WPA2_PSK";
						break;

					case WIFI_AUTH_WPA_WPA2_PSK:
						authmode = "WIFI_AUTH_WPA_WPA2_PSK";
						break;

					default:
						authmode = "Unknown";
						break;
				}
				printf("ssid=%s, rssi=%d, authmode=%s\n",list[i].ssid, list[i].rssi, authmode);
			}
			free(list);

			break;
		}

		default: // Ignore the other event types
			ESP_LOGD(tag, "Wifi other evety tpyes?.");
			break;
	} // Switch event

	return ESP_OK;
}


/*******************************************************************************
*  Global define
*
********************************************************************************/
static void wifi_conn_init(void)
{
	//create wifi eventgroup and init mgWifiEvent
	//
	wifi_event_group = xEventGroupCreate();
	if( wifi_event_group == NULL ) {
		//create wifi eventgroup fail
		//
		ESP_LOGD(tag, "- Create wifi eventgroup fail.");
		return;
	}
	wifi_event_group_mux = xSemaphoreCreateMutex();
	if (!wifi_event_group_mux) {
        return ESP_ERR_NO_MEM;
    }
	memset((void*)&mgWifiEventMail, 0, sizeof(WifiEvent_t));

	//Create the queue that is going to be used as a mailbox. The queue has a
	//length of 1 to allow it to be used with the xQueueOverwrite() API function.
	xWifiEventMailbox = xQueueCreate( 1, sizeof( WifiEvent_t ) );
	if( !xWifiEventMailbox ){
		ESP_LOGD(tag, "- Create wifi event mailbox fail.");
		return;
	}

    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

	//set wifi storage type,now we use flash type
	//
	//ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

	//set auto connect to be true
	//
	//ESP_ERROR_CHECK(esp_wifi_set_auto_connect(true));

	ESP_LOGD(tag, "- Starting being an access point in STA+AP mode ...");
	// We are always in STA+AP mode,so set AP and station config
	//
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

	//we want to test scan func

	//AP config,first we set default ssid
	//
	wifi_config_t apConfig = {
		.ap = {
			.ssid="LOMA",
			.ssid_len=0,
			.password="123456",
			.channel=0,
			.authmode=WIFI_AUTH_OPEN,
			.ssid_hidden=0,
			.max_connection=4,
			.beacon_interval=100
		}
	};
	esp_wifi_set_ap_ssid( &apConfig );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfig));

	//close STATION config
	/*
	//STA config
	//
	wifi_config_t sta_config = {
    .sta = {
      .ssid      = AP_TARGET_SSID,
      .password  = AP_TARGET_PASSWORD,
      .bssid_set = 0
		}
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	*/

	//start wifi module
	//
	ESP_ERROR_CHECK(esp_wifi_start());
	printf("wifi_start\n");
	//try to connect to AP if use FLASH storage
	//
	wifi_config_t sta_conf;
	char apSSID[32];
	if( esp_wifi_get_config( WIFI_IF_STA, &sta_conf ) == ESP_OK ){
			wifi_sta_config_t *pSta_conf = &sta_conf.sta;
			printf("try to connect , ssid :%s, password:%s\n", pSta_conf->ssid, pSta_conf->password );

			sprintf( apSSID, "%s",  pSta_conf->ssid );
			if( strlen( apSSID ) > 0 ){
				ESP_ERROR_CHECK(esp_wifi_connect());
			}
	}
	//set timezone env
	//
	setenv("TZ", "GMT+8GMT+7,M3.2.0/2,M11.1.0/2", 1);
	tzset();

	//start wifi monitor task
	xTaskCreatePinnedToCore(&wifiMonitorTask, "wifiMonitorTask", 2048, NULL, 5, NULL,0);

	// Let us test a WiFi scan
	//
	/*
	wifi_scan_config_t scanConf = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = 1
	};
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 0));
	*/
}


/*******************************************************************************
*  Global define
*
********************************************************************************/
void nvs_flash_load_config()
{
	if( nvs_get_MeterConfigInfo( &meter_config_info ) == -1 ){
		printf("----> NVS get meter config to init failed.\n");
	}
	else {
		printf("----> NVS get meter config to init successful.\n");
	}
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
void nvs_flash_load_meter_calibration_para()
{
	if( nvs_get_MeterCalibrationPara( &meterCalibrationPara ) == -1 ) {
		printf(" ----> NVS get meter calibration parameters failed.\n");
	}
	else{
		printf(" ----->NVS get meter calibration parameters successful.\n");
	}
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
void nvs_flash_load_meter_active_energy()
{
	if( nvs_get_MeterActiveEnergy(  ) == -1 ){
		printf("----> NVS get meter active energy to init failed.\n");
	}
	else {
		printf("----> NVS get meter active energy to init successful.\n");
	}
}


/*******************************************************************************
*  Global define
*
********************************************************************************/
void app_main(void)
{
	//esp_log_level_set(tag,ESP_LOG_INFO);

	ESP_LOGD(tag,">> bootwifi");
	printf("-----bootwifi----\n");
	//nvs module init
	//
	nvs_flash_init();						//init nvs flash
	nvs_flash_load_config();				//load meter config form nvs flash
	nvs_flash_load_meter_calibration_para();//load calibration parameter from NVS here
	//nvs_flash_load_meter_active_energy();	//load energy pulse count and value here


    tcpip_adapter_init();					//tcpip adapter init
	wifi_conn_init();						//wifi connect module init
	mail_init();							//mail module init

	//only after ESP32 AP is ok, we can init and start AFE module
	while( !selfAccessPointOk ) {
        ESP_LOGD(tag, ".......AFE module wait for wifi status.......");
        vTaskDelay(1000 / portTICK_RATE_MS);
    }

	//AFE module init
	if( selfAccessPointOk == 1 ){
		ESP_LOGD(tag, ".......After wifi init, AFE module wait for 5 secs before init cs5480.......");
		vTaskDelay(5000 / portTICK_RATE_MS);
		afe_init_meter_snapshotdb();
		afe_comm_init();
	}

	ESP_LOGD(tag, "<< bootwifi");
	vTaskDelete(NULL);
}

