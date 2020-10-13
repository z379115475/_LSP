/*
void Test()
{
	LOG(_T("Test Log"));
	LOG1(_T("Arg:%d"), 1);
	LOG2(_T("Arg:%d, %d"), 1, 2);

	LOG_WARN(_T("Test Log"));
	LOG_WARN1(_T("Arg:%d"), 1);

	LOG_LAST_ERROR();

	try
	{
		throw std::runtime_error("test of c ++ exception.");
	}
	catch (const std::exception &e)
	{
		LOGE(e);
	}

	BYTE bin[256];
	for(int i = 0; i < 256; i++)
		bin[i] = i;

	LOG_BIN(bin, 256);

	#ifdef __GNUC__ 
		LOGN(_T("test for LogN :%d, %d, %d"), 1, 2, 3);
	#endif
}
*/


#ifndef _CX_LOG_H_
#define	_CX_LOG_H_

#if defined(_MSC_VER)
	#pragma warning(disable: 4530)
#endif
#pragma warning(disable:4996)

#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <list>
#include <iterator>
#include <stdexcept>
#include <exception>

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)  
	#include <comdef.h>
#endif

#include <tchar.h>
#include <process.h>
#include <windows.h>

extern TCHAR         g_szLogPath[];

//////////////////////////////////////////////////////////////////////////
///文件型多线程日志类 class for log file

#ifndef CX_LOG_DEFAULT_FORMAT
	#if (defined(_MSC_VER) && _MSC_VER<=1310) || defined(__BORLANDC__)  || (defined(__GNUC__) && defined(_UNICODE))
		#define CX_LOG_DEFAULT_FORMAT _T("%s %s %s(%d): %s\r\n")
	#else
		#define CX_LOG_DEFAULT_FORMAT _T("%s %s %s(%s, %d): %s\r\n")
	#endif
#endif

#ifndef CX_LOG_DEFAULT_FORMAT_SIZE
	#define CX_LOG_DEFAULT_FORMAT_SIZE 32768		//32768
#endif

#ifndef CX_LOG_REPLACE_CONTROL
	#define CX_LOG_REPLACE_CONTROL true
#endif

#ifndef CX_LOG_REPLACE_ASCII
	#define CX_LOG_REPLACE_ASCII _T(' ')
#endif

class CxLog
{
public:
	enum EnumType{
		CX_LOG_MESSAGE = 0,
		CX_LOG_WARNING,
		CX_LOG_EXCEPTION,
		CX_LOG_ERROR
	};

	static CxLog& Instance()
	{
		static bool alive = false;
		static CxLog log(alive);

		if(!alive)
		{
			OutputDebugString(_T("CxLog has destroy!"));
			throw std::runtime_error("CxLog has destroy!");
		}

		return log;
	}

	struct Item
	{
		SYSTEMTIME _Time;//>time stamp
		TCHAR _szTime[24];

		LPTSTR _szSrc;//>source file name
		LPTSTR _szFunc;//>founction name
		ULONG _uLine;//>line number

		EnumType _eType;//
		LPTSTR _szDesc;//>content

		LPBYTE _pBin;//>binary data szBuffer
		ULONG _uBinSize;//>the size of binary data szBuffer

		Item() 
		{
			InitItem(NULL, NULL, 0, CX_LOG_MESSAGE, NULL, NULL, 0);
		}

		Item(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, EnumType eType, 
			LPCTSTR szDesc, LPVOID pBin = NULL, ULONG uSize = 0) 
		{
			InitItem(szSrc, szFunc, uLine, eType, szDesc, pBin, uSize);
		}

		~Item()
		{
			ReleaseStringBuffer(_szSrc);	
			ReleaseStringBuffer(_szFunc);
			ReleaseStringBuffer(_szDesc);
			
			if(_pBin)
			{
				delete []_pBin;
				_pBin = NULL;
			}
		}

