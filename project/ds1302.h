#ifndef __DS1302_H_
#define __DS1302_H_

//---包含头文件---//
#include<reg52.h>
#include<intrins.h>
#include"i2c.h"


//---重定义关键词---//


//---定义ds1302使用的IO口---//
sbit DSIO=P3^6;
sbit RST=P3^5;
sbit SCLK=P3^7;

//---定义全局函数---//
void Ds1302Write(uchar addr, uchar dat);
uchar Ds1302Read(uchar addr);
void Ds1302Init();
void Ds1302ReadTime();
void time_change();



//---加入全局变量--//
extern uchar TIME[7];
extern uchar TIME2[7];	

#endif