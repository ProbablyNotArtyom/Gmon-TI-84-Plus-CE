//---------------------------------------------------
//
//	GBoot v0.0
//	NotArtyom
//	02/11/18
//
//---------------------------------------------------

	#include <stddef.h>
	#include <stdarg.h>
	#include <stdbool.h>
	#include <stdio.h>

	#include <assert.h>
	#include <tice.h>
	#include <keypadc.h>
	#include <ctype.h>
	#include <string.h>

	#include "gshell.h"

//---------------------------------------------------

extern bool doExit;
extern char* parse;
extern char *current_addr;
extern char *end_addr;

extern uint32_t mon_vars[];

uint32_t mon_vars[6];
bool isCurrentVar;

//---------------------------------------------------

int putchar(int c) {
	char tmp_str[2];
	tmp_str[0] = c;
	tmp_str[1] = '\0';
	if (c == '\n' || c == '\r') _OS(asm_NewLine);
	else if (c != '\0') os_PutStrFull(tmp_str);
	return true;
}

void puts(char *str) {
	int i;
	int len = strlen((char*)str);
	for (i = 0; i < len; i++) putchar(str[i]);
	_OS(asm_NewLine);
}

bool alphaToggle, altToggle;
char *gets(char *buff, int len) {
	const char *std_chars = "\0\0\0\0\0\0\0\0\0\0+-*/^\0\0?369\)G\0\0.258\(FC\0000147\,EB\0\0XSNIDA\0\\%\0\0\0\0\0";
	const char *alpha_chars = "\0\0\0\0\0\0\0\0\0\0\"WRMH\0\0?[VQLG\0\0:ZUPKFC\0 YTOJEB\0\0XSNIDA\0\\%\0\0\0\0\0";
	const char *alt_chars = "\0\0\0\0\0\0\0\0\0\0\0\]\[e\0\0\0\0\0\0w\}\0\0\0\0\0\0v\{\0\0\000\0\0\0u\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	uint8_t key, j, i = 0;
	alphaToggle = 0;
	altToggle = 0;

	while((key = os_GetCSC()) != sk_Enter && i < len) {
		if (key == sk_Clear) {
			_OS(asm_ClrLCD);
			_OS(asm_HomeUp);
			print_prompt();
			for (j = 0; buff[j] != '\0' && j < i; j++) putchar(buff[j]);
		} else if (key == sk_Del && i > 0) {
			unsigned int col, row;
			os_GetCursorPos(&row, &col);	// Get the current cursor position
			col -= 1;
			os_SetCursorPos(row, col);	// Set the cursor position back by 1
			putchar(' ');
			os_SetCursorPos(row, col);	// Set the cursor position back by 1
			i--;
			buff[i] = ' ';
		} else if (key == sk_Alpha) alphaToggle = ~alphaToggle;
		else if (key == sk_2nd) altToggle = ~altToggle;
		else {
			if (alphaToggle && std_chars[key]) {
				buff[i] = std_chars[key];
				if (altToggle && alt_chars[key]) buff[i] = alt_chars[key];
				putchar(buff[i++]);
			} else if (!alphaToggle && alpha_chars[key]) {
				buff[i] = alpha_chars[key];
				if (altToggle && alt_chars[key]) buff[i] = alt_chars[key];
				putchar(buff[i++]);
			}
		}
	}

	buff[i] = '\0';
	return buff;
}

void print_prompt(void) {
	putchar('(');
	printLong((uint32_t)current_addr);
	if (end_addr != 0x00) {
		putchar('.');
		printLong((uint32_t)end_addr);
	}
	vprintf(")\n");
	vprintf(" ~ ");
}

bool isRange() {
	char *tmpParse = parse;
	while (*tmpParse != '.' && *tmpParse != ',' && *tmpParse != '\0') tmpParse++;
	if (*tmpParse != '.' && *tmpParse != ',') return false;
	return true;
}

bool isVar() {
	if (islower(*parse) && (*(parse+1) == ' ' || !isalnum(*(parse+1)))) return true;
	return false;
}

bool isArg() {
	return (isVar() || isAddr());
}

bool setCurrents() {
	skipBlank();
	if (isVar()) isCurrentVar = true;
	else if (isAddr()) isCurrentVar = false;

	return getRange(&current_addr, &end_addr);
}

char* skipBlank() {
	while (*parse == ' ') parse++;
	return parse;
}

char* skipToken() {
	while (*parse != ' ' && *parse != '\0') parse++;
	return parse;
}

char* skipHex() {
	uint8_t i;
	while (true){
		for (i = 0; *parse != hexTable[i] && hexTable[i] != '\0'; i++);
		if (hexTable[i] == '\0') return parse;
		parse++;
	}
}

bool isAddr() {
	uint8_t i;
	for (i = 0; *parse != hexTable[i] && hexTable[i] != '\0'; i++);
	return (hexTable[i] != '\0');
}

bool funcCmp(const char s1, const char s2){
	if (s1 == s2) return true;
	return false;
}

ADDRSIZE strToHEX() {
	uint8_t i;
	ADDRSIZE val = 0;
	val = strtoul(parse, NULL, 16);
	return val;
}

enum errList throw(enum errList index){
	if (index == errNONE) return errNONE;
	else if (index == errDOEXIT) return errDOEXIT;
	vprintf("\n%s\n", errors[index]);
	return errNONE;
}

void evalScript() {


}

uint32_t *getMonVar(char var) {
	if (islower(var)){
		return &mon_vars[ (var - 0x60) ];
	} else {
		return 0x00000000;
	}
}

void setMonVar(char var, uint32_t val){
	if (isascii(var) && islower(var)){
		mon_vars[ var & 0x00011111 ] = val;
	} else {
		return;
	}
}

bool getRange(void **lower, void **upper){
	skipBlank();
	if (isAddr()){
		*(ADDRSIZE*)lower = strToHEX();
	} else if (isVar()){
		*lower = getMonVar(*parse);
	} else if (*parse == '.' || *parse == ','){
		*lower = current_addr;
	} else {
		return false;
	}
	if (isCurrentVar == false && (*parse == '.' || *parse == ',')){
		char tmp = *parse;
		parse++;
		skipBlank();
		ifEOI(errNOARGS);
		if (!isAddr()) return errSYNTAX;
		if (tmp == '.')
			*(ADDRSIZE*)upper = strToHEX();
		else
			*(ADDRSIZE*)upper = (strToHEX() + (uint32_t)*lower);
	} else {
		if (*parse != ' ' || *parse != '\0'){
			skipToken();
		}
		*upper = NULL;
	}
	return true;
}

void printHex(char num){
	putchar(hexTable[(num & 0x0F)]);
	return;
}

void printByte(char num){
	putchar(hexTable[(num & 0xF0) >> 4]);
	putchar(hexTable[(num & 0x0F)]);
	return;
}

void printWord(uint16_t num){
	putchar(hexTable[(num & 0xF000) >> 12]);
	putchar(hexTable[(num & 0x0F00) >> 8]);
	putchar(hexTable[(num & 0x00F0) >> 4]);
	putchar(hexTable[(num & 0x000F)]);
	return;
}

void printLong(uint32_t num){
	printWord((num & 0xFFFF0000) >> 16);
	printWord(num & 0x0000FFFF);
	return;
}