		VOID InitItem(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, EnumType eType, 
			LPCTSTR szDesc, LPVOID pBin, ULONG uSize)
		{
			GetLocalTime(&_Time);
			wsprintf(_szTime, _T("%04d-%02d-%02d %02d:%02d:%02d.%03d"), 
				_Time.wYear,
				_Time.wMonth,
				_Time.wDay,
				_Time.wHour,
				_Time.wMinute,
				_Time.wSecond,
				_Time.wMilliseconds
				);

			_eType = eType;
			_uBinSize = _uLine = 0;
			_szSrc = _szFunc = _szDesc = NULL;
			_pBin = NULL;

			if(szSrc)
			{
				LPCTSTR p = szSrc;

			#ifndef CX_LOG_FULL_SOURCE_NAME
				p = szSrc + _tcslen(szSrc);
				while(p>szSrc && *p!=_T('\\'))
					p--;
				
				if(*p == _T('\\'))
					p++;
			#endif

				AllocStringBuffer(_szSrc, p);
				_uLine = uLine;
			}
			
			AllocStringBuffer(_szFunc, szFunc);
			AllocStringBuffer(_szDesc, szDesc);

			if(pBin && uSize)
			{
				_pBin = new BYTE[uSize];
				assert(_pBin);
				memcpy(_pBin, pBin, uSize);
				_uBinSize = uSize;
			}
		}

		VOID AllocStringBuffer(LPTSTR& szDest, LPCTSTR& szSrc)
		{
			if(szSrc)
			{
				size_t uSize = _tcslen(szSrc) + 1;
				szDest = new TCHAR[uSize];
				assert(szDest);
				memcpy(szDest, szSrc, uSize*sizeof(TCHAR));
			}
		}

		VOID ReleaseStringBuffer(LPTSTR& lpDest)
		{
			if(lpDest)
			{
				delete []lpDest;
				lpDest = NULL;
			}
		}
		
		LPTSTR Format(LPTSTR szBuffer, ULONG uSize, ULONG* pSize = NULL)
		{
			static LPCTSTR szType[] = {_T("Message"), _T("Warning"), _T("Exception"), _T("Error")};

			if(!szBuffer)
				return szBuffer;

			int iLen;

			#if (defined(_MSC_VER) && _MSC_VER<=1310) || defined(__BORLANDC__)  || (defined(__GNUC__) && defined(_UNICODE))
				iLen = _sntprintf_s(szBuffer, uSize, 
					_TRUNCATE,
					CX_LOG_DEFAULT_FORMAT, 
					_szTime, szType[_eType], 
					_szSrc?_szSrc:_T(""), _uLine,
					_szDesc?_szDesc:_T(""));
			#else
				iLen = _sntprintf_s(szBuffer, uSize, 
					_TRUNCATE,
					CX_LOG_DEFAULT_FORMAT, 
					_szTime, szType[_eType], 
					_szSrc?_szSrc:_T(""), _szFunc?_szFunc:_T(""), _uLine,
					_szDesc?_szDesc:_T(""));
			#endif

			if(iLen>4 && !_tcsncmp(szBuffer+iLen-4, _T("\r\n"), 2))
				*(szBuffer+iLen-2) = TCHAR(NULL), iLen -= 2;
			
			if(pSize)
				*pSize = iLen;

			return szBuffer;
		}

		LPTSTR FormatBinary(LPTSTR szBuffer, ULONG uSize, ULONG* pSize = NULL,
			bool bReplace  = CX_LOG_REPLACE_CONTROL, 
			TCHAR chReplaceAscii = CX_LOG_REPLACE_ASCII)
		{
			return FormatBinary(_pBin, _uBinSize, szBuffer, uSize,  pSize, 
				bReplace, chReplaceAscii);
		}
		
