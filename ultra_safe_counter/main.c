#include <msp430.h> 
#include <stdio.h>


/**
 * main.c
 */
void print_screen(unsigned long);
void init_LCD();
void Init_GPIO(void);
unsigned long int read(void);
void write(unsigned long int);
void init_mapping_table();
unsigned long encode(unsigned long);
long decode(unsigned long);

const unsigned int position[6] = {4, 6, 8, 10, 2, 18};

#define FRAM_TEST_START 0x1800

const char digit[10] =
{
    0xFC,                                                      // "0"
    0x60,                                                      // "1"
    0xDB,                                                      // "2"
    0xF3,                                                      // "3"
    0x67,                                                      // "4"
    0xB7,                                                      // "5"
    0xBF,                                                      // "6"
    0xE4,                                                      // "7"
    0xFF,                                                      // "8"
    0xF7                                                       // "9"
};

volatile unsigned char * mapping_table = &BAKMEM0_L;               // Store seconds in the backup RAM module
volatile unsigned char * cur_time_stamp = &BAKMEM0_H;               // Store minutes in the backup RAM module

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	if(SYSRSTIV == SYSRSTIV_LPM5WU){
	    unsigned long counter = read();
	    print_screen(counter++);
	    write(counter);
	    PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
	    PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
	    __bis_SR_register(LPM3_bits | GIE);
	}
	else{
	    Init_GPIO();
	    init_mapping_table();
	    P4SEL0 |= BIT1 | BIT2;                              // P4.2~P4.1: crystal pins
	    do
	    {
	        CSCTL7 &= ~(XT1OFFG | DCOFFG);                  // Clear XT1 and DCO fault flag
	        SFRIFG1 &= ~OFIFG;
	    }while (SFRIFG1 & OFIFG);                           // Test oscillator fault flag
	    CSCTL6 = (CSCTL6 & ~(XT1DRIVE_3)) | XT1DRIVE_2;     // Higher drive strength and current consumption for XT1 oscillator
	    PM5CTL0 &= ~LOCKLPM5;
	    RTCCTL |= RTCSS__XT1CLK | RTCIE;                    // Initialize RTC to use XT1 and enable RTC interrupt
	    RTCMOD = 32768;                                     // Set RTC modulo to 32768 to trigger interrupt each second
	    init_LCD();
	    unsigned long counter = read();
	    print_screen(counter++);
	    write(counter);
	    PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
	    PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
	    __bis_SR_register(LPM3_bits | GIE);                 // Enter LPM3.5
	}
}
/*#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = RTC_VECTOR
__interrupt void RTC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(RTC_VECTOR))) RTC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(RTCIV, RTCIV_RTCIF))
    {
        case RTCIV_NONE :
            break;

        case RTCIV_RTCIF:
            break;

        default:
            break;
    }
}*/

#pragma vector = RTC_VECTOR
__interrupt void RTC_ISR(void){
    switch(__even_in_range(RTCIV, RTCIV_RTCIF))
    {
        case RTCIV_NONE :
            break;

        case RTCIV_RTCIF:
            break;

        default:
            break;
    }
}


void init_mapping_table(void){
    unsigned long *ptr = (unsigned long *)FRAM_TEST_START;
    long data1 = decode(*ptr);
    ptr += 1;
    long data2 = decode(*ptr);
    if (data1 == -1 && data2 != -1){
        *mapping_table = 1;
        *cur_time_stamp = data2 & 0b111111;
    }
    else if (data1 != -1 && data2 == -1){
        *mapping_table = 0;
        *cur_time_stamp = data1 & 0b111111;
    }
    else if (data1 == -1 && data2 == -1){
        *mapping_table = 0;
        *cur_time_stamp = 0;
        write(0);
        write(0);
    }
    else{
        char data1_time_stamp = data1 & 0b111111;
        char data2_time_stamp = data2 & 0b111111;
        if(data1_time_stamp > data2_time_stamp){
            if(data2 == 0){
                *mapping_table = 1;
                *cur_time_stamp = data2_time_stamp;
            }
            else{
                *mapping_table = 0;
                *cur_time_stamp = data1_time_stamp;
            }
        }
        else{
            if(data1 == 0){
                *mapping_table = 0;
                *cur_time_stamp = data1_time_stamp;
            }
            else{
                *mapping_table = 1;
                *cur_time_stamp = data2_time_stamp;
            }
        }
    }
}

