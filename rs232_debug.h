#define BITDELAY _delay_us(26)
#define TXPIN PB1

//simple macros to set port to on or off
#define TXPIN_LOW (PORTB &= ~(1<<TXPIN))
#define TXPIN_HIGH (PORTB |= (1<<TXPIN))

extern void TXInit(void);
extern void SendChar(char);
extern void SendCR(void);
extern void SendString(char *);

