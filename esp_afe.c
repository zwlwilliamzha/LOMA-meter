
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <nvs.h>
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h" 
#include "sdkconfig.h" 

#include "esp_afe.h"
#include "esp_lcd.h"
/*******************************************************************************
*  Global define
*
********************************************************************************/

static const char *TAG = "afe";

volatile Dual gAFEBUF;//4bytes TXD/RXD buffer

static bool isCalibrationParaValid;

//event queue that AFE module receive from outside
xQueueHandle afe_evt_queue = NULL;

//epg gpio event queue
static xQueueHandle epg_gpio_evt_queue = NULL;

//uart2 event queue
QueueHandle_t uart2_evt_queue;


//calibration parameter
MeterCalibrationPara meterCalibrationPara;

//snapshot db for meter metric
MeterSnapshotDb meterSnapshotDb;

//energy pulse count 
MeterEnergyPulseCounter meterEPC;

//Active power phase,0:forward,1:backward
volatile unsigned short bAeCurrentDirectionPhaseA;
volatile unsigned short bAeCurrentDirectionPhaseB;
volatile unsigned short bAeCurrentDirectionPhaseC;

//Reactive power phase, 0:forward,1:backward
volatile unsigned short bReCurrentDirectionPhaseA;
volatile unsigned short bReCurrentDirectionPhaseB;
volatile unsigned short bReCurrentDirectionPhaseC;



/*************************************************************************************************************************
*	function: 
*
**************************************************************************************************************************/

int afe_nvs_get_CalibrationPara( MeterCalibrationPara * pCalibrationPara  ) {
	nvs_handle handle;
	size_t size;
	esp_err_t err;
	uint32_t version;

	err = nvs_open(CALIBRATION_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "nvs_open: %x", err);
		nvs_close(handle);
		return -1;
	}

	size = sizeof( MeterCalibrationPara );
	err = nvs_get_blob(handle, KEY_CALIBRATION_INFO, pCalibrationPara, &size);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "No record found (%d).", err);
		nvs_close(handle);
		return -1;
	}
	else {
		ESP_LOGI(TAG," find calibration para in nvs.");
	}
	
	// Cleanup nvs handle
	//
	nvs_close(handle);

	return 0;
}




/*************************************************************************************************************************
*	function: cs5480 select page
*
**************************************************************************************************************************/

unsigned short cs5480_select_page(int uart_num, unsigned char page)
{
	unsigned short status = 0;

	gAFEBUF.auc[0]=(0x80|page);
	uart_write_bytes(uart_num,(const char*)(&gAFEBUF),1); 
	
	return status;

}


/*************************************************************************************************************************
*	function: cs5480 write register
*
**************************************************************************************************************************/
unsigned short cs5480_write_reg(int uart_num, unsigned char addr,unsigned char high,unsigned char mid,unsigned char low)
{
	unsigned short status = 0;

	gAFEBUF.auc[0]=(0x40|addr);
	gAFEBUF.auc[1]=low;//low
	gAFEBUF.auc[2]=mid;//mid
	gAFEBUF.auc[3]=high;//high
	uart_write_bytes(uart_num,(const char*)(&gAFEBUF),4); 

	return(status);
}

/******************************************************************************
*	function: 
*
******************************************************************************/
int afe_util_convert_HexStrToDec( int a, char *s)
{
	int i,t;
	long sum=0;

	for(i=0;i<a;i++) 
	{ 
		if(s[i]<='9') 
			t=s[i]-'0'; 
		else 
			t=s[i]-'a'+10; 

		sum=sum*16+t; 
	}
	return sum;



}

/******************************************************************************
*	function: cs5480 read register
*
******************************************************************************/

unsigned short cs5480_read_reg(int uart_num, unsigned char addr)
{
	unsigned short status = 0;
	
	gAFEBUF.auc[0]=addr;
	uint8_t* data = (unsigned char*)(&gAFEBUF.auc[1]);   //use 1 index
	int len = uart_read_bytes(uart_num, data, 3, 100 / portTICK_RATE_MS);	//clean anything in buffer
	
	uart_write_bytes(uart_num, (const char*)(&gAFEBUF),1);

	//3-Byte recv data is 0xFFFFFF if there isn't uart feedback
	gAFEBUF.auc[1]=0xFF;//low
	gAFEBUF.auc[2]=0xFF;//mid
	gAFEBUF.auc[3]=0xFF;//high

	len = uart_read_bytes(uart_num, data, 3, 1000 / portTICK_RATE_MS);

	//response length must be equal to 3 for cs5480
	//
    if(len > 0 && len == 3) {
            //printf( "uart read : %d\n", len);
			//printf("#### read reg data end!, low: %x, mid:%x, high:%x\n",gAFEBUF.auc[1],gAFEBUF.auc[2],gAFEBUF.auc[3]  );
			if( addr == 0x00)
			ESP_LOGI(TAG, "ADDR: %x, Len: %d, LowByte: %x, MidByte:%x, HighByte:%x\n",gAFEBUF.auc[0], len, gAFEBUF.auc[1],gAFEBUF.auc[2],gAFEBUF.auc[3]  );
		
	}
	else{
		//it need to be statistics for read error counts here
		ESP_LOGI(TAG, "cs5480_read_reg no return anything.");
	}

	return status;
}

/******************************************************************************
*	function: cs5480 send command
*
******************************************************************************/
unsigned short cs5480_send_cmd(int uart_num, unsigned char command)
{
	unsigned short status = 0;
	gAFEBUF.auc[0]=(0xC0|command);

	uart_write_bytes(uart_num, (const char*)(&gAFEBUF),1);

	return(status);
}

/******************************************************************************
*	function: cs5480 validate
*
******************************************************************************/
unsigned short cs5480_validate_check( int uart_num, int phase_num )
{
	unsigned short status = 0;
	unsigned short errValidation = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Config2);

	if( status == 0 ){
		if( ( gAFEBUF.auc[1] == 0x00 ) && ( gAFEBUF.auc[2] == 0x02 ) && ( gAFEBUF.auc[3] == 0xAA ) )
		{
			printf("######## cs5480 validate check successful. ###########\n");
		}
		else{
			printf("###### cs5480 validate check error.#######\n");
			errValidation = 1;
		}
	}
	else{
		printf("###### cs5480 validate check error.#######\n");
		errValidation = 1;
	}

	return errValidation;	
}