		LPTSTR FormatBinary(LPVOID lpBin, ULONG uBinSize, 
			LPTSTR szBuffer, ULONG uSize,  ULONG* pSize = NULL, 
			bool bReplace  = CX_LOG_REPLACE_CONTROL, 
			TCHAR chReplaceAscii = CX_LOG_REPLACE_ASCII)
		{
			TCHAR temp[8+2+3*16+16+2+1];
			ULONG uActualSize = 0;

			if(!_pBin)
			{
				if(pSize)
					*pSize = uActualSize;
				return szBuffer;
			}

			if(!szBuffer)
			{
				uSize = ((_uBinSize>>4)+1)*(8+2+3*16+16+2)+1;
				szBuffer = new TCHAR[uSize];
				assert(szBuffer);
			}

			for(ULONG p = 0; p<_uBinSize && uActualSize<uSize-1; p += 16)
			{
				wsprintf(temp, _T("%08X: "), p); 

				int i;

				for(i = 0; i < 16; i++)
				{
					if(p+i<_uBinSize)
					{
						BYTE x = *(_pBin+p+i), t;

						t = (x&0xF0) >> 4;
						*(temp+8+2+3*i) = t>9 ?  _T('A')+t-10 : _T('0')+t;
						t = x&0x0F;
						*(temp+8+2+3*i+1) = t>9 ? _T('A')+t-10 : _T('0')+t;

						*(temp+8+2+3*i+2) = _T(' ');
					}
					else
						_tcscpy_s(temp + 8 + 2 + 3 * i, 8+2+3*16+16+2+1, _T("   "));
				}

				for(i = 0; i < 16; i++)
				{
					if(p+i<_uBinSize)
						*(temp+8+2+3*16+i) = (bReplace&&_istcntrl(TCHAR(*(_pBin+p+i)))) ? chReplaceAscii : *(_pBin+p+i);
					else
						*(temp+8+2+3*16+i) = _T(' ');
				}
				
					
				_tcscpy(temp+8+2+3*16+16, _T("\r\n"));

				ULONG uLen = uSize-uActualSize-1>8+2+3*16+16+2? 8+2+3*16+16+2 : uSize-uActualSize-1;
				memcpy(szBuffer+uActualSize, temp, uLen*sizeof(TCHAR));
				uActualSize += uLen;
			}

			if(pSize)
				*pSize = uActualSize;

			szBuffer[uActualSize] = TCHAR(NULL);

			return szBuffer;
		}
	};

	LPTSTR GetLogFileName()
	{
		TCHAR tszFileName[MAX_PATH] = {0}; 
	#if defined(CX_LOG_BY_DAY) || defined(CX_LOG_BY_WEEK) || defined(CX_LOG_BY_MONTH) || defined(CX_LOG_BY_YEAR)
		time_t now = time(NULL);
		#if defined(CX_LOG_BY_DAY)
			_tcsftime(tszFileName, MAX_PATH, _T("%Y%m%d.log"), localtime(&now));
		#elif defined(CX_LOG_BY_WEEK)
			_tcsftime(tszFileName, MAX_PATH, _T("%Y%W.log"), localtime(&now));
		#elif defined(CX_LOG_BY_MONTH)
			_tcsftime(tszFileName, MAX_PATH, _T("%Y%m.log"), localtime(&now));
		#elif defined(CX_LOG_BY_YEAR)
			_tcsftime(tszFileName, MAX_PATH, _T("%Y.log"), localtime(&now));
		#endif
			_sntprintf_s(_szFileName, MAX_PATH, _TRUNCATE, TEXT("%s\\%s"), g_szModulePath, tszFileName);
	#endif
		
		return _szFileName;
	}

public:
	CxLog(bool &alive) : _bAlive(alive)
	{
		_bAlive = true;
		TCHAR tszModulePath[MAX_PATH] = {0};
		TCHAR tszFileName[MAX_PATH] = {0};

	#ifdef CX_LOG_FILE_NAME
		ULONG uSize = _tcslen(CX_LOG_FILE_NAME) + 1;
		uSize = uSize>MAX_PATH ? MAX_PATH : uSize;
		memcpy(_szFileName, CX_LOG_FILE_NAME, uSize*sizeof(TCHAR));
	#else
		_sntprintf_s(tszModulePath, MAX_PATH, _TRUNCATE, TEXT("%sLog"), g_szLogPath);
		CreateDirectory(tszModulePath, NULL);  //>create log sub dir
		#if !defined(CX_LOG_BY_DAY) && !defined(CX_LOG_BY_WEEK) && !defined(CX_LOG_BY_MONTH) && !defined(CX_LOG_BY_YEAR)
			time_t now = time(NULL);
			_tcsftime(tszFileName, MAX_PATH, _T("%Y%m%d.log"), localtime(&now));
			_sntprintf_s(_szFileName, MAX_PATH, _TRUNCATE, TEXT("%s\\%s"), tszModulePath, tszFileName);
		#endif 
	#endif

		_TerminateEvent = CreateEvent(0, TRUE, FALSE, NULL);
		_LogHandle = CreateEvent(0, FALSE, FALSE, NULL);
		::InitializeCriticalSection(&_LogMutex);

	#ifdef _MT
		unsigned int id;
		_hThreadHandle = (HANDLE)::_beginthreadex(NULL, 0, StaticThreadProc, this, 0, &id);
	#else
		DWORD id;
		_hThreadHandle = ::CreateThread(NULL, 0, StaticThreadProc, this, 0, &id); 
	#endif	
	}

