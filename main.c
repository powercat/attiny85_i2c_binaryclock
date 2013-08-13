//#define F_CPU 8000000 //Needed for delay.h
//Include libraries necessary for project

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <compat/twi.h>
#include <avr/power.h>
#include "hc4094.h"

//comment out to disable debugging
//#define DEBUGGING

#ifdef DEBUGGING

#include "rs232_debug.h"

#endif // DEBUGGING

/*
*
* This section is Avinash's Software I2C implementation
*
*/

#define SCLPORT	PORTB	//TAKE PORTD as SCL OUTPUT WRITE
#define SCLDDR	DDRB	//TAKE DDRB as SCL INPUT/OUTPUT configure

#define SDAPORT	PORTB	//TAKE PORTD as SDA OUTPUT WRITE
#define SDADDR	DDRB	//TAKE PORTD as SDA INPUT configure

#define SDAPIN	PINB	//TAKE PORTD TO READ DATA
#define SCLPIN	PINB	//TAKE PORTD TO READ DATA

#define SCL	PB4		//PORTb.0 PIN AS SCL PIN
#define SDA	PB3		//PORTb.3 PIN AS SDA PIN

#define SOFT_I2C_SDA_LOW	SDADDR|=((1<<SDA)) //Macros to toggle the ports
#define SOFT_I2C_SDA_HIGH	SDADDR&=(~(1<<SDA))

#define SOFT_I2C_SCL_LOW	SCLDDR|=((1<<SCL))
#define SOFT_I2C_SCL_HIGH	SCLDDR&=(~(1<<SCL))

#define Q_DEL _delay_loop_2(3) //Some delay functions
#define H_DEL _delay_loop_2(5)

void SoftI2CInit() //I2C init timing sequence. details in datasheet
{
	SDAPORT&=(1<<SDA);
	SCLPORT&=(1<<SCL);

	SOFT_I2C_SDA_HIGH;
	SOFT_I2C_SCL_HIGH;

}

void SoftI2CStart() //I2C start sequence. see datasheet
{
	SOFT_I2C_SCL_HIGH;
	H_DEL;

	SOFT_I2C_SDA_LOW;
	H_DEL;
}

void SoftI2CStop()//I2C stop sequence. see datasheet
{
	SOFT_I2C_SDA_LOW;
	H_DEL;
	SOFT_I2C_SCL_HIGH;
	Q_DEL;
	SOFT_I2C_SDA_HIGH;
	H_DEL;
}

uint8_t SoftI2CWriteByte(uint8_t data) //I2C write byte sequence. see datasheet
{
	uint8_t i;

	for(i=0;i<8;i++)
	{
		SOFT_I2C_SCL_LOW;
		Q_DEL;

		if(data & 0x80)
		SOFT_I2C_SDA_HIGH;
		else
		SOFT_I2C_SDA_LOW;

		H_DEL;

		SOFT_I2C_SCL_HIGH;
		H_DEL;

		while((SCLPIN & (1<<SCL))==0);

		data=data<<1;
	}

	//The 9th clock (ACK Phase)
	SOFT_I2C_SCL_LOW;
	Q_DEL;

	SOFT_I2C_SDA_HIGH;
	H_DEL;

	SOFT_I2C_SCL_HIGH;
	H_DEL;

	uint8_t ack=!(SDAPIN & (1<<SDA));

	SOFT_I2C_SCL_LOW;
	H_DEL;

	return ack;

}


uint8_t SoftI2CReadByte(uint8_t ack)//I2C read byte. see datasheet
{
	uint8_t data=0x00;
	uint8_t i;

	for(i=0;i<8;i++)
	{

		SOFT_I2C_SCL_LOW;
		H_DEL;
		SOFT_I2C_SCL_HIGH;
		H_DEL;

		while((SCLPIN & (1<<SCL))==0);

		if(SDAPIN &(1<<SDA))
		data|=(0x80>>i);

	}

	SOFT_I2C_SCL_LOW;
	Q_DEL;						//Soft_I2C_Put_Ack

	if(ack)
	{
		SOFT_I2C_SDA_LOW;
	}
	else
	{
		SOFT_I2C_SDA_HIGH;
	}
	H_DEL;

	SOFT_I2C_SCL_HIGH;
	H_DEL;

	SOFT_I2C_SCL_LOW;
	H_DEL;

	return data;

}

/*
*
* DS1307 functions from Avinash's site
*
*/

#define BOOL uint8_t //Define a datatype

#define DS1307_SLA_W 0xD0 //Write address of the DS1307
#define DS1307_SLA_R 0xD1 //Read address

#define TRUE	1 //Define return values from Soft I2C functions
#define FALSE	0

void DS1307Init(void)
{
	SoftI2CInit();
}

/***************************************************

Function To Read Internal Registers of DS1307
---------------------------------------------

address : Address of the register
data: value of register is copied to this.


Returns:
0= Failure
1= Success
***************************************************/