/******************************************************************************
*	function: cs5480 update voltage for V1
*
******************************************************************************/
void cs5480_update_U( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	int iVmax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, V1rms);

	if( status == 0 ){

		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];

		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseA.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseB.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseC.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else{
				//if it get calibration parameters from NVS,but Vmax is not set, use the default value
				iVmax = CS5480_BASE_U;
				fValue = ((sum*1.0)/10066329)*iVmax;
			}
		}
		else {
			//if it do not get calibration parameters from NVS,use the default parameter
			//
			iVmax = CS5480_BASE_U;
			fValue = ((sum*1.0)/10066329)*iVmax;
		}
		printf("V------>phase:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);


		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Vr.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Vr.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Vr.measurementValue = fValue;
		}
		else {
			//do nothing?
			printf(" other phase for voltage?\n");
		}
		
	}
	else {
		//error happen, stat it
	}
}
float cs5480_update_U_r( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = -9999;
	int iVmax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, V1rms);

	if( status == 0 ){

		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];

		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseA.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseB.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csVmax > 0 )
			{
				iVmax = meterCalibrationPara.PhaseC.csVmax;
				fValue = ((sum*1.0)/10066329) * iVmax;
			}
			else{
				//if it get calibration parameters from NVS,but Vmax is not set, use the default value
				iVmax = CS5480_BASE_U;
				fValue = ((sum*1.0)/10066329)*iVmax;
			}
		}
		else {
			//if it do not get calibration parameters from NVS,use the default parameter
			//
			iVmax = CS5480_BASE_U;
			fValue = ((sum*1.0)/10066329)*iVmax;
		}
		printf("V------>phase:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);
        //lcd_test(fValue);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Vr.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Vr.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Vr.measurementValue = fValue;
		}
		else {
			//do nothing?
			printf(" other phase for voltage?\n");
		}
		
	}
	else {
		//error happen, stat it
	}
    return fValue;
}
/******************************************************************************
*	function: cs5480 update current for I1
*
******************************************************************************/
void cs5480_update_I( int uart_num, int phase_num ) 
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	int iImax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, I1rms);

	if( status == 0 ){
		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseA.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseB.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
				
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseC.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
			else{
				//if it get calibration parameters from NVS,but Vmax is not set, use the default value
				iImax = CS5480_BASE_I;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
		}
		else{
			iImax = CS5480_BASE_I;
			fValue = ((sum*1.0)/10066329)*iImax;
		}
		//printf("I------>phase_num:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Ir.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Ir.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Ir.measurementValue = fValue;
		}
		else {
			//do nothing?
			printf(" other phase for current?\n");
		}
	}
	else{
		//error happen,stat it
		//
	}
}
float cs5480_update_I_r( int uart_num, int phase_num ) 
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = -9999;
	int iImax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, I1rms);

	if( status == 0 ){
		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseA.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseB.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
				
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csImax > 0 )
			{
				iImax = meterCalibrationPara.PhaseC.csImax;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
			else{
				//if it get calibration parameters from NVS,but Vmax is not set, use the default value
				iImax = CS5480_BASE_I;
				fValue = ((sum*1.0)/10066329)*iImax;
			}
		}
		else{
			iImax = CS5480_BASE_I;
			fValue = ((sum*1.0)/10066329)*iImax;
		}
		printf("I------>phase_num:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Ir.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Ir.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Ir.measurementValue = fValue;
		}
		else {
			//do nothing?
			printf(" other phase for current?\n");
		}
	}
	else{
		//error happen,stat it
		//
	}
    return fValue;
}
/******************************************************************************
*	function: cs5480 update active power
*
******************************************************************************/
void cs5480_update_P( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	int iPmax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, P1avg);

	if( status == 0 )
	{
		
		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		if (sum >= 8388608) {
			sum = sum - 16777216;	
		}
		
		//determine current direction for PhaseA/B/C here
		if( phase_num == 1 ){
			if( gAFEBUF.auc[3] & 0x80) 
			{
				bAeCurrentDirectionPhaseA = 1;
			}
			else {
				bAeCurrentDirectionPhaseA = 0;
			}
		}
		else if( phase_num == 2 ) {
			if( gAFEBUF.auc[3] & 0x80) 
			{
				bAeCurrentDirectionPhaseB = 1;
			}
			else{
				bAeCurrentDirectionPhaseB = 0;
			}
		}
		else if( phase_num == 3 ) {
			if( gAFEBUF.auc[3] & 0x80) 
			{
				bAeCurrentDirectionPhaseC = 1;
			}
			else{
				bAeCurrentDirectionPhaseC = 0;
			}
		}
		else {
			//do nothing?
		}

		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csImax > 0 && meterCalibrationPara.PhaseA.csVmax > 0 ){
				iPmax = meterCalibrationPara.PhaseA.csImax * meterCalibrationPara.PhaseA.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csImax > 0 && meterCalibrationPara.PhaseB.csVmax > 0  ){
				iPmax = meterCalibrationPara.PhaseB.csImax * meterCalibrationPara.PhaseB.csVmax ;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csImax > 0 && meterCalibrationPara.PhaseC.csVmax > 0 ) {
				iPmax = meterCalibrationPara.PhaseC.csImax * meterCalibrationPara.PhaseC.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else {
				iPmax = CS5480_BASE_U * CS5480_BASE_I;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
		}
		else{
			iPmax = CS5480_BASE_U * CS5480_BASE_I;
			fValue = ((sum*1.0)/3019898)* iPmax;
		}
		//printf("P------>phase_num:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		//write value to snapshotdb 
		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Ap.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Ap.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Ap.measurementValue = fValue;
		}
		else {
			//do nothing?
		}

	}
	else{
		//read reg error happen, stat it
		//
	}
}
float cs5480_update_P_r( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	int iPmax = 0;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, P1avg);

	if( status == 0 )
	{

		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		if (sum >= 8388608) {
			sum = sum - 16777216;
		}

		//determine current direction for PhaseA/B/C here
		if( phase_num == 1 ){
			if( gAFEBUF.auc[3] & 0x80)
			{
				bAeCurrentDirectionPhaseA = 1;
			}
			else {
				bAeCurrentDirectionPhaseA = 0;
			}
		}
		else if( phase_num == 2 ) {
			if( gAFEBUF.auc[3] & 0x80)
			{
				bAeCurrentDirectionPhaseB = 1;
			}
			else{
				bAeCurrentDirectionPhaseB = 0;
			}
		}
		else if( phase_num == 3 ) {
			if( gAFEBUF.auc[3] & 0x80)
			{
				bAeCurrentDirectionPhaseC = 1;
			}
			else{
				bAeCurrentDirectionPhaseC = 0;
			}
		}
		else {
			//do nothing?
		}

		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csImax > 0 && meterCalibrationPara.PhaseA.csVmax > 0 ){
				iPmax = meterCalibrationPara.PhaseA.csImax * meterCalibrationPara.PhaseA.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csImax > 0 && meterCalibrationPara.PhaseB.csVmax > 0  ){
				iPmax = meterCalibrationPara.PhaseB.csImax * meterCalibrationPara.PhaseB.csVmax ;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csImax > 0 && meterCalibrationPara.PhaseC.csVmax > 0 ) {
				iPmax = meterCalibrationPara.PhaseC.csImax * meterCalibrationPara.PhaseC.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else {
				iPmax = CS5480_BASE_U * CS5480_BASE_I;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
		}
		else{
			iPmax = CS5480_BASE_U * CS5480_BASE_I;
			fValue = ((sum*1.0)/3019898)* iPmax;
		}
		//printf("P------>phase_num:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		//write value to snapshotdb
		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Ap.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Ap.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Ap.measurementValue = fValue;
		}
		else {
			//do nothing?
		}

	}
	else{
		//read reg error happen, stat it
		//
	}
	return fValue;
}

