#ifndef __AFE_H__
#define __AFE_H__

#include "freertos/FreeRTOS.h"

/***************************************************************************
**	Register definition
***************************************************************************/
#define CHANNEL1 0x00
#define CHANNEL2 0x01
//Page0
#define	Config0	0x00//Default=0x000000
#define	Config1	0x01//Default=0x00EEEE
#define	Mask	0x03
#define	PCreg		0x05
#define	SerialCtrl	0x07
#define	PulseWidth	0x08
#define	PulseCtrl	0x09
#define	Status0	0x17
#define	Status1	0x18
#define	Status2	0x19
#define	RegLock	0x22
#define V1PeakVoltage  0x24
#define I1PeakCurrent  0x25
#define V2PeakVoltage  0x26
#define I2PeakVoltage  0x27
#define PSDC  0x30
#define ZXnum    0x37
//Page16
#define	Config2	0x00
#define	RegChk	0x01
#define I1	0x02
#define V1	0x03
#define Power1	0x04
#define	P1avg	0x05
#define	I1rms	0x06
#define	V1rms	0x07
#define I2	0x08
#define V2	0x09
#define Power2	0x0A
#define	P2avg	0x0B
#define	I2rms	0x0C
#define	V2rms	0x0D
#define Q1avg	0x0E
#define Q1	0x0F
#define Q2avg	0x10
#define Q2	0x11
#define S1	0x14
#define	PF1	0x15
#define	S2	0x18
#define	PF2	0x19
#define Temperature	0x1B

#define I1peak	0x12
#define V1peak	0x13

#define I2peak	0x16
#define V2peak	0x17


#define Psum	0x1D
#define Ssum	0x1E
#define Qsum	0x1F
#define I1dcoff	0x20
#define	I1gain	0x21
#define	V1dcoff	0x22
#define	V1gain	0x23
#define	P1off	0x24
#define I1acoff	0x25
#define V1acoff	0x26
#define I2dcoff	0x27
#define	I2gain	0x28
#define V2dcoff	0x29
#define	V2gain	0x2A
#define	P2off	0x2B
#define I2acoff	0x2C
#define Q2off	0x2D
#define	Epsilon	0x31
#define	Ichlevel	0x32
#define	SampleCount	0x33

#define	Tgain	0x36
#define	Toff	0x37
#define Pmin	0x38
#define	Tsettle	0x39
#define	Loadmin	0x3A
#define VFrms   0x3B
#define	SysGain	0x3C
#define	SysTime	0x3D


//Page17
#define	V1Sagdur	0x00
#define	V1Saglevel	0x01
#define	I1Overdur	0x04
#define	I1Overlevel	0x05
#define	V2Sagdur	0x08
#define V2Saglevel	0x09
#define	I2Overtdur	0x0C
#define	I2Overlevel	0x0D

//Page18
#define IZXlevel    0x18
#define	PulseRate	0x1C
#define	INTgain		0x2B
#define	V1Swelldur	0x2E
#define	V1Swelllevel	0x2F
#define	V2Swelldur	0x32
#define	V2Swelllevel	0x33
#define	ZXlevel	0x3A
#define	CycleCount	0x3E
#define	Scale	0x3F

//Instruction
#define	AFESWReset	0x01
#define	AFEStandby	0x02
#define	AFEWakeup	0x03
#define	SigneConv	0x14
#define	ContiConv	0x15
#define StopConv	0x18
#define CaliI1acoff 0x31
#define CaliI2acoff 0x33
#define CaliI1dcoff 0x21
#define CaliI2dcoff 0x23
#define	CaliI1gn	0x39
#define	CaliV1gn	0x3A
#define	CaliI2gn	0x3B
#define	CaliV2gn	0x3C

#define BUF_SIZE (1024)

/***************************************************************************
**  Macro define
***************************************************************************/
#define	WRITE_PROTECT	1
#define USE_SERIAL_CHECKSUM	 0  //use checksum
#define AFEREV	0xB0


#define BUF_SIZE (1024)
#define CALIBRATION_BUF_SIZE 64

#define ESP_INTR_FLAG_DEFAULT 0

#define UART2_TXD  (1)
#define UART2_RXD  (3)
#define UART0_TXD  (16)
#define UART0_RXD  (17)

#define GPIO_OUTPUT_IO_18    18
#define GPIO_OUTPUT_IO_15    15
#define GPIO_OUTPUT_IO_5    5

#define GPIO_INPUT_IO_12     12
#define GPIO_INPUT_IO_14     14
#define GPIO_INPUT_IO_32     32
#define GPIO_INPUT_IO_33     33
#define GPIO_INPUT_IO_34     34
#define GPIO_INPUT_IO_35     35


//#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_18)  | (1<<GPIO_OUTPUT_IO_15) | (1<<GPIO_OUTPUT_IO_5)  )
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_18) )



/***************************************************************************
*	data type definition
***************************************************************************/
typedef union
{        
	unsigned char auc[4];
	unsigned long ans;
}Dual;


typedef struct{
	char reqCmdType;
	char buff[64];  //equal to CALIBRATION_BUF_SIZE
	int buffLen;
}calibrationReqData;

typedef enum {
    PHASE_NUM_0 = 0x0,  
    PHASE_NUM_A = 0x1,  
    PHASE_NUM_B = 0x2,  
	PHASE_NUM_C = 0x3,
    PHASE_NUM_MAX,
} meter_phase_t;