BOOL DS1307Read(uint8_t address,uint8_t *data)
{
	uint8_t res;   //result

	//Start

	SoftI2CStart();

	//SLA+W (for dummy write to set register pointer)
	res=SoftI2CWriteByte(DS1307_SLA_W); //DS1307 address + W

	//Error
	if(!res) return FALSE;

	//Now send the address of required register

	res=SoftI2CWriteByte(address);

	//Error
	if(!res) return FALSE;

	//Repeat Start
	SoftI2CStart();

	//SLA + R
	res=SoftI2CWriteByte(DS1307_SLA_R); //DS1307 Address + R

	//Error
	if(!res) return FALSE;

	//Now read the value with NACK
	*data=SoftI2CReadByte(0);

	//Error

	if(!res) return FALSE;

	//STOP
	SoftI2CStop();

	return TRUE;
}

/***************************************************

Function To Write Internal Registers of DS1307

---------------------------------------------

address : Address of the register
data: value to write.


Returns:
0= Failure
1= Success
***************************************************/

BOOL DS1307Write(uint8_t address,uint8_t data)
{
	uint8_t res;   //result

	//Start
	SoftI2CStart();

	//SLA+W
	res=SoftI2CWriteByte(DS1307_SLA_W); //DS1307 address + W

	//Error
	if(!res) return FALSE;

	//Now send the address of required register
	res=SoftI2CWriteByte(address);

	//Error
	if(!res) return FALSE;

	//Now write the value

	res=SoftI2CWriteByte(data);

	//Error
	if(!res) return FALSE;

	//STOP
	SoftI2CStop();

	return TRUE;
}

//Writes two bytes to shift register. Data1 will be the second shift register
void DualHC4094Write(uint8_t data1, uint8_t data2)
{
	//Send each 8 bits serially
	uint8_t i;
	for(i=0;i<8;i++)
	{
		//Output the data on DS line according to the
		//Value of MSB
		if(data1 & 0b00000001)
		{
			//MSB is 1 so output high
			HC4094DataHigh();
		}
		else
		{
			//MSB is 0 so output low
			HC4094DataLow();
		}
		HC4094Pulse();  //Pulse the Clock line
		data1=data1>>1;  //Now bring next bit at MSB position
	}
    i = 0;
	for(i=0;i<8;i++)
	{
		//Output the data on DS line according to the
		//Value of MSB
		if(data2 & 0b00000001)
		{
			//MSB is 1 so output high
			HC4094DataHigh();
		}
		else
		{
			//MSB is 0 so output low
			HC4094DataLow();
		}
		HC4094Pulse();  //Pulse the Clock line
		data2=data2>>1;  //Now bring next bit at MSB position
	}

	//Now all 8 bits have been transferred to shift register
	//Move them to output latch at one
	HC4094Latch();
}

void Wait()
{
	_delay_ms(500);
}



#ifdef DEBUGGING


void TXRegisters(void)
{
	uint8_t temp1; // variable to hold data
	BOOL result1;
	result1 = DS1307Read(0x00,&temp1);
	SendChar(temp1);
	result1 = DS1307Read(0x01,&temp1);
	SendChar(temp1);
	result1 = DS1307Read(0x02,&temp1);
	SendChar(temp1);
	/*
	result1 = DS1307Read(0x03,&temp1);
	SendChar(temp1);
	result1 = DS1307Read(0x04,&temp1);
	SendChar(temp1);
	result1 = DS1307Read(0x05,&temp1);
	SendChar(temp1);
	result1 = DS1307Read(0x06,&temp1);
	SendChar(temp1);
	*/
	SendChar(0xff);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_ms(1000);
}

void TXTime(void)
{
	char Time[12];	//hh:mm:ss AM/PM
	//Now Read and format time
	uint8_t data;
	DS1307Read(0x00,&data);
	Time[7]=48+(data & 0b00001111);
	Time[6]=48+((data & 0b01110000)>>4);
	Time[5]=':';
	DS1307Read(0x01,&data);
	Time[4]=48+(data & 0b00001111);
	Time[3]=48+((data & 0b01110000)>>4);
	Time[2]=':';
	DS1307Read(0x02,&data);
	Time[1]=48+(data & 0b00001111);
	Time[0]=48+((data & 0b00110000)>>4);
	SendString(Time);
	SendCR();
	//_delay_ms(1000);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
}

//giving some garbage characters
void TXTimeX(void)
{
	char Time[12];	//hh:mm:ss AM/PM
	//Now Read and format time
	uint8_t data;
	DS1307Read(0x00,&data);
	Time[7]=48+(data & 0b00001111);
	Time[6]=48+((data & 0b01110000)>>4);
	Time[5]=':';
	DS1307Read(0x01,&data);
	Time[4]=48+(data & 0b00001111);
	Time[3]=48+((data & 0b01110000)>>4);
	Time[2]=':';
	DS1307Read(0x02,&data);
	Time[1]=48+(data & 0b00001111);
	Time[0]=48+((data & 0b00110000)>>4);
	SendString(Time);
	SendCR();
	//_delay_ms(1000);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
}

