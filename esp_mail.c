
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <tcpip_adapter.h>
#include <apps/sntp/sntp.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "esp_smtp.h"
#include "esp_mail.h"
#include "esp_afe.h"

#include "esp_platform.h"
#include "esp_wifi_func.h"

static char tag[] = "sntpmail";

//email server 
#define EMAIL_SERVER_ADDRESS         "xxxxxxxx"
//clientmail timer period
//
#define CLIENTMAIL_TIMER_PERIOD pdMS_TO_TICKS( 300000 )

static xQueueHandle MailQueueStop = NULL;
extern int sntp_started;
//client mail queue
static xQueueHandle ClientMailQueueStop = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////
// server mail callback function                                                             //
//////////////////////////////////////////////////////////////////////////////////////////////
void smtp_mail_meter_data_callback_fn(void *arg, u8_t smtp_result, u16_t srv_err, err_t err)
 {
    printf("interval snapshot data mail (%p) sent with results: 0x%02x, 0x%04x, 0x%08x\n", arg,
           smtp_result, srv_err, err);
	
  }


///////////////////////////////////////////////////////////////////////////////////////////////
// client mail callback function                                                             //
//////////////////////////////////////////////////////////////////////////////////////////////
void client_mail_meter_data_callback_fn(void *arg, u8_t smtp_result, u16_t srv_err, err_t err)
 {
    printf("client interval snapshot data mail (%p) sent with results: 0x%02x, 0x%04x, 0x%08x\n", arg,
           smtp_result, srv_err, err);

 }



