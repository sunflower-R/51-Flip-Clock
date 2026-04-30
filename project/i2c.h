#ifndef __I2C_H_
#define __I2C_H_
#include <reg52.h>



typedef  unsigned char uchar;
typedef unsigned int  uint;

sbit SCL=P1^0;
sbit SDA=P1^1;


void I2cStart();
void I2cStop();
uchar I2cSendByte(uchar dat);

void oled_Write_cmd(uchar cmd);
void oled_Write_data(uchar dat);
void oled_initial();
void oled_clear( );
void delay( uchar i);
void Delay10us();
void oled_put_char_16x16(uchar x,uchar y,uchar t);
 
                       
                        



#endif