/******************************************************************************
*	function: cs5480 update reactive power
*
******************************************************************************/

void cs5480_update_Q( int uart_num , int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	int iPmax = 0;

	//status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Q1avg);

	if( status == 0 )
	{
		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		
		if (sum >= 8388608) {
			sum = sum - 16777216;	
		}

		//determine current direction for PhaseABC here
		if( phase_num == 1 ){
			if( gAFEBUF.auc[3] & 0x80 ){
				bReCurrentDirectionPhaseA = 1;
			}
			else{
				bReCurrentDirectionPhaseA = 0;
			}
		}
		else if( phase_num == 2 )
		{
			if( gAFEBUF.auc[3] & 0x80 ){
				bReCurrentDirectionPhaseB = 1;
			}
			else{
				bReCurrentDirectionPhaseB = 0;
			}
		}
		else if( phase_num == 3 )
		{
			if( gAFEBUF.auc[3] & 0x80 ){
				bReCurrentDirectionPhaseC = 1;
			}
			else{
				bReCurrentDirectionPhaseC = 0;
			}
		}
		else{
			//do nothing?
		}


		if( isCalibrationParaValid == true )
		{
			if( phase_num == 1 && meterCalibrationPara.PhaseA.csImax > 0 && meterCalibrationPara.PhaseA.csVmax > 0 ){
				iPmax = meterCalibrationPara.PhaseA.csImax * meterCalibrationPara.PhaseA.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 2 && meterCalibrationPara.PhaseB.csImax > 0 && meterCalibrationPara.PhaseB.csVmax > 0  ){
				iPmax = meterCalibrationPara.PhaseB.csImax * meterCalibrationPara.PhaseB.csVmax ;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else if( phase_num == 3 && meterCalibrationPara.PhaseC.csImax > 0 && meterCalibrationPara.PhaseC.csVmax > 0 ) {
				iPmax = meterCalibrationPara.PhaseC.csImax * meterCalibrationPara.PhaseC.csVmax;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}
			else {
				iPmax = CS5480_BASE_U * CS5480_BASE_I;
				fValue = ((sum*1.0)/3019898)* iPmax;
			}

		}
		else{
			//use default voltage and current 
			iPmax = CS5480_BASE_U * CS5480_BASE_I;
			fValue = ((sum*1.0)/3019898)* iPmax;
		}
		//printf("Q------>phase_num:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Rp.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Rp.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Rp.measurementValue = fValue;
		}
		else {
			//do nothing?
		}
	}
	else{

	}
}

/******************************************************************************
*	function: cs5480 update power factor
*
******************************************************************************/

void cs5480_update_PF( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;
	

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, PF1);

	if( status == 0 ){
		//sprintf( strval, "%x%x%x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		if(sum <= 8388607){
			fValue = (sum*1.0)/8388607;
		}
		else {
			fValue = (-1.0*(sum-8388608))/8388607;
		}
		if(sum == 0)
			fValue = 1.0;	
		//printf("PF------>phase:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);


		/************** add engineering compution *******************/
	
	
		//convert 'Dual' to 'float' data type
		//
		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Pf.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Pf.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Pf.measurementValue = fValue;
		}
		else {
			//do nothing?
		}
	}
	else{

	}
}
/******************************************************************************
*	function: cs5480 update frequency
*
******************************************************************************/

void cs5480_update_F( int uart_num , int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Epsilon);

	if( status == 0 ){
		//sprintf( strval, "%x%2x%2x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];

		/************** add engineering compution *******************/
		printf("F------>phase:%d, sum:%d, fValue:%f\n",phase_num, sum, ((sum*1.0)/8388607)*4000.0);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Hz.measurementValue = ((sum*1.0)/8388607)*4000.0;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Hz.measurementValue = ((sum*1.0)/8388607)*4000.0;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Hz.measurementValue = ((sum*1.0)/8388607)*4000.0;
		}
		else {
			//do nothing?
		}
	}
	else{

	}
}
float cs5480_update_F_r( int uart_num , int phase_num )
{
	unsigned short status = 0;
	char strval[10];

	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = -9999;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Epsilon);

	if( status == 0 ){
		//sprintf( strval, "%x%2x%2x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d", strval, strlen(strval));
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];

		/************** add engineering compution *******************/
        fValue = ((sum*1.0)/8388607)*4000.0;
		printf("F------>phase:%d, sum:%d, fValue:%f\n",phase_num, sum, fValue);

		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Hz.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Hz.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Hz.measurementValue = fValue;
		}
		else {
			//do nothing?
		}
	}
	else{

	}
    return fValue;
}
/******************************************************************************
*	function: cs5480 update temperature
*
******************************************************************************/
void cs5480_update_T( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = 0.00;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Temperature);

	if( status == 0 ){
		//sprintf( strval, "%2x%2x%2x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d -----%d %d %d %d", strval, strlen(strval), gAFEBUF.auc[3], 
		//	gAFEBUF.auc[2],gAFEBUF.auc[1],gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1]);
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		ESP_LOGI(TAG, "sum: %d", sum );
		fValue = (sum*1.0)/65535;
		//printf("T------>sum:%d, fValue:%f\n",sum, fValue);
	
		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Tc.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Tc.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Tc.measurementValue = fValue;
		}
		else {
			//do nothing?
		}

	}
	else{
		//read reg error happen, stat it
		//
	}
}
float cs5480_update_T_r( int uart_num, int phase_num )
{
	unsigned short status = 0;
	char strval[10];
	memset((void*)strval, 0, 10 );
	int sum =0;
	float fValue = -9999;

	status=cs5480_select_page(uart_num, 0x10);//select page 16
	status=cs5480_read_reg(uart_num, Temperature);

	if( status == 0 ){
		//sprintf( strval, "%2x%2x%2x", gAFEBUF.auc[3],gAFEBUF.auc[2],gAFEBUF.auc[1]  );
		//ESP_LOGI(TAG, "strval: %s,  length: %d -----%d %d %d %d", strval, strlen(strval), gAFEBUF.auc[3], 
		//	gAFEBUF.auc[2],gAFEBUF.auc[1],gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1]);
		sum=gAFEBUF.auc[3]*65536+gAFEBUF.auc[2]*256+gAFEBUF.auc[1];
		ESP_LOGI(TAG, "sum: %d", sum );
		fValue = (sum*1.0)/65535;
		printf("T------>sum:%d, fValue:%f\n",sum, fValue);
	
		if( phase_num == 1 ){
			meterSnapshotDb.phaseA.Tc.measurementValue = fValue;
		}
		else if( phase_num == 2 ) {
			meterSnapshotDb.phaseB.Tc.measurementValue = fValue;
		}
		else if( phase_num == 3 ) {
			meterSnapshotDb.phaseC.Tc.measurementValue = fValue;
		}
		else {
			//do nothing?
		}

	}
	else{
		//read reg error happen, stat it
		//
	}
    return fValue;
}
/******************************************************************************
*	function: calibration read register
*
******************************************************************************/
unsigned short afe_calibration_read_reg(int uart_num, unsigned char addr)
{
	unsigned short status = 0;

	gAFEBUF.auc[0]=addr;
	uart_write_bytes(uart_num, (const char*)(&gAFEBUF),1);

	uint8_t* data = (unsigned char*)(&gAFEBUF.auc[1]);   //use 1 index

	int len = uart_read_bytes(uart_num, data, 3, 1000 / portTICK_RATE_MS);

    if(len > 0) {
            //printf( "uart read : %d\n", len);
			//printf("#### read reg data end!, low: %x, mid:%x, high:%x\n",gAFEBUF.auc[1],gAFEBUF.auc[2],gAFEBUF.auc[3]  );
		    //ESP_LOGI(TAG, "LowByte: %x, MidByte:%x, HighByte:%x", gAFEBUF.auc[1],gAFEBUF.auc[2],gAFEBUF.auc[3]  );
	}
	else{
		//printf("uart read error.\n");
		ESP_LOGI(TAG, "afe_calibration_read_reg error.");
	}

	return status;
}


