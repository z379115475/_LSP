//#include "stdafx.h"
#include <windows.h>

typedef LONG (__stdcall *pInitCompression)();
typedef LONG (__stdcall *pCreateCompression)(LONG *context, LONG flags);
typedef LONG (__stdcall *pCompress)(
                            LONG context,
                            const BYTE *in,
                            LONG insize,
                            BYTE *out,
                            LONG outsize,
                            LONG *inused,
                            LONG *outused,
                            LONG compression_level);
typedef LONG (__stdcall *pDestroyCompression)(LONG context);
typedef LONG (__stdcall *pDeInitCompression)();

HMODULE hInst = LoadLibrary(TEXT("E:\\LSP\\LSP\\gzip.dll"));
pInitCompression	InitCompression	= (pInitCompression)GetProcAddress(hInst, "InitCompression");
pCreateCompression	CreateCompression	= (pCreateCompression)GetProcAddress(hInst, "CreateCompression");
pCompress		Compress		= (pCompress)GetProcAddress(hInst, "Compress");
pDestroyCompression	DestroyCompression	= (pDestroyCompression)GetProcAddress(hInst, "DestroyCompression");
pDeInitCompression	DeInitCompression	= (pDeInitCompression)GetProcAddress(hInst, "DeInitCompression");


typedef LONG (__stdcall *pInitDecompression)();
typedef LONG (__stdcall *pCreateDecompression)(LONG *context, LONG flags);
typedef LONG (__stdcall *pDecompress)(
                            LONG context,
                            const BYTE *in,
                            LONG insize,
                            BYTE *out,
                            LONG outsize,
                            LONG *inused,
                            LONG *outused);
typedef LONG (__stdcall *pDestroyDecompression)(LONG context);
typedef LONG (__stdcall *pDeInitDecompression)();
pInitDecompression	InitDecompression	= (pInitDecompression)GetProcAddress(hInst, "InitDecompression");
pCreateDecompression	CreateDecompression	= (pCreateDecompression)GetProcAddress(hInst, "CreateDecompression");
pDecompress		Decompress		= (pDecompress)GetProcAddress(hInst, "Decompress");
pDestroyDecompression	DestroyDecompression	= (pDestroyDecompression)GetProcAddress(hInst, "DestroyDecompression");
pDeInitDecompression	DeInitDecompression	= (pDeInitDecompression)GetProcAddress(hInst, "DeInitDecompression");

int ungzip(char* source, int len, char* des, int outsize)
{
	const LONG GZIP_LVL = 1;
    LONG ctx = 0;
	LONG inused = 0;
    LONG outused = 0;
	LONG lret=0;

	InitDecompression();
    CreateDecompression(&ctx, GZIP_LVL);
    if (ctx) {
		LONG linused = 0, loutused = 0;
        do{
			lret = Decompress(ctx, (BYTE*)(source + linused), len, (BYTE*)(des +loutused), outsize, &inused, &outused);
			linused += inused;
			loutused += outused;
			if (outused > 0) {								
				len = len - inused;
				outsize -= outused;
			}
			
		} while(lret==0);
        //DestroyCompression(ctx);
    }

     DeInitDecompression();

	return 0;
}