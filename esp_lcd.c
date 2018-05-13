#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h" 
#define GPIO_OUTPUT_IO_18    18
#define GPIO_OUTPUT_IO_21    21		//LCD Data
#define GPIO_OUTPUT_IO_25    25		//LCD CS
#define GPIO_OUTPUT_IO_26    26		//LCD RD
#define GPIO_OUTPUT_IO_27    27		//LCD WR
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_18)  | (1<<GPIO_OUTPUT_IO_21) | (1<<GPIO_OUTPUT_IO_25) | (1<<GPIO_OUTPUT_IO_26) | (1<<GPIO_OUTPUT_IO_27)  )

void lcd_cs()
{
	gpio_set_level(GPIO_NUM_25, 0);					//CS_LOW
	vTaskDelay(10 / portTICK_RATE_MS);
}

void lcd_cs_clear()
{
	vTaskDelay(10 / portTICK_RATE_MS);
	gpio_set_level(GPIO_NUM_25, 1);	 				//CS_HIGH
	vTaskDelay(20 / portTICK_RATE_MS);
}

void lcd_wr(int cmdbit)
{
	gpio_set_level(GPIO_NUM_27, 0);					//WR_LOW
	vTaskDelay(10 / portTICK_RATE_MS);
	gpio_set_level(GPIO_NUM_21, cmdbit);			//WR_LOW
	vTaskDelay(10 / portTICK_RATE_MS);
	gpio_set_level(GPIO_NUM_27, 1);					//WR_HIGH
	vTaskDelay(20 / portTICK_RATE_MS);
}

void lcd_config()
{
    lcd_wr(1);lcd_wr(0);lcd_wr(0);					//Operation Command 100
    lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);				//SYS EN
    lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(1);lcd_wr(1);    		
    lcd_wr(0);lcd_wr(0);lcd_wr(1);lcd_wr(0);				//BIAS 1/3 COM 4
    lcd_wr(1);lcd_wr(0);lcd_wr(1);lcd_wr(1);lcd_wr(1);    
    lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);				//LCD ON
    lcd_wr(0);lcd_wr(0);lcd_wr(1);lcd_wr(1);lcd_wr(1);    
}
void lcd_clear_all()
{
	int seg=0;
    lcd_cs();
	vTaskDelay(50 / portTICK_RATE_MS);
	lcd_wr(1);lcd_wr(0);lcd_wr(1);					//Write Command 101
	lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);	//Address A[5]->A[0]
	for (seg = 0; seg < 18; seg++){
    	lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0); 		//Data  D[0]->D[3]
	}
	vTaskDelay(50 / portTICK_RATE_MS);
    lcd_cs_clear();
}