/******************************************************************************
*	function: hex to string
*
******************************************************************************/
void afe_util_convert_HexStrToByte(const char* source, unsigned char* dest, int sourceLen)
 {
     short i;
     unsigned char highByte, lowByte;
     
     for (i = 0; i < sourceLen; i += 2)
     {
         highByte = (unsigned char)toupper((int)source[i]);
         lowByte  = (unsigned char)toupper((int)source[i + 1]);

         if (highByte > 0x39)
             highByte -= 0x37;
         else
             highByte -= 0x30;

         if (lowByte > 0x39)
             lowByte -= 0x37;
         else
             lowByte -= 0x30;

         dest[i / 2] = (highByte << 4) | lowByte;
     }
     return ;
 }




/**********************************************************************************************
 * @brief  this task monitor uart2 event 
 *
 **********************************************************************************************/
 void uart2_evt_task(void *pvParameters)
{
    int uart_num = (int) pvParameters;
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart2_evt_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            //ESP_LOGI(TAG, "uart[%d] event:", uart_num);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.
                in this example, we don't process data in event, but read data outside.*/
                case UART_DATA:
                    uart_get_buffered_data_len(uart_num, &buffered_size);
                    //ESP_LOGI(TAG, "data, len: %d; buffered len: %d", event.size, buffered_size);
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow\n");
                    //If fifo overflow happened, you should consider adding flow control for your application.
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full\n");
                    //If buffer full happened, you should consider encreasing your buffer size
                    //We can read data out out the buffer, or directly flush the rx buffer.
                    uart_flush(uart_num);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break\n");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error\n");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error\n");
                    break;
                //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "uart pattern detected\n");
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d\n", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
 



 /**********************************************************************************************
 * @brief  
 *
 **********************************************************************************************/
  void afe_init_meter_snapshotdb()
 {
	 memset((void*)&meterSnapshotDb, 0 , sizeof( MeterSnapshotDb ));

	 //Recovery active energy for phaseABC
	 meterSnapshotDb.phaseA.Ae.measurementValue = meterEPC.PhaseAEPC * 0.0001;
	 meterSnapshotDb.phaseA.ReverseAe.measurementValue = meterEPC.ReversePhaseAEPC * 0.0001;
	 meterSnapshotDb.phaseA.Re.measurementValue = meterEPC.CapacitiveRePhaseAEPC * 0.0001;
	 meterSnapshotDb.phaseA.ReverseRe.measurementValue = meterEPC.InductiveRePhaseAEPC * 0.0001;

	 meterSnapshotDb.phaseB.Ae.measurementValue = meterEPC.PhaseBEPC * 0.0001;
	 meterSnapshotDb.phaseB.ReverseAe.measurementValue = meterEPC.ReversePhaseBEPC * 0.0001;
	 meterSnapshotDb.phaseB.Re.measurementValue = meterEPC.CapacitiveRePhaseBEPC * 0.0001;
	 meterSnapshotDb.phaseB.ReverseRe.measurementValue = meterEPC.InductiveRePhaseBEPC * 0.0001;

	 meterSnapshotDb.phaseC.Ae.measurementValue = meterEPC.PhaseCEPC * 0.0001;
	 meterSnapshotDb.phaseC.ReverseAe.measurementValue = meterEPC.ReversePhaseCEPC * 0.0001;
	 meterSnapshotDb.phaseC.Re.measurementValue = meterEPC.CapacitiveRePhaseCEPC  * 0.0001;
	 meterSnapshotDb.phaseC.ReverseRe.measurementValue = meterEPC.InductiveRePhaseCEPC * 0.0001;

	printf(" #### Recovery meter energy end, Ae:%f,Be:%f, Ce:%f\n", meterSnapshotDb.phaseA.Ae.measurementValue, meterSnapshotDb.phaseB.Ae.measurementValue, meterSnapshotDb.phaseC.Ae.measurementValue);
	
 }


/**********************************************************************************************
 * @brief  
 *
 **********************************************************************************************/
