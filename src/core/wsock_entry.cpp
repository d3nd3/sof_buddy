// Ensure target Windows version is set, but don't redefine if provided via build flags
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

// #define WINSOCK_ASSERT
// #define WINSOCK_LOGGING

// Winsock must be included before windows.h
#include <winsock2.h>
#include <windows.h>
#include <shlwapi.h>

#include <io.h>
// #include <unistd.h>

#include <stdio.h>
#include <stdint.h>

#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <psapi.h>

#include <wchar.h>
#include <shlwapi.h> // Make sure to link against Shlwapi.lib
#include <dbghelp.h>

#include "DetourXS/detourxs.h"
#include "debug/callsite_classifier.h"
#include "debug/hook_callsite.h"

#include "sof_buddy.h"
#include "sof_compat.h"

#include "util.h"
#include "crc32.h"
#include "debug/parent_recorder.h"

// Vectored exception handler pointer
static PVOID g_vectored_handler = NULL;
static bool g_dbghelp_initialized = false;

// Function pointers for dynamically loaded vectored exception handlers (XP SP1+)
typedef LONG (WINAPI *PVECTORED_EXCEPTION_HANDLER_t)(PEXCEPTION_POINTERS);
typedef PVOID (WINAPI *AddVectoredExceptionHandler_t)(ULONG, PVECTORED_EXCEPTION_HANDLER_t);
typedef ULONG (WINAPI *RemoveVectoredExceptionHandler_t)(PVOID);
static AddVectoredExceptionHandler_t pAddVectoredExceptionHandler = NULL;
static RemoveVectoredExceptionHandler_t pRemoveVectoredExceptionHandler = NULL;
static bool g_vectored_handlers_available = false;

static void load_vectored_exception_handlers(void) {
	if (g_vectored_handlers_available) return;
	HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
	if (hKernel32) {
		pAddVectoredExceptionHandler = (AddVectoredExceptionHandler_t)GetProcAddress(hKernel32, "AddVectoredExceptionHandler");
		pRemoveVectoredExceptionHandler = (RemoveVectoredExceptionHandler_t)GetProcAddress(hKernel32, "RemoveVectoredExceptionHandler");
		g_vectored_handlers_available = (pAddVectoredExceptionHandler != NULL && pRemoveVectoredExceptionHandler != NULL);
	}
}

// Capture and print a simple stack backtrace (addresses + resolved symbols when possible)
static void print_stack_backtrace(PEXCEPTION_POINTERS pExceptionInfo)
{
    void* backtrace[62];
    USHORT frames = CaptureStackBackTrace(0, _countof(backtrace), backtrace, NULL);

    PrintOut(PRINT_BAD, "Stack backtrace (frames=%u) thread=%lu:\n", frames, GetCurrentThreadId());

    if (g_dbghelp_initialized) {
        SYMBOL_INFO *symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
        if (symbol) {
            memset(symbol, 0, sizeof(SYMBOL_INFO) + 256 * sizeof(char));
            symbol->MaxNameLen = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

            IMAGEHLP_LINE64 lineInfo;
            DWORD displacement = 0;

            for (USHORT i = 0; i < frames; ++i) {
                DWORD64 addr = (DWORD64)(uintptr_t)backtrace[i];
                if (SymFromAddr(GetCurrentProcess(), addr, 0, symbol)) {
                    memset(&lineInfo, 0, sizeof(lineInfo));
                    DWORD displacement32 = 0;
                    if (SymGetLineFromAddr64(GetCurrentProcess(), addr, &displacement32, &lineInfo)) {
                        PrintOut(PRINT_BAD, "  #%02u: %s + 0x%llx (%s:%lu) [0x%p]\n", i, symbol->Name, (unsigned long long)(addr - symbol->Address), lineInfo.FileName, lineInfo.LineNumber, (void*)addr);
                    } else {
                        PrintOut(PRINT_BAD, "  #%02u: %s [0x%p]\n", i, symbol->Name, (void*)addr);
                    }
                } else {
                    PrintOut(PRINT_BAD, "  #%02u: 0x%p\n", i, (void*)addr);
                }
            }

            free(symbol);
        }
    } else {
        for (USHORT i = 0; i < frames; ++i) {
            PrintOut(PRINT_BAD, "  #%02u: 0x%p\n", i, backtrace[i]);
        }
    }
}