//general event define for AFE module
//
typedef enum {
	eNoEvent = 0,
	eCalibrationCmdEvent,
	eUnknownEvent
}eAFEEventType_t;

typedef struct {
	//an enumerated type that identifies the event
	eAFEEventType_t eEventType;
	//a generic pointer that can point to a buffer
	void *pvData;
}AFEEvent_t;


//measurement snapshot define
typedef struct {
	float measurementValue;
	char* measurementUnit;
}MetricMeasurement;


typedef struct { 
	uint32_t timeStamp;
	MetricMeasurement Vr;
	MetricMeasurement Ir;
	MetricMeasurement Ap;
	MetricMeasurement Rp;
	MetricMeasurement Ae;
	MetricMeasurement Re;
	MetricMeasurement Pf; 
	MetricMeasurement Hz;
	MetricMeasurement Tc;
	MetricMeasurement Ad;
	MetricMeasurement Rd;
	MetricMeasurement LastQuarterAe;
	MetricMeasurement LastQuarterRe;
	MetricMeasurement ReverseAe;
	MetricMeasurement ReverseRe;

}MeterSinglePhaseMeasurement;

typedef struct{
	char meterSN[32];
	SemaphoreHandle_t ssdb_mux;           /*snapshotdb mutex*/
	MeterSinglePhaseMeasurement phaseA;
	MeterSinglePhaseMeasurement phaseB;
	MeterSinglePhaseMeasurement phaseC;

}MeterSnapshotDb;


#define KEY_CALIBRATION_INFO "calibrationpara" // Key used in NVS for calibration
#define CALIBRATION_NAMESPACE "calibration" // Namespace in NVS for calibration

#define CS5480_BASE_U  240
#define CS5480_BASE_I  5



//calibration struct 
typedef struct {
	unsigned char csI1gain[3];
	unsigned char csI1acoff[3];
	unsigned char csI1dcoff[3];
	unsigned char csV1gain[3];
	unsigned char csV1dcoff[3];
	unsigned char csPulseRate[3];
	unsigned char csPulseWidth[3];
	unsigned char csConfig0[3];
	unsigned char csConfig1[3];
	unsigned char csConfig2[3];
	unsigned char csTsettle[3];
	int csVmax;
	int csImax;

}CS5480_CalibrationPara;

typedef struct {
	char meterSN[32];   //we can get meterSN by use of calibration
	CS5480_CalibrationPara PhaseA;
	CS5480_CalibrationPara PhaseB;
	CS5480_CalibrationPara PhaseC;

}MeterCalibrationPara;

//Energy Pulse Count here
typedef struct {
	int PhaseAEPC;          //Ae
	int PhaseBEPC;          //Be
	int PhaseCEPC;          //Ce
	int ReversePhaseAEPC;    //ReverseAe
	int ReversePhaseBEPC;    //ReverseBe
	int ReversePhaseCEPC;    //ReverseCe
	int CapacitiveRePhaseAEPC;    //CapReA
	int CapacitiveRePhaseBEPC;    //CapReB
	int CapacitiveRePhaseCEPC;    //CapReC
	int InductiveRePhaseAEPC;     //IndReA
	int InductiveRePhaseBEPC;     //IndReB
	int InductiveRePhaseCEPC;     //IndReC
	
}MeterEnergyPulseCounter;


/***************************************************************************
**	Variable declaration
***************************************************************************/

extern volatile Dual gAFEBUF;//4bytes TXD/RXD buffer
extern MeterCalibrationPara meterCalibrationPara;
//snapshot db for meter metric
extern MeterSnapshotDb meterSnapshotDb;
//meter active energy pulse counter
extern MeterEnergyPulseCounter meterEPC;

void afe_calibration_ev_handler( const char * pstr );
void afe_calibration_ev_handler_Irms( const char * pstr );
void afe_calibration_ev_handler_Vrms( const char * pstr );
void afe_calibration_ev_handler_Vgain( const char * pstr );
void afe_calibration_ev_handler_Vcal( const char * pstr );
void afe_calibration_ev_handler_Igain( const char * pstr );
void afe_calibration_ev_handler_Ical( const char * pstr );
void afe_calibration_ev_handler_Vipf( const char * pstr );
void afe_calibration_ev_handler_Iacoff( const char * pstr );
void afe_calibration_ev_handler_Idcoff( const char * pstr );
void afe_calibration_ev_handler_Iacoffcal( const char * pstr );
void afe_calibration_ev_handler_Idcoffcal( const char * pstr );
void afe_calibration_ev_handler_Pulse( const char * pstr );
void afe_calibration_ev_handler_Width( const char * pstr );
void afe_calibration_ev_handler_Config0( const char * pstr );
void afe_calibration_ev_handler_Config1( const char * pstr );
void afe_calibration_ev_handler_Config2( const char * pstr );
void afe_calibration_ev_handler_Conva( const char * pstr );
void afe_calibration_ev_handler_Convb( const char * pstr );
void afe_calibration_ev_handler_Convc( const char * pstr );
void afe_calibration_ev_handler_Tsettle( const char * pstr );
void afe_calibration_ev_handler_Sysgain( const char * pstr );
void afe_calibration_ev_handler_Vmax( const char * pstr );
void afe_calibration_ev_handler_Imax( const char * pstr );
void afe_calibration_ev_handler_cmd( const char * pstr );

  void afe_init_meter_snapshotdb();
esp_err_t afe_comm_init();
esp_err_t afe_init_uart2();


#endif