esp_err_t afe_init_uart2()
{
	esp_err_t err = ESP_OK;
	int uart_num = UART_NUM_2;   
    uart_config_t uart_config = {
		.baud_rate = 600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 122,
    };
    //Set UART parameters
    uart_param_config(uart_num, &uart_config);
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Install UART driver, and get the queue.
    uart_driver_install(uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart2_evt_queue, 0);
    //Set UART pins,(-1: default pin, no change.)
    //For UART0, we can just use the default pins.
    uart_set_pin(uart_num, UART2_TXD, UART2_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Set uart pattern detect function.
    uart_enable_pattern_det_intr(uart_num, '+', 3, 10000, 10, 10);
    //Create a task to handler UART event from ISR
    xTaskCreate(uart2_evt_task, "uart2_evt_task", 2048, (void*)uart_num, 11, NULL);


    //初始化GPIO18
    //
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);


	unsigned short status = 0;
	unsigned short cmd = 0;
	unsigned int count =0;

	vTaskDelay(1000 / portTICK_RATE_MS);

	printf("intiialize CS5480........\n");
	ESP_LOGI(TAG, "I am herei\n");
	gpio_set_level(GPIO_NUM_15, 1);
	lcd_init();
	/*
	lcd_display_test(123.3,781.3,"IB");
	vTaskDelay(3000 / portTICK_RATE_MS);
	ESP_LOGI(TAG, "lcd_display_test()");
	vTaskDelay(3000 / portTICK_RATE_MS);

	vTaskDelay(50 / portTICK_RATE_MS);
	lcd_cs_clear();
	vTaskDelay(1000 / portTICK_RATE_MS);
	*/
	calibrationReqData reqData;
	char *pstr = NULL, *pstr1 = NULL, *pstr2 = NULL;
	int nbytes = 0 , i=0;
	unsigned char bits[10] ;
	memset((void*)&reqData, 0 , sizeof( calibrationReqData ));

	 for(i=0; i < 10;) {
        //if(xQueueReceive(calibration_evt_queue, (const char*)data, portMAX_DELAY))
		 if(xQueueReceive(epg_gpio_evt_queue, (const char*)&reqData, portMAX_DELAY)){
			ESP_LOGI(TAG, "len: %d , data: %s\n\n", reqData.buffLen, reqData.buff);
//            switch((i+4)%4) {
//                case 0:
//                	lcd_display(1234.3,1,7819.3,1,"");
//                    break;
//                case 1:
//                	lcd_display(111.3,1,222.3,1,"E");
//                    break;
//                case 2:
//                	lcd_display(10000,0,110.6,1,"V");
//                    break;
//                case 3:
//                	lcd_display(10000,0,13.5,1,"I");
//                    break;
//            }
//            i++;
//			if(i == 4){
//				i=0;
//			}
//
//
//			vTaskDelay(1000 / portTICK_RATE_MS);
//			lcd_cs_clear();
//

//			//handle calibration request command by use of command strings
//			//IRMSA/B/C command
//			//
//			pstr = strstr( reqData.buff, "IRMS");
//			if( pstr != NULL ){
//
//				if( strstr( reqData.buff, "IRMS" ) != NULL ){
//                    //lcd_test0();
//                    vTaskDelay(50 / portTICK_RATE_MS);
//                    lcd_cs_clear();
//                    vTaskDelay(1000 / portTICK_RATE_MS);
//                    lcd_cs();
//                    vTaskDelay(50 / portTICK_RATE_MS);
//                    lcd_wr(1);lcd_wr(0);lcd_wr(1);					//Write Command 101
//                    lcd_wr(0);lcd_wr(0);lcd_wr(1);lcd_wr(0);lcd_wr(0);lcd_wr(0);	//Address A[5]->A[0]
//                    vTaskDelay(50 / portTICK_RATE_MS);
//                    lcd_cs_clear();
//                    vTaskDelay(1000 / portTICK_RATE_MS);
//                    lcd_clear_all();
//                    vTaskDelay(1000 / portTICK_RATE_MS);
//                    lcd_wr(1);lcd_wr(0);lcd_wr(1);					//Write Command 101
//                    lcd_wr(0);lcd_wr(0);lcd_wr(1);lcd_wr(0);lcd_wr(0);lcd_wr(0);	//Address A[5]->A[0]
//                    int n = 0;
//                    //ESP_LOGI(TAG, "data: %d = \n", n);
//                    for(n=0;n<reqData.buffLen;n++){
//                        //ESP_LOGI(TAG, "data: %d = \n", n);
//
//                        if(reqData.buff[n] == '0'){//0
//                            ESP_LOGI(TAG, ":: %d\n", reqData.buff[n]);
//                            lcd_wr(0);
//                        }
//                        if(reqData.buff[n] == '1'){//1
//                            ESP_LOGI(TAG, ":: %d\n", reqData.buff[n]);
//                            lcd_wr(1);
//                        }
//                    }
//                    vTaskDelay(50 / portTICK_RATE_MS);
//                    lcd_cs_clear();
//                    vTaskDelay(1000 / portTICK_RATE_MS);
//                    lcd_clear_all();
//                    vTaskDelay(1000 / portTICK_RATE_MS);
//                    ESP_LOGI(TAG, "buff finished.......!");
//				}
//				else {
//					//do nothing
//					ESP_LOGI(TAG, "Sorry, i donot know this command, please try again!");
//					}
//			}
//			//VRMS command
//			//
//			pstr = strstr( reqData.buff, "VRMS");
//			if( pstr != NULL ){
//				if( strstr( reqData.buff, "VRMS") != NULL ) {
//					//read reg
////					gpio_set_level(GPIO_NUM_15, 0);
////					vTaskDelay(50 / portTICK_RATE_MS);
////
////					status=cs5480_select_page(uart_num, 0x10);//select page 16
////					status=afe_calibration_read_reg(uart_num, V1rms);
////					gpio_set_level(GPIO_NUM_15, 1);
//				}
//				else {
//					ESP_LOGI(TAG, "Sorry, i donot know this command, please try again!");
//				}
//			}
//			//TEMP
//			pstr = strstr( reqData.buff, "TEMP" );
//			if( pstr != NULL ){
////					gpio_set_level(GPIO_NUM_15, 0);
////					vTaskDelay(50 / portTICK_RATE_MS);
////					status=cs5480_select_page(uart_num, 0x10);
////					status=afe_calibration_read_reg(uart_num, Temperature );
////					gpio_set_level(GPIO_NUM_15, 1);
//			}
//			//meterSN
//			pstr = strstr( reqData.buff, "SN" );
//			if( pstr != NULL ) {
//				pstr1 = strstr( reqData.buff, "=" );
//				if( pstr1 != NULL ) {
//					pstr2 = pstr1+1;
//					printf("string for SN : %s\n", pstr2 );////////////////////////////////////////////
//					//write data to memory
//					sprintf(meterCalibrationPara.meterSN, "%s", pstr2 );
//				}
//
//			}
			//reset buffe
			memset((void*)&reqData, 0 , sizeof( calibrationReqData ));

		}//Qreceive
	 }


	return err;
}


/**********************************************************************************************
 * @brief  
 *
 **********************************************************************************************/
esp_err_t afe_init_sig_gpio()
{
	esp_err_t err = ESP_OK;

	//15、5、18这三个GPIO分别分别对应3个CS5480的GPIO标识号
	//
	gpio_pad_select_gpio(GPIO_NUM_15); 
	gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT); 
	gpio_pad_select_gpio(GPIO_NUM_5); 
	gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT); 
	
	//初始化GPIO18
	//
	gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode        
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
	//
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);


	return err;
}

/*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(epg_gpio_evt_queue, &gpio_num, NULL);
}


/**********************************************************************************************
 * @brief  
 *
 **********************************************************************************************/