// Vectored exception handler to log access violations (page faults)
LONG WINAPI vectored_exception_handler(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (pExceptionInfo == NULL || pExceptionInfo->ExceptionRecord == NULL)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR violation_type = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR fault_addr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        void *ip = pExceptionInfo->ExceptionRecord->ExceptionAddress;
        const char *type_str = (violation_type == 0) ? "read" : (violation_type == 1) ? "write" : (violation_type == 8) ? "execute" : "unknown";

        PrintOut(PRINT_BAD, "Unhandled page fault on %s access to 0x%p at ip=%p thread=%lu\n", type_str, (void*)fault_addr, ip, GetCurrentThreadId());

        // Print a stack backtrace to help locate the cause
        print_stack_backtrace(pExceptionInfo);

#ifdef _M_X64
        PrintOut(PRINT_BAD, "RIP=0x%p RSP=0x%p RAX=0x%p RBX=0x%p RCX=0x%p RDX=0x%p\n",
            (void*)pExceptionInfo->ContextRecord->Rip, (void*)pExceptionInfo->ContextRecord->Rsp,
            (void*)pExceptionInfo->ContextRecord->Rax, (void*)pExceptionInfo->ContextRecord->Rbx,
            (void*)pExceptionInfo->ContextRecord->Rcx, (void*)pExceptionInfo->ContextRecord->Rdx);
#else
        PrintOut(PRINT_BAD, "EIP=0x%p ESP=0x%p EAX=0x%p EBX=0x%p ECX=0x%p EDX=0x%p\n",
            (void*)pExceptionInfo->ContextRecord->Eip, (void*)pExceptionInfo->ContextRecord->Esp,
            (void*)pExceptionInfo->ContextRecord->Eax, (void*)pExceptionInfo->ContextRecord->Ebx,
            (void*)pExceptionInfo->ContextRecord->Ecx, (void*)pExceptionInfo->ContextRecord->Edx);
#endif
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

/*
	CAUTION: Don't use directly without void* typecast
	#include <typeinfo> // For typeid
	struct HINSTANCE__; // Forward declaration: "There exists a struct called HINSTANCE__"
	                    // But we don't know its size or members yet.

	typedef struct HINSTANCE__ *HINSTANCE; // HINSTANCE is a pointer to this incomplete struct.
	                                     // This is now an opaque pointer.
*/
// Handle to sofplus module
void* o_sofplus = NULL;
bool sofplus_initialized = false;


void GenerateRandomString(wchar_t* randomString, int length) {
	const wchar_t charset[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	for (int i = 0; i < length - 1; i++) {
		int index = rand() % (int)(sizeof(charset) / sizeof(charset[0]) - 1);
		randomString[i] = charset[index];
	}
	randomString[length - 1] = L'\0'; // Null-terminate the string
}




bool SoFplusLoadFn(HMODULE sofplus, void **out_fn, const char *func);

// spcl.dll:  25 exports
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);

typedef int (__stdcall * lpfn_bind)(SOCKET s,const struct sockaddr *name,int namelen);
lpfn_bind sp_bind = (lpfn_bind)bind;
extern "C" int __stdcall sys_bind(SOCKET s,const struct sockaddr *name,int namelen);

typedef int (__stdcall * lpfn_closesocket)(SOCKET s);
lpfn_closesocket sp_closesocket = (lpfn_closesocket)closesocket;
extern "C" int __stdcall sys_closesocket(SOCKET s);

typedef int (__stdcall * lpfn_connect)(SOCKET s,const struct sockaddr *name,int namelen);
lpfn_connect sp_connect = (lpfn_connect)connect;
extern "C" int __stdcall sys_connect(SOCKET s,const struct sockaddr *name,int namelen);

typedef int (__stdcall * lpfn_getsockname)(SOCKET s,struct sockaddr *name,int *namelen);
lpfn_getsockname sp_getsockname = (lpfn_getsockname)getsockname;
extern "C" int __stdcall sys_getsockname(SOCKET s,struct sockaddr *name,int *namelen);

typedef u_long (__stdcall * lpfn_htonl)(u_long hostlong);
lpfn_htonl sp_htonl = (lpfn_htonl)htonl;
extern "C" u_long __stdcall sys_htonl(u_long hostlong);

typedef u_short (__stdcall * lpfn_htons)(u_short hostshort);
lpfn_htons sp_htons = (lpfn_htons)htons;
extern "C" u_short __stdcall sys_htons(u_short hostshort);

typedef unsigned long (__stdcall * lpfn_inet_addr)(const char *cp);
lpfn_inet_addr sp_inet_addr = (lpfn_inet_addr)inet_addr;
extern "C" unsigned long __stdcall sys_inet_addr(const char *cp);

typedef char* (__stdcall * lpfn_inet_ntoa)(struct in_addr in);
lpfn_inet_ntoa sp_inet_ntoa = (lpfn_inet_ntoa)inet_ntoa;
extern "C" char* __stdcall sys_inet_ntoa(struct in_addr in);

typedef int (__stdcall * lpfn_ioctlsocket)(SOCKET s,long cmd,u_long *argp);
lpfn_ioctlsocket sp_ioctlsocket = (lpfn_ioctlsocket)ioctlsocket;
extern "C" int __stdcall sys_ioctlsocket(SOCKET s,long cmd,u_long *argp);

typedef u_long (__stdcall * lpfn_ntohl)(u_long netlong);
lpfn_ntohl sp_ntohl = (lpfn_ntohl)ntohl;
extern "C" u_long __stdcall sys_ntohl(u_long netlong);

typedef u_short (__stdcall * lpfn_ntohs)(u_short netshort);
lpfn_ntohs sp_ntohs = (lpfn_ntohs)ntohs;
extern "C" u_short __stdcall sys_ntohs(u_short netshort);

typedef int (__stdcall * lpfn_recv)(SOCKET s,char *buf,int len,int flags);
lpfn_recv sp_recv = (lpfn_recv)recv;
extern "C" int __stdcall sys_recv(SOCKET s,char *buf,int len,int flags);

typedef int (__stdcall * lpfn_recvfrom)(SOCKET s,char *buf,int len,int flags,struct sockaddr *from,int *fromlen);
lpfn_recvfrom sp_recvfrom = (lpfn_recvfrom)recvfrom;
extern "C" int __stdcall sys_recvfrom(SOCKET s,char *buf,int len,int flags,struct sockaddr *from,int *fromlen);

typedef int (__stdcall * lpfn_select)(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,const struct timeval *timeout);
lpfn_select sp_select = (lpfn_select)select;
extern "C" int __stdcall sys_select(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,const struct timeval *timeout);

typedef int (__stdcall * lpfn_send)(SOCKET s,const char *buf,int len,int flags);
lpfn_send sp_send = (lpfn_send)send;
extern "C" int __stdcall sys_send(SOCKET s,const char *buf,int len,int flags);

typedef int (__stdcall * lpfn_sendto)(SOCKET s,const char *buf,int len,int flags,const struct sockaddr *to,int tolen);
lpfn_sendto sp_sendto = (lpfn_sendto)sendto;
extern "C" int __stdcall sys_sendto(SOCKET s,const char *buf,int len,int flags,const struct sockaddr *to,int tolen);

typedef int (__stdcall * lpfn_setsockopt)(SOCKET s,int level,int optname,const char *optval,int optlen);
lpfn_setsockopt sp_setsockopt = (lpfn_setsockopt)setsockopt;
extern "C" int __stdcall sys_setsockopt(SOCKET s,int level,int optname,const char *optval,int optlen);

typedef int (__stdcall * lpfn_shutdown)(SOCKET s,int how);
lpfn_shutdown sp_shutdown = (lpfn_shutdown)shutdown;
extern "C" int __stdcall sys_shutdown(SOCKET s,int how);


typedef SOCKET (__stdcall * lpfn_socket)(int af,int type,int protocol);
lpfn_socket sp_socket = (lpfn_socket)socket;
extern "C" SOCKET __stdcall sys_socket(int af,int type,int protocol);

typedef struct hostent* (__stdcall * lpfn_gethostbyname)(const char *name);
lpfn_gethostbyname sp_gethostbyname = (lpfn_gethostbyname)gethostbyname;
extern "C" struct hostent* __stdcall sys_gethostbyname(const char *name);

typedef int (__stdcall * lpfn_gethostname)(char *name,int namelen);
lpfn_gethostname sp_gethostname = (lpfn_gethostname)gethostname;
extern "C" int __stdcall sys_gethostname(char *name,int namelen);

typedef int (__stdcall * lpfn_WSAGetLastError)(void);
lpfn_WSAGetLastError sp_WSAGetLastError = (lpfn_WSAGetLastError)WSAGetLastError;
extern "C" int __stdcall sys_WSAGetLastError(void);

typedef int (__stdcall * lpfn_WSAStartup)(WORD wVersionRequested,LPWSADATA lpWSAData);
lpfn_WSAStartup sp_WSAStartup = (lpfn_WSAStartup)WSAStartup;
extern "C" int __stdcall sys_WSAStartup(WORD wVersionRequested,LPWSADATA lpWSAData);

typedef int (__stdcall * lpfn_WSACleanup)(void);
lpfn_WSACleanup sp_WSACleanup = (lpfn_WSACleanup)WSACleanup;
extern "C" int __stdcall sys_WSACleanup(void);

typedef int (__stdcall * lpfn___WSAFDIsSet)(SOCKET fd,fd_set *set);
lpfn___WSAFDIsSet sp___WSAFDIsSet = (lpfn___WSAFDIsSet)__WSAFDIsSet;
extern "C" int __stdcall sys___WSAFDIsSet(SOCKET fd,fd_set *set);



int __stdcall sys_bind(SOCKET s,const struct sockaddr *name,int namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_bind == NULL ) {
		orig_Com_Printf("bind\n");
		ExitProcess(1);
	}
	#endif

	sockaddr_in * sock_in = (sockaddr_in*)name;
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING bind! %s %hu\n",sock_in->sin_addr.s_addr,ntohs(sock_in->sin_port));
	#endif

	return sp_bind(s,name,namelen);
}