//high data
int lcd_h_num(int num)
{
    int numB = 0b0;
    switch(num) {
        case 0:numB = 0b01111101;break;
        case 1:numB = 0b00000101;break;
        case 2:numB = 0b01011011;break;
        case 3:numB = 0b00011111;break;
        case 4:numB = 0b00100111;break;
        case 5:numB = 0b00111110;break;
        case 6:numB = 0b01111110;break;
        case 7:numB = 0b00010101;break;
        case 8:numB = 0b01111111;break;
        case 9:numB = 0b00111111;break;
        default:numB = 0b00000000;break;
    }
    return numB;
}
//low data
int lcd_l_num(int num)
{
    int numB = 0b0;
    switch(num) {
        case 0:numB = 0b11101011;break;
        case 1:numB = 0b01100000;break;
        case 2:numB = 0b11000111;break;
        case 3:numB = 0b11100101;break;
        case 4:numB = 0b01101100;break;
        case 5:numB = 0b10101101;break;
        case 6:numB = 0b10101111;break;
        case 7:numB = 0b11100000;break;
        case 8:numB = 0b11101111;break;
        case 9:numB = 0b11101101;break;
        default:numB = 0b00000000;break;
    }
    return numB;
}
//根据数据num,小数位数pos 上部或下部显示highFlag, 返回上部或下部数组rs
void getNumArr(double num, int pos, bool highFlag, int *rs){
    int i = 0, n = 4, dot = -1, len = 0;
    int dotPos = highFlag ? 0b10000000 : 0b00010000;
    int rsArr[4]={0,0,0,0};
    char str[64];
    if(pos > 3 || pos < 0) pos = 1;
    if(num < 1000){
    	switch(pos){
    		case 0 : sprintf(str, "%.0f", num);break;
    		case 1 : sprintf(str, "%.1f", num);break;
    		case 2 : sprintf(str, "%.2f", num);break;
    		case 3 : sprintf(str, "%.3f", num);break;
    		default:sprintf(str, "%.1f", num);break;
    	}
    }
    else{
        sprintf(str, "%.0f", num);
    }
    //printf("str=%s\n",str);
    len = strlen(str);
    int m = 0;
    for(i = len-1; i >= 0; i--) {
        if(str[i] == '-' || str[i] == '.') {
            if(str[i] == '.'){
                dot = m;
            }
        }else {
            if(n-- > 0){
                *(rs+n) = highFlag ? lcd_h_num(str[i]-'0') : lcd_l_num(str[i]-'0');
                //printf("=%d\n", *(rs+n));
            }
        }
        m++;
    }
    if(highFlag){
        rsArr[0] = *(rs);
        rsArr[1] = *(rs+1);
        rsArr[2] = *(rs+2);
        rsArr[3] = *(rs+3);
    }
    else{
        rsArr[0] = *(rs+3);
        rsArr[1] = *(rs+2);
        rsArr[2] = *(rs+1);
        rsArr[3] = *(rs);
    }
    //printf("len=%d,dot=%d\n",len,dot);
	if(dot > 0 && dot < 4){
		if(!highFlag) dot = 4 - dot;
		rsArr[3+1-dot] = rsArr[3+1-dot] | dotPos;
	}
    *(rs) = rsArr[0];
    *(rs+1) = rsArr[1];
    *(rs+2) = rsArr[2];
    *(rs+3) = rsArr[3];
}
//8位二进制显示命令
void display_bit8(int num){
    int i;
    for(i=7; i>=0; i--){
        int tt = num>>i&1;
        //printf( "%d", tt);
        //printf( "lcd_wr(%d);", tt);
        lcd_wr(tt);
    }
    //printf( "\n");
}
//4位二进制显示命令
void display_bit4(int num){
    int i;
    for(i=3; i>=0; i--){
        int tt = num>>i&1;
        //printf( "%d", tt);
        //printf( "lcd_wr(%d);", tt);
        lcd_wr(tt);
    }
    //printf( "\n");
}
//led 命令 显示上部，下部数值，文本
void lcd_display_cmd(double highNum, int highPos, double lowNum, int lowPos, char txt[]){
    int h_rs[4]={0,0,0,0};
    int l_rs[4]={0,0,0,0};
    int l_rs_o[4]={0,0,0,0};
    if(highNum < -9999 || highNum > 9999){
        
    }
    else{
        getNumArr(highNum, highPos, true, h_rs);
    }
    if(lowNum < -9999 || lowNum > 9999){
    }
    else{
        getNumArr(lowNum, lowPos, false, l_rs);
    }

    int str_mid = 0b0000;
    int str_end = 0b0000;
    int i;
    for(i = 0; txt[i]; i++) {
        switch(txt[i]){
            case 'V':h_rs[0] = h_rs[0]|0b10000000;break;
            case '$':l_rs[0] = l_rs[0]|0b00010000;break;
            
            case 'I':str_end = str_end | 0b1000;break;
            case 'W':str_end = str_end | 0b0100;break;
            case 'E':str_end = str_end | 0b0010;break;
            case 'T':str_end = str_end | 0b0001;break;
            
            case 'R':str_mid = str_mid | 0b1000;break;
            case 'C':str_mid = str_mid | 0b0100;break;
            case 'B':str_mid = str_mid | 0b0010;break;
            case 'A':str_mid = str_mid | 0b0001;break;
            default:break;
        }
    }
    
   ///printf( "-----\n");
   for( i = 0; i < 4; i++ ){
       //printf( "h_rs %d : %d\n", i, h_rs[i]);
       display_bit8(h_rs[i]);
   }
   //printf( "str_mid : %d\n", str_mid);
   display_bit4(str_mid);
   for( i = 0; i < 4; i++ ){
       //printf( "l_rs %d : %d\n", i, l_rs[i]);
       display_bit8(l_rs[i]);
   }
   //printf( "str_mid : %d\n", str_end);
   display_bit4(str_end);
   ///printf( "-----\n");
}


//显示上下数值。0.1
void lcd_display(double hNum, int hPos, double lNum, int lPos, char txt[])
{
    lcd_clear_all();
    printf("===> in lcd_test %.1f, %.1f, %s\n",hNum,lNum,txt);
    lcd_cs();
    vTaskDelay(50 / portTICK_RATE_MS);
    lcd_wr(1);lcd_wr(0);lcd_wr(1);					//Write Command 101
    lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);lcd_wr(0);	//Address A[5]->A[0]
    lcd_display_cmd(hNum,hPos,lNum,lPos,txt);

    vTaskDelay(2000 / portTICK_RATE_MS);
    lcd_cs_clear();   
}


void lcd_init()
{
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
    
    vTaskDelay(3000 / portTICK_RATE_MS);
    lcd_cs();
    vTaskDelay(50 / portTICK_RATE_MS);
    lcd_config();
    vTaskDelay(50 / portTICK_RATE_MS);
    lcd_cs_clear();
    vTaskDelay(1000 / portTICK_RATE_MS);
    lcd_clear_all();
    vTaskDelay(1000 / portTICK_RATE_MS);
}
