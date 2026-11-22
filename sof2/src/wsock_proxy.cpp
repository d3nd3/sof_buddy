#include "feature.h"
#include <winsock2.h>
#include <windows.h>

#define FWD(ret, name, ...) typedef ret (WINAPI *fn_##name)(__VA_ARGS__); static fn_##name p_##name;
FWD(int, bind, SOCKET s, const struct sockaddr* a, int n)
FWD(int, closesocket, SOCKET s)
FWD(int, connect, SOCKET s, const struct sockaddr* a, int n)
FWD(int, getsockname, SOCKET s, struct sockaddr* a, int* n)
FWD(u_long, htonl, u_long x)
FWD(u_short, htons, u_short x)
FWD(unsigned long, inet_addr, const char* s)
FWD(char*, inet_ntoa, struct in_addr in)
FWD(int, ioctlsocket, SOCKET s, long c, u_long* a)
FWD(u_long, ntohl, u_long x)
FWD(u_short, ntohs, u_short x)
FWD(int, recv, SOCKET s, char* b, int l, int f)
FWD(int, recvfrom, SOCKET s, char* b, int l, int f, struct sockaddr* from, int* fl)
FWD(int, select, int n, fd_set* r, fd_set* w, fd_set* e, const struct timeval* t)
FWD(int, send, SOCKET s, const char* b, int l, int f)
FWD(int, sendto, SOCKET s, const char* b, int l, int f, const struct sockaddr* to, int tl)
FWD(int, setsockopt, SOCKET s, int lv, int opt, const char* v, int ol)
FWD(int, shutdown, SOCKET s, int how)
FWD(SOCKET, socket, int af, int type, int proto)
FWD(struct hostent*, gethostbyname, const char* n)
FWD(int, gethostname, char* n, int l)
FWD(int, WSAGetLastError, void)
FWD(int, WSAStartup, WORD v, LPWSADATA d)
FWD(int, WSACleanup, void)
FWD(int, __WSAFDIsSet, SOCKET fd, fd_set* set)

static HMODULE load_real_wsock32(void) {
	char path[MAX_PATH];
	UINT n = GetSystemDirectoryA(path, MAX_PATH);
	if (n > 0 && n < MAX_PATH - 16) {
		lstrcatA(path, "\\WSOCK32.dll");
		HMODULE w = LoadLibraryA(path);
		if (w) return w;
	}
	return LoadLibraryA("WSOCK32.dll");
}

void sof2_wsock_link(void) {
	static volatile LONG linked = 0;
	if (InterlockedCompareExchange(&linked, 1, 0) != 0) return;
	HMODULE w = load_real_wsock32();
	if (!w) { MessageBoxA(nullptr, "LoadLibrary(WSOCK32)", "sof2bud", MB_ICONERROR); ExitProcess(1); }
#define LOAD(n) p_##n = (fn_##n)GetProcAddress(w, #n);
	LOAD(bind) LOAD(closesocket) LOAD(connect) LOAD(getsockname) LOAD(htonl) LOAD(htons)
	LOAD(inet_addr) LOAD(inet_ntoa) LOAD(ioctlsocket) LOAD(ntohl) LOAD(ntohs)
	LOAD(recv) LOAD(recvfrom) LOAD(select) LOAD(send) LOAD(sendto) LOAD(setsockopt)
	LOAD(shutdown) LOAD(socket) LOAD(gethostbyname) LOAD(gethostname)
	LOAD(WSAGetLastError) LOAD(WSAStartup) LOAD(WSACleanup) LOAD(__WSAFDIsSet)
#undef LOAD
}

extern "C" {
int WINAPI sys_bind(SOCKET s, const struct sockaddr* a, int n) { return p_bind(s, a, n); }
int WINAPI sys_closesocket(SOCKET s) { return p_closesocket(s); }
int WINAPI sys_connect(SOCKET s, const struct sockaddr* a, int n) { return p_connect(s, a, n); }
int WINAPI sys_getsockname(SOCKET s, struct sockaddr* a, int* n) { return p_getsockname(s, a, n); }
u_long WINAPI sys_htonl(u_long x) { return p_htonl(x); }
u_short WINAPI sys_htons(u_short x) { return p_htons(x); }
unsigned long WINAPI sys_inet_addr(const char* s) { return p_inet_addr(s); }
char* WINAPI sys_inet_ntoa(struct in_addr in) { return p_inet_ntoa(in); }
int WINAPI sys_ioctlsocket(SOCKET s, long c, u_long* a) { return p_ioctlsocket(s, c, a); }
u_long WINAPI sys_ntohl(u_long x) { return p_ntohl(x); }
u_short WINAPI sys_ntohs(u_short x) { return p_ntohs(x); }
int WINAPI sys_recv(SOCKET s, char* b, int l, int f) { return p_recv(s, b, l, f); }
int WINAPI sys_recvfrom(SOCKET s, char* b, int l, int f, struct sockaddr* from, int* fl) {
	return p_recvfrom(s, b, l, f, from, fl);
}
int WINAPI sys_select(int n, fd_set* r, fd_set* w, fd_set* e, const struct timeval* t) {
	return p_select(n, r, w, e, t);
}
int WINAPI sys_send(SOCKET s, const char* b, int l, int f) { return p_send(s, b, l, f); }
int WINAPI sys_sendto(SOCKET s, const char* b, int l, int f, const struct sockaddr* to, int tl) {
	return p_sendto(s, b, l, f, to, tl);
}
int WINAPI sys_setsockopt(SOCKET s, int lv, int opt, const char* v, int ol) { return p_setsockopt(s, lv, opt, v, ol); }
int WINAPI sys_shutdown(SOCKET s, int how) { return p_shutdown(s, how); }
SOCKET WINAPI sys_socket(int af, int type, int proto) { return p_socket(af, type, proto); }
struct hostent* WINAPI sys_gethostbyname(const char* n) { return p_gethostbyname(n); }
int WINAPI sys_gethostname(char* n, int l) { return p_gethostname(n, l); }
int WINAPI sys_WSAGetLastError() { return p_WSAGetLastError(); }
int WINAPI sys_WSAStartup(WORD v, LPWSADATA d) {
	if (!p_WSAStartup) return WSAEINVAL;
	sof2_lifecycle_on_wsock_ready();
	return p_WSAStartup(v, d);
}
int WINAPI sys_WSACleanup() { return p_WSACleanup(); }
int WINAPI sys___WSAFDIsSet(SOCKET fd, fd_set* set) { return p___WSAFDIsSet(fd, set); }
}