int sys_closesocket(SOCKET s)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_closesocket == NULL ) {
		orig_Com_Printf("closesocket\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING closesocket!\n");
	#endif
	return sp_closesocket(s);
	//return NULL;
}

int sys_connect(SOCKET s,const struct sockaddr *name,int namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_connect == NULL ) {
		orig_Com_Printf("connect\n");
		ExitProcess(1);
	}
	#endif
	sockaddr_in * sock_in=(sockaddr_in*)name;
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING connect! %s : %hu\n",inet_ntoa(sock_in->sin_addr),ntohs(sock_in->sin_port));
	#endif
	return sp_connect(s,name,namelen);
}

int sys_getsockname(SOCKET s, struct sockaddr *name,int *namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_getsockname == NULL ) {
		orig_Com_Printf("getsockname\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING getsockname!\n");
	#endif
	return sp_getsockname(s,name,namelen);
}

u_long sys_htonl(u_long hostlong)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_htonl == NULL ) {
		orig_Com_Printf("htonl\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING htonl!\n");
	#endif
	return sp_htonl(hostlong);
}

u_short sys_htons(u_short hostshort)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_htons == NULL ) {
		orig_Com_Printf("htons\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING htons!\n");
	#endif
	return sp_htons(hostshort);
}

unsigned long sys_inet_addr(const char *cp)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_inet_addr == NULL ) {
		orig_Com_Printf("inet_addr\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING inet_addr %s!\n",cp);
	#endif
	return sp_inet_addr(cp);
}

char* sys_inet_ntoa(struct in_addr in)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_inet_ntoa == NULL ) {
		orig_Com_Printf("inet_ntoa\n");
		ExitProcess(1);
	}
	#endif
	char * pc_out;
	pc_out=sp_inet_ntoa(in);
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING inet_ntoa %s\n",pc_out);
	#endif
	return pc_out;
}

int sys_ioctlsocket(SOCKET s,long cmd,u_long *argp)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_ioctlsocket == NULL ) {
		orig_Com_Printf("ioctlsocket\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING ioctlscoket!\n");
	#endif

	return sp_ioctlsocket(s,cmd,argp);
	//return NULL;
}

u_long sys_ntohl(u_long netlong)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_ntohl == NULL ) {
		orig_Com_Printf("ntohl\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING sys_ntohl!\n");
	#endif
	return sp_ntohl(netlong);
}

