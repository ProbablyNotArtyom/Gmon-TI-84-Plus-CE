/*
 *	Copyright (C) 2020  Carson Herrington
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

	#include <stdbool.h>
	#include <stddef.h>
	#include <stdint.h>
	#include <stdio.h>

	#include <assert.h>
	#include <debug.h>
	#include <tice.h>
	#include <keypadc.h>

	#include "gshell.h"

//-----------------------------------------------------------------------------

bool gsh_do_cmd(uint8_t num);

static enum errList gsh_help();
static enum errList gsh_exit();
static enum errList gsh_echo();
static enum errList gsh_execute();
static enum errList gsh_delay();
static enum errList gsh_deposit();
static enum errList gsh_view();
static enum errList gsh_view_ascii();
static enum errList gsh_copy();
static enum errList gsh_move();
static enum errList gsh_fill();

static enum errList read_range(char *ptr, char *end);

//-----------------------------------Tables------------------------------------

enum errList const (* const funcTable[])() = {
	gsh_help,
	gsh_exit,
	gsh_echo,
	gsh_execute,
	gsh_delay,
	gsh_deposit,
	gsh_view,
	gsh_view_ascii,
	gsh_copy,
	gsh_move,
	gsh_fill,
	NULL
};

const char funcKeys[] = {
	'?',
	'Q',
	'^',
	'G',
	'W',
	':',
	'R',
	'O',
	'N',
	'M',
	'X',
	0
};

const char* const errors[] = {
	"",
	"Syntax error",
	"Undefined function",
	"Unexpected arguments",
	"Unexpected end of input",
	"Invalid hex",
	"Invalid range",
	"Break"
};

const char hexTable[] = "0123456789ABCDEF";
const char *helpText[] = {
	"G'Mon Version " GMON_VERSION,
	"-=-=- Commands -=-=-=-=-=-",
	"R (<$|%>) Dump memory",
	"N <%>, $  Copy memory",
	"M <%>, $  Move memory",
	"X <%>, #  Fill memory",
	": (#)     Deposit bytes",
	"G <$|%>   Jump to ASM",
	"^ ...     Print formatted",
	"W #       Delay # ms",
	"?         Print help",
	"\0"
};

//-----------------------------------------------------------------------------

bool doExit;
char *parse;
char *current_addr;
char *end_addr;
char *cmdStart;
char *tmp;

uint8_t numCMDs;
uint32_t numLoops;
char inBuffer[BUFFLEN];					// Our input buffer

int main(void) {
    os_ClrHome();

	vprintf("G'Mon Version " GMON_VERSION "\n");
	doExit = false;
	current_addr = 0x00;
	end_addr = 0x00;
	while (doExit == false){
		print_prompt();
		parse = inBuffer;							// Set the parse pointer to the beginning of the buffer
		gets(inBuffer, BUFFLEN);					// Get user input
		skipBlank();								// Skip and leading spaces
		if (!isEOI()){
			puts("");
			numLoops = 1;
			numCMDs = 0x01;
			skipBlank();
			cmdStart = parse;
			if (*cmdStart == '{'){
				while (*cmdStart != '}') cmdStart++;
				numLoops = strtoul(parse+1, NULL, 10);
				cmdStart++;
			}
			parse = cmdStart;
			for (tmp = parse; *tmp != '\0'; tmp++) if (*tmp == ';') numCMDs += 1;
			while (numLoops > 0){
				gsh_do_cmd(numCMDs);
				// Break if Clear is pressed
				if (os_GetCSC() == sk_Clear) {
					throw(errBREAK);
					numLoops = 0;
				} else {
					numLoops--;
					parse = cmdStart;
				}
			}
		} else puts("");
	}

	return 0;
}

bool gsh_do_cmd(uint8_t num){
	uint8_t i;
	char *tmp;
	for (tmp = parse; *tmp != '\0'; tmp++)
		if (*tmp == ';') *tmp = '\0';

	while(num > 0){
		skipBlank();
		for (i = 0; funcCmp(*parse, funcKeys[i]) == false && i < NUM_FUNCS-1; i++);	// Identify what function it is
		if (i == NUM_FUNCS-1) {
			if (setCurrents() == false){
				throw(errUNDEF);			// If none matches, complain
				return false;
			}
		} else {
			skipToken();							// Skip over the function name itself
			if (throw((*funcTable[i])()) != errNONE) return false;
		}
		while (*parse != '\0') parse++;
		parse++;
		num--;
	}
	return true;
}

//----------------------------------Builtins-----------------------------------

/* Exits the monitor */
static enum errList gsh_exit() {
	doExit = true;
	return errNONE;
}