	~CxLog()
	{
		Destroy();	
	}

	VOID Destroy()
	{
		_bAlive = false;
		if(_hThreadHandle)
		{
			SetEvent(_TerminateEvent);
			if(::WaitForSingleObject(_hThreadHandle, 500) != WAIT_OBJECT_0)
				::TerminateThread(_hThreadHandle, 0);
			CloseHandle(_hThreadHandle);
		}
		::DeleteCriticalSection(&_LogMutex);
	}

	VOID Lock()
	{
		::EnterCriticalSection(&_LogMutex);
	}

	VOID Unlock()
	{
		::LeaveCriticalSection(&_LogMutex);
	}

	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, EnumType eType, 
			LPCTSTR szDesc, LPVOID pBin = NULL, ULONG uSize = 0)
	{
		Item* p = new Item(szSrc, szFunc, uLine, eType, szDesc, pBin, uSize);

	#if defined(_CONSOLE) || defined(_DEBUG)
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		p->Format(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE);

		LPTSTR pBinStr = p->FormatBinary(p->_pBin, p->_uBinSize, NULL, 0,  &uSize);

		#ifdef _CONSOLE
			if(szBuffer)
				_tprintf(szBuffer);

			if(pBinStr)
				_tprintf(pBinStr);
		#endif

		#ifdef _DEBUG
			OutputDebugString(szBuffer);
			OutputDebugString(pBinStr);
		#endif

		delete []pBinStr;
	#endif

		Lock();
		_ItemList.push_back(p);
		Unlock();
		
		SetEvent(_LogHandle);
	}

	VOID LogBin(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, EnumType eType, LPVOID pBin, ULONG uSize)
	{
		Log(szSrc, szFunc, uLine, eType, NULL, pBin, uSize);
	}

	VOID LogN(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, EnumType eType, LPCTSTR szFormat, ...) 
	{
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE] = {0};

		va_list va;
		va_start(va, szFormat);
		_vsntprintf(szBuffer, 1024-1, szFormat, va);
		va_end(va);

		Log(szSrc, szFunc, uLine, eType, szBuffer);
	}

#ifdef _INC_COMDEF
	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, _com_error &e)
	{
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE] = {0};
		_sntprintf(szBuffer, 1024, _T("_com_error Code=%08X Meaning=%s Source=%s Description=%s"), 
			e.Error(),
			(LPCSTR)_bstr_t(e.ErrorMessage()),
			(LPCSTR)e.Source(),
			(LPCSTR)e.Description());

		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	}
#endif

	VOID LogLastError(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, DWORD dwError)
	{
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE] = {0};

		int iLen = _stprintf(szBuffer, _T("(%d):"), dwError);
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			szBuffer + iLen,
			CX_LOG_DEFAULT_FORMAT_SIZE - iLen - 3,
			NULL
			);

		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_ERROR, szBuffer);
	}

#if defined(INC_VCL) && defined(__BORLANDC__)
	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const Exception *e)
	{
	#ifdef _UNICODE
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		e->Message.WideChar(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	#else
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, e->Message.c_str());
	#endif
	}

	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const Exception &e)
	{
	#ifdef _UNICODE
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		e.Message.WideChar(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	#else
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, e.Message.c_str());
	#endif
	}
#endif

