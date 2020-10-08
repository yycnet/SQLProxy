
#include "stdafx.h"
#include <process.h>

#include "Socket.h"


static unsigned int __stdcall ThreadProc(void *lpParameter)
{
	CSocket *pSocket;

	pSocket = (CSocket *)lpParameter;
	pSocket->Run();
	return 0;
}


//解析指定的域名,only for IPV4
unsigned long CSocket :: Host2IP(const char *host)
{
	unsigned long ipAddr=inet_addr(host);
	if(ipAddr!=INADDR_NONE) return ipAddr;
	//指定的不是一个有效的ip地址可能是一个主机域名
	struct hostent * p=gethostbyname(host);
	if(p==NULL) return INADDR_NONE;
	ipAddr=(*((struct in_addr *)p->h_addr)).s_addr;
	return ipAddr;
}

CSocket::CSocket(int nType)
{
	m_nType = nType;  //socket type
	m_Socket = INVALID_SOCKET;
	memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
	m_bActive = false;
	m_hThread = NULL;
	m_lUserTag=0;
}

CSocket::~CSocket()
{
	Close();
}

int CSocket::GetType(void)
{
	return m_nType;  //socket type
}

bool CSocket::Connect(const char *host,int port,ISocketListener *pListener)
{
	//判断是否为一个有效的IP
	unsigned long ipAddr=CSocket::Host2IP(host);
	if(ipAddr==INADDR_NONE) return false;
	SOCKET socket =::socket(AF_INET,SOCK_STREAM,0);
	if (socket == INVALID_SOCKET)
		return false;

	struct sockaddr_in localAddr;
	struct sockaddr_in &remoteAddr=m_RemoteAddr;
	memset(&localAddr, 0, sizeof(localAddr));
	memset(&remoteAddr, 0, sizeof(remoteAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port=htons(port);
	remoteAddr.sin_addr.s_addr=ipAddr;

	int sr=::connect(socket,(struct sockaddr *) &remoteAddr, sizeof(remoteAddr));
	if(sr!=0){
		closesocket(socket); return false;
	}
	//否则成功
	m_nType=SOCK_STREAM; 
	this->Open(socket,pListener,NULL);
	return true;
}

void CSocket::Bind(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr)
{
	Close();

	m_Socket = socket;  //socket
	m_pListener = pListener;  //listener
	if (pRemoteAddr != NULL)
		m_RemoteAddr = *pRemoteAddr;  //remote address

}
bool CSocket::Open() //启动工作线程
{
	if (m_Socket == INVALID_SOCKET) return false;
	if(m_bActive) return true;

	unsigned int id;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &id);  //begin thread
	m_bActive=(m_hThread!=0 && (long)m_hThread!=-1);
	if(!m_bActive) m_hThread=NULL;
	return m_bActive;
}
void CSocket::Open(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr)
{
	Close();

	m_Socket = socket;  //socket
	m_pListener = pListener;  //listener
	if (pRemoteAddr != NULL)
		m_RemoteAddr = *pRemoteAddr;  //remote address
	if (m_Socket != INVALID_SOCKET)
	{
		unsigned int id;
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &id);  //begin thread
		m_bActive=(m_hThread!=0 && (long)m_hThread!=-1);
		if(!m_bActive) m_hThread=NULL;
	}
}
//bMandatory: 强制关闭，不等待工作线程结束
void CSocket::Close(bool bMandatory)
{
	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
	
	if(!bMandatory && m_bActive) 
		WaitForSingleObject(m_hThread, INFINITE);
	if(m_hThread!=NULL)
		CloseHandle(m_hThread);
	m_hThread = NULL;
}

void CSocket::Send(void)
{
	if (m_Socket == INVALID_SOCKET)
		return;

	if (m_nType == SOCK_STREAM)
		send(m_Socket, (const char *)m_SocketStream.GetOutput(), m_SocketStream.GetOutputSize(), 0);
	else if (m_nType == SOCK_DGRAM)
		sendto(m_Socket, (const char *)m_SocketStream.GetOutput(), m_SocketStream.GetOutputSize(), 0, (struct sockaddr *)&m_RemoteAddr, sizeof(struct sockaddr));
	m_SocketStream.OutputReset();  //clear output buffer
}


char *CSocket::GetRemoteAddress(void)
{
	return inet_ntoa(m_RemoteAddr.sin_addr);
}

unsigned long CSocket::GetRemoteAddr(void)
{
	return m_RemoteAddr.sin_addr.S_un.S_addr;
}
int CSocket::GetRemotePort(void)
{
	return htons(m_RemoteAddr.sin_port);
}

IInputStream *CSocket::GetInputStream(void)
{
	return &m_SocketStream;
}

IOutputStream *CSocket::GetOutputStream(void)
{
	return &m_SocketStream;
}

void CSocket::Run(void)
{
	int nBytes,nSize = sizeof(m_RemoteAddr);
	for (;;)
	{
		unsigned int iInbuffSize=m_SocketStream.GetInBufferSize(); //可用缓冲空间大小
		unsigned int offset=m_SocketStream.GetInputSize()+m_SocketStream.GetReadPos();
		char *pInbuffer=(char *)m_SocketStream.GetInput()+offset;
		if (m_nType == SOCK_STREAM)
			nBytes = recv(m_Socket, pInbuffer, iInbuffSize, 0);
		else if (m_nType == SOCK_DGRAM)
			nBytes = recvfrom(m_Socket, pInbuffer, iInbuffSize, 0, (struct sockaddr *)&m_RemoteAddr, &nSize);
		if (nBytes > 0)
		{
			m_SocketStream.SetInputSize(nBytes);  //bytes received
			if(m_pListener != NULL){//notify listener
				if(!m_pListener->SocketReceived(this)) break; //通迅数据错误，关闭连接
			}
			m_SocketStream.SetInputSize(0);//移动剩余未处理的数据
		}else 
			break; //否则有错误发生
	}
	
	//printf("[SQLProxy] %s:%d thread end\r\n",GetRemoteAddress(),GetRemotePort());
	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
	if(m_pListener != NULL) m_pListener->SocketClosed(this);
	m_bActive = false;
}