static enum errList gsh_delay() {
	uint32_t *arg;
	if (isEOI()) return errSYNTAX;
	else arg = (uint32_t*)strToHEX();

	delay(*arg);
	return errNONE;
}

static enum errList gsh_echo() {
	char *end;
	uint32_t *val;
	char *ptr = parse;
	puts("");
	while (*ptr != '\0' && *ptr != '\"') ptr++;
	if (*ptr == '\0') return errSYNTAX;
	ptr++;
	end = ptr;
	while (*end != '\0' && *end != '\"') end++;
	if (*end == '\0') return errSYNTAX;
	*end = '\0';

	parse = (end+1);

	if (isEOI()) return errSYNTAX;

	getArg(val);
	vprintf(ptr, *val);
	*end = '\"';
	return errNONE;
}

/* Starts executing code from a place in memory */
static enum errList gsh_execute() {
	void (*ptr)(void) = (void*)current_addr;
	if (!isEOI() && isAddr()) ptr = (void*)strToHEX();

	(*ptr)();			// Call that function
	return errNONE;		// Return error free, assuming that whatever we call actually returns (good chance it wont)
}

static enum errList gsh_help() {
	int i;
	for (i = 0; helpText[i] != NULL; i++) {
		puts(helpText[i]);
		if (i % 2) while ((os_GetCSC()) != sk_Enter);	// Require a keypress to print another 2 lines
	}
	return errNONE;
}

/* Writes bytes to memory */
static enum errList gsh_deposit() {
	uint8_t *end, *ptr = (uint8_t*)current_addr;	// Create pointers for the start and end of the section
	uint8_t val;

	skipBlank();
	if (isCurrentVar){
		ptr = (uint8_t*)strToHEX();
	} else {
		if (*skipBlank() == '\0') return errNOARGS;
		while (!isEOI()) {
			val = (uint8_t)strToHEX();
			*(uint8_t*)(ptr++) = (uint8_t)val;
			skipHex();
		}
	}
	return errNONE;
}

static enum errList read_range(char *ptr,char *end) {
	if (end != NULL){									// If we hit a range identifier...
		uint8_t column;									// Create something to track how many columns have been printed so far
		char *addrBuff;
		column = 0;
		while (ptr <= end){								// Continue until we've reached the end of the range
			int i;
			if (ptr <= end){
				vprintf("\n");							// Then set up a new line
				column = 0;								// And print out the location header
				printLong((uint32_t)ptr);
				vprintf(" | ");
			}
			while (column < 4 && ptr <= end){
				printByte(*ptr++);						// Print data byte at this address
				putchar(' ');
				column++;								// Increase our column number
				queryBreak();
			}
		}
	} else {
		vprintf("\n");									// Then set up a new line
		printLong((uint32_t)ptr);
		vprintf(" | ");
		printByte(*ptr);
	}
	return errNONE;
}

static enum errList read_range_ascii(char *ptr,char *end) {
	if (end != NULL){									// If we hit a range identifier...
		uint8_t column;									// Create something to track how many columns have been printed so far
		char *addrBuff;
		column = 0;
		while (ptr <= end){								// Continue until we've reached the end of the range
			int i;
			if (ptr <= end){
				vprintf("\n");							// Then set up a new line
				column = 0;								// And print out the location header
				printLong((uint32_t)ptr);
				vprintf(" | ");
			}
			while (column < 8 && ptr <= end){
				if (*ptr >= 0x20 && *ptr < 0x7F) putchar(*ptr++);
				else putchar('.');
				column++;								// Increase our column number
				queryBreak();
			}
		}
	} else {
		vprintf("\n");									// Then set up a new line
		printLong((uint32_t)ptr);
		vprintf(" | ");
		if (*ptr >= 0x20 && *ptr < 0x7F) putchar(*ptr);
		else putchar('.');
	}
	return errNONE;
}

