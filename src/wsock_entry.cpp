#define _WIN32_WINNT 0x501

#include <windows.h>
#include <pathcch.h>

#include <io.h> 
// #include <unistd.h>
#include <winsock2.h>

#include <stdio.h>
#include <stdint.h>

#include <string>

#include <stdio.h>

#include "DetourXS/detourxs.h"

#include "sof_buddy.h"
#include "sof_compat.h"

#include "util.h"
#include "crc32.h"

#include "features.h"



HMODULE o_sofplus = NULL;


bool SoFplusLoadFn(HMODULE sofplus,void ** out_fn,char * func);

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


//#define WINSOCK_ASSERT
int __stdcall sys_bind(SOCKET s,const struct sockaddr *name,int namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_bind == NULL ) {
		orig_Com_Printf("bind\n");
		ExitProcess(1);
	}
	#endif

	sockaddr_in * sock_in = (sockaddr_in*)name;
	//PrintOut(PRINT_LOG,"CALLING bind! %s %hu\n",sock_in->sin_addr.s_addr,ntohs(sock_in->sin_port));


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
	//PrintOut(PRINT_LOG,"CALLING closesocket!\n");
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
	//PrintOut(PRINT_LOG,"CALLING connect! %s : %hu\n",inet_ntoa(sock_in->sin_addr),ntohs(sock_in->sin_port));
	sp_connect(s,name,namelen);
}

int sys_getsockname(SOCKET s, struct sockaddr *name,int *namelen)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_getsockname == NULL ) {
		orig_Com_Printf("getsockname\n");
		ExitProcess(1);
	}
	#endif
	//PrintOut(PRINT_LOG,"CALLING getsockname!\n");
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
	//PrintOut(PRINT_LOG,"CALLING htonl!\n");
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
	//PrintOut(PRINT_LOG,"CALLING htons!\n");
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
	//PrintOut(PRINT_LOG,"CALLING inet_addr %s!\n",cp);
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
	//PrintOut(PRINT_LOG,"CALLING inet_ntoa %s\n",pc_out);
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
	//PrintOut(PRINT_LOG,"CALLING ioctlscoket!\n");

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
	//PrintOut(PRINT_LOG,"CALLING ntohs!\n");
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
	//PrintOut("CALLING select!\n");
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
	//PrintOut(PRINT_LOG,"CALLING setsockopt!\n");
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
	//PrintOut(PRINT_LOG,"CALLING shutdown!\n");
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
	//PrintOut(PRINT_LOG,"CALLING socket! %i %i %i\n",af,type,protocol);
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
	//PrintOut(PRINT_LOG,"CALLING gethostbyname %s!\n",name);
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
	//PrintOut(PRINT_LOG,"CALLING gethostname! %s\n",name);
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
	//PrintOut(PRINT_LOG,"CALLING WSAGetLastError!\n");
	return sp_WSAGetLastError();
}

int sys_WSAStartup(WORD wVersionRequested,LPWSADATA lpWSAData)
{
	#ifdef WINSOCK_ASSERT
	if ( sp_WSAStartup == NULL ) {
		orig_Com_Printf("WSAStartup\n");
		ExitProcess(1);
	}
	#endif
	//PrintOut(PRINT_LOG,"CALLING WSA_STARTUP!\n");

	int ret = sp_WSAStartup(wVersionRequested,lpWSAData);
	resetTimers(0); 
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
	//PrintOut(PRINT_LOG,"CALLING WSA_CLEANUP!\n");
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
	//PrintOut(PRINT_LOG,"CALLING __WSAFDIsSet!\n");
	return sp___WSAFDIsSet(fd,set);
}