void mail_sendmail_test_func()
{
	char *pbuf = (char*)malloc(512);
	char *pSubject = (char*)malloc(256);
	char *pMailbox = (char*)malloc(256);
	char measureValueStr[64];

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[64];
	esp_err_t err;

	//sprintf( pMailbox, "%s", "junzha@msn.com");
	sprintf( pMailbox, "%s", "heibel.john@gmail.com");

	//mail subject, get now time if sntp time is OK
	if( sntp_enabled() == 1 && sntp_started == 1 ) {
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			printf(" Correct IP for mail and sntp time ok, so we can send mail.\n" );
			strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
			printf("The current date/time is: %s\n", strftime_buf);
		}
		else {
			printf(" get time from sntp is erro, so it cannot send mail now.\n");
			//release memory
			if( pbuf ) { free(pbuf); pbuf=NULL; }
			if( pSubject ) { free( pSubject ); pSubject = NULL; }
			if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
			err = ESP_FAIL; 
			return err;
		}
	}
	else {
		//if sntp time is not ok, so donot send mail
		printf(" sntp time is not ok, so donot send mail now.\n");
		//release memory
		if( pbuf ) { free(pbuf); pbuf=NULL; }
		if( pSubject ) { free( pSubject ); pSubject = NULL; }
		if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
		err = ESP_FAIL; 
		return err;
	}


	sprintf( pSubject, "%s", "Meter:LOMA00001");
	strcat( pSubject, "@DateTimestamp:");
	strcat( pSubject, strftime_buf );
	strcat( pSubject, "#SnapshotValues");

	
	memset((void*)pbuf, 0, 512 );

	//Voltage
	sprintf( pbuf, "%s", "Voltage:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Vr.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Vr.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "Current:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePower:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePower:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");


	//PowerFactor
	strcat( pbuf, "PowerFactor:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "Frequency:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "Temperature:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, "|");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	//strcat( pbuf, ",");



	printf(" mail buffer is : %s\n", pbuf );

	smtp_set_server_addr( "157.22.33.37"); 
	smtp_set_auth("Admin", "Cisco123");
	printf("----->smtp rcpt mailbox : %s\n",pMailbox );

	smtp_send_mail("lomameter@mydomain.com", pMailbox, pSubject, pbuf, (smtp_result_fn)smtp_mail_meter_data_callback_fn, NULL);

	//release memory
	//
	if(pbuf){
		free(pbuf);
		pbuf=NULL;
	}

	if( pSubject ){
		free( pSubject);
		pSubject = NULL;
	}

	if( pMailbox ) {
		free( pMailbox );
		pMailbox = NULL;
	}

}

/*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t mail_send_servermail_to_snapshot()
{
	char *pbuf = (char*)malloc(2048);
	char *pSubject = (char*)malloc(256);
	char *pMailbox = (char*)malloc(256);
	char measureValueStr[64];

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[64];
	esp_err_t err;

	//server rcptmailbox 
	//
	//sprintf( pMailbox, "%s", "shendongyan@msn.com");
	//sprintf( pMailbox, "%s", "heibel.john@gmail.com");
	sprintf( pMailbox, "%s", "gccmeterDataServer@mydomain.com" );


	//mail subject, first get now time if sntp time is OK
	//
	if( sntp_enabled() == 1 ) {
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			printf(" Correct IP for mail and sntp time ok, so we can send mail.\n" );
			//strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:00", &timeinfo);
			printf("The current date/time is: %s\n", strftime_buf);
		}
		else {
			printf(" get time from sntp is erro, so it cannot send mail now.\n");
			//release memory
			if( pbuf ) { free(pbuf); pbuf=NULL; }
			if( pSubject ) { free( pSubject ); pSubject = NULL; }
			if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
			err = ESP_FAIL; 
			return err;
		}
	}
	else {
		//if sntp time is not ok, so dontot send mail
		printf(" sntp time is not ok, so donot send mail now.\n");
		//release memory
		if( pbuf ) { free(pbuf); pbuf=NULL; }
		if( pSubject ) { free( pSubject ); pSubject = NULL; }
		if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
		err = ESP_FAIL; 
		return err;
	}
	//construct mail subject
	//
	sprintf( pSubject, "%s", "Meter:");
	if( strlen( meterSnapshotDb.meterSN ) > 0 ){
		strcat( pSubject, meterSnapshotDb.meterSN );
		printf("----Subject---%s---%s---%d\n",pSubject, meterSnapshotDb.meterSN,strlen( meterSnapshotDb.meterSN )) ;
		
	}
	else{
		strcat( pSubject, "LOMADefaultMeter" );
	}
	strcat( pSubject, "@DateTimestamp:");
	strcat( pSubject, strftime_buf );
	strcat( pSubject, "#SnapshotValues");

	//init mail body buffer
	//
	memset((void*)pbuf, 0, 2048 );

	//mail body
	//
	//MeterSN needed 
	sprintf( pbuf, "%s", "MeterSN:");
	if( strlen( meterSnapshotDb.meterSN ) > 0 ){
		strcat( pbuf, meterSnapshotDb.meterSN );
		printf("----pbuf---%s---%s---%d\n",pbuf, meterSnapshotDb.meterSN,strlen( meterSnapshotDb.meterSN )) ;
	}
	else{
		strcat( pbuf, "LOMADefaultMeter" );
	}
	strcat( pbuf, ",");

	//Time needed
	strcat( pbuf, "Time:");
	strcat( pbuf, strftime_buf );
	strcat( pbuf, ",");

	//////////////////PhaseA /////////////////////////
	//Status
	strcat( pbuf, "StatusA:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentA:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//////////////// PhaseB///////////////////////
	//Status
	strcat( pbuf, "StatusB:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentB:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//////////////// PhaseC///////////////////////
	//Status
	strcat( pbuf, "StatusC:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentC:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	printf(" mail buffer is : %s, len is : %d\n", pbuf, strlen(pbuf) );


	//send mail to mail server
	//
	smtp_set_server_addr( "157.22.33.37"); 
	smtp_set_auth("Admin", "Cisco123");
	printf("----->smtp rcpt mailbox : %s,  subject: %s\n",pMailbox, pSubject );

	smtp_send_mail("lomameter@mydomain.com", pMailbox, pSubject, pbuf, (smtp_result_fn)smtp_mail_meter_data_callback_fn, NULL);

	

	//release memory
	//
	if(pbuf){
		free(pbuf);
		pbuf=NULL;
	}

	if( pSubject ){
		free( pSubject);
		pSubject = NULL;
	}

	if( pMailbox ) {
		free( pMailbox );
		pMailbox = NULL;
	}

	err = ESP_OK;
	return err;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t mail_send_mail( const char * pDestMailbox )
{
	char *pbuf = (char*)malloc(2048);
	char *pSubject = (char*)malloc(256);
	char *pMailbox = (char*)malloc(256);
	char measureValueStr[64];

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[64];
	esp_err_t err;

	if( pDestMailbox == NULL ) {
		return ESP_FAIL;
	}

	//client rcptmailbox 
	//
	sprintf( pMailbox, "%s", pDestMailbox );

	//mail subject, first get now time if sntp time is OK
	if( sntp_enabled() == 1 ) {
		time(&now);
		localtime_r(&now, &timeinfo);
		if (timeinfo.tm_year > (2016 - 1900)) {
			printf(" ClientMail: Correct IP for mail and sntp time ok, so we can send client mail.\n" );
			strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:00", &timeinfo);
			printf("The current date/time is: %s\n", strftime_buf);
		}
		else {
			printf(" get time from sntp is erro, so it cannot send client mail now.\n");
			//release memory
			if( pbuf ) { free(pbuf); pbuf=NULL; }
			if( pSubject ) { free( pSubject ); pSubject = NULL; }
			if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
			err = ESP_FAIL; 
			return err;
		}
	}
	else {
		//if sntp time is not ok, so dontot send mail
		printf(" sntp time is not ok, so donot send client mail now.\n");
		//release memory
		if( pbuf ) { free(pbuf); pbuf=NULL; }
		if( pSubject ) { free( pSubject ); pSubject = NULL; }
		if( pMailbox ) { free( pMailbox ); pMailbox = NULL; }
		err = ESP_FAIL; 
		return err;
	}
	//construct mail subject
	//
	sprintf( pSubject, "%s", "Meter:");
	if( strlen( meterSnapshotDb.meterSN ) > 0 ){
		strcat( pSubject, meterSnapshotDb.meterSN );
	}
	else{
		strcat( pSubject, "LOMADefaultMeter" );
	}
	strcat( pSubject, "@DateTimestamp:");
	strcat( pSubject, strftime_buf );
	strcat( pSubject, "#SnapshotValues");

	//init mail body buffer
	//
	memset((void*)pbuf, 0, 2048 );

	//mail body
	//
	//MeterSN needed 
	sprintf( pbuf, "%s", "MeterSN:");
	if( strlen( meterSnapshotDb.meterSN ) > 0 ){
		strcat( pbuf, meterSnapshotDb.meterSN );
	}
	else{
		strcat( pbuf, "LOMADefaultMeter" );
	}
	strcat( pbuf, ",");

	//Time needed
	strcat( pbuf, "Time:");
	strcat( pbuf, strftime_buf );
	strcat( pbuf, ",");

	//////////////////PhaseA /////////////////////////
	//Status
	strcat( pbuf, "StatusA:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentA:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyA:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseA.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//////////////// PhaseB///////////////////////
	//Status
	strcat( pbuf, "StatusB:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentB:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyB:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseB.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//////////////// PhaseC///////////////////////
	//Status
	strcat( pbuf, "StatusC:HH");
	strcat( pbuf, ",");

	//Voltage
	strcat( pbuf, "VoltageC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Vr.measurementValue );
	strcat( pbuf,  measureValueStr );
	strcat( pbuf, ",");

	//Current
	strcat( pbuf, "CurrentC:" );
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ir.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//PowerFactor
	strcat( pbuf, "PowerFactorC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Pf.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Temperature
	strcat( pbuf, "TemperatureC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Tc.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Forward
	strcat( pbuf, "ActiveEnergyFC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ae.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Energy kWh Reverse
	strcat( pbuf, "ActiveEnergyRC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseAe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ActivePower
	strcat( pbuf, "ActivePowerC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ap.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Active Demand
	strcat( pbuf, "ActiveDemandC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Ad.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Capacitive
	strcat( pbuf, "ReactiveEnergyCC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Re.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Energy kVARh Inductive
	strcat( pbuf, "ReactiveEnergyIC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.ReverseRe.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//ReactivePower
	strcat( pbuf, "ReactivePowerC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Rp.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Reactive Demand
	strcat( pbuf, "ReactiveDemandC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Rd.measurementValue*1000.0 );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	//Frequency
	strcat( pbuf, "FrequencyC:");
	sprintf( measureValueStr, "%f", meterSnapshotDb.phaseC.Hz.measurementValue );
	strcat( pbuf, measureValueStr );
	strcat( pbuf, ",");

	printf("client mail buffer is : %s, len is : %d\n", pbuf, strlen(pbuf) );


	//send mail to mail server
	//
	smtp_set_server_addr( "157.22.33.37"); 
	smtp_set_auth("Admin", "Cisco123");
	printf("----->client rcpt mailbox : %s,  subject: %s\n",pMailbox, pSubject );

	smtp_send_mail("lomameter@mydomain.com", pMailbox, pSubject, pbuf, (smtp_result_fn)client_mail_meter_data_callback_fn, NULL);

	//release memory
	//
	if(pbuf){
		free(pbuf);
		pbuf=NULL;
	}

	if( pSubject ){
		free( pSubject);
		pSubject = NULL;
	}

	if( pMailbox ) {
		free( pMailbox );
		pMailbox = NULL;
	}

	err = ESP_OK;
	return err;

}
/*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t mail_handle_max_demand()
{
	float maxAeDemandVal = 0.00;
	float maxReDemandVal = 0.00;


	printf("................ handle max demand ...............\n");

	//phase A max demand process
	//
		printf("A: P last = %f, now = %f\n", meterSnapshotDb.phaseA.LastQuarterAe.measurementValue, meterSnapshotDb.phaseA.Ae.measurementValue);
		printf("A: Q last = %f, now = %f\n", meterSnapshotDb.phaseA.LastQuarterRe.measurementValue, meterSnapshotDb.phaseA.Re.measurementValue);
		maxAeDemandVal = ( meterSnapshotDb.phaseA.Ae.measurementValue - meterSnapshotDb.phaseA.LastQuarterAe.measurementValue) * 4;
		maxReDemandVal = ( meterSnapshotDb.phaseA.Re.measurementValue - meterSnapshotDb.phaseA.LastQuarterRe.measurementValue) * 4;
		if( maxAeDemandVal > meterSnapshotDb.phaseA.Ad.measurementValue ){
			meterSnapshotDb.phaseA.Ad.measurementValue = maxAeDemandVal;
		}
		if( maxReDemandVal > meterSnapshotDb.phaseA.Rd.measurementValue ){
			meterSnapshotDb.phaseA.Rd.measurementValue = maxReDemandVal;
		}
	printf("A: P Current Demand = %f, Max Demand = %f\n", maxAeDemandVal, meterSnapshotDb.phaseA.Ad.measurementValue);
	printf("A: Q Current Demand = %f, Max Demand = %f\n", maxReDemandVal, meterSnapshotDb.phaseA.Rd.measurementValue);
	meterSnapshotDb.phaseA.LastQuarterAe.measurementValue = meterSnapshotDb.phaseA.Ae.measurementValue;
	meterSnapshotDb.phaseA.LastQuarterRe.measurementValue = meterSnapshotDb.phaseA.Re.measurementValue;

	//phase B max demand process
	//
		printf("B: P last = %f, now = %f\n", meterSnapshotDb.phaseB.LastQuarterAe.measurementValue, meterSnapshotDb.phaseB.Ae.measurementValue);
		printf("B: Q last = %f, now = %f\n", meterSnapshotDb.phaseB.LastQuarterRe.measurementValue, meterSnapshotDb.phaseB.Re.measurementValue);
		maxAeDemandVal = ( meterSnapshotDb.phaseB.Ae.measurementValue - meterSnapshotDb.phaseB.LastQuarterAe.measurementValue) * 4;
		maxReDemandVal = ( meterSnapshotDb.phaseB.Re.measurementValue - meterSnapshotDb.phaseB.LastQuarterRe.measurementValue) * 4;
		if( maxAeDemandVal > meterSnapshotDb.phaseB.Ad.measurementValue ){
			meterSnapshotDb.phaseB.Ad.measurementValue = maxAeDemandVal;
		}
		if( maxReDemandVal > meterSnapshotDb.phaseB.Rd.measurementValue ){
			meterSnapshotDb.phaseB.Rd.measurementValue = maxReDemandVal;
		}
	printf("B: P Current Demand = %f, Max Demand = %f\n", maxAeDemandVal, meterSnapshotDb.phaseB.Ad.measurementValue);
	printf("B: Q Current Demand = %f, Max Demand = %f\n", maxReDemandVal, meterSnapshotDb.phaseB.Rd.measurementValue);
	meterSnapshotDb.phaseB.LastQuarterAe.measurementValue = meterSnapshotDb.phaseB.Ae.measurementValue;
	meterSnapshotDb.phaseB.LastQuarterRe.measurementValue = meterSnapshotDb.phaseB.Re.measurementValue;

	//phase C max demand process
	//
		printf("C: P last = %f, now = %f\n", meterSnapshotDb.phaseC.LastQuarterAe.measurementValue, meterSnapshotDb.phaseC.Ae.measurementValue);
		printf("C: Q last = %f, now = %f\n", meterSnapshotDb.phaseC.LastQuarterRe.measurementValue, meterSnapshotDb.phaseC.Re.measurementValue);
		maxAeDemandVal = ( meterSnapshotDb.phaseC.Ae.measurementValue - meterSnapshotDb.phaseC.LastQuarterAe.measurementValue) * 4;
		maxReDemandVal = ( meterSnapshotDb.phaseC.Re.measurementValue - meterSnapshotDb.phaseC.LastQuarterRe.measurementValue) * 4;
		if( maxAeDemandVal > meterSnapshotDb.phaseC.Ad.measurementValue ){
			meterSnapshotDb.phaseC.Ad.measurementValue = maxAeDemandVal;
		}
		if( maxReDemandVal > meterSnapshotDb.phaseC.Rd.measurementValue ){
			meterSnapshotDb.phaseC.Rd.measurementValue = maxReDemandVal;
		}
	printf("C: P Current Demand = %f, Max Demand = %f\n", maxAeDemandVal, meterSnapshotDb.phaseC.Ad.measurementValue);
	printf("C: Q Current Demand = %f, Max Demand = %f\n", maxReDemandVal, meterSnapshotDb.phaseC.Rd.measurementValue);
	meterSnapshotDb.phaseC.LastQuarterAe.measurementValue = meterSnapshotDb.phaseC.Ae.measurementValue;
	meterSnapshotDb.phaseC.LastQuarterRe.measurementValue = meterSnapshotDb.phaseC.Re.measurementValue;


	return ESP_OK;
}

/*******************************************************************************
*  Global define
*    分 - 取值区间为[0,59] 
*    时 - 取值区间为[0,23]
*    0时0分 - 索引为0
********************************************************************************/
int getHistoryEnergyIndexByTime(  int tm_min,  int tm_hour )     
{
	int idx = tm_hour*12 + tm_min/5 ;

	return idx;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t mail_handle_energy_flash( struct tm * pTimeInfo )
{
	printf("................ handle meter energy flash ...............\n");
	char epcIdx[20];
	char strIndex[10];

	//open nvs for meter enrgy
	nvs_handle handle;
	size_t size;
	esp_err_t err;
	int idx = 0;
	int32_t energyPulseCounter = 0; // value will default to 0, if not set yet in NVS

	err = nvs_open("meterenergyns", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGI(tag, "nvs_open: %x for meter energy", err);
		nvs_close(handle);
		return -1;
	}
	else{
		ESP_LOGI(tag, "nvs_open for meter energy OK!");

		//get index by use if datetime
		idx = getHistoryEnergyIndexByTime( pTimeInfo->tm_min, pTimeInfo->tm_hour );
		
		sprintf( strIndex, "%d", idx );

		
		//PhaseA active energy
		sprintf( epcIdx, "Ae%s", strIndex );
		energyPulseCounter = meterEPC.PhaseAEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set phaseA energy Failed!\n" : "Set phaseA energy Done\n");

		//ReverseAe
		sprintf( epcIdx, "ReverseAe%s", strIndex );
		energyPulseCounter = meterEPC.ReversePhaseAEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set ReverseAe Failed!\n" : "Set ReverseAe Done\n");

		//CapReA
		sprintf( epcIdx, "CapReA%s", strIndex );
		energyPulseCounter = meterEPC.CapacitiveRePhaseAEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set CapReA Failed!\n" : "Set CapReA Done\n");
		
		//IndReA
		sprintf( epcIdx, "IndReA%s", strIndex );
		energyPulseCounter = meterEPC.InductiveRePhaseAEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set IndReA Failed!\n" : "Set IndReA Done\n");




		//PhaseB active energy
		sprintf( epcIdx, "Be%s", strIndex );
		energyPulseCounter = meterEPC.PhaseBEPC;
		
		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set phaseB energy Failed!\n" : "Set phaseB energy Done\n");

		//ReverseBe
		sprintf( epcIdx, "ReverseBe%s", strIndex );
		energyPulseCounter = meterEPC.ReversePhaseBEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set ReverseBe Failed!\n" : "Set ReverseBe Done\n");

		//CapReB
		sprintf( epcIdx, "CapReB%s", strIndex );
		energyPulseCounter = meterEPC.CapacitiveRePhaseBEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set CapReB Failed!\n" : "Set CapReB Done\n");
		
		//IndReB
		sprintf( epcIdx, "IndReB%s", strIndex );
		energyPulseCounter = meterEPC.InductiveRePhaseBEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set IndReB Failed!\n" : "Set IndReB Done\n");




		//PhaseC active energy
		sprintf( epcIdx, "Ce%s", strIndex );
		energyPulseCounter = meterEPC.PhaseCEPC;
		
		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set phaseC energy Failed!\n" : "Set phaseC energy Done\n");

		//ReverseCe
		sprintf( epcIdx, "ReverseCe%s", strIndex );
		energyPulseCounter = meterEPC.ReversePhaseCEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set ReverseCe Failed!\n" : "Set ReverseCe Done\n");

		//CapReC
		sprintf( epcIdx, "CapReC%s", strIndex );
		energyPulseCounter = meterEPC.CapacitiveRePhaseCEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set CapReC Failed!\n" : "Set CapReC Done\n");
		
		//IndReC
		sprintf( epcIdx, "IndReC%s", strIndex );
		energyPulseCounter = meterEPC.InductiveRePhaseCEPC;

		err = nvs_set_i32(handle, epcIdx , energyPulseCounter);
		printf((err != ESP_OK) ? "Set IndReC Failed!\n" : "Set IndReC Done\n");




		//commit update for meter energy
		printf("Committing updates in NVS for meter energy... ");
        err = nvs_commit(handle);
        printf((err != ESP_OK) ? "Commit Failed!\n" : "Commit Done\n");

	}


	// Cleanup nvs handle
	//
	nvs_close(handle);

	return ESP_OK;
}


/*******************************************************************************
*  Global define
*
********************************************************************************/
static void prvClientMailTimerCallback( TimerHandle_t xTimer )
{
	char rcptmailbox[32];
	memset( rcptmailbox, 0 ,sizeof( rcptmailbox ));
	uint8_t clientmail_enable_val = 0;
	uint8_t clientmail_interval_val = 0;
	tcpip_adapter_ip_info_t current_ip_info;
	time_t now = 0;
	struct tm timeinfo = { 0 };

	esp_platform_get_clientmail_interval ( &clientmail_interval_val );
	esp_platform_get_clientmail_enable( &clientmail_enable_val );

	if( (esp_platform_get_clientmail_rcptmailbox( rcptmailbox ) == ESP_OK) &&
		(strlen( rcptmailbox ) > 0 ) && ( clientmail_enable_val == 1) && ( clientmail_interval_val > 0 )  ){
		//if sntp time is OK, it can do next process
		if( sntp_enabled() == 1 ) {
			//wifi is ok? it can mail
			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &current_ip_info)); 
			if (!(ip4_addr_isany_val(current_ip_info.ip) || ip4_addr_isany_val(current_ip_info.netmask) || ip4_addr_isany_val(current_ip_info.gw))) {
				
				time(&now);
				localtime_r(&now, &timeinfo);

				//check time interval set
				
				//send mail
				mail_send_mail( rcptmailbox );
			}
			else {
				printf(" clientmail timer execute: wifi status is not ok, so it cannot send clientmail now.\n");
			}
		}
		else{
			//time is not ok, it cannot send mail to client
			printf(" clientmail timer execute: sntp time is not ok, so it can not send clientmail now.\n");
		}
	}
	else{
		printf(" clientmail timer execute: it need not do client mail send.\n");
	}


}