#if (defined(_MFC_VER) && defined(__AFX_H__))
	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const CException *e)
	{
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		e->GetErrorMessage(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE, NULL);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	}

	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const CException &e)
	{
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		e.GetErrorMessage(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE, NULL);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	}
#endif

	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const std::exception *e)
	{
	#ifdef _UNICODE
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		MultiByteToWideChar(CP_ACP, 0, e->what(), -1, szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	#else
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, e->what());
	#endif
	}

	VOID Log(LPCTSTR szSrc, LPCTSTR szFunc, ULONG uLine, const std::exception &e)
	{
	#ifdef _UNICODE
		TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
		MultiByteToWideChar(CP_ACP, 0, e.what(), -1, szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE);
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, szBuffer);
	#else
		Log(szSrc, szFunc, uLine, CxLog::CX_LOG_EXCEPTION, e.what());
	#endif
	}

protected:
	bool& _bAlive;
	CRITICAL_SECTION _LogMutex;
	std::list<Item*> _ItemList;
	TCHAR _szFileName[MAX_PATH];
	HANDLE _hThreadHandle, _LogHandle, _TerminateEvent;

	virtual VOID Run()
	{
		HANDLE HandleArray[2];
		HandleArray[0] = _LogHandle;
		HandleArray[1] = _TerminateEvent;

		for(;;)
		{
			DWORD ret = ::WaitForMultipleObjects(2, HandleArray, false, INFINITE);
			if(ret == WAIT_OBJECT_0)
			{
				HANDLE hHandle = ::CreateFile(
					GetLogFileName(),
					GENERIC_READ | GENERIC_WRITE ,
					FILE_SHARE_READ,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL, 
					NULL
					);

				if(hHandle == INVALID_HANDLE_VALUE) {
					//LOGN_ERRN(TEXT("Open file %s failed.(%d)"), GetLogFileName(), GetLastError());
					return;
				}
				
				DWORD NumberOfBytesWritten;

				SetFilePointer(hHandle, 0, 0, FILE_END);

				try
				{
					Lock();
					while(_ItemList.size())
					{
						Item *p = _ItemList.front();

						TCHAR szBuffer[CX_LOG_DEFAULT_FORMAT_SIZE];
						ULONG uSize = 0;
						p->Format(szBuffer, CX_LOG_DEFAULT_FORMAT_SIZE, &uSize);
						WriteFile(hHandle, szBuffer, uSize*sizeof(TCHAR), &NumberOfBytesWritten, NULL);

						if(p->_pBin && p->_uBinSize)
						{
							ULONG uSize = 0;
							LPTSTR t = p->FormatBinary(p->_pBin, p->_uBinSize, NULL, 0,  &uSize);
							WriteFile(hHandle, t, uSize*sizeof(TCHAR), &NumberOfBytesWritten, NULL);
							delete []t;
						}

						_ItemList.pop_front();
						delete p;
					}
					Unlock();
				}
				catch (...) 
				{
					Unlock();
				}
								
				SetEndOfFile(hHandle);
				CloseHandle(hHandle);
			}

            if(ret == WAIT_OBJECT_0 + 1)
				break;
		}	
	}

private:
	CxLog(const CxLog&);
	CxLog& operator=(CxLog&);

#ifdef _MT
	static UINT APIENTRY StaticThreadProc(LPVOID lpPara) //允许C多线程运行库
#else
	static DWORD WINAPI StaticThreadProc(LPVOID lpPara)
#endif
	{
		((CxLog*)(lpPara))->Run();

	#ifdef _MT
		::_endthreadex(0);
	#endif
		return 0;
	}
};

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)

#define __WFILE__ WIDEN(__FILE__)
#ifdef _UNICODE
	#define __TFILE__ __WFILE__
#else
	#define __TFILE__ __FILE__
#endif

#if (_MSC_VER && _MSC_VER<=1200) || (__BORLANDC__) 
	#define __FUNCTION__ NULL
	#define __WFUNCTION__ NULL
#else //_MSC_VER>1200 __GNUC__ __INTEL_COMPILER
	#define __WFUNCTION__ WIDEN(__FUNCTION__)
