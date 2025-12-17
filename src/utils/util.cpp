#include "sof_buddy.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "sof_compat.h"
#include "util.h"

// see util.h for debugging toggles 

void (*orig_Z_Free) (void * v);
char *(*orig_CopyString) (char *in);
void (*orig_Cmd_ExecuteString)(const char * string);

FILE * go_logfile = NULL;

#ifndef NDEBUG
// Global variables to store assert information for GDB inspection
// Use static storage to avoid corruption
static char g_assert_file_storage[512] = {0};
static char g_assert_condition_storage[256] = {0};
static char g_assert_function_storage[256] = {0};
static int g_assert_line_storage = 0;
static volatile bool g_assert_fired = false;

// Function that GDB can break on when DLL loads (only in debug-gdb builds)
// GDB can set a breakpoint on this function: "break sofbuddy_debug_breakpoint"
#ifdef GDB
extern "C" void sofbuddy_debug_breakpoint(void)
{
    // This function exists so GDB can set a breakpoint on it for symbol loading.
    // Build with: make debug-gdb
}
#endif

// Public pointers for GDB inspection
const char* g_assert_file = g_assert_file_storage;
int g_assert_line = 0;
const char* g_assert_condition = g_assert_condition_storage;
const char* g_assert_function = g_assert_function_storage;

// Function that GDB can break on - set breakpoint: "break sofbuddy_assert_failed"
// This function is designed to be catchable even without symbols loaded
extern "C" void sofbuddy_assert_failed(const char* file, int line, const char* condition, const char* function)
{
    // Mark that assert fired (atomic operation)
    g_assert_fired = true;
    
    // Store assert info in static storage (safer than pointers)
    if (file) {
        strncpy(g_assert_file_storage, file, sizeof(g_assert_file_storage) - 1);
        g_assert_file_storage[sizeof(g_assert_file_storage) - 1] = '\0';
    } else {
        g_assert_file_storage[0] = '\0';
    }
    
    if (condition) {
        strncpy(g_assert_condition_storage, condition, sizeof(g_assert_condition_storage) - 1);
        g_assert_condition_storage[sizeof(g_assert_condition_storage) - 1] = '\0';
    } else {
        g_assert_condition_storage[0] = '\0';
    }
    
    if (function) {
        strncpy(g_assert_function_storage, function, sizeof(g_assert_function_storage) - 1);
        g_assert_function_storage[sizeof(g_assert_function_storage) - 1] = '\0';
    } else {
        g_assert_function_storage[0] = '\0';
    }
    
    g_assert_line_storage = line;
    g_assert_line = line;
    
    // Update public pointers
    g_assert_file = g_assert_file_storage;
    g_assert_condition = g_assert_condition_storage;
    g_assert_function = g_assert_function_storage;
    
    // Print detailed info
    PrintOut(PRINT_BAD, "=== ASSERT FAILED - GDB BREAKPOINT HERE ===\n");
    PrintOut(PRINT_BAD, "File: %s\n", file ? file : "<unknown>");
    PrintOut(PRINT_BAD, "Line: %d\n", line);
    PrintOut(PRINT_BAD, "Function: %s\n", function ? function : "<unknown>");
    PrintOut(PRINT_BAD, "Condition: %s\n", condition ? condition : "<unknown>");
    PrintOut(PRINT_BAD, "=== In GDB, inspect: g_assert_file, g_assert_line, g_assert_condition, g_assert_function ===\n");
    
    // Insert a breakpoint instruction that GDB can catch even without symbols
    // This is a well-known pattern: INT3 (0xCC) on x86
    // GDB can catch this via "catch signal SIGTRAP" or by setting a breakpoint on the function address
    #ifdef _WIN32
        // Use DebugBreak() which generates INT3
        DebugBreak();
    #else
        // On Linux/Wine, use inline assembly to insert INT3 directly
        // This ensures GDB can catch it even without symbols
        asm volatile("int $3" : : : "memory");
    #endif
    
    // Fallback: also use __builtin_trap() in case INT3 doesn't work
    // This creates an illegal instruction that GDB can catch
    __builtin_trap();
}
#endif