/*******************************************************************************
*  Global define
*
********************************************************************************/
void  mail_monitor_task(void *pvParameters)
{
	bool ValueFromReceive = false;
    portBASE_TYPE xStatus;
	tcpip_adapter_ip_info_t current_ip_info;

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[64];
	esp_err_t err;
	char rcptmailbox[32];

	memset( rcptmailbox, 0 ,sizeof( rcptmailbox ));

	
	//loop forever for mail
	while(1){

		//check exit signal
		xStatus = xQueueReceive(MailQueueStop,&ValueFromReceive,0);
        if ( pdPASS == xStatus &&  ValueFromReceive == true ){
            printf("mail_monitor_task recv exit signal!\n");
            break;
        } 

		//if sntp time is OK, it can do next process
		if( sntp_enabled() == 1 ) {
			//wifi is ok? it can mail
			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &current_ip_info)); 
			if (!(ip4_addr_isany_val(current_ip_info.ip) || ip4_addr_isany_val(current_ip_info.netmask) || ip4_addr_isany_val(current_ip_info.gw))) {

				//get time now 
				time(&now);
				localtime_r(&now, &timeinfo);
				if (timeinfo.tm_year > (2016 - 1900)) {
					//every quarter, it need to handle max demand
					if( timeinfo.tm_min % 15 == 0 || timeinfo.tm_min == 0 ){
						mail_handle_max_demand();
					}
					//send email to server
					if( timeinfo.tm_min % 5 == 0 ){  //5 minutes
						strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
						printf("The current date/time is: %s, now we can send mail to server.....\n", strftime_buf);
						//send mail now
						mail_send_servermail_to_snapshot();
						//if(esp_platform_get_clientmail_rcptmailbox( rcptmailbox ) == ESP_OK)
						//mail_send_mail( rcptmailbox );
						//meterSnapshotDb.phaseA.Ae.measurementValue = meterSnapshotDb.phaseA.Ae.measurementValue + 0.001;
						//meterSnapshotDb.phaseB.Ae.measurementValue = meterSnapshotDb.phaseB.Ae.measurementValue + 0.002;
						//meterSnapshotDb.phaseC.Ae.measurementValue = meterSnapshotDb.phaseC.Ae.measurementValue + 0.003;
					}
				}
			}
			else {
				//check mailbox for wifi status 
				//
				printf("................... Wifi status is not ok, so it cannot send mail.............\n");
			}
		}
		else{
			printf(" Now sntp time is bad for this mail loop.\n");
		}

		printf("Mail monitor task: come to sleep for waiting next trigger...........\n");
		vTaskDelay(60000 / portTICK_PERIOD_MS);


	}

	vQueueDelete(MailQueueStop);
    MailQueueStop = NULL;
    vTaskDelete(NULL);

}

