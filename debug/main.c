#include <msp430.h> 
#

#define pos1 4                                               // Digit A1 - L4
#define pos2 6                                               // Digit A2 - L6
#define pos3 8                                               // Digit A3 - L8
#define pos4 10                                              // Digit A4 - L10
#define pos5 2                                               // Digit A5 - L2
#define pos6 18                                              // Digit A6 - L18

const char digit[10] =
{
    0xFC,                                                    // "0"
    0x60,                                                    // "1"
    0xDB,                                                    // "2"
    0xF3,                                                    // "3"
    0x67,                                                    // "4"
    0xB7,                                                    // "5"
    0xBF,                                                    // "6"
    0xE4,                                                    // "7"
    0xFF,                                                    // "8"
    0xF7                                                     // "9"
};

volatile unsigned int * Seconds = &BAKMEM0;               // Store seconds in the backup RAM module
volatile unsigned long *FRAM_write_ptr1;
volatile unsigned long *FRAM_write_ptr2;


void Init_GPIO(void);
void Inc_RTC(void);

int checkBit(volatile unsigned long *);
unsigned int ECCcreate(unsigned int);
void FRAMWrite(volatile unsigned long *);
int ECCcheck(unsigned long*);


#define FRAM_write_place1 0x1800
#define FRAM_write_place2 0x1804

/**
 * main.c
 */
int main( void )
{
    WDTCTL = WDTPW | WDTHOLD;                               // Stop watchdog timer

    if (SYSRSTIV == SYSRSTIV_LPM5WU)                        // If LPM3.5 wakeup
    {
        Inc_RTC();                                          // Real clock

        PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
        PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF
        __bis_SR_register(LPM3_bits | GIE);                 // Re-enter LPM3.5
    }
    else
    {
        // Initialize GPIO pins for low power
        Init_GPIO();

        FRAM_write_ptr1 = (unsigned long *)FRAM_write_place1;
        FRAM_write_ptr2 = (unsigned long *)FRAM_write_place2;

        *Seconds = 0x1005;

        SYSCFG0 &= ~DFWP;
        *FRAM_write_ptr1 = 0x00000101;
        *FRAM_write_ptr2 = 0x00000011;
        SYSCFG0 |= DFWP;

        if(*FRAM_write_ptr1 > *FRAM_write_ptr2){
            *Seconds = *FRAM_write_ptr1 >> 1;
        }
        else{// just started
            *Seconds = *FRAM_write_ptr2 >> 1;                                   // Set initial time
        }

        // Configure XT1 oscillator
        P4SEL0 |= BIT1 | BIT2;                              // P4.2~P4.1: crystal pins
        do
        {
            CSCTL7 &= ~(XT1OFFG | DCOFFG);                  // Clear XT1 and DCO fault flag
            SFRIFG1 &= ~OFIFG;
        }while (SFRIFG1 & OFIFG);                           // Test oscillator fault flag
        CSCTL6 = (CSCTL6 & ~(XT1DRIVE_3)) | XT1DRIVE_2;     // Higher drive strength and current consumption for XT1 oscillator

        // Disable the GPIO power-on default high-impedance mode
        // to activate previously configured port settings
        PM5CTL0 &= ~LOCKLPM5;

        // Configure RTC
        RTCCTL |= RTCSS__XT1CLK | RTCIE;                    // Initialize RTC to use XT1 and enable RTC interrupt
        RTCMOD = 32768;                                     // Set RTC modulo to 32768 to trigger interrupt each second

        // Configure LCD pins
        SYSCFG2 |= LCDPCTL;                                 // R13/R23/R33/LCDCAP0/LCDCAP1 pins selected

        LCDPCTL0 = 0xFFFF;
        LCDPCTL1 = 0x07FF;
        LCDPCTL2 = 0x00F0;                                  // L0~L26 & L36~L39 pins selected

        LCDCTL0 = LCDSSEL_0 | LCDDIV_7;                     // flcd ref freq is xtclk

        // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz
        LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

        LCDMEMCTL |= LCDCLRM;                               // Clear LCD memory

        LCDCSSEL0 = 0x000F;                                 // Configure COMs and SEGs
        LCDCSSEL1 = 0x0000;                                 // L0, L1, L2, L3: COM pins
        LCDCSSEL2 = 0x0000;

        LCDM0 = 0x21;                                       // L0 = COM0, L1 = COM1
        LCDM1 = 0x84;                                       // L2 = COM2, L3 = COM3

        LCDCTL0 |= LCD4MUX | LCDON;                         // Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)

        // Display time
        unsigned int counter = *Seconds;

        LCDMEM[pos6] = digit[counter % 10];
        counter /= 10;
        LCDMEM[pos5] = digit[counter % 10];
        counter /= 10;
        LCDMEM[pos4] = digit[counter % 10];
        counter /= 10;
        LCDMEM[pos3] = digit[counter % 10];
        counter /= 10;
        LCDMEM[pos2] = digit[counter % 10];
        counter /= 10;
        LCDMEM[pos1] = digit[counter % 10];


        PMMCTL0_H = PMMPW_H;                                // Open PMM Registers for write
        PMMCTL0_L |= PMMREGOFF_L;                           // and set PMMREGOFF

        __bis_SR_register(LPM3_bits | GIE);                 // Enter LPM3.5
        __no_operation();                                   // For debugger
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
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
}

void Init_GPIO()
{
    // Configure all GPIO to Output Low
    P1OUT = 0x00;P2OUT = 0x00;P3OUT = 0x00;P4OUT = 0x00;
    P5OUT = 0x00;P6OUT = 0x00;P7OUT = 0x00;P8OUT = 0x00;

    P1DIR = 0xFF;P2DIR = 0xFF;P3DIR = 0xFF;P4DIR = 0xFF;
    P5DIR = 0xFF;P6DIR = 0xFF;P7DIR = 0xFF;P8DIR = 0xFF;
}


void Inc_RTC()
{
    if(*FRAM_write_ptr1 > *FRAM_write_ptr2){
        *Seconds = (*FRAM_write_ptr1) >> 1;
    }
    else{// just started
        *Seconds = (*FRAM_write_ptr2) >> 1;
    }

    // Deal with second
    (*Seconds)++;
    //(*Seconds) %= 60
    unsigned int counter = *Seconds;

    LCDMEM[pos6] = digit[counter % 10];
    counter /= 10;
    LCDMEM[pos5] = digit[counter % 10];
    counter /= 10;
    LCDMEM[pos4] = digit[counter % 10];
    counter /= 10;
    LCDMEM[pos3] = digit[counter % 10];
    counter /= 10;
    LCDMEM[pos2] = digit[counter % 10];
    counter /= 10;
    LCDMEM[pos1] = digit[counter % 10];

    if(*FRAM_write_ptr1 > *FRAM_write_ptr2){
        FRAMWrite(FRAM_write_ptr2);
    }
    else{
        FRAMWrite(FRAM_write_ptr1);
    }

}

int checkBit(volatile unsigned long *ptr){
    int count = 0;
    int i = 0;
    for(i= 0; i<16; ++i){
        if(*ptr & (1 << i)){
            count++;
        }
    }
    return count;
}

void FRAMWrite (volatile unsigned long *ptr)
{
    SYSCFG0 &= ~DFWP;
    *ptr = *Seconds;
    *ptr = *ptr << 1;
    SYSCFG0 |= DFWP;
}