typedef BOOL(WINAPI* DllMainFunction)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	wchar_t tempFileName[MAX_PATH];
	HANDLE hTempFile;
	if ( dwReason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls(hInstance);

		static bool init = false;
		if (init==false) {
			char ac_sofplus[MAX_PATH];
			
			bool b_sofplus = false;

			strcpy(ac_sofplus,"spcl.dll");
			if( access( ac_sofplus, F_OK ) != -1 ) {
				b_sofplus = true;
			}
			else {
				MessageBox(NULL, "No spcl.dll found", "Error", MB_ICONERROR | MB_OK);
				return 1;
				#if 0
				strcpy(ac_sofplus,"SoFplus.dll");
				if( access( ac_sofplus, F_OK ) != -1 ) {
					b_sofplus = true;
				}
				#endif
			}
			DWORD dwProt=NULL;
			if ( b_sofplus == true) {
				PrintOut(PRINT_GOOD,"SoFplus detected\n");

//-----------------------------------------
				
				#if 1
				HANDLE hFile = CreateFileW(L"spcl.dll", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
			    // Handle error
					MessageBox(NULL, "CreateFileW", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

			// Get the size of the file
				DWORD fileSize = GetFileSize(hFile, NULL);
				if (fileSize == INVALID_FILE_SIZE) {
			    // Handle error
					CloseHandle(hFile);
					MessageBox(NULL, "GetFileSize", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

			// Read the entire file into a buffer
				LPBYTE pBuffer = (LPBYTE)malloc(fileSize);
				if (pBuffer == NULL) {
			    // Handle memory allocation error
					CloseHandle(hFile);
					MessageBox(NULL, "Memory allocation error!", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

				DWORD bytesRead;
				if (!ReadFile(hFile, pBuffer, fileSize, &bytesRead, NULL)) {
			    // Handle error
					free(pBuffer);
					CloseHandle(hFile);
					MessageBox(NULL, "ReadFile", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

				// Now you can modify the pBuffer as needed

			    #if 1
		    	void * p = pBuffer+0x9EBD;
			    WriteByte(p,0x90);
			    WriteByte(p+1,0x90);
			    WriteByte(p+2,0x90);
			    WriteByte(p+3,0x90);
			    WriteByte(p+4,0x90);
				

			    #if 1
		    	WriteByte(pBuffer+0x9EC4,0xEB);
				#else
		    	WriteByte(pMappedFile+0x9EC4,0x90);
		    	WriteByte(pMappedFile+0x9EC5,0x90);
				#endif
				#endif

				// Get the path to the temporary directory
				wchar_t tempPath[MAX_PATH];
				if (GetTempPathW(MAX_PATH, tempPath) == 0) {
			        // Handle error
					free(pBuffer);
					CloseHandle(hFile);
					MessageBox(NULL, "GetTempPath Error!", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

	    	// Append your desired folder name
				wchar_t tempFolder[MAX_PATH];
				wcscpy_s(tempFolder, MAX_PATH, tempPath);
				wcscat_s(tempFolder, MAX_PATH, L"sof_buddy");

				if (!CreateDirectoryW(tempFolder, NULL)) {
	    	    // Check if the folder already exists
					if (GetLastError() != ERROR_ALREADY_EXISTS) {
	    	        // Handle error
						ExitProcess(1);
						return 1;
					}
				}

				if (PathCchCombine(tempFileName, MAX_PATH, tempFolder, L"spcl.dll") != S_OK) {
	    	        // Handle error
	    	        ExitProcess(1);
					return 1;
				}

			// Open the temp file for writing
				HANDLE hTempFile = CreateFileW(tempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hTempFile == INVALID_HANDLE_VALUE) {
			    // Handle error
					free(pBuffer);
					CloseHandle(hFile);
					MessageBox(NULL, "CreateFile Error!", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

			// Write the modified buffer to the temp file
				DWORD bytesWritten;
				if (!WriteFile(hTempFile, pBuffer, bytesRead, &bytesWritten, NULL)) {
			    // Handle error
					free(pBuffer);
					CloseHandle(hTempFile);
					CloseHandle(hFile);
					MessageBox(NULL, "WriteFile Error!", "Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
					return 1;
				}

			// Close handles and free memory
				free(pBuffer);
				CloseHandle(hTempFile);
				CloseHandle(hFile);

				//MessageBoxW(NULL, (std::wstring(L"File successfully modified! : ") + tempFileName).c_str(), L"Success", MB_ICONINFORMATION | MB_OK);
//-----------------------------
				#endif
		    	// Load the modified DLL from the temp file
				o_sofplus = LoadLibraryW(tempFileName);
	    	    //o_sofplus = LoadLibrary("spcl.dll");
				if (o_sofplus == NULL) {
					DWORD error = GetLastError();
					MessageBoxW(NULL, (L"Cannot load sofplus!! Error code: " + std::to_wstring(error)).c_str(), L"Error", MB_ICONERROR | MB_OK);
					ExitProcess(1);
	    	        // Handle error
					return 1;
				} else {
					//MessageBox(NULL, "Success", "Error", MB_ICONERROR | MB_OK);
					PrintOut(PRINT_GOOD,"Successfully loaded sofplus! %08X\n",o_sofplus);

			    	//sofplus patches done.
			    	
					//Couldn't GetProcAddress these for some reason.
	    	    	//"closesocket"
	    	    	//(void**)&sp_closesocket
	    	    	sp_closesocket = &closesocket;
	    	    	//"ioctlsocket"
	    	    	//(void**)&sp_ioctlsocket
	    	    	sp_ioctlsocket = &ioctlsocket;
					char ac_funcs[24][32] = {"bind","connect","getsockname","htonl","htons","inet_addr","inet_ntoa","ntohl","ntohs","recv","recvfrom","select","send","sendto","bind","setsockopt","shutdown","socket","gethostbyname","gethostname","WSAGetLastError","WSAStartup","WSACleanup","__WSAFDIsSet"};
					void **pv_funcs[24] = {(void**)&sp_bind,(void**)&sp_connect,(void**)&sp_getsockname,(void**)&sp_htonl,(void**)&sp_htons,(void**)&sp_inet_addr,(void**)&sp_inet_ntoa,(void**)&sp_ntohl,(void**)&sp_ntohs,(void**)&sp_recv,(void**)&sp_recvfrom,(void**)&sp_select,(void**)&sp_send,(void**)&sp_sendto,(void**)&sp_bind,(void**)&sp_setsockopt,(void**)&sp_shutdown,(void**)&sp_socket,(void**)&sp_gethostbyname,(void**)&sp_gethostname,(void**)&sp_WSAGetLastError,(void**)&sp_WSAStartup,(void**)&sp_WSACleanup,(void**)&sp___WSAFDIsSet};
					for ( int i = 0; i < 24; i++ ) {
						if ( SoFplusLoadFn(o_sofplus,(void**)pv_funcs[i],&ac_funcs[i][0]) == false )
						{
							//MessageBox(NULL, (std::string(&ac_funcs[i][0]) + "Couldn't Load a sofplus function").c_str(), "Error", MB_ICONERROR | MB_OK);
							b_sofplus = false;
							PrintOut(PRINT_BAD,"Couldn't load : %s\n",ac_funcs[i]);
						}
					}

					//some functions not found in mingw ws32?
					if ( b_sofplus == false ){
						PrintOut(PRINT_BAD,"ERROR: Couldn't Load a sofplus function\n");
						//MessageBox(NULL, "Couldn't Load a sofplus function", "Error", MB_ICONERROR | MB_OK);
						//ExitProcess(1);
					}

				}
			} else{
				PrintOut(PRINT_BAD,"SoFplus not found\n");
			}	
			afterSoFplusInit();
			//CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)sofbuddy_thread,NULL,0,NULL);
			
			init=true;


		}//doonce //init

	} else if ( dwReason == DLL_PROCESS_DETACH ) { 
	    //FreeLibrary(o_sofplus);
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


bool SoFplusLoadFn(HMODULE sofplus,void ** out_fn,char * func)
{
	
	*out_fn = (void*)GetProcAddress(sofplus,func);
	if (*out_fn == NULL) return false;
}