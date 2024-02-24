#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "sof_compat.h"
#include "util.h"

#define __FILEOUT__
//#define __TERMINALOUT__
//#define __LOGGING__

FILE * go_logfile = NULL;

void PrintOut(int mode,char *msg,...)
{
	
	char ac_buf[1464];
	char ac_tmp[1400];
	va_list args;
	va_start (args, msg);
	vsprintf (ac_buf,msg, args);
	// perror (ac_buf);
	va_end (args);

#ifndef __LOGGING__
	if ( mode == PRINT_LOG || mode == PRINT_LOG_EMPTY ) return;
#endif

	//printf("length = %i\n",strlen(ac_buf));
	if ( strlen(ac_buf) <= 1400)
		strcpy(ac_tmp,ac_buf);
	else
		return;
	//printf("length = %i\n",strlen(ac_tmp));
#ifdef __TERMINALOUT__
	switch (mode) {
		case PRINT_LOG :
			strcpy(ac_buf,"SoFBuddy Log: ");
			strcat(ac_buf,ac_tmp);
			printf("%s",ac_buf);
		break;
		case PRINT_LOG_EMPTY :
			printf("%s",ac_tmp);
		break;
		case PRINT_GOOD :

			strcpy(ac_buf,"SoFBuddy Success: ");
			strcat(ac_buf,ac_tmp);
			printf("%s",ac_buf);

		break;
		case PRINT_BAD :

			strcpy(ac_buf,"SoFBuddy Error: ");
			strcat(ac_buf,ac_tmp);
			printf("%s",ac_buf);

		break;
		default :
			printf("%s",ac_tmp);
	}
#endif
#ifdef __FILEOUT__
	if ( go_logfile == NULL ){
		go_logfile = fopen("SoFBuddy.log","a+");
	}
	switch (mode) {
		case PRINT_LOG :

				strcpy(ac_buf,"SoFree Log: ");
				strcat(ac_buf,ac_tmp);
				fprintf(go_logfile,"%s",ac_buf);

		break;
		case PRINT_GOOD :

				strcpy(ac_buf,"SoFree Success: ");
				strcat(ac_buf,ac_tmp);
				fprintf(go_logfile,"%s",ac_buf);

		break;
		case PRINT_BAD :

				strcpy(ac_buf,"SoFree Error: ");
				strcat(ac_buf,ac_tmp);
				fprintf(go_logfile,"%s",ac_buf);

		break;
		default :
			fprintf(go_logfile,"%s",ac_tmp);
	}
	
#endif

	switch (mode) {
		case PRINT_LOG :

				strcpy(ac_buf,P_CYAN"~~~~~~~~~~"P_BLACK"\x24\xF8\x46"P_CYAN"\xAE\xC9\xC9 : "P_ORANGE);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);

		break;
		case PRINT_GOOD :

				strcpy(ac_buf,P_CYAN"~~~~~~~~~~"P_BLACK"\x24\xF8\x46"P_CYAN"\xAE\xC9\xC9 : "P_GREEN);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);

		break;
		case PRINT_BAD :

				strcpy(ac_buf,P_CYAN"~~~~~~~~~~"P_BLACK"\x24\xF8\x46"P_CYAN"\xAE\xC9\xC9 : "P_RED);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);
		break;
		default :
			orig_Com_Printf("%s",ac_tmp);
	}

}


void writeUnsignedIntegerAt(void * addr, unsigned int value)
{
	DWORD dwProt=NULL;
	// Enable writing to original
	VirtualProtect(addr, 4, PAGE_READWRITE, &dwProt);
	*(unsigned int*)(addr) = (unsigned int)value;
	// Reset original mem flags
	VirtualProtect(addr, 4, dwProt, &dwProt);
}

void writeIntegerAt(void * addr, int value)
{
	DWORD dwProt=NULL;
	// Enable writing to original
	VirtualProtect(addr, 4, PAGE_READWRITE, &dwProt);
	*(int*)(addr) = (int)value;
	// Reset original mem flags
	VirtualProtect(addr, 4, dwProt, &dwProt);
}

void WriteE8Call(void * where,void * dest)
{
	DWORD dwProt=NULL;
	VirtualProtect(where, 5, PAGE_READWRITE, &dwProt);
	*(unsigned char*)where = 0xE8;
	*(int*)(where+1) = dest - (int)where - 5;
	VirtualProtect(where, 5, dwProt, &dwProt);
}

void WriteE9Jmp(void * where, void * dest)
{
	DWORD dwProt=NULL;
	VirtualProtect(where, 5, PAGE_READWRITE, &dwProt);
	*(unsigned char*)where = 0xE9;
	*(int*)(where+1) = dest - (int)where - 5;
	VirtualProtect(where, 5, dwProt, &dwProt);
}

void WriteByte(void * where, unsigned char value)
{
	DWORD dwProt=NULL;
	VirtualProtect(where, 1, PAGE_READWRITE, &dwProt);
	*(unsigned char*)where = value;
	VirtualProtect(where, 1, dwProt, &dwProt);
}