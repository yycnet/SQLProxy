#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "SocketListener.h"
#include "SocketStream.h"
#include <winsock.h>
#pragma comment(lib,"ws2_32") //静态连接ws2_32.dll动态库的lib库


//条件变量对象定义 Beign=================================
#define pthread_cond_t HANDLE
#define pthread_cond_init(pCond,p) \
{ \
	*(pCond)=CreateEvent(NULL,false,false,NULL); \
}
#define pthread_cond_destroy(pCond) CloseHandle(*(pCond))
#define pthread_cond_wait(pCond) WaitForSingleObject(*(pCond),INFINITE)
#define pthread_cond_timedwait(pCond,ms) WaitForSingleObject(*(pCond),ms)
#define pthread_cond_signal(pCond) SetEvent(*(pCond))
class clsCond
{
	pthread_cond_t m_cond;
	int m_status; //0处于非等待状态，否则处于等待状态
public:
	long long m_ltag;
	clsCond():m_status(0)
	{
		m_ltag=0; //激活时额外返回参数
        pthread_cond_init(&m_cond,NULL);
	}
	~clsCond()
	{
		if(m_status!=0) pthread_cond_signal(&m_cond);
        pthread_cond_destroy(&m_cond);
	}
	bool wait(DWORD mseconds=0) //返回真--被激活，否则超时
	{
		if(m_status!=0) return false;
		m_status=1;//等待激活状态
		DWORD dwRet=(mseconds==0)?pthread_cond_wait(&m_cond):	//无限等待，直到被激活
					pthread_cond_timedwait(&m_cond,mseconds);	//等待指定的毫秒
		m_status=0; 
		return (dwRet==WAIT_OBJECT_0); //WAIT_TIMEOUT 超时
	}
	void active(long long ltag)
	{
		m_ltag=ltag;
        if(m_status!=0) pthread_cond_signal(&m_cond);
	}
	//是否为等待状态
	bool status() { return (m_status!=0)?true:false;}
};
//条件变量对象定义 End=================================

class CSocket
{
public:
	//解析指定的域名 ,only for IPV4
	static unsigned long Host2IP(const char *host);
	static const char *IP2A(unsigned long ipAddr)
		{
			struct in_addr in;
			in.S_un.S_addr =ipAddr;
			return inet_ntoa(in);
		}
	void SetUserTag(long tag){ m_lUserTag=tag;}
	long GetUserTag(){ return m_lUserTag; }
private:
	long m_lUserTag;	//保存上层用户自定义私有数据
public:
	CSocket(int nType);
	virtual ~CSocket();
	int GetType(void);
	bool Connect(const char *host,int port,ISocketListener *pListener);
	void Bind(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr = NULL);
	bool Open(); //启动工作线程
	//绑定socket、ISocketListener并启动工作线程
	void Open(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr = NULL);
	//bMandatory: 强制关闭，不等待工作线程结束，慎用！！
	void Close(bool bMandatory=false);
	void Send(void); //发送输出缓冲中的数据
	int Send(void *pdata,unsigned int nsize){ //直接发送数据
		return ::send(m_Socket, (const char *)pdata,nsize, 0);
	}
	bool Active(void){ return m_bActive; }  //thread is active or not
	char *GetRemoteAddress(void);
	unsigned long GetRemoteAddr(void);
	int GetRemotePort(void);
	IInputStream *GetInputStream(void);
	IOutputStream *GetOutputStream(void);
	void Run(void);

private:
	int m_nType;
	SOCKET m_Socket;
	struct sockaddr_in m_RemoteAddr;
	ISocketListener *m_pListener;
	CSocketStream m_SocketStream;
	bool m_bActive;
	HANDLE m_hThread;
};

#endif
