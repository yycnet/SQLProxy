
#include "stdafx.h"
#include <winsock.h>
#include <stdlib.h>
#include <stdio.h>
#pragma comment(lib,"ws2_32") //静态连接ws2_32.dll动态库的lib库

#include "DebugLog.h"

#define ICMP_SIZE 32
//ICMP header structure
typedef struct _ICMP_HDR
{
	BYTE	type;
	BYTE	code;
	USHORT	checksum;
	USHORT  id;
	USHORT  seq;
}ICMP_HDR;

//ping option informatiom structure
typedef struct  _IP_OPTION_INFORMATION
{
	unsigned char ttl;
	unsigned char tos;
	unsigned char flags;
	unsigned char options_size;
	unsigned char *options_data;
}IP_OPTION_INFORMATION, *PIP_OPTION_INFORMATION;

//ping echo reply structure
typedef struct _IP_ECHO_REPLY 
{
	DWORD address;
	unsigned long status;
	unsigned long round_trip_time;
	unsigned long data_size;
	unsigned long reserved;
	void *data;
	IP_OPTION_INFORMATION options;
}IP_ECHO_REPLY, * PIP_ECHO_REPLY;

typedef HANDLE (WINAPI* picf)(void);
typedef BOOL   (WINAPI* pich)(HANDLE);
typedef DWORD  (WINAPI* pise)(HANDLE, struct in_addr, LPVOID, WORD, PIP_OPTION_INFORMATION
							  ,LPVOID, DWORD, DWORD);
//Ping指定的主机 0 成功 -1失败 >0其它错误 1 加载ICMP.dll失败
int PingHost(const char *szHost,unsigned long ipAddr)
{
	struct in_addr hostaddr;
	char ping_buf[ICMP_SIZE]= {0};
	memset(ping_buf, '\xAA', sizeof(ping_buf));
	PIP_ECHO_REPLY pIpe= (PIP_ECHO_REPLY)::malloc(sizeof(IP_ECHO_REPLY)+ICMP_SIZE+1);
	if(pIpe==NULL) ::free(pIpe);
	HANDLE hPing=NULL;
	HINSTANCE hIcmp= LoadLibrary("ICMP.DLL");
	if(hIcmp==NULL){ ::free(pIpe); return 1; }
	
	picf pIcmpCreateFile= (picf)GetProcAddress(hIcmp, "IcmpCreateFile");
	pich pIcmpCloseHandle= (pich)GetProcAddress(hIcmp, "IcmpCloseHandle");
	pise pIcmpSendEcho= (pise)GetProcAddress(hIcmp, "IcmpSendEcho");
	if (pIcmpCreateFile== NULL || pIcmpCloseHandle== NULL || pIcmpSendEcho== NULL)
	{
		::free(pIpe); ::FreeLibrary(hIcmp); return 2;
	}
	if( (hPing= pIcmpCreateFile()) == INVALID_HANDLE_VALUE)
	{
		::free(pIpe); FreeLibrary(hIcmp); return 3;
	}
	
	 hostaddr.S_un.S_addr= ipAddr;
	 pIpe->data= ping_buf;
	 
	 RW_LOG_PRINT(LOGLEVEL_ERROR,"Pinging %s with %d bytes of data:\r\n", szHost, ICMP_SIZE);
	 unsigned int index,ncount=3,timeout=1000; //ping 3次,最大延迟1000ms
	  int packets_received= 0;
	 //keep pinging until user stops or count runs out
	 for (index= 0; index< ncount; index++)
	 {
		 DWORD status= pIcmpSendEcho(hPing, hostaddr, ping_buf, sizeof(ping_buf), NULL
			 , pIpe,sizeof(IP_ECHO_REPLY)+sizeof(ping_buf), timeout);
		 if (status)
		 {
			 RW_LOG_PRINT(LOGLEVEL_ERROR,"Reply from %s: bytes=%d time=%dms TTL=%d %s\n", szHost
				 , pIpe->data_size, pIpe->round_trip_time, pIpe->options.ttl
				 ,(pIpe->data_size<=0)?"(Destination Host Unreachable)":"");	 
		 }else
			 RW_LOG_PRINT(LOGLEVEL_ERROR,0,"Request timed out.\n");

		 if (status && pIpe->data_size>0) ++packets_received;
		 if (index!= ncount-1) Sleep(1000);
	 }
	 pIcmpCloseHandle(hPing);
	 ::FreeLibrary(hIcmp);
	 ::free(pIpe); 
	 return (packets_received==0)?-1:0;
}