u_short sys_ntohs(u_short netshort)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_ntohs == NULL ) {
		orig_Com_Printf("ntohs\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING ntohs!\n");
	#endif
	return sp_ntohs(netshort);
}

int sys_recv(SOCKET s,char *buf,int len,int flags)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_recv == NULL ) {
		orig_Com_Printf("recv\n");
		ExitProcess(1);
	}
	#endif
	int i_ret;
	i_ret = sp_recv(s,buf,len,flags);
	return i_ret;
	#if 0
	if ( i_ret == -1 )
	{
		return i_ret;
	}
	else if(i_ret > 1400)
	{
		//PrintOut(PRINT_LOG,"Received Larger than our buffer!\n");
		return i_ret;
	}
	
	unsigned char ac_buf[1401];
	memcpy(ac_buf,buf,i_ret);
	ac_buf[i_ret]=0x00;
	//PrintOut(PRINT_LOG,"CALLING recv ");
	int i;
	for (i = 0; i < i_ret; i++)
	{
		if (i > 0) PrintOut(PRINT_LOG_EMPTY,":");
		PrintOut(PRINT_LOG_EMPTY,"%c", ac_buf[i],true);
	}
	PrintOut(PRINT_LOG_EMPTY,"\n",true);

	return i_ret;
	#endif

}

int sys_recvfrom(SOCKET s,char *buf,int len,int flags,struct sockaddr *from,int *fromlen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_recvfrom == NULL ) {
		orig_Com_Printf("recvfrom\n");
		ExitProcess(1);
	}
	#endif
	int i_ret;
	i_ret = sp_recvfrom(s,buf,len,flags,from,fromlen);
	return i_ret;
	#if 0
	sockaddr_in * sock_in = (sockaddr_in *)from;
	
	if ( i_ret == -1 )
	{
		return i_ret;
	}
	else if(i_ret > 1400)
	{
		PrintOut(PRINT_LOG,"Received Larger than our buffer!\n");
		return i_ret;
	}
	unsigned char ac_buf[1401];
	memcpy(ac_buf,buf,i_ret);
	ac_buf[i_ret]=0x00;
	PrintOut(PRINT_LOG,"PRINT_LOG,CALLING recvfrom %s:%hu ",inet_ntoa(sock_in->sin_addr),ntohs(sock_in->sin_port));
	int i;
	for (i = 0; i < i_ret; i++)
	{
		if (i > 0) PrintOut(PRINT_LOG_EMPTY,":",true);
		PrintOut(PRINT_LOG_EMPTY,"%c", ac_buf[i],true);
	}
	PrintOut(PRINT_LOG_EMPTY,"\n",true);

	return i_ret;
	#endif

}

int sys_select(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,const struct timeval *timeout)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_select == NULL ) {
		orig_Com_Printf("select\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	// PrintOut(PRINT_LOG,"CALLING select!\n");
	#endif
	return sp_select(nfds,readfds,writefds,exceptfds,timeout);
}

int sys_send(SOCKET s,const char *buf,int len,int flags)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_send == NULL ) {
		orig_Com_Printf("send\n");
		ExitProcess(1);
	}
	#endif
	int i_ret;
	i_ret = sp_send(s,buf,len,flags);
	return i_ret;
	#if 0

	unsigned char ac_buf[1401];
	if ( len > 1400 ) 
	{
		PrintOut(PRINT_LOG,"Sending Larger than our buffer!\n");
		return i_ret;
	}
	memcpy(ac_buf,buf,len);
	ac_buf[len]=0x00;
	PrintOut(PRINT_LOG,"CALLING send ");
	int i;
	for (i = 0; i < len; i++)
	{
		if (i > 0) PrintOut(PRINT_LOG_EMPTY,":",true);
		PrintOut(PRINT_LOG_EMPTY,"%c", ac_buf[i],true);
	}
	PrintOut(PRINT_LOG_EMPTY,"\n",true);

	return i_ret;
	#endif
}

int sys_sendto(SOCKET s,const char *buf,int len,int flags,const struct sockaddr *to,int tolen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_sendto == NULL ) {
		orig_Com_Printf("sendto\n");
		ExitProcess(1);
	}
	#endif
	int i_ret;
	i_ret = sp_sendto(s,buf,len,flags,to,tolen);
	return i_ret;

	#if 0
	sockaddr_in * sock_in = (sockaddr_in *)to;
	unsigned char ac_buf[1401];
	if ( len > 1400 ) 
	{
		PrintOut(PRINT_LOG,"Sending Larger than our buffer!\n");
		return i_ret; 
	}
	memcpy(ac_buf,buf,len);
	ac_buf[len]=0x00;
	PrintOut(PRINT_LOG,"CALLING sendto %s:%hu ",inet_ntoa(sock_in->sin_addr),ntohs(sock_in->sin_port));
	int i;
	for (i = 0; i < len; i++)
	{
		if (i > 0) PrintOut(PRINT_LOG_EMPTY,":",true);
		PrintOut(PRINT_LOG_EMPTY,"%c", ac_buf[i],true);
	}
	PrintOut(PRINT_LOG_EMPTY,"\n",true);

	return i_ret;
	#endif
}

int sys_setsockopt(SOCKET s,int level,int optname,const char *optval,int optlen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_setsockopt == NULL ) {
		orig_Com_Printf("setsockopt\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING setsockopt!\n");
	#endif
	return sp_setsockopt(s,level,optname,optval,optlen);
}

int sys_shutdown(SOCKET s,int how)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_shutdown == NULL ) {
		orig_Com_Printf("shutdown\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING shutdown!\n");
	#endif
	return shutdown(s,how);
}

