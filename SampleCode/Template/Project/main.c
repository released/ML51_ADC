/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2019 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/

//***********************************************************************************************************
//  Website: http://www.nuvoton.com
//  E-Mail : MicroC-8bit@nuvoton.com
//***********************************************************************************************************

//***********************************************************************************************************
//  File Function: ML51 simple GPIO toggle out demo code
//***********************************************************************************************************

#include "ML51.h"
#include <stdio.h>

#define ENABLE_UART0
#define ENABLE_ADC_LOG		(1)

typedef enum{
	target_CH0 = 0 ,
	target_CH1 ,
	target_CH2 ,
	target_CH3 ,	
	
	target_CH4 ,
	target_CH5 ,
	target_CH6 ,
	target_CH7 ,

	target_CH_DEFAULT	
}Channel_Index;

typedef enum{
	flag_ADC_conv_ready = 0 ,
	flag_ADC_conv_start , 

	flag_TIMER_LOG , 
	
	flag_DEFAULT	
}Flag_Index;

volatile uint32_t BitFlag = 0;
#define ReadBit(bit)									(uint32_t)(1<<bit)
#define BitFlag_ON(flag)								(BitFlag|=flag)
#define BitFlag_OFF(flag)								(BitFlag&=~flag)
#define BitFlag_READ(flag)								((BitFlag&flag)?1:0)
#define is_flag_set(idx)								(BitFlag_READ(ReadBit(idx)))
#define set_flag(idx,en)								( (en == 1) ? (BitFlag_ON(ReadBit(idx))) : (BitFlag_OFF(ReadBit(idx))))

#define FREQ_HIRC									(24000000ul)
#define FREQ_LIRC									(38400ul)

//ADC
double  VDD_Voltage,Bandgap_Voltage;
xdata uint16_t adc_data = 0;
#define ADC_RESOLUTION								(4096ul)
#define ADC_REF_VOLTAGE								(5000ul)//(3300ul)	//(float)(3.3f)

#define ADC_SAMPLE_COUNT 							(4ul)			// 8
#define ADC_SAMPLE_POWER 							(2ul)			//(5)	 	// 3	,// 2 ^ ?

#define ADC_DIGITAL_SCALE(void) 						(0xFFFU >> ((0) >> (3U - 1U)))		//0: 12 BIT 
#define ADC_CALC_DATA_TO_VOLTAGE(DATA,VREF) 		((DATA) * (VREF) / ADC_DIGITAL_SCALE())


#define TIMER_LOG_MS								(1000ul)
#define TIMER_DIV4_VALUE_10ms_FOSC_38400        	(65536-96)      //96*4/38400 = 10 mS,       // Timer divider = 4

uint8_t u8TH0_Tmp = 0;
uint8_t u8TL0_Tmp = 0;

xdata uint16_t LOG_TIMER = 0;

/*****************************************************************************/

void ADC_average (void)
{
	static uint16_t cnt = 0;
	static uint32_t sum = 0;
	uint16_t avg = 0;

	
	if (is_flag_set(flag_ADC_conv_ready))
	{
		sum += adc_data;
		if (cnt++ >= (ADC_SAMPLE_COUNT-1))
		{
			cnt = 0;
			avg = (uint16_t)(sum >> ADC_SAMPLE_POWER) ;	//	/ADC_SAMPLE_COUNT;
			sum = 0;
			#if 1	//debug
			printf("ADC:0x%3X (%.3fmv), VDD_Voltage:%.3fmv , Bandgap_Voltage:%.3fmv\r\n",avg,ADC_CALC_DATA_TO_VOLTAGE(avg,VDD_Voltage) , VDD_Voltage,Bandgap_Voltage);
			#endif
		}
		set_flag(flag_ADC_conv_ready , Disable);		

	}	

	if (is_flag_set(flag_ADC_conv_start))
	{
		set_flag(flag_ADC_conv_start , Disable);			
		set_ADCCON0_ADCS; 
	}
	
}


void ADC_ISR(void) interrupt 11          // Vector @  0x5B
{	
    _push_(SFRS);
	
	set_flag(flag_ADC_conv_ready , Enable);	
    clr_ADCCON0_ADCF; //clear ADC interrupt flag

	adc_data = (ADCRH<<4) + ADCRL;

    _pop_(SFRS);
}


