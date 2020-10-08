
#include "stdafx.h"
#include <process.h>
#include <assert.h>

#include "ServerSocket.h"


static unsigned int __stdcall ThreadProc(void *lpParameter)
{
	((CServerSocket *)lpParameter)->Run();
	return 0;
}

CServerSocket::CServerSocket()
{
	m_nPort = 0;
	m_bClosed = true;
	m_pListener = NULL;
	m_pSockets = NULL;
	m_hThread=NULL;
}

CServerSocket::~CServerSocket()
{
	Close();
}

void CServerSocket::SetListener(ISocketListener *pListener)
{
	m_pListener = pListener;
}

bool CServerSocket::Open(int nPort, int nMaxNum)
{
	struct sockaddr_in localAddr;
	int i;
	unsigned int id;

	Close();

	m_nPort = nPort;
	 //max number of concurrent clients
	if( (m_nMaxNum = nMaxNum)<=0 ) m_nMaxNum=100; 
	m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ServerSocket == INVALID_SOCKET)
		return false;

	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(m_nPort);
	localAddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(m_ServerSocket, (struct sockaddr *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
	{
		closesocket(m_ServerSocket);
		return false;
	}
	if (listen(m_ServerSocket, m_nMaxNum) == SOCKET_ERROR)
	{
		closesocket(m_ServerSocket);
		return false;
	}
	m_pSockets = new CSocket *[m_nMaxNum];
	for (i = 0; i < m_nMaxNum; i++) m_pSockets[i] =NULL;
		
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &id);  //begin thread
	m_bClosed=(m_hThread!=0 && (long)m_hThread!=-1)?false:true;
	if(m_bClosed) m_hThread=NULL;
	return (!m_bClosed);
}

void CServerSocket::Close(void)
{
	//if (m_bClosed) return;
	m_bClosed = true;
	closesocket(m_ServerSocket);

	if (m_hThread != NULL)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread); m_hThread=NULL;
	}
	
	if (m_pSockets != NULL)
	{
		for (int i = 0; i < m_nMaxNum; i++)
			delete m_pSockets[i];
		delete[] m_pSockets;
		m_pSockets = NULL;
	}
}

int CServerSocket::GetPort(void)
{
	return m_nPort;
}

void CServerSocket::Run(void)
{
	int i, nSize;
	struct sockaddr_in remoteAddr;
	SOCKET socket;

	nSize = sizeof(remoteAddr);
	while (!m_bClosed)
	{
		socket = accept(m_ServerSocket, (struct sockaddr *)&remoteAddr, &nSize);  //accept connection
		if (socket != INVALID_SOCKET)
		{
			for (i = 0; i < m_nMaxNum; i++)
			{
				if(m_pSockets[i]==NULL) m_pSockets[i]=new CSocket(SOCK_STREAM);
				if (m_pSockets[i] && !m_pSockets[i]->Active())  
				{//find an inactive CSocket
					//先绑定但并不启动工作线程，当接收成功后才启动
					m_pSockets[i]->Bind(socket, m_pListener, &remoteAddr);
					//Listener处理完SocketAccepted事件后才开始处理received数据
					bool bAccepted=(m_pListener==NULL)?false:
									m_pListener->SocketAccepted(m_pSockets[i]);
					(bAccepted)?m_pSockets[i]->Open(): //启动数据处理工作线程
								m_pSockets[i]->Close(); //不接受，关闭此socket
					break;
				}
			}
			if(i>=m_nMaxNum) closesocket(socket);
			//assert(i < m_nMaxNum);
		}
	}
}