SOCKET sys_socket(int af,int type,int protocol)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_socket == NULL ) {
		orig_Com_Printf("socket\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING socket! %i %i %i\n",af,type,protocol);
	#endif
	return sp_socket(af,type,protocol);
}

struct hostent* sys_gethostbyname(const char *name)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_gethostbyname == NULL ) {
		orig_Com_Printf("gethostbyname\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING gethostbyname %s!\n",name);
	#endif
	return sp_gethostbyname(name);
}

int sys_gethostname(char *name,int namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_gethostname == NULL ) {
		orig_Com_Printf("gethostname\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING gethostname! %s\n",name);
	#endif
	return sp_gethostname(name,namelen);

}

int sys_WSAGetLastError(void)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_WSAGetLastError == NULL ) {
		orig_Com_Printf("WSAGetLastError\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING WSAGetLastError!\n");
	#endif
	return sp_WSAGetLastError();
}

/*
	SoFPlus init timing notes

	- Called from NET_Init() (WSAStartup) during Qcommon_Init(), before Cbuf_AddLateCommands.
	- SoFPlus runs addon init via Cbuf_AddText("exec sofplus.cfg").
	- sp_sc_timer code enqueues commands that execute in the next Cbuf_Execute() inside the main loop.
	- Effective timer stages: here (WSAStartup), then WinMain pacing, then Cbuf_Execute.
*/
int sys_WSAStartup(WORD wVersionRequested,LPWSADATA lpWSAData)
{
	static bool doOnce = false;
	#ifdef WINSOCK_ASSERT
	if ( sp_WSAStartup == NULL ) {
		orig_Com_Printf("WSAStartup\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING WSA_STARTUP!\n");
	#endif

	if (!doOnce) {
		doOnce = true;

	}
	// __asm__("int $3");
	int ret = sp_WSAStartup(wVersionRequested,lpWSAData);

	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING WSA_STARTUP_AFTERXX!\n");
	#endif

	// Mark sofplus as fully initialized
	sofplus_initialized = true;
	PrintOut(PRINT_LOG, "sofplus_initialized set to true\n");
	return ret;
}

int sys_WSACleanup(void)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_WSACleanup == NULL ) {
		orig_Com_Printf("WSACleanup\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING WSA_CLEANUP!\n");
	#endif
	return sp_WSACleanup();
}

int sys___WSAFDIsSet(SOCKET fd,fd_set *set)
{   
	#ifdef WINSOCK_ASSERT
	if ( sp___WSAFDIsSet == NULL ) {
		orig_Com_Printf("__WSAFDIsSet\n");
		ExitProcess(1);
	}
	#endif
	#ifdef WINSOCK_LOGGING
	PrintOut(PRINT_LOG,"CALLING __WSAFDIsSet!\n");
	#endif
	return sp___WSAFDIsSet(fd,set);
}

typedef BOOL(WINAPI* DllMainFunction)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);


// A function that returns the base address of the current process
DWORD get_base_address() {
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hModule = GetModuleHandle(NULL);
    MODULEINFO moduleInfo;
    GetModuleInformation(hProcess, hModule, &moduleInfo, sizeof(moduleInfo));
    return (DWORD)moduleInfo.lpBaseOfDll;
}

// A function that reads a null-terminated string from a given memory address
char* read_string(uintptr_t address,char * result) {
    int i = 0; // a counter for the buffer index
    char c; // a variable to store the current character
    do {
        c = *(char*)address; // read a character from the address
        result[i] = c; // store the character in the buffer
        i++; // increment the buffer index
        address++; // increment the address
    } while (c != '\0' && i < 256); // repeat until the end of the string or the buffer limit

    return result;
}

// A function that formats the error message from a given error code
const char* format_error_message(DWORD error_code) {
    char* message = NULL;
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message,
        0,
        NULL
    );
    if (length == 0) {
        return "Unknown error";
    } else {
        return message;
    }
}