void ADC_Bandgap_Init(void)
{
    unsigned int  ADC_BG_Result;
	
    ADC_Open(ADC_SINGLE,VBG);	
/* For the best result wait 10us delay for each sampling, ADCDIV=3, ADCAQT=7 is better */
    ADC_SamplingTime(3,7);                             
    clr_ADCCON0_ADCF;
    set_ADCCON0_ADCS;
    while(!(ADCCON0&SET_BIT7));
    ADC_BG_Result = (ADCRH<<4) + ADCRL;

	VDD_Voltage = ((float)READ_BANDGAP()/(float)ADC_BG_Result)*3072;
	Bandgap_Voltage = ((float)READ_BANDGAP()*3/4/1000);
	
	printf ("VDD_Voltage = %e , Bandgap_Voltage = %e\r\n",VDD_Voltage,Bandgap_Voltage);
	
}

//P2.0 , ADC_CH5
void ADC_Init(void)
{
    VREF_Open(LEVEL3);
	ADC_Bandgap_Init();

	
    ADC_Open(ADC_SINGLE,5);

    ADC_SamplingTime(2,7);
    ADC_Interrupt(Enable,ADC_INT_CONTDONE);                     
    ENABLE_GLOBAL_INTERRUPT;
	
	clr_ADCCON0_ADCF;
	set_ADCCON0_ADCS;

	set_flag(flag_ADC_conv_ready , Disable);

}

void Loop(void)
{

	if (is_flag_set(flag_TIMER_LOG))
	{
		set_flag(flag_TIMER_LOG ,Disable);		
    	printf("Timer LOG: %bd\r\n", LOG_TIMER++);
	}

	ADC_average();
	
}

void LED_Init(void)
{
	MFP_P03_GPIO;
	P03_PUSHPULL_MODE;
}

void Timer0_IRQHandler(void)
{
	static uint16_t CNT_TIMER = 0;
	static uint16_t CNT_ADC = 0;
	
	if (CNT_TIMER++ >= TIMER_LOG_MS)
	{		
		CNT_TIMER = 0;
		P03 = !P03;
		
		set_flag(flag_TIMER_LOG ,Enable);
	}

	if (CNT_ADC++ >= 50)
	{		
		CNT_ADC = 0;
		set_flag(flag_ADC_conv_start ,Enable);
	}
	
}

void Timer0_ISR(void) interrupt 1        // Vector @  0x0B
{
    TH0 = u8TH0_Tmp;
    TL0 = u8TL0_Tmp;
    clr_TCON_TF0;
	
	Timer0_IRQHandler();
}

void Timer0_Init(void)
{
	uint16_t res = 0;

	ENABLE_TIMER0_MODE1;

	u8TH0_Tmp = HIBYTE(TIMER_DIV12_VALUE_1ms_FOSC_240000);
	u8TL0_Tmp = LOBYTE(TIMER_DIV12_VALUE_1ms_FOSC_240000); 

    TH0 = u8TH0_Tmp;
    TL0 = u8TL0_Tmp;

    ENABLE_TIMER0_INTERRUPT;                       //enable Timer0 interrupt
    ENABLE_GLOBAL_INTERRUPT;                       //enable interrupts
  
    set_TCON_TR0;                                  //Timer0 run
}

void UART0_Init(void)
{
	MFP_P31_UART0_TXD;                              // UART0 TXD use P1.6
	P31_QUASI_MODE;                                  // set P1.6 as Quasi mode for UART0 trasnfer
	UART_Open(FREQ_HIRC,UART0_Timer3,115200);        // Open UART0 use timer1 as baudrate generate and baud rate = 115200
	ENABLE_UART0_PRINTF;


	printf("UART0_Init\r\n");
}

void SYS_Init(void)
{
	FsysSelect(FSYS_HIRC);

    ALL_GPIO_QUASI_MODE;
//    ENABLE_GLOBAL_INTERRUPT;                // global enable bit	
}

void main (void) 
{
	/*
	For UART0 P3.1 as TXD output setting 
	* include uart.c in Common Setting for UART0 
	*/
    SYS_Init();

	#if defined (ENABLE_UART0)
	UART0_Init();
	#endif

	LED_Init();

	Timer0_Init();

	ADC_Init();

	while(1)
	{
		Loop();
	}
}



