#ifndef _SOCKETLISTENER_H_
#define _SOCKETLISTENER_H_

class CSocket;

class ISocketListener
{
public:
	//返回假，数据处理错误
	virtual bool SocketReceived(CSocket *pSocket) = 0;
	virtual void SocketClosed(CSocket *pSocket)=0;
	//返回成功则接受可处理，否则拒绝接受关闭socket (Only for Server)
	virtual bool SocketAccepted(CSocket *pSocket){ return true; }
};

#endif
