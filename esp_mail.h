#ifndef _ESP_MAIL_H
#define _ESP_MAIL_H



void mail_sendmail_test_func();
esp_err_t mail_send_mail( const char * pDestMailbox );

void mail_init(void);
int  mail_deinit(void);




#endif