void write(unsigned long num){
    *mapping_table = *mapping_table ^ 1;
    num = num << 6;
    num += *cur_time_stamp & 0b111111;
    num = encode(num);
    SYSCFG0 &= ~DFWP;
    unsigned long *ptr = (unsigned long *)FRAM_TEST_START;
    ptr += *mapping_table;
    *ptr = num;
    SYSCFG0 |= DFWP;
}

unsigned long read(void){
    unsigned long *ptr = (unsigned long *)FRAM_TEST_START;
    ptr += *mapping_table;
    unsigned long num = *ptr;
    long result = decode(num);
    if(result == -1){// current data crush
        ptr -= *mapping_table;
        *mapping_table = *mapping_table ^ 1;
        ptr += *mapping_table;
        result = decode(*ptr);
        if(result == -1){// both data crush
               return 0;
        }
        else{
            return result >> 6;
        }
    }
    else{
        return result >> 6;
    }
}

unsigned long encode(unsigned long num){
    unsigned long result = 0;
    unsigned long ecc = 0;
    int sum = 0;
    num = num << 3;
    result += num & 0b1000;
    num = num << 1;
    result += num & 0b11100000;
    num = num << 1;
    result += num & 0b1111111000000000;
    num = num << 1;
    result += num & 0b11111111111111100000000000000000;
    int i = 0;
    unsigned long mask = 1;
    for(i = 0; i < 32; i++){
        sum ^= (result & mask) >> i;
        if(result & mask){
            ecc ^= i;
        }
        mask = mask << 1;
    }
    result += (ecc & 1) << 1;
    ecc = ecc >> 1;
    result += (ecc & 1) << 2;
    ecc = ecc >> 1;
    result += (ecc & 1) << 4;
    ecc = ecc >> 1;
    result += (ecc & 1) << 8;
    ecc = ecc >> 1;
    result += (ecc & 1) << 16;
    result += sum;
    return result;
}

long decode(unsigned long num){
    int i = 0;
    int pos = 0;
    const int mask = 0b1;
    int error_detected = 0;
    int result = 0;
    unsigned long num_copy = num;
    for(i = 0; i < 32; i++){
        error_detected ^= num_copy & mask;
        if(num_copy & mask){
            pos ^= i;
        }
        num_copy = num_copy >> 1;
    }
    if(error_detected){
        num ^= mask << pos;
    }
    else if(pos){ //double error detected
        return -1;
    }
    num = num >> 3;
    result += num & 0b1;
    num = num >> 1;
    result += num & 0b1110;
    num = num >> 1;
    result += num & 0b11111110000;
    num = num >> 1;
    result += num & 0b11111111111111100000000000;
    return result;
}

void init_LCD(){
    SYSCFG2 |= LCDPCTL;                                        // R13/R23/R33/LCDCAP0/LCDCAP1 pins selected
    LCDPCTL0 = 0xFFFF;
    LCDPCTL1 = 0x07FF;
    LCDPCTL2 = 0x00F0;                                         // L0~L26 & L36~L39 pins selected

    LCDCTL0 = LCDSSEL_0 | LCDDIV_7;                            // flcd ref freq is xtclk

    // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz
    LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

    LCDMEMCTL |= LCDCLRM;                                      // Clear LCD memory

    LCDCSSEL0 = 0x000F;                                        // Configure COMs and SEGs
    LCDCSSEL1 = 0x0000;                                        // L0, L1, L2, L3: COM pins
    LCDCSSEL2 = 0x0000;

    LCDM0 = 0x21;                                              // L0 = COM0, L1 = COM1
    LCDM1 = 0x84;
    // Display "123456"
    LCDCTL0 |= LCD4MUX | LCDON;
}

void Init_GPIO()
{
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
}

void print_screen(unsigned long num){
    unsigned int i = 0;
    for(i = 6; i ; i--){
        LCDMEM[position[i - 1]] = digit[num % 10];
        num = num / 10;
    }
}
