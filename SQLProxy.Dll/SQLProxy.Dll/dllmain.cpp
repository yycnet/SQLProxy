// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#pragma warning(disable:4996)
#include <winsock.h>
#include "DebugLog.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//初始化windows的网络环境----------------------------
		WSADATA wsaData;
		WSAStartup(0x0101, &wsaData);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		WSACleanup(); //清除网络环境
		break;
	}
	return TRUE;
}

