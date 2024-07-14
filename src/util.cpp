#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "sof_compat.h"
#include "util.h"

#define __FILEOUT__
//#define __TERMINALOUT__


void (*orig_Z_Free) (void * v) = 0x2001EBC0;
char *(*orig_CopyString) (char *in) = 0x2001EA20;
void (*orig_Cmd_ExecuteString)(const char * string) = 0x200194F0;

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

	#if 0
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
	#endif
	
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

/*
Cvar Util Funcs
*/
cvar_t * findCvar(char * cvarname)
{
	cvar_t	*var;
	cvar_t * cvar_vars = *(unsigned int*)0x2024B1D8;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (cvarname, var->name))
			return var;
	return NULL;
}

void setCvarUnsignedInt(cvar_t * which,unsigned int val){
	which->modified = true;
	orig_Z_Free(which->string);
	char intstring[64];
	which->value = (float)val;
	sprintf(intstring,"%u",val);
	which->string = orig_CopyString(intstring);
}


void setCvarInt(cvar_t * which,int val){
	which->modified = true;
	orig_Z_Free(which->string);
	char intstring[64];
	which->value = (float)val;
	sprintf(intstring,"%d",val);
	which->string = orig_CopyString(intstring);
}

void setCvarByte(cvar_t * which, unsigned char val) {
	which->modified = true;
	orig_Z_Free(which->string);
	char bytestring[64];
	sprintf(bytestring,"%hhu",val);
	which->value = atof(bytestring);
	which->string = orig_CopyString(bytestring);
}


void setCvarFloat(cvar_t * which, float val) {

	which->modified = true;
	orig_Z_Free(which->string);
	char floatstring[64];
	sprintf(floatstring,"%f",val);
	which->string = orig_CopyString(floatstring);
	which->value = val;
}

void setCvarString(cvar_t * which, char * newstr) {

	which->modified = true;
	orig_Z_Free(which->string);
	
	
	which->string = orig_CopyString(newstr);
	which->value = atof(which->string);
}