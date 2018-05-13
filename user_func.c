/*
 * user_func.c
 *
 *  Created on: 2018年3月13日
 *      Author: Disblue
 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>


/*******************************************************************************
*  Global define
*
********************************************************************************/
int f_hexStrToByte2(char s[],char bits[]) {
    int i,n = 0;
    for(i = 0; s[i]; i += 2) {
        if(s[i] >= 'A' && s[i] <= 'F')
            bits[n] = s[i] - 'A' + 10;
        else bits[n] = s[i] - '0';

        if(s[i + 1] >= 'A' && s[i + 1] <= 'F')
            bits[n] = (bits[n] << 4) | (s[i + 1] - 'A' + 10);
        else bits[n] = (bits[n] << 4) | (s[i + 1] - '0');
        ++n;
    }
    return n;
}
/*******************************************************************************
*  Global define
*
********************************************************************************/
int f_testHexStrToByte2(void) {
    char s[] = "E4F1C3A81F";
    char bits[10];
    int i,n = f_hexStrToByte2(s,bits);
    printf ("------>source:%s\n",s);
    for(i = 0;i < n;i++)
        printf ("%x ",0xFF & bits[i]);

    printf("\n");
    return 0;

}
/*******************************************************************************
*  Global define
*
********************************************************************************/
void f_hexStrToByte(const char* source, unsigned char* dest, int sourceLen)
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

/*******************************************************************************
*  Global define
*
********************************************************************************/
int f_testHexStrToByte(void) {
	char s[] = "E4F1C3A81F";
    unsigned char bits[10];
	f_hexStrToByte( s, bits, 10 );
	for(int i = 0;i <9 ;i++)
        printf ("%x ",0xFF & bits[i]);

    printf("\n");
	return 0;
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
int f_translat(char c)
{
	if(c<='9'&&c>='0') return c-'0';
	if(c>='a' && c<='f') return c-87;
	if(c>='A' && c<='F') return c-55;
	return -1;//其它字符返回-1
}

/*******************************************************************************
*  Global define
*
********************************************************************************/
int f_hextoi(char *str)
{
	int length=strlen(str);
	if(length==0) return 0;
	int i,n=0,stat;
	for(i=0;i<length;i++){
	  stat=f_translat(str[i]);//防错处理
	  if(stat>=0) n=n*16+stat;
	}
	return n;
}
