//---------------------------------------------------
//
//	GBoot v0.0
//	NotArtyom
//	02/11/18
//
//---------------------------------------------------
// Standard exit codes for use with G'DOS shell

#ifndef HEADER_GSHELL
#define HEADER_GSHELL

	#include <tice.h>
	#include <stdint.h>

//---------------------------------------------------

	#define GMON_VERSION	"1.2"
	#define	BUFFLEN			0xFF

	#define	ADDRSIZE	uint32_t
	#define NUM_FUNCS		12

//---------------------------------------------------

#define isEOI()		(*skipBlank() == '\0')

#define ifEOI(err)	if (*skipBlank() == '\0') return err;

#define getArg(var)						\
			if (*skipBlank() == '\0') return errNOARGS;		\
			if (isVar()) var = (void*)getMonVar(*parse++); \
			else var = (void*)strToHEX();

#define queryBreak()	if (os_GetCSC() == sk_Clear) return errBREAK;

#define outb(val, addr)		((*(volatile uint8_t *) (addr)) = (val))
#define outw(val, addr)		((*(volatile uint16_t *) (addr)) = (val))
#define outl(val, addr)		((*(volatile uint32_t *) (addr)) = (val))

#define inb(addr)			({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define inw(addr)			({ unsigned short __v = (*(volatile unsigned short *) (addr)); __v; })
#define inl(addr)			({ unsigned long __v = (*(volatile unsigned long *) (addr)); __v; })


static enum errList {
		errNONE,
		errSYNTAX,
		errUNDEF,
		errNOARGS,
		errEND,
		errHEX,
		errBADRANGE,
		errBREAK,
		errDOEXIT
};

extern const enum errList (*funcTable[])();
extern const char funcKeys[];
extern const char* const errors[];
extern const char hexTable[];

extern bool isCurrentVar;
extern char *parse;
extern char *current_addr;
extern char *end_addr;
extern char *cmdStart;

int putchar(int c);
void puts(char *str);
int vprintf(const char *format, ...);
char *gets(char *buff, int len);

void print_prompt(void);
bool isRange();
bool isVar();
bool isArg();
void evalScript();
bool setCurrents();
bool isAddr();
bool isRange();
char* skipBlank();
char* skipToken();
char* skipHex();
bool funcCmp(const char s1, const char s2);
ADDRSIZE strToHEX();
enum errList throw(enum errList index);
uint32_t *getMonVar(char var);
void setMonVar(char var, uint32_t val);
bool getRange(void **lower, void **upper);
void printHex(char num);
void printByte(char num);
void printWord(uint16_t num);
void printLong(uint32_t num);

//---------------------------------------------------

#endif