void TXBinTime(void)
{
	//get minutes
	uint8_t data;
	DS1307Read(0x01,&data);
	//data = minutes
	int tmp;
	tmp = (data & 0b00001111);
	//at minutes, with &, should be the last digit of minutes
	int tmp1;
	tmp1 = ((data & 0b01110000)>>4);
	//tmp1 should be the first digit of minutes, so multiply by 10
	tmp1 *= 10;
	int minutes;
	minutes = tmp1 + tmp;
	SendChar(minutes);

	//get hours
	DS1307Read(0x02,&data);
	tmp = (data & 0b00001111);
	//last digit of hours
	tmp1 = ((data & 0b00110000)>>4);
	//tmp1 should be the first digit of minutes, so multiply by 10
	tmp1 *= 10;

	int hours;
	hours = tmp1 + tmp;
	SendChar(hours);

	SendChar(0xff);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_loop_2(65535);
	_delay_ms(1000);
}

#endif

/*
*
* Functions to change the Binary Coded Decimal (BCD) time to regular binary
*
*/

int GetBinMinutes(void)
{
	int minutes;
	int tmp1;
	int tmp;
	uint8_t data;
	//Get minutes
	DS1307Read(0x01,&data);
	//data = minutes
	//first 4 bits of 01 address are the 0-9 range for the last digit of minutes
	//Apply an & with the right mask to extract the active bits
	tmp = (data & 0b00001111);
	//Same for 10 place of minutes, range is 0 to 5
	//Shift right by 4 places to be able to calculate regular decimal
	tmp1 = ((data & 0b01110000)>>4);
	//tmp1 holds a number from 0 to 5, but since it's the first place of
	//the number, multiply it by 10
	tmp1 *= 10;
	minutes = tmp1 + tmp;
	return minutes;
}

int GetBinHours(void)
{
	int hours;
	uint8_t data;
	int tmp;
	int tmp1;
	//Get hours
	DS1307Read(0x02,&data);
	//data = hours
	//first 4 bits of 01 address are the 0-9 range for the last digit of hours
	//Apply an & with the right mask to extract the active bits
	tmp = (data & 0b00001111);
	//Same for 10 place of hours, range is 1 or 2
	//Shift right by 4 places to be able to calculate regular decimal
	tmp1 = ((data & 0b00110000)>>4);
	//tmp1 holds a number from 0 to 5, but since it's the first place of
	//the number, multiply it by 10
	tmp1 *= 10;
	hours = tmp1 + tmp;
	return hours;
}

/*
*
* Function to set the initial time of the DS1307
* Must be edited manually and carefully to set the time.
* Run it once then remove it from the code & reupload.
* Add more lines to set the DATE registers if you wish
* But not needed here
*
*/

void SetTime(void)
{
	uint8_t temp;
	BOOL result;
	result = DS1307Read(0x00,&temp); // read current bits in 00 register
	temp &= ~(0b10000000); //Clear CH Bit (this enables the clock)
	result = DS1307Write(0x00,temp); //write back with ch = 0


	result = DS1307Read(0x02,&temp); //Set 24 Hour Mode
	temp |= 0b01000000; //Set 24 Hour BIT
	result = DS1307Write(0x02,temp); //Write Back to DS1307

	//Set initial time. Check datasheet to know how to set it up
	//set hour to 23
	temp = 0b00000111;
	result = DS1307Write(0x02,temp);
	//set minutes to 40
	temp = 0b01010000;
	result = DS1307Write(0x01,temp);
	//set seconds to 0
	temp = 0b00000000;
	result = DS1307Write(0x00,temp);
}

void Timer0_Init(void)
{
    TCCR0B = (1<<CS00)|(1<<CS02); // 1024 prescaler
    // initialize counter
    TCNT0 = 0;

    // enable overflow interrupt
    TIMSK |= (1 << TOIE0);

    // enable global interrupts
    sei();
}

volatile int overflow_counter;

ISR(TIMER0_OVF_vect)
{
    // keep a track of number of overflows
    if (overflow_counter >= 20)
    {
        DualHC4094Write(GetBinMinutes(), GetBinHours());
        #ifdef DEBUGGING
        SendChar(0xff);
        #endif
        overflow_counter = 0;
    }
    overflow_counter++;
}

/*
*
* Start main loop here
*
*/

int main(void)
{
    //sleep mode test



	DS1307Init();
	HC4094Init();
	Timer0_Init();
	#ifdef DEBUGGING
	TXInit();
	#endif
	//SetTime();
	/*
	MCUCR |= (1<<SE);
    MCUCR |= (1<<SM1);
    sleep
    */
    while (1)
	{

		//Simply loop and update the LEDs
		//DualHC4094Write(GetBinMinutes(), GetBinHours());
		/*
		#ifdef DEBUGGING
		TXRegisters();
		//TXTime();
		//TXBinTime();
		_delay_ms(500);
		#endif
		*/
	}
}