/*******************************************************************************
*  Global define
*
********************************************************************************/
void  clientmail_monitor_task(void *pvParameters)
{
	bool ValueFromReceive = false;
    portBASE_TYPE xStatus;
	tcpip_adapter_ip_info_t current_ip_info;

	time_t now = 0;
	struct tm timeinfo = { 0 };
	char strftime_buf[64];
	esp_err_t err;
	char rcptmailbox[32];

	memset( rcptmailbox, 0 ,sizeof( rcptmailbox ));

	///////////////////////for debug////////////////////////////////////
	//
	//esp_platform_set_clientmail_rcptmailbox( "shendongyan@msn.com" );
	//
	/////////////////////////////////////////////////////////////////////


	//loop forever for mail
	while(1){

		//check exit signal
		xStatus = xQueueReceive(ClientMailQueueStop,&ValueFromReceive,0);
        if ( pdPASS == xStatus &&  ValueFromReceive == true ){
            printf("clientmail_monitor_task recv exit signal!\n");
            break;
        } 

		//if sntp time is OK, it can do next process
		if( sntp_enabled() == 1) {
			//wifi is ok? it can mail
			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &current_ip_info)); 
			if (!(ip4_addr_isany_val(current_ip_info.ip) || ip4_addr_isany_val(current_ip_info.netmask) || ip4_addr_isany_val(current_ip_info.gw))) {

				//get time now 
				time(&now);
				localtime_r(&now, &timeinfo);
				if (timeinfo.tm_year > (2016 - 1900)) {
					if( timeinfo.tm_min % 5 == 0 ){  //5 minutes
						strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
						printf("The current date/time is: %s, now we can send mail to client.....\n", strftime_buf);
						//send mail now
						if(esp_platform_get_clientmail_rcptmailbox( rcptmailbox ) == ESP_OK){
							mail_send_mail( rcptmailbox );
						}
						//flush meter energy to flash
						//
						//mail_handle_energy_flash( &timeinfo );
					}
				}
			}
			else {
				//check mailbox for wifi status 
				//
				printf("................... Wifi status is not ok, so it cannot send client mail.............\n");
			}
		}
		else{
				printf(" Now sntp time is bad for this client mail loop.\n");
		}

		printf("ClientMail monitor task: come to sleep for waiting next trigger...........\n");
		vTaskDelay(60000 / portTICK_PERIOD_MS);
	}

	vQueueDelete(ClientMailQueueStop);
    ClientMailQueueStop = NULL;
    vTaskDelete(NULL);


}









