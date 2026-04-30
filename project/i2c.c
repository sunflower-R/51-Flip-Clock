#include"i2c.h"

/*******************************************************************************
* 函数名         : Delay10us()
* 函数功能		   : 延时10us
* 输入           : 无
* 输出         	 : 无
*******************************************************************************/


void Delay10us()
{
	uchar a,b;
	for(b=1;b>0;b--)
	{
	for(a=2;a>0;a--);
	}
}

void delay(uchar i)
{
    while(i--);
}
/*******************************************************************************
* 函数名         : I2cStart()
* 函数功能		 : 起始信号：在SCL时钟信号在高电平期间SDA信号产生一个下降沿
* 输入           : 无
* 输出         	 : 无
* 备注           : 起始之后SDA和SCL都为0
*******************************************************************************/

void I2cStart()
{
	SDA=1;
	Delay10us();
	SCL=1;
	Delay10us();//建立时间是SDA保持时间>4.7us
	SDA=0;
	Delay10us();//保持时间是>4us
	SCL=0;			
	Delay10us();		
}
/*******************************************************************************
* 函数名         : I2cStop()
* 函数功能		 : 终止信号：在SCL时钟信号高电平期间SDA信号产生一个上升沿
* 输入           : 无
* 输出         	 : 无
* 备注           : 结束之后保持SDA和SCL都为1；表示总线空闲
*******************************************************************************/

void I2cStop()
{
	SDA=0;
	Delay10us();
	SCL=1;
	Delay10us();//建立时间大于4.7us
	SDA=1;
	Delay10us();		
}
/*******************************************************************************
* 函数名         : I2cSendByte(unsigned char dat)
* 函数功能		 : 通过I2C发送一个字节。在SCL时钟信号高电平期间，保持发送信号SDA保持稳定
* 输入           : num
* 输出         	 : 0或1。发送成功返回1，发送失败返回0
* 备注           : 发送完一个字节SCL=0,SDA=1
*******************************************************************************/

uchar I2cSendByte(uchar dat)
{
    uchar a=0,b=0;//最大255，一个机器周期为1us，最大延时255us。		
	for(a=0;a<8;a++)//要发送8位，从最高位开始
	{
		SDA=dat>>7;	 //起始信号之后SCL=0，所以可以直接改变SDA信号
		dat=dat<<1;
		Delay10us();
		SCL=1;
		Delay10us();//建立时间>4.7us
		SCL=0;
		Delay10us();//时间大于4us		
	}
	SDA=1;
	Delay10us();
	SCL=1;
	while(SDA)//等待应答，也就是等待从设备把SDA拉低
	{
		b++;
		if(b>200)	 //如果超过2000us没有应答发送失败，或者为非应答，表示接收结束
		{
			SCL=0;
			Delay10us();
			return 0;
		}
	}
	SCL=0;
	Delay10us();
 	return 1;		
}



/*******************************************************************************
* 函数名         : void oled_Write_cmd(unsigned char addr,unsigned char cmd)
* 函数功能		   : 往oled写命令，与lcd1602相似
* 输入           : 无
* 输出         	 : 无
*******************************************************************************/

void oled_Write_cmd(uchar cmd)
{
	I2cStart();
	I2cSendByte(0x78);//发送写器件地址
	I2cSendByte(0x00);//发送要写入内存地址
	I2cSendByte(cmd);	//写入命令
	I2cStop();
}

/*******************************************************************************
* 函数名         : void oled_Write_data(unsigned char dat)
* 函数功能		   : 往oled写数据，与lcd1602相似
* 输入           : 无
* 输出         	 : 无
*******************************************************************************/

void oled_Write_data(uchar dat)
{
	I2cStart();
	I2cSendByte(0x78);//发送写器件地址
	I2cSendByte(0x40);//发送要写入内存地址
	I2cSendByte(dat);	//写入数据
	I2cStop();
}

void oled_clear()  //页寻址下的oled清屏函数
{	uchar i,j;
     
	oled_Write_cmd(0x20);
	oled_Write_cmd(0x02);
    for(i=0;i<8;i++)
	{
	   	oled_Write_cmd(0xb0+i);
		oled_Write_cmd(0x00);
		oled_Write_cmd(0x10);
		for(j=0;j<128;j++)
		{
		   oled_Write_data(0x00); 
		}

	}	
}



void oled_initial()//oled初始化函数
{
    delay(500);
			 
	oled_Write_cmd(0xae);//--turn off oled panel 关闭显示
    oled_Write_cmd(0x00);//---set low column address设置起始列的低四位 0x0x
    oled_Write_cmd(0x10);//---set high column address设置起始列的高四位0x1x
    oled_Write_cmd(0x40);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    oled_Write_cmd(0x81);//--set contrast control register设置对比度
    oled_Write_cmd(0xff); // Set SEG Output Current Brightness对比度为oxff
    oled_Write_cmd(0xa1);//--Set SEG/Column Mapping     0xa0左右反置 0xa1正常
    oled_Write_cmd(0xc8);//Set COM/Row Scan Direction   0xc0上下反置 0xc8正常
    oled_Write_cmd(0xa6);//--set normal display
    oled_Write_cmd(0xa8);//--set multiplex ratio(1 to 64)
    oled_Write_cmd(0x3f);//--1/64 duty
    oled_Write_cmd(0xd3);//-set display offset    Shift Mapping RAM Counter (0x00~0x3F)
    oled_Write_cmd(0x00);//-not offset
    oled_Write_cmd(0xd5);//--set display clock divide ratio/oscillator frequency
    oled_Write_cmd(0x80);//--set divide ratio, Set Clock as 100 Frames/Sec
    oled_Write_cmd(0xd9);//--set pre-charge period
    oled_Write_cmd(0xf1);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    oled_Write_cmd(0xda);//--set com pins hardware configuration
    oled_Write_cmd(0x12);
    oled_Write_cmd(0xdb);//--set vcomh
    oled_Write_cmd(0x40);//Set VCOM Deselect Level
    oled_Write_cmd(0x20);//-Set Page Addressing Mode (0x00/0x01/0x02)设置地址模式
						//水平寻址，垂直寻址，页寻址
    oled_Write_cmd(0x02);//	地址模式为页寻址
    oled_Write_cmd(0x8d);//--set Charge Pump enable/disable
    oled_Write_cmd(0x14);//--set(0x10) disable
    oled_Write_cmd(0xa4);// Disable Entire Display On (0xa4/0xa5)
    oled_Write_cmd(0xa6);// Disable Inverse Display On (0xa6/a7) 
    oled_Write_cmd(0xaf);//--turn on oled panel开启显示

	delay(100);
	oled_clear();//清屏
}

void oled_put_char_16x16(uchar x,uchar y,uchar t)/*设置显示坐标函数,t为0时，字符为8x16
                                                    t为1时，字符为16x16*/                 
{	 
     oled_Write_cmd(0x20);
	 oled_Write_cmd(0x00);//设置地址模式为水平选址
     //set page
     oled_Write_cmd(0x22);
	 oled_Write_cmd(y*2);
	 oled_Write_cmd(0x01+y*2);

	 //set colum
     oled_Write_cmd(0x21);
	 oled_Write_cmd((0x08+0x08*t)*x);
	 oled_Write_cmd((0x08+0x08*t)*x+(0x07+0x08*t));

	 




}