#endif

#ifdef _UNICODE
	#ifdef __GNUC__
		#define __TFUNCTION__ NULL
	#else
		#define __TFUNCTION__ __WFUNCTION__
	#endif
#else
	#define __TFUNCTION__ __FUNCTION__
#endif

#define BASE_LOG(type, msg) CxLog::Instance().Log(__TFILE__, __TFUNCTION__, __LINE__, (type), (msg))
#define LOG(msg) BASE_LOG(CxLog::CX_LOG_MESSAGE, (msg))
#define LOG_WARN(msg) BASE_LOG(CxLog::CX_LOG_WARNING, (msg))
#define LOGE(e) CxLog::Instance().Log(__TFILE__, __TFUNCTION__, __LINE__, (e))
#define LOG_ERR(msg) BASE_LOG(CxLog::CX_LOG_ERROR, (msg))

#define BASE_LOG_BIN(type, bin, size) CxLog::Instance().LogBin(__TFILE__, __TFUNCTION__, __LINE__, (type), (bin), (size))
#define LOG_BIN(bin, size) BASE_LOG_BIN(CxLog::CX_LOG_MESSAGE, (bin), (size))
#define LOG_WARN_BIN(bin, size) BASE_LOG_BIN(CxLog::CX_LOG_WARNING, (bin), (size))
#define LOG_ERR_BIN(bin, size) BASE_LOG_BIN(CxLog::CX_LOG_ERROR, (bin), (size))

#if __GNUC__ || __INTEL_COMPILER || _MSC_VER>1310
	#define BASE_LOGN(type, msg, ...) CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, (type), (msg), ##__VA_ARGS__)
	#define LOGN(msg, ...) BASE_LOGN(CxLog::CX_LOG_MESSAGE, (msg), ##__VA_ARGS__)
	#define LOGN_WARNN(msg, ...) BASE_LOGN(CxLog::CX_LOG_WARNING, (msg), ##__VA_ARGS__)
	#define LOGN_ERRN(msg, ...) BASE_LOGN(CxLog::CX_LOG_ERROR, (msg), ##__VA_ARGS__)
#endif


#define PRE_LOG CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, CxLog::CX_LOG_MESSAGE,
#define LOG1(str, p1) PRE_LOG (str), (p1))
#define LOG2(str, p1, p2) PRE_LOG (str), (p1), (p2))
#define LOG3(str, p1, p2, p3) PRE_LOG (str), (p1), (p2), (p3))
#define LOG4(str, p1, p2, p3, p4) PRE_LOG (str), (p1), (p2), (p3), (p4))
#define LOG5(str, p1, p2, p3, p4, p5) PRE_LOG (str), (p1), (p2), (p3), (p4), (p5))
#define LOG6(str, p1, p2, p3, p4, p5, p6) PRE_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6))
#define LOG7(str, p1, p2, p3, p4, p5, p6, p7) PRE_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7))
#define LOG8(str, p1, p2, p3, p4, p5, p6, p7, p8) PRE_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8))
#define LOG9(str, p1, p2, p3, p4, p5, p6, p7, p8, p9) PRE_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8), (p9))

#define PRE_T_LOG CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, 
#define LOG_T1(t, str, p1) PRE_T_LOG (t), (str), (p1))
#define LOG_T2(t, str, p1, p2) PRE_T_LOG (t), (str), (p1), (p2))
#define LOG_T3(t, str, p1, p2, p3) PRE_T_LOG (t), (str), (p1), (p2), (p3))
#define LOG_T4(t, str, p1, p2, p3, p4) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4))
#define LOG_T5(t, str, p1, p2, p3, p4, p5) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4), (p5))
#define LOG_T6(t, str, p1, p2, p3, p4, p5, p6) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4), (p5), (p6))
#define LOG_T7(t, str, p1, p2, p3, p4, p5, p6, p7) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7))
#define LOG_T8(t, str, p1, p2, p3, p4, p5, p6, p7, p8) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8))
#define LOG_T9(t, str, p1, p2, p3, p4, p5, p6, p7, p8, p9) PRE_T_LOG (t), (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8), (p9))