esp_err_t afe_init_epg_gpio()
{
	esp_err_t err = ESP_OK;
	gpio_config_t io_conf;

	gpio_pad_select_gpio(GPIO_NUM_12); 
	gpio_set_direction(GPIO_NUM_12, GPIO_MODE_INPUT); 
	gpio_pad_select_gpio(GPIO_NUM_14); 
	gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT); 
	gpio_pad_select_gpio(GPIO_NUM_32); 
	gpio_set_direction(GPIO_NUM_32, GPIO_MODE_INPUT); 
	gpio_pad_select_gpio(GPIO_NUM_33); 
	gpio_set_direction(GPIO_NUM_33, GPIO_MODE_INPUT); 
	gpio_pad_select_gpio(GPIO_NUM_34); 
	gpio_set_direction(GPIO_NUM_34, GPIO_MODE_INPUT); 
	gpio_pad_select_gpio(GPIO_NUM_35); 
	gpio_set_direction(GPIO_NUM_35, GPIO_MODE_INPUT); 

	//bit mask
	uint64_t bitMask =0;
	bitMask = 1;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO32/33/... here
    //io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.pin_bit_mask = ( bitMask<<32 | bitMask<<33 | bitMask<<12 | bitMask<<14 | bitMask<<34 |bitMask<<35  );
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

	//install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_12, gpio_isr_handler, (void*) GPIO_INPUT_IO_12);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_14, gpio_isr_handler, (void*) GPIO_INPUT_IO_14);

	gpio_isr_handler_add(GPIO_INPUT_IO_32, gpio_isr_handler, (void*) GPIO_INPUT_IO_32);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_33, gpio_isr_handler, (void*) GPIO_INPUT_IO_33);
	gpio_isr_handler_add(GPIO_INPUT_IO_34, gpio_isr_handler, (void*) GPIO_INPUT_IO_34);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_35, gpio_isr_handler, (void*) GPIO_INPUT_IO_35);



	return err;
}



/**********************************************************************************************
 * @brief  
 *
 **********************************************************************************************/
esp_err_t afe_init_cs5480()
{
	esp_err_t err = ESP_OK;

	unsigned short status = 0;
	int uart_num = UART_NUM_2;   

	ESP_LOGI(TAG, "intiialize CS5480........");

	/********* 0 : read config parameters from NVS **********/
	//It have already load calibration parameters from NVS in main loop
	//so donot do this again
	//
	/*
	if( afe_nvs_get_CalibrationPara( &meterCalibrationPara ) == -1 ){
		ESP_LOGI(TAG, "get calibration parameters from NVS failed.");
		isCalibrationParaValid = false;
	}
	else {
		ESP_LOGI(TAG, "get calibration parameters from NVS  successful.");
		isCalibrationParaValid = true;
		//snapshotdb need get meterSN from calibration parameters
		//
		if( strlen( meterCalibrationPara.meterSN ) > 0 ){
			sprintf( meterSnapshotDb.meterSN, "%s", meterCalibrationPara.meterSN );
		}
	}
	*/

	if( strlen( meterCalibrationPara.meterSN ) > 0  ){
		isCalibrationParaValid = true;
		//snapshotdb need get meterSN from calibration parameters
		//
		sprintf( meterSnapshotDb.meterSN, "%s", meterCalibrationPara.meterSN );
	}
	else{
		ESP_LOGI(TAG, "get calibration parameters from NVS failed.");
		isCalibrationParaValid = false;
	}


	/*******  1: write common register for all phases  *******/

	gpio_set_level(GPIO_NUM_15, 0);
	vTaskDelay(200 / portTICK_RATE_MS);

	
	status = cs5480_send_cmd( uart_num, AFESWReset );
	//vTaskDelay(500 / portTICK_RATE_MS);

	//init EPG 
	//
	status=cs5480_select_page(uart_num, 0x00);//select page 0
	status=cs5480_write_reg(uart_num, Config1,0x30,0xEE,0x10);//enable EPG1
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x00);//select page 0
	status=cs5480_write_reg(uart_num, PulseWidth,0x00,0x10,0x00);//config PulseWidth
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x00);//select page 0
	status=cs5480_write_reg(uart_num, PulseCtrl,0x00,0x00,0x30);//for PulseCtrl
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x12);//select page 18
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_write_reg(uart_num, PulseRate,0x00,0x97,0xB5);//for PulseRate
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x10);//select page 16
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_write_reg(uart_num, Config2,0x00,0x02,0xAA);//for Config2
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x10);//select page 16
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_write_reg(uart_num, Tsettle,0x00,0x1F,0x40);//for Tsettle
	vTaskDelay(50 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x10);//select page 16
	vTaskDelay(50 / portTICK_RATE_MS);
	//status=cs5480_write_reg(uart_num, SampleCount,0x00,0x3E,0x80);//for SampleCount
	status=cs5480_write_reg(uart_num, SampleCount,0x00,0x0F,0xA0);//for SampleCount

	gpio_set_level(GPIO_NUM_15, 1);

	
	/****************** 2: write single register for every phase ***************/
	    
	if( isCalibrationParaValid == true ){
		vTaskDelay(200 / portTICK_RATE_MS);
		//Phase A
		gpio_set_level(GPIO_NUM_15, 0);
		vTaskDelay(300 / portTICK_RATE_MS);
		status=cs5480_select_page(uart_num, 0x10);
		vTaskDelay(300 / portTICK_RATE_MS);
		//high,mid,low
		status=cs5480_write_reg(uart_num, I1acoff,meterCalibrationPara.PhaseA.csI1acoff[2],meterCalibrationPara.PhaseA.csI1acoff[1], meterCalibrationPara.PhaseA.csI1acoff[0]);
		vTaskDelay(300 / portTICK_RATE_MS);
		status=cs5480_write_reg(uart_num, I1gain,meterCalibrationPara.PhaseA.csI1gain[2],meterCalibrationPara.PhaseA.csI1gain[1], meterCalibrationPara.PhaseA.csI1gain[0]);
		vTaskDelay(300 / portTICK_RATE_MS);
		status=cs5480_write_reg(uart_num, V1gain,meterCalibrationPara.PhaseA.csV1gain[2],meterCalibrationPara.PhaseA.csV1gain[1], meterCalibrationPara.PhaseA.csV1gain[0]);
		vTaskDelay(300 / portTICK_RATE_MS);
		gpio_set_level(GPIO_NUM_15, 1);
	}
	else{
		//error happen for NVS
		printf(".............CalibrationPara is not valid.............\n");
	}

	/**************** 3: send conv command  for all phases *********************/
	vTaskDelay(1000 / portTICK_RATE_MS);

	gpio_set_level(GPIO_NUM_15, 0);
	vTaskDelay(1000 / portTICK_RATE_MS);

	//send conv command
	//
	vTaskDelay(300 / portTICK_RATE_MS);
	status=cs5480_select_page(uart_num, 0x00);//select page 0
	vTaskDelay(300 / portTICK_RATE_MS);
	status = cs5480_send_cmd( uart_num, ContiConv );
	vTaskDelay(300 / portTICK_RATE_MS);
				

	gpio_set_level(GPIO_NUM_15, 1);

	ESP_LOGI(TAG, "intiialize CS5480 end.");

	return err;
}