void wsock_link(void)
{
    HMODULE o_wsock = LoadLibrary("WSOCK32");
	if ( o_wsock == NULL ) {
		MessageBox(NULL, "LoadLibrary(WSOCK32)", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
	}
	char ac_funcs[24][32] = {"bind","connect","getsockname","htonl","htons","inet_addr","inet_ntoa","ntohl","ntohs","recv","recvfrom","select","send","sendto","bind","setsockopt","shutdown","socket","gethostbyname","gethostname","WSAGetLastError","WSAStartup","WSACleanup","__WSAFDIsSet"};
	void **pv_funcs[24] = {(void**)&sp_bind,(void**)&sp_connect,(void**)&sp_getsockname,(void**)&sp_htonl,(void**)&sp_htons,(void**)&sp_inet_addr,(void**)&sp_inet_ntoa,(void**)&sp_ntohl,(void**)&sp_ntohs,(void**)&sp_recv,(void**)&sp_recvfrom,(void**)&sp_select,(void**)&sp_send,(void**)&sp_sendto,(void**)&sp_bind,(void**)&sp_setsockopt,(void**)&sp_shutdown,(void**)&sp_socket,(void**)&sp_gethostbyname,(void**)&sp_gethostname,(void**)&sp_WSAGetLastError,(void**)&sp_WSAStartup,(void**)&sp_WSACleanup,(void**)&sp___WSAFDIsSet};
	bool wsockerror = false;
	for ( int i = 0; i < 24; i++ ) {
            if ( SoFplusLoadFn(o_wsock,(void**)pv_funcs[i],&ac_funcs[i][0]) == false )
		{
			MessageBox(NULL, (std::string(&ac_funcs[i][0]) + "Couldn't Load a wsock function").c_str(), "Error", MB_ICONERROR | MB_OK);
			wsockerror = true;
		}
	}
	sp___WSAFDIsSet = (lpfn___WSAFDIsSet)__WSAFDIsSet;

	if (wsockerror) {
		ExitProcess(1);
	}
}
/*
	Patches spcl.dll file before load.
*/
void sofplus_copy(void)
{
	#if 0
	wchar_t tempFileName[MAX_PATH];
	//-----------------------------------------
				
	// open spcl.dll file
	HANDLE hFile = CreateFileW(L"spcl.dll", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
	// Handle error
		MessageBox(NULL, "CreateFileW", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
		return 1;
	}

	// spcl.dll file size
	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
	// Handle error
		CloseHandle(hFile);
		MessageBox(NULL, "GetFileSize", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
		return 1;
	}

	// malloc a buffer equal size to spcl.dll
	LPBYTE pBuffer = (LPBYTE)malloc(fileSize);
	if (pBuffer == NULL) {
	// Handle memory allocation error
		CloseHandle(hFile);
		MessageBox(NULL, "Memory allocation error!", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
		return 1;
	}

	// copy spcl.dll bytes into buffer
	DWORD bytesRead;
	if (!ReadFile(hFile, pBuffer, fileSize, &bytesRead, NULL)) {
	// Handle error
		free(pBuffer);
		CloseHandle(hFile);
		MessageBox(NULL, "ReadFile", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
		return 1;
	}

//=================================================================================
	// Modify the in-memory file spcl.dll
//=================================================================================

	/*
	    Why do we NOP this?
		Nop the DllMain init function.
	*/

	#ifdef NOP_SOFPLUS_INIT_FUNCTION
	void * p = pBuffer+0x9EBD;
	WriteByte(p,0x90);
	WriteByte(p+1,0x90);
	WriteByte(p+2,0x90);
	WriteByte(p+3,0x90);
	WriteByte(p+4,0x90);
	#endif
	// Force return SUCCESS.
	#if 1
	WriteByte(pBuffer+0x9EC4,0xEB);
	#else
	WriteByte(pMappedFile+0x9EC4,0x90);
	WriteByte(pMappedFile+0x9EC5,0x90);
	#endif

//=================================================================================
// Get the path to the temporary directory
//=================================================================================
	wchar_t tempPath[MAX_PATH];
	if (GetTempPathW(MAX_PATH, tempPath) == 0) {
		// Handle error
		free(pBuffer);
		CloseHandle(hFile);
		MessageBox(NULL, "GetTempPath Error!", "Error", MB_ICONERROR | MB_OK);
		ExitProcess(1);
		return 1;
	}
	// char erm[256];
	// wsprintf(erm,"%s",tempPath);
	// MessageBox(NULL, erm, "NoIdea", MB_ICONERROR | MB_OK);

	// Append your desired folder name
	wchar_t tempFolder[MAX_PATH];
	wcscpy_s(tempFolder, _countof(tempFolder), tempPath);
	wcscat_s(tempFolder, _countof(tempFolder), L"sof_buddy");

	if (!CreateDirectoryW(tempFolder, NULL)) {
	// Check if the folder already exists
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
		// Handle error
			ExitProcess(1);
			return 1;
		}
	}

	// Combine the folder and filename using PathCombine
	PathCombineW(tempFileName, tempFolder, L"spcl");
	PrintOut(PRINT_LOG,"Forming fileName %ls from %ls where temppath is %ls\n",tempFileName,tempFolder,tempPath);
	// Verify that the resulting path is not too long
	if (wcslen(tempFileName) >= MAX_PATH) {
		// Handle error
		ExitProcess(1);
		return 1;
	}

	#if 0
	// Append a random string to the filename
	wchar_t randomString[8]; // Change the length as needed
	GenerateRandomString(randomString, sizeof(randomString) / sizeof(randomString[0]));

	// Append the random string to the filename
	wcscat(tempFileName, L"_");
	wcscat(tempFileName, randomString);
	wcscat(tempFileName, L".dll");
	#else
	wcscat(tempFileName, L".dll");
	#endif

	/*
	This argument:           |             Exists            Does not exist
	-------------------------+------------------------------------------------------
	CREATE_ALWAYS            |            Truncates             Creates
	CREATE_NEW         +-----------+        Fails               Creates
	OPEN_ALWAYS     ===| does this |===>    Opens               Creates
	OPEN_EXISTING      +-----------+        Opens                Fails
	TRUNCATE_EXISTING        |            Truncates              Fails
	*/

//=================================================================================
	// Write the tempfile containing the modified spcl.dll buffer we read earlier.
//=================================================================================
	HANDLE hTempFile = CreateFileW(
		tempFileName, 
		GENERIC_WRITE, 
		0, 
		NULL, 
		OPEN_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, 
		NULL);
	if ( !hTempFile ) {
		free(pBuffer);
		CloseHandle(hFile);
		ExitProcess(1);
		return 1;
	}

	DWORD error_code1 = GetLastError();

	#if 0
	char buffer[32];
	sprintf(buffer, "%d", error_code1);
	//MessageBoxA(NULL, format_error_message(error_code1), "WriteFile Error", MB_ICONERROR | MB_OK);
	MessageBoxA(NULL, buffer, "WriteFile Error", MB_ICONERROR | MB_OK);
	#endif
	bool spcl_tmp_exists = false;
	if (error_code1 == ERROR_ALREADY_EXISTS || error_code1 == ERROR_SHARING_VIOLATION)
		spcl_tmp_exists = true;
	
	//We need to write to it.
	if ( !spcl_tmp_exists ) { 
		// Write the modified buffer to the temp file
		DWORD bytesWritten;
		if (!WriteFile(hTempFile, pBuffer, bytesRead, &bytesWritten, NULL)) {
		// Handle error
			free(pBuffer);
			CloseHandle(hTempFile);
			CloseHandle(hFile);

			// Handle error
			DWORD error_code = GetLastError();
			char* error_message = format_error_message(error_code);
			MessageBoxA(NULL, error_message, "WriteFile Error", MB_ICONERROR | MB_OK);
	
			ExitProcess(1);
			return 1;
		}
	}
	
	// Close handles and free memory
	free(pBuffer);
	CloseHandle(hTempFile);
	CloseHandle(hFile);

	//=================================================================================

	//Now check if it has correct checksum of spcl.dll
	FILE* fp;
	char buf[32768];
    if ((fp = _wfopen(tempFileName, L"rb")) != NULL) {
		uint32_t crc = 0;
		while (!feof(fp) && !ferror(fp))
			crc32(buf, fread(buf, 1, sizeof(buf), fp), &crc);

		if (!ferror(fp)) {
			// Successfully read the file, perform further operations
			// MessageBoxW(NULL, (std::wstring(L"spcl.dll temp checksum: ") + std::to_wstring(crc)).c_str(), L"Success", MB_ICONINFORMATION | MB_OK);
			if ( crc != 2057889219 ) {
				fclose(fp);
				DeleteFileW(tempFileName);
				MessageBox(NULL,"Incorrect tempfile checksum, deleted, try again.","Error",MB_ICONERROR|MB_OK);
				ExitProcess(1);
				return 1;
			}
		} else {
			// Handle file reading error
			MessageBoxW(NULL, L"Error reading spcl.dll temp file", L"Error", MB_ICONERROR | MB_OK);
			fclose(fp);
			ExitProcess(1);
			return 1;
		}

		fclose(fp);
	}


//-----------------------------
	// Load the modified DLL from the temp file
	//MessageBoxW(NULL, (std::wstring(L"Path is : ") + tempFileName).c_str(), L"Success", MB_ICONINFORMATION | MB_OK);
	// o_sofplus = LoadLibraryExW(tempFileName,NULL,LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );
	PrintOut(PRINT_LOG,"Loading %ls\n",tempFileName);
	o_sofplus = LoadLibraryW(tempFileName);
	#else
		o_sofplus = LoadLibrary("spcl.dll");
	#endif

	if (o_sofplus == NULL) {
		DWORD error = GetLastError();
		MessageBoxW(NULL, (L"Cannot load sofplus!! Error code: " + std::to_wstring(error)).c_str(), L"Error", MB_ICONERROR | MB_OK);
        ExitProcess(1);
        // Handle error
        return;
	} else {
		//MessageBox(NULL, "Success", "Error", MB_ICONERROR | MB_OK);
		PrintOut(PRINT_GOOD,"Successfully loaded sofplus! %08X\n",o_sofplus);

		//sofplus patches done.
		
		
		char ac_funcs[24][32] = {"bind","connect","getsockname","htonl","htons","inet_addr","inet_ntoa","ntohl","ntohs","recv","recvfrom","select","send","sendto","bind","setsockopt","shutdown","socket","gethostbyname","gethostname","WSAGetLastError","WSAStartup","WSACleanup","__WSAFDIsSet"};
		void **pv_funcs[24] = {(void**)&sp_bind,(void**)&sp_connect,(void**)&sp_getsockname,(void**)&sp_htonl,(void**)&sp_htons,(void**)&sp_inet_addr,(void**)&sp_inet_ntoa,(void**)&sp_ntohl,(void**)&sp_ntohs,(void**)&sp_recv,(void**)&sp_recvfrom,(void**)&sp_select,(void**)&sp_send,(void**)&sp_sendto,(void**)&sp_bind,(void**)&sp_setsockopt,(void**)&sp_shutdown,(void**)&sp_socket,(void**)&sp_gethostbyname,(void**)&sp_gethostname,(void**)&sp_WSAGetLastError,(void**)&sp_WSAStartup,(void**)&sp_WSACleanup,(void**)&sp___WSAFDIsSet};
		for ( int i = 0; i < 24; i++ ) {
            if ( SoFplusLoadFn((HMODULE)o_sofplus,(void**)pv_funcs[i],&ac_funcs[i][0]) == false )
			{
				#ifdef __LOGGING__
				MessageBox(NULL, (std::string(&ac_funcs[i][0]) + "Couldn't Load a sofplus function").c_str(), "Error", MB_ICONERROR | MB_OK);
				#endif
			}
		}

		//Couldn't GetProcAddress these for some reason.
		//"closesocket"
		//(void**)&sp_closesocket
		sp_closesocket = &closesocket;
		//"ioctlsocket"
		//(void**)&sp_ioctlsocket
		sp_ioctlsocket = &ioctlsocket;

		

	}
	
}

#define SPCL_CHECKSUM 4207493893
/*
DLL_PROCESS_ATTACH (1): DLL is loaded into a process.
DLL_THREAD_ATTACH (2): A new thread is created in the process.
DLL_THREAD_DETACH (3): A thread exits cleanly.
DLL_PROCESS_DETACH (0): DLL is unloaded from the process.
*/
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	HANDLE hTempFile;
	if ( dwReason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls(hInstance);

		load_vectored_exception_handlers();
		if (g_vectored_handlers_available && g_vectored_handler == NULL) {
			g_vectored_handler = pAddVectoredExceptionHandler(1, vectored_exception_handler);
			PrintOut(PRINT_LOG, "Registered vectored exception handler: %p\n", g_vectored_handler);
		} else if (!g_vectored_handlers_available) {
			PrintOut(PRINT_LOG, "Vectored exception handlers not available (requires XP SP1+)\n");
		}

		// Initialize dbghelp for symbol resolution
		if (!g_dbghelp_initialized) {
			if (SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
				SymSetOptions(SYMOPT_UNDNAME);
				g_dbghelp_initialized = true;
				PrintOut(PRINT_LOG, "DbgHelp initialized\n");
			} else {
				PrintOut(PRINT_LOG, "DbgHelp failed to initialize: %p\n", GetLastError());
			}
		}

		static bool init = false;
		if (init==false) {
			PrintOut(PRINT_LOG,"DllMain\n");
			/*
				we are loaded thus they have sofbuddy in sof.exe as wsock wrapper.
				we check if spcl.dll exists, as to whether to load it or not.
			*/
			char ac_sofplus[MAX_PATH];
			
			bool b_sofplus = false;

			strcpy(ac_sofplus,"spcl.dll");
			PrintOut(PRINT_LOG,"DllMain 2\n");
			PrintOut(PRINT_LOG,"=== About to get base address ===\n");
			DWORD base_address = get_base_address(); // get the base address of the process
			PrintOut(PRINT_LOG,"Base address: 0x%08X\n", base_address);
			PrintOut(PRINT_LOG,"=== About to read memory ===\n");
			DWORD offset = 0x11AD72; // the offset to the memory address
			DWORD address = base_address + offset; // the absolute memory address
			PrintOut(PRINT_LOG,"Reading from address: 0x%08X\n", address);
			char result[256];
		    read_string(address,result);
			/*
			MessageBox(NULL, result, "Success", MB_OK);
			ExitProcess(1);
			if (strcmp(result, "sofbuddy.dll") == 0) { // compare the result with the target
			    MessageBox(NULL, result, "Success", MB_OK); // show a message box with the result
			} else if (strcmp(result, "spcl.dll") == 0) {
			   
			} else {
				 MessageBox(NULL, "No match", "Failure", MB_OK); // show a message box with no match
			}

			ExitProcess(1);
			*/
			
			#if 1
			if( access( ac_sofplus, F_OK ) != -1 ) {
				PrintOut(PRINT_LOG,"spcl.dll exists\n");
				//spcl.dll exists.
				FILE *fp;

				char buf[32768];
                if ((fp = fopen(ac_sofplus, "rb")) != NULL) {
					uint32_t crc = 0;
					while(!feof(fp) && !ferror(fp))
						crc32(buf, fread(buf, 1, sizeof(buf), fp), &crc);
					if(!ferror(fp)) {
						// PrintOut(PRINT_GOOD,"%08x : spsv.dll\n", crc);
						// MessageBox(NULL, (std::string("spcl.dll checksum") + std::to_string(crc)).c_str(), "Error", MB_ICONERROR | MB_OK);
						if ( crc == SPCL_CHECKSUM ) {
							b_sofplus = true;
							PrintOut(PRINT_LOG,"GOOD SPCL FILE\n");
						}
						else {
							PrintOut(PRINT_LOG,"BAD SPCL FILE\n");
							MessageBox(NULL, "Sofplus not installed correctly. Requires version 20140531.", "Error", MB_ICONERROR | MB_OK);
							ExitProcess(1);
							return 1;
						}
					}
					fclose(fp);
				}
			}
			#endif
			
			if (
				#if 1
				b_sofplus
				#else
				false
				#endif
			) {
				PrintOut(PRINT_LOG,"SOFPLUS LOAD ATTEMPT\n");
				PrintOut(PRINT_GOOD,"SoFplus detected\n");
				sofplus_copy();
			} else {
				PrintOut(PRINT_LOG,"NON SOFPLUS LOAD ATTEMPT\n");
				wsock_link();
			}

			// Cache modules that are already loaded at DLL startup
			// SoF.exe is always loaded, sof_buddy.dll is us
			CallsiteClassifier::cacheModuleLoaded(Module::SofExe);
			HookCallsite::cacheSelfModule();
			
			// Cache spcl.dll if it was loaded (o_sofplus)
			if (o_sofplus) {
				CallsiteClassifier::cacheModuleLoaded(Module::SpclDll);
			}
			
			PrintOut(PRINT_LOG,"Before lifecycle_EarlyStartup()\n");
			lifecycle_EarlyStartup();
			//CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)sofbuddy_thread,NULL,0,NULL);
			
			init=true;
		}//doonce //init

	} else if ( dwReason == DLL_PROCESS_DETACH ) { 
		//We don't want to call PrintOut/Com_Printf here - noticed crash on linux.
		#ifndef NDEBUG
		ParentRecorder::Instance().flushAll();
		#endif
		//FreeLibrary(o_sofplus);
		if (g_vectored_handler != NULL && g_vectored_handlers_available && pRemoveVectoredExceptionHandler != NULL) {
			pRemoveVectoredExceptionHandler(g_vectored_handler);
			g_vectored_handler = NULL;
		}

		if (g_dbghelp_initialized) {
			SymCleanup(GetCurrentProcess());
			g_dbghelp_initialized = false;
		}
		#if 0
		if (!DeleteFileW(tempFileName)) {
				// Handle error
				MessageBox(NULL, "Unable to delete temp spcl file!", "Error", MB_ICONERROR | MB_OK);
				return 1;
		}
		#endif
	}

	return TRUE;
}


bool SoFplusLoadFn(HMODULE sofplus, void **out_fn, const char *func)
{
	PrintOut(PRINT_LOG,"SoFplusLoadFn()\n");
	*out_fn = (void*)GetProcAddress(sofplus,func);
	if (*out_fn == NULL) return false;

	return true;
}