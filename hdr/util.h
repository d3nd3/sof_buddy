#ifndef SOF_BUDDY_UTIL_H
#define SOF_BUDDY_UTIL_H
#include "sof_compat.h"
#include <stddef.h>
#include <stdint.h>

#ifndef NDEBUG
#include <windows.h>
#include <cassert>
#include <cstdio>
#define SOFBUDDY_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            char msg[512]; \
            sprintf(msg, \
                "Assertion failed!\n\n" \
                "File: %s\n" \
                "Line: %d\n" \
                "Condition: %s\n\n" \
                "This indicates a bug in sof_buddy. Please report this error.", \
                __FILE__, __LINE__, #condition); \
            MessageBoxA(NULL, msg, "sof_buddy Assertion Failed", MB_ICONERROR | MB_OK); \
            assert(condition); \
        } \
    } while(0)
#else
#define SOFBUDDY_ASSERT(condition) ((void)0)
#endif

#define PRINT_GOOD 1
#define PRINT_BAD 2
#define PRINT_LOG 3
#define PRINT_LOG_EMPTY 4

// show debug-log-type-messages?
// Controlled by __LOGGING__ define in makefile for debug builds

//Extra output print places than in-game.
// #define __FILEOUT__
// #define __TERMINALOUT__


#define M_PI   3.14159265358979323846264338327950288
#define degToRad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
#define radToDeg(angleInRadians) ((angleInRadians) * 180.0 / M_PI)

#define P_WHITE		"\001"
#define P_RED  		"\002"
#define P_GREEN		"\003"
#define P_YELLOW	"\004"
#define P_BLUE		"\005"
#define P_PURPLE	"\006"
#define P_CYAN		"\007"
#define P_BLACK		"\010"
#define P_HWHITE  	"\011"
#define P_DONT_USE1	"\012"
#define P_HRED    	"\013"
#define P_HGREEN  	"\014"
#define P_DONT_USE2	"\015"
#define P_HYELLOW	"\016"
#define P_HBLUE		"\017"
#define P_CAMOBROWN	"\020"
#define P_CAMOGREEN	"\021"
#define P_SEAGREEN 	"\022"
#define P_SEABLUE  	"\023"
#define P_METAL    	"\024"
#define P_DBLUE    	"\025"
#define P_DPURPLE  	"\026"
#define P_DGREY    	"\027"
#define P_PINK	  	"\030"
#define P_BLOODRED 	"\031"
#define P_RUSSET   	"\032"
#define P_BROWN    	"\033"
#define P_TEXT     	"\034"
#define P_BAIGE    	"\035"
#define P_LBROWN   	"\036"
#define P_ORANGE   	"\037"

void PrintOutImpl(int mode, const char *msg, ...);
#ifdef __LOGGING__
#define PrintOut(mode, msg, ...) PrintOutImpl(mode, msg, ##__VA_ARGS__)
#else
#define PrintOut(mode, msg, ...) do { \
    if (mode == PRINT_GOOD || mode == PRINT_BAD) \
        PrintOutImpl(mode, msg, ##__VA_ARGS__); \
    else \
        do { } while(0); \
} while(0)
#endif

void writeUnsignedIntegerAt(void * addr, unsigned int value);
void writeIntegerAt(void * addr, int value);
void writeFloatAt(void * addr, float value);
void writeDoubleAt(void * addr, double value);
void WriteE8Call(void * where,void * dest);
void WriteE9Jmp(void * where, void * dest);
void WriteByte(void * where, unsigned char value);
void WriteNops(void * where, int count);

void* GetModuleBase(const char* moduleName);
void* RVAToAddress(void* rva, const char* moduleName);
void* RVAToAddress(uintptr_t rva, const char* moduleName);
void* rvaToAbsExe(void* rva);
void* rvaToAbsRef(void* rva);
void* rvaToAbsGame(void* rva);
void* rvaToAbsSoFPlus(void* rva);

cvar_t * findCvar(char * cvarname);
void setCvarUnsignedInt(cvar_t * which,unsigned int val);
void setCvarInt(cvar_t * which,int val);
void setCvarByte(cvar_t * which, unsigned char val);
void setCvarFloat(cvar_t * which, float val);
void setCvarString(cvar_t * which, char * newstr);
size_t strlen_custom(const char *str);

const char* get_nth_entry(const char* str, int n);

#endif // SOF_BUDDY_UTIL_H