/*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/

void epg_gpio_task_handler(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(epg_gpio_evt_queue, &io_num, portMAX_DELAY)) {
            //printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
			//process active/reactive power energy
			//It will add direction logic in the future
			//
			if( io_num == 32 ){  //phaseA/GPIO15 ===> Active energy
				//meterSnapshotDb.phaseA.Ae.measurementValue += 0.0001;
				//add energy pulse count for phase A
				//
				if( bAeCurrentDirectionPhaseA == 0 ){
					//increment forward active energy pulse count for phaseA
					meterEPC.PhaseAEPC++;
					//increment forward active energy value for phaseA
					meterSnapshotDb.phaseA.Ae.measurementValue += 0.0001;
				}
				else{
					//increment reverse active energy pulse count for phaseA
					meterEPC.ReversePhaseAEPC++;
					//increment reverse active energy  value for phaseA
					meterSnapshotDb.phaseA.ReverseAe.measurementValue += 0.0001;
				}
			}
			else if( io_num == 33 ){ //phaseA ======> Reactive energy
				//meterSnapshotDb.phaseA.Re.measurementValue += 0.0001;
				//Reactive energy pulse count for phaseA
				//
				if( bReCurrentDirectionPhaseA == 0 ){
					// increment Capacitive Reactive Energy  pulse count for phaseA
					meterEPC.CapacitiveRePhaseAEPC++;
					//increment Capacitive Reactive energy value for phaseA
					meterSnapshotDb.phaseA.Re.measurementValue += 0.0001;
				}
				else {
					//increment Inductive Reactive Energy pulse count for phaseA
					meterEPC.InductiveRePhaseAEPC++;
					//increment Inductive Reactive Energy value for phaseA
					meterSnapshotDb.phaseA.ReverseRe.measurementValue += 0.0001;
				}
			}
			else if( io_num == 34 ) { //phaseB/GPIO5  =========> Active energy
				//meterSnapshotDb.phaseB.Ae.measurementValue += 0.0001;
				//add energy pulse count for phase B
				//
				if( bAeCurrentDirectionPhaseB == 0 ){
					//increment forward active energy pulse count for phaseB
					meterEPC.PhaseBEPC++;
					//increment forward active energy value for phaseB
					meterSnapshotDb.phaseB.Ae.measurementValue += 0.0001;
				}
				else{
					//increment reverse active energy pulse count for phaseB
					meterEPC.ReversePhaseBEPC++;
					//increment reverse active energy value for phaseB
					meterSnapshotDb.phaseB.ReverseAe.measurementValue += 0.0001;
				}
			}
			else if( io_num == 35 ) { //phaseB============>Reactive energy
				//meterSnapshotDb.phaseB.Re.measurementValue += 0.0001;
				//Reactive energy pulse count for phaseB
				//
				if( bReCurrentDirectionPhaseB == 0 ){
					meterEPC.CapacitiveRePhaseBEPC++;
					meterSnapshotDb.phaseB.Re.measurementValue += 0.0001;
				}
				else{
					meterEPC.InductiveRePhaseBEPC++;
					meterSnapshotDb.phaseB.ReverseRe.measurementValue += 0.0001;
				}
			}
			else if( io_num == 12 ) { //phaseC/GPIO18===========>Active energy
				//meterSnapshotDb.phaseC.Ae.measurementValue += 0.0001;
				//add energy pulse count for phase C
				//
				if( bAeCurrentDirectionPhaseC == 0 ){
					//increment forward active energy pulse count for phaseC
					meterEPC.PhaseCEPC++;
					//increment forward active energy value for phaseC
					meterSnapshotDb.phaseC.Ae.measurementValue += 0.0001;
				}
				else{
					//increment reverse active energy pulse count for phaseC
					meterEPC.ReversePhaseCEPC++;
					//increment reverse active energy value for phaseC
					meterSnapshotDb.phaseC.ReverseAe.measurementValue += 0.0001;
				}
			}
			else if( io_num == 14 ) {  //phaseC=================>Reactive energy
				//meterSnapshotDb.phaseC.Re.measurementValue += 0.0001;
				//Reactive energy pulse count for phaseC
				//
				if( bReCurrentDirectionPhaseC == 0 ){
					meterEPC.CapacitiveRePhaseCEPC++;
					meterSnapshotDb.phaseC.Re.measurementValue += 0.0001;
				}
				else{
					meterEPC.InductiveRePhaseCEPC++;
					meterSnapshotDb.phaseC.ReverseRe.measurementValue += 0.0001;
				}
			}
			else {
				ESP_LOGI(TAG,"Unknown GPIO NUM: %d for EPG", io_num );
			}
        }
    }
}


/*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/
void afe_calibration_ev_handler_cmd( const char * pstr )
{

}


/*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/
void afe_calibration_ev_handler( const char * pstr )
{
	//it donot need now!
}



/*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/
 void afe_calibration_command_monitor()
{
    int uart_num = UART_NUM_0;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    //Configure UART1 parameters
    uart_param_config(uart_num, &uart_config);
    //Set UART1 pins(TX: IO4, RX: I05, RTS: IO18, CTS: IO19)
    uart_set_pin(uart_num, UART0_TXD, UART0_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Install UART driver( We don't need an event queue here)
    //In this example we don't even use a buffer for sending data.
    uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0);

	//receive buffer from UART0 for calibration command
	uint8_t data[CALIBRATION_BUF_SIZE];
	memset((void*)data, 0, CALIBRATION_BUF_SIZE );

	char *pcStringToSend = NULL;
	//general event struct
	AFEEvent_t xAFEEvent;   


    while(1) {
        //Read data from UART0
        int len = uart_read_bytes(uart_num, data, CALIBRATION_BUF_SIZE, 20 / portTICK_RATE_MS);

		if( len > 0 ){
			//Write data back to UART0
			uart_write_bytes(uart_num, (const char*) data, len);

			//send sth to event queue
			pcStringToSend = (char*) malloc(CALIBRATION_BUF_SIZE);
			if( pcStringToSend != NULL ) {
				sprintf( pcStringToSend, "%s", data );

				xAFEEvent.eEventType = eCalibrationCmdEvent;
				xAFEEvent.pvData = (void*)pcStringToSend;

				xQueueSendToBack(afe_evt_queue, (const char*)&xAFEEvent , NULL);
				memset((void*)data, 0, CALIBRATION_BUF_SIZE );
			}
			else{
				//if memory error happened,exit the task
				ESP_LOGI(TAG, "malloc error happened in calibration cmd monitor task");
				break;
			}
		}
    }//while

    vTaskDelete(NULL);

}


 /*******************************************************************************
*  Global define
*
********************************************************************************/
esp_err_t afe_send_meter_snapshotdb_data_test()
{
	esp_err_t err;

	char *pbuf = (char*)malloc(512);
	if( pbuf == NULL ){
		err = ESP_ERR_NO_MEM;
		return err;
	}
	float measureValue = 0.0;
	char measureValueStr[64];

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

	if( pbuf ){
		free( pbuf );
		pbuf = NULL;
	}


	return ESP_OK;
}

