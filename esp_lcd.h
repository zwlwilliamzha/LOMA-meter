
#if !defined(_ESP_LCD_H_)
#define _ESP_LCD_H_
#include <sys/time.h>
#include <stdint.h>

void    lcd_cs_clear();
void    lcd_cs();
void    lcd_wr(int cmdbit);
void    lcd_l1_num(int num);
void    lcd_l2_num(int num);
void    lcd_config();
void    lcd_clear_all();
void    lcd_init();
void 	lcd_display(double hNum, int hPos, double lNum, int lPos, char txt[]);
#endif /* _ESP_LCD_H_ */