/* Handles viewing of memory */
static enum errList gsh_view() {
	char *ptr, *end;									// Create start and end pointers
	if (!isEOI()){
		while(*parse != '\0'){
			skipBlank();
			if (isVar()){
				read_range((char*)getMonVar(*parse), (char*)0x00000000);
				parse++;
			} else {
				if (!getRange(&ptr, &end)) return errSYNTAX;
				read_range(ptr, end);
			}
		}
	} else {
		if (isCurrentVar) read_range(current_addr, end_addr);
		else read_range(current_addr, end_addr);
	}
	puts("");
	return errNONE;										// Return error free
}

static enum errList gsh_view_ascii() {
	char *ptr, *end;									// Create start and end pointers
	if (!isEOI()){
		while(*parse != '\0'){
			skipBlank();
			if (isVar()){
				read_range_ascii((char*)getMonVar(*parse), (char*)0x00000000);
				parse++;
			} else {
				if (!getRange(&ptr, &end)) return errSYNTAX;
				read_range_ascii(ptr, end);
			}
		}
	} else {
		if (isCurrentVar) read_range_ascii(current_addr, end_addr);
		else read_range_ascii(current_addr, end_addr);
	}
	puts("");
	return errNONE;
}

/* Copies a range of data */
static enum errList gsh_copy() {
	char *ptr, *end, *dest;								// Create pointers for start, end, and destination of block

	if (isRange()){
		if (!getRange(&ptr, &end)) return errSYNTAX;
	} else {
		ptr = current_addr;
		end = end_addr;
	}
	getArg(dest);

	if (dest <= ptr) {									// If the destination is below the source in memory,
		while (ptr <= end) outb(*ptr++, dest++);		// then copy it starting at the beginning
	} else {
		dest += end - ptr;								// If the destination is above the start of the source,
		while (end >= ptr) outb(*end--, dest--);		// then copy starting at the end of the source
	}													// This is done to avoid overwriting the source before we can copy it

	return errNONE;										// Return error free
}

/* Moves a block of data, filling the old space with 00 */
static enum errList gsh_move() {
	char *ptr, *end, *dest;								// Create pointers for start, end, and destination of block

	if (isRange()){
		if (!getRange(&ptr, &end)) return errSYNTAX;
	} else {
		ptr = current_addr;
		end = end_addr;
	}
	getArg(dest);

	if (dest <= ptr) {									// If the destination is below the source in memory,
		while (ptr <= end){
			outb(*ptr, dest++);						// then copy it starting at the beginning
			outb((uint8_t)NULL, ptr++);
		}
	} else {
		dest += end - ptr;								// If the destination is above the start of the source,
		while (end >= ptr){
			outb(*end, dest--);						// then copy starting at the end of the source
			outb((uint8_t)NULL, end--);
		}
	}													// This is done to avoid overwriting the source before we can copy it

	return errNONE;										// Return error free
}

/* Fills a range with a pattern byte */
static enum errList gsh_fill() {
	char *ptr, *end;									// Create pointers for the start and end of the section
	uint8_t val;										// The fill pattern itself

	if (isRange()){
		if (!getRange(&ptr, &end)) return errSYNTAX;
	} else {
		ptr = current_addr;
		if (end_addr == NULL) end = current_addr;
		else end = end_addr;
	}
	val = (uint8_t)strToHEX();

	while (ptr <= end) *(uint8_t*)(ptr++) = val;		// Set every byte from *ptr to *end to the pattern in val
	return errNONE;										// Return error free
}