void test_time_utc_local()
{
    time_t now = 1464248488;
    setenv("TZ", "UTC-8", 1);

    struct tm *tm_utc = gmtime(&now);
	const char* time_str1 = asctime(tm_utc);	
	printf(" time_str1 : %s\n", time_str1 );	
    struct tm *tm_local = localtime(&now);
	const char* time_str2 = asctime(tm_local);
	printf(" time_str2 : %s\n", time_str2 );

	//test2
	 char buf[64];
    struct tm tm = { 0 };
    tm.tm_year = 2016 - 1900;
    tm.tm_mon = 0;
    tm.tm_mday = 10;
    tm.tm_hour = 16;
    tm.tm_min = 30;
    tm.tm_sec = 0;
    time_t t = mktime(&tm);
    const char* time_str = asctime(&tm);
    strlcpy(buf, time_str, sizeof(buf));
    printf("Setting time: %s", time_str);
    struct timeval now1 = { .tv_sec = t };
    settimeofday(&now1, NULL);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t mtime = tv.tv_sec;
    struct tm mtm;
    localtime_r(&mtime, &mtm);
    time_str = asctime(&mtm);
    printf("Got time: %s", time_str);
    


}




 /*********************************************************************************************
 * @brief  calibration command monitor task here
 *
 *********************************************************************************************/
 void afe_poll_task()
 {
	int uart_num = UART_NUM_2;   
	portBASE_TYPE res;
	unsigned int loopcnt=0;
	char *pstr = NULL;

	unsigned short status = 0;
	 AFEEvent_t xReceivedAFEEvent;
	 xReceivedAFEEvent.eEventType = eNoEvent;
	 size_t heap_size;

	//start poll task now
	//
    int t = 0;
    float fVal = 0.0;
	do {
		 //wait for 0.2 sec for AFEEvent from other tasks
		 //
		 res = xQueueReceive(afe_evt_queue, (const char*)&xReceivedAFEEvent, 200 / portTICK_PERIOD_MS );
		 loopcnt++;

		 if(res == pdTRUE) {
			 //reset loop counter for next poll
			 loopcnt = 0;
            		
			  switch( xReceivedAFEEvent.eEventType ) 
			  {
			  case eCalibrationCmdEvent:

					printf(" recv calibration event, it is : %s\n", (char*)xReceivedAFEEvent.pvData );
					
					//get event buffer from queue item
					//
					pstr = xReceivedAFEEvent.pvData;

					//handle calibration event
					//
					afe_calibration_ev_handler( pstr );

					//free heap allocated by uart0 calibration command monitor task
					//
					if( pstr != NULL ){
						printf(" free sender buffer for calibration event .\n");
						free( pstr );
					}

					break;
			  default:
				  break;
			  }
		 }
		 else {
			 if( loopcnt >= 200 ) {

				heap_size = esp_get_free_heap_size();
				ESP_LOGI(TAG, "after 5 secs, so it can poll, heap size now is : %d.", heap_size);
				loopcnt = 0;

				//GPIO15 update
				//
				gpio_set_level(GPIO_NUM_15, 0);
				vTaskDelay(500 / portTICK_RATE_MS);
				//status=cs5480_read_reg(uart_num, Config0);
				//status=cs5480_read_reg(uart_num, Config1);
				//status=cs5480_read_reg(uart_num, PulseWidth);
				status=cs5480_select_page(uart_num, 0x10);//select page 16
				vTaskDelay(500 / portTICK_RATE_MS);
				status=cs5480_read_reg(uart_num, Config2);

                printf("sn:%s",meterSnapshotDb.meterSN);
                lcd_display(1234,0,5678,0,"");
                vTaskDelay(1000 / portTICK_RATE_MS);

				fVal = cs5480_update_U_r( uart_num, PHASE_NUM_A );
                printf("V----> %.1f\n",fVal);
                char strV[] = "V";
                lcd_display(fVal,1,10000,0,strV);
                vTaskDelay(1000 / portTICK_RATE_MS);
                
				fVal = cs5480_update_I_r( uart_num, PHASE_NUM_A );
                char strI[] = "I";
                lcd_display(fVal,1,10000,0,strI);
                printf("I----> %.1f\n",fVal);;
                vTaskDelay(1000 / portTICK_RATE_MS);

				fVal = cs5480_update_T_r( uart_num, PHASE_NUM_A );
                printf("T----> %.1f\n",fVal);
                char strT[] = "T";
                lcd_display(10000,0,fVal,1, strT);
                vTaskDelay(1000 / portTICK_RATE_MS);

				//fVal = cs5480_update_P_r( uart_num, PHASE_NUM_A );
				fVal = meterSnapshotDb.phaseA.Ae.measurementValue;
                printf("P----> %.3f\n",fVal);
                char strP[] = "E";
                int hNum = fVal/10;
                float lNum = fVal - hNum*10;
                if (hNum == 0) hNum = 10000;

                lcd_display(hNum,0,lNum,3,strP);
				vTaskDelay(100 / portTICK_RATE_MS);

				gpio_set_level(GPIO_NUM_15, 1);
                t++;
                if(t == 3) t=0;


                //lcd_V(meterSnapshotDb.phaseA.Vr.measurementValue);
                //vTaskDelay(500 / portTICK_RATE_MS);
                //lcd_I(meterSnapshotDb.phaseA.Ir.measurementValue);
                //vTaskDelay(500 / portTICK_RATE_MS);
                //char strR[] = "WR";
                //lcd_display_test(123.4,789.1,strR);
                //lcd_test(567.8);
                //lcd_E(meterSnapshotDb.phaseA.Ir.measurementValue);

			 }

		 }

	 }while(1);


	for(;;);
 }




//app main will call this
//
esp_err_t afe_comm_init()
{
	//create a queue to handle events to AFE from outside 
	//use AFEEvent_t struct  
    afe_evt_queue = xQueueCreate(10, sizeof( AFEEvent_t ));
	if( !afe_evt_queue ) {
		ESP_LOGI(TAG, "afe event queue create failed.");
		return ESP_ERR_NO_MEM;
	}


	//create a queue to handle gpio event from isr
    epg_gpio_evt_queue = xQueueCreate(50, sizeof(uint32_t));
	if( !epg_gpio_evt_queue ){
		ESP_LOGI(TAG, "epg gpio event queue create failed.");
		return ESP_ERR_NO_MEM;
	}



	//init  gpio for cs5480 select page signal
	//
	afe_init_sig_gpio();

	//init cs5480 EPG gpio for 12/14/32/33/34/35
	afe_init_epg_gpio();


	//init cs5480 and start softwarereset and contiv command
	//
	afe_init_cs5480();


	//before poll, dely 5 secs after init cs5480 
	//
	vTaskDelay(5000 / portTICK_RATE_MS);

	
	 //start epg gpio handler task
	 //
    xTaskCreate(epg_gpio_task_handler, "epg_gpio_task_handler", 2048, NULL, 12, NULL);

	//start uart2 calibration cmd monitor task
	//
	xTaskCreate(afe_calibration_command_monitor, "calibration_cmd_monitor", 1024, NULL, 10, NULL);

	//start afe poll task
	//
	xTaskCreate(afe_poll_task, "afe_poll_task", 2048, NULL, 10, NULL);


	//init uart2 and start uart2 event monitor task
	//
	afe_init_uart2();

	return ESP_OK;
}