void PrintOutImpl(int mode, const char *msg,...)
{
	char ac_buf[1464];
	char ac_tmp[1400];
	va_list args;
	va_start (args, msg);
	vsprintf (ac_buf,msg, args);
	// perror (ac_buf);
	va_end (args);

//ignore debug-log-type-messages?
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
		go_logfile = fopen("SoFBuddy.log","w");
	}
	switch (mode) {
		case PRINT_LOG :

				strcpy(ac_buf,"SoFree Log: ");
				strcat(ac_buf,ac_tmp);
				fprintf(go_logfile,"%s",ac_buf);

		break;
		case PRINT_LOG_EMPTY :
			fprintf(go_logfile,"%s",ac_tmp);
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

    extern void (*orig_Com_Printf)(const char*, ...);
    if (!orig_Com_Printf) return;
	//in-game print
	switch (mode) {
		case PRINT_LOG :

                strcpy(ac_buf, P_CYAN "~~~~~~~~~~" P_BLACK "\x24\xF8\x46" P_CYAN "\xAE\xC9\xC9 : " P_ORANGE);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);

		break;
		case PRINT_LOG_EMPTY :
			orig_Com_Printf("%s",ac_tmp);
		break;
		case PRINT_GOOD :

                strcpy(ac_buf, P_CYAN "~~~~~~~~~~" P_BLACK "\x24\xF8\x46" P_CYAN "\xAE\xC9\xC9 : " P_GREEN);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);

		break;
		case PRINT_BAD :

                strcpy(ac_buf, P_CYAN "~~~~~~~~~~" P_BLACK "\x24\xF8\x46" P_CYAN "\xAE\xC9\xC9 : " P_RED);
				strcat(ac_buf,ac_tmp);
				orig_Com_Printf("%s",ac_buf);
		break;
		default :
			orig_Com_Printf("%s",ac_tmp);
	}
	
}


void writeUnsignedIntegerAt(void * addr, unsigned int value)
{
    DWORD dwProt=0;
	// Enable writing to original
	VirtualProtect(addr, 4, PAGE_READWRITE, &dwProt);
	*(unsigned int*)(addr) = (unsigned int)value;
	// Reset original mem flags
	VirtualProtect(addr, 4, dwProt, &dwProt);
}

void writeIntegerAt(void * addr, int value)
{
    DWORD dwProt=0;
	// Enable writing to original
	VirtualProtect(addr, 4, PAGE_READWRITE, &dwProt);
	*(int*)(addr) = (int)value;
	// Reset original mem flags
	VirtualProtect(addr, 4, dwProt, &dwProt);
}

void writeFloatAt(void * addr, float value)
{
    DWORD dwProt=0;
	VirtualProtect(addr, sizeof(float), PAGE_READWRITE, &dwProt);
	*(float*)(addr) = value;
	VirtualProtect(addr, sizeof(float), dwProt, &dwProt);
}

void writeDoubleAt(void * addr, double value)
{
    DWORD dwProt=0;
	VirtualProtect(addr, sizeof(double), PAGE_READWRITE, &dwProt);
	*(double*)(addr) = value;
	VirtualProtect(addr, sizeof(double), dwProt, &dwProt);
}

void WriteE8Call(void * where,void * dest)
{
    DWORD dwProt=0;
    VirtualProtect(where, 5, PAGE_READWRITE, &dwProt);
    *(unsigned char*)where = 0xE8;
    *(int*)((unsigned char*)where + 1) = (int)((char*)dest - (char*)where - 5);
    VirtualProtect(where, 5, dwProt, &dwProt);
}

/*
	When calculating jumps use:
	DestinationTo - (Address of JMP + SIZE_OF_JMP)
*/
void WriteE9Jmp(void * where, void * dest)
{
    DWORD dwProt=0;
    VirtualProtect(where, 5, PAGE_READWRITE, &dwProt);
    *(unsigned char*)where = 0xE9;
    *(int*)((unsigned char*)where + 1) = (int)((char*)dest - (char*)where - 5);
    VirtualProtect(where, 5, dwProt, &dwProt);
}

void WriteByte(void * where, unsigned char value)
{
    DWORD dwProt=0;
    VirtualProtect(where, 1, PAGE_READWRITE, &dwProt);
    *(unsigned char*)where = value;
    VirtualProtect(where, 1, dwProt, &dwProt);
}