#define PRE_WARN_LOG CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, CxLog::CX_LOG_WARNING, 
#define LOG_WARN1(str, p1) PRE_WARN_LOG (str), (p1))
#define LOG_WARN2(str, p1, p2) PRE_WARN_LOG (str), (p1), (p2))
#define LOG_WARN3(str, p1, p2, p3) PRE_WARN_LOG (str), (p1), (p2), (p3))
#define LOG_WARN4(str, p1, p2, p3, p4) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4))
#define LOG_WARN5(str, p1, p2, p3, p4, p5) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4), (p5))
#define LOG_WARN6(str, p1, p2, p3, p4, p5, p6) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6))
#define LOG_WARN7(str, p1, p2, p3, p4, p5, p6, p7) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7))
#define LOG_WARN8(str, p1, p2, p3, p4, p5, p6, p7, p8) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8))
#define LOG_WARN9(str, p1, p2, p3, p4, p5, p6, p7, p8, p9) PRE_WARN_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8), (p9))

#define PRE_EXCEPTION_LOG CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, CxLog::CX_LOG_EXCEPTION,
#define LOG_EXCEPTION1(str, p1) PRE_EXCEPTION_LOG (str), (p1))
#define LOG_EXCEPTION2(str, p1, p2) PRE_EXCEPTION_LOG (str), (p1), (p2))
#define LOG_EXCEPTION3(str, p1, p2, p3) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3))
#define LOG_EXCEPTION4(str, p1, p2, p3, p4) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4))
#define LOG_EXCEPTION5(str, p1, p2, p3, p4, p5) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4), (p5))
#define LOG_EXCEPTION6(str, p1, p2, p3, p4, p5, p6) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6))
#define LOG_EXCEPTION7(str, p1, p2, p3, p4, p5, p6, p7) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7))
#define LOG_EXCEPTION8(str, p1, p2, p3, p4, p5, p6, p7, p8) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8))
#define LOG_EXCEPTION9(str, p1, p2, p3, p4, p5, p6, p7, p8, p9) PRE_EXCEPTION_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8), (p9))

#define PRE_ERR_LOG CxLog::Instance().LogN(__TFILE__, __TFUNCTION__, __LINE__, CxLog::CX_LOG_ERROR,
#define LOG_ERR1(str, p1) PRE_ERR_LOG (str), (p1))
#define LOG_ERR2(str, p1, p2) PRE_ERR_LOG (str), (p1), (p2))
#define LOG_ERR3(str, p1, p2, p3) PRE_ERR_LOG (str), (p1), (p2), (p3))
#define LOG_ERR4(str, p1, p2, p3, p4) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4))
#define LOG_ERR5(str, p1, p2, p3, p4, p5) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4), (p5))
#define LOG_ERR6(str, p1, p2, p3, p4, p5, p6) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6))
#define LOG_ERR7(str, p1, p2, p3, p4, p5, p6, p7) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7))
#define LOG_ERR8(str, p1, p2, p3, p4, p5, p6, p7, p8) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8))
#define LOG_ERR9(str, p1, p2, p3, p4, p5, p6, p7, p8, p9) PRE_ERR_LOG (str), (p1), (p2), (p3), (p4), (p5), (p6), (p7), (p8), (p9))

#define LOG_LAST_ERROR() CxLog::Instance().LogLastError(__TFILE__, __TFUNCTION__, __LINE__, GetLastError())

int AnsiToLocal(LPTSTR pLocal, LPCSTR pAnsi, DWORD dwChars)
{
	*pLocal = 0;

#ifdef UNICODE
	MultiByteToWideChar( CP_ACP, 
		0, 
		pAnsi, 
		-1,
		pLocal, 
		dwChars); 
#else
	lstrcpyn(pAnsi, pLocal, dwChars);
#endif

	return lstrlen(pLocal);
}

#define LOGANSI(pAsic, dwSize) \
	LPTSTR pStr = new TCHAR[dwSize + 1];\
	AnsiToLocal(pStr, pAsic, dwSize + 1);\
	LOG(pStr);\
	delete[] pStr;



#endif //_CX_LOG_H_ 