/*******************************************************************************
*  Global define
*
********************************************************************************/
void mail_init(void)
{
	TimerHandle_t xClientMailTimer;
	BaseType_t xTimerStarted;

	//server mail
	 if (MailQueueStop == NULL){
        MailQueueStop = xQueueCreate(1,1);
	 }

	if (MailQueueStop != NULL){
		//xTaskCreate(mail_monitor_task, "mail_monitor_task", 1024, NULL, 9, NULL);
		xTaskCreatePinnedToCore(&mail_monitor_task, "mail_monitor_task", 2048, NULL, 9, NULL, 0);
    
		printf(" _______create task for mail.\n");
	}

	//client mail 
	if (ClientMailQueueStop == NULL){
        ClientMailQueueStop = xQueueCreate(1,1);
	 }

	if( ClientMailQueueStop != NULL ){
		xTaskCreatePinnedToCore(&clientmail_monitor_task, "clientmail_monitor_task", 2048, NULL, 7, NULL, 0);
    
		printf(" _______create task for client mail.\n");

	}




	//start clientmail timer
	//
	/* Create the clientmail timer, storing the handle to the created timer in xClientMailTimer. */
	/* xClientMailTimer = xTimerCreate(
		"ClientMail",		
		CLIENTMAIL_TIMER_PERIOD,
		pdTRUE,
		0,
		prvClientMailTimerCallback ); 

	if( xClientMailTimer != NULL  )
	{
				if( xTimerStarted == pdPASS  )
		{
			printf(" start client mail timer end.\n");
		}
		else{
			printf(" start client mail timer failed.\n");
		}
	} */



}
/*******************************************************************************
*  Global define
*
********************************************************************************/
int  mail_deinit(void)
{
    bool ValueToSend = true;
    portBASE_TYPE xStatus;
    if (MailQueueStop == NULL)
        return -1;

    xStatus = xQueueSend(MailQueueStop,&ValueToSend,0);
    if (xStatus != pdPASS){
        printf("It could not send to the queue for notify mail task!\n");
        return -1;
    } else {
        taskYIELD();
        return pdPASS;
    }
}