void WriteNops(void * where, int count)
{
    if (count <= 0) return;
    DWORD dwProt=0;
    VirtualProtect(where, count, PAGE_READWRITE, &dwProt);
    for (int i = 0; i < count; i++) {
        *((unsigned char*)where + i) = 0x90;
    }
    VirtualProtect(where, count, dwProt, &dwProt);
}

/*
Cvar Util Funcs
*/
cvar_t * findCvar(char * cvarname)
{
	cvar_t	*var;
    static cvar_t * cvar_vars;
    if (!cvar_vars) cvar_vars = (cvar_t *)*(unsigned int*)rvaToAbsExe((void*)0x0024B1D8);
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (cvarname, var->name))
			return var;
	return NULL;
}

inline void setCvarUnsignedInt(cvar_t * which,unsigned int val){
    which->modified = true;
    orig_Z_Free(which->string);
    char intstring[64];
    which->value = (float)val;
    sprintf(intstring,"%u",val);
    which->string = orig_CopyString(intstring);
}


inline void setCvarInt(cvar_t * which,int val){
    which->modified = true;
    orig_Z_Free(which->string);
    char intstring[64];
    which->value = (float)val;
    sprintf(intstring,"%d",val);
    which->string = orig_CopyString(intstring);
}

inline void setCvarByte(cvar_t * which, unsigned char val) {
    which->modified = true;
    orig_Z_Free(which->string);
    char bytestring[64];
    sprintf(bytestring,"%hhu",val);
    which->value = atof(bytestring);
    which->string = orig_CopyString(bytestring);
}


inline void setCvarFloat(cvar_t * which, float val) {

    which->modified = true;
    orig_Z_Free(which->string);
    char floatstring[64];
    sprintf(floatstring,"%f",val);
    which->string = orig_CopyString(floatstring);
    which->value = val;
}

inline void setCvarString(cvar_t * which, char * newstr) {

    which->modified = true;
    orig_Z_Free(which->string);
    
    
    which->string = orig_CopyString(newstr);
    which->value = atof(which->string);
}

size_t strlen_custom(const char *str) {
    // Handle NULL pointer input for robustness
    if (str == NULL) {
        return 0;
    }

    size_t length = 0;
    // Loop until the null terminator (*str == 0x00 or *str == '\0')
    while (*str != '\0') {
        // Check if the character's value is >= 0x20
        // Note: We cast to unsigned char to avoid potential issues if 'char'
        // is signed and the value is > 127 on some systems, although for
        // comparing against 0x20 it's usually fine either way.
        if ((unsigned char)*str >= 0x20) {
            length++; // Increment length only if condition is met
        }
        str++; // Move to the next character regardless of the condition
    }

    return length;
}

// Helper function to extract the nth entry from a '/'-separated string (0-based)
const char* get_nth_entry(const char* str, int n) {
    int slashCount = 0;
    const char* p = str;
    while (*p) {
        if (*p == '/') {
            ++slashCount;
            if (slashCount == n) {
                return p + 1; // nth entry starts after this slash
            }
        }
        ++p;
    }
    return nullptr;
}

void* GetModuleBase(const char* moduleName) {
#ifdef _WIN32
    HMODULE hModule = GetModuleHandleA(moduleName);
    return hModule ? (void*)hModule : nullptr;
#else
    return nullptr;
#endif
}

void* RVAToAddress(void* rva, const char* moduleName) {
    void* base = GetModuleBase(moduleName);
    if (!base) return nullptr;
    return (void*)((uintptr_t)base + (uintptr_t)rva);
}

void* RVAToAddress(uintptr_t rva, const char* moduleName) {
    void* base = GetModuleBase(moduleName);
    if (!base) return nullptr;
    return (void*)((uintptr_t)base + rva);
}

void* rvaToAbsExe(void* rva) { return RVAToAddress((uintptr_t)rva, "SoF.exe"); }
void* rvaToAbsRef(void* rva) { return RVAToAddress((uintptr_t)rva, "ref_gl.dll"); }
void* rvaToAbsGame(void* rva) { return RVAToAddress((uintptr_t)rva, "gamex86.dll"); }

void* rvaToAbsSoFPlus(void* rva) {
#ifdef _WIN32
    extern void* o_sofplus;
    if (o_sofplus) return (void*)((char*)o_sofplus + (uintptr_t)rva);
    return nullptr;
#else
    return nullptr;
#endif
}