#include "stdafx.h"
#pragma warning(disable:4996)

#include "export.h"
#include "dbclient.h"
#include "DebugLog.h"

//数据库连通性测试,返回0 成功 1 无效的IP或域名 2、主机不可达 3 服务不可用 4帐号密码错误 5数据库名错误
extern int PingHost(const char *szHost,unsigned long ipAddr);
extern "C"
SQLPROXY_API int _stdcall DBTest(const char  *szHost,int iPort,const char *szUser,const char *szPswd,const char *dbname)
{
	if(iPort<=0) iPort=1433;
	RW_LOG_PRINT(LOGLEVEL_ERROR,"正测试数据库 %s:%d ，请等待...\r\n",szHost,iPort);
	//1、IP或主机名有效性检测
	if(szHost==NULL || szHost[0]==0 || CSocket::Host2IP(szHost)==INADDR_NONE)
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d 异常，无效的IP或主机名\r\n\r\n",szHost,iPort);
		return 1;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"主机名检查：有效的IP或主机名\r\n");
	//2、主机有效性ping 检查，判断主机是否可到达(有些主机关闭了Ping，因此Ping不通不能算有问题)
	if(PingHost(szHost,CSocket::Host2IP(szHost))==-1)
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d 异常，主机不可达或ping被阻止\r\n\r\n",szHost,iPort);
		return 2;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"Ping检查：主机可访问\r\n");
	//3、尝试连接指定的服务和端口
	CSocket sock(SOCK_STREAM);
	if( !sock.Connect(szHost,iPort,NULL))
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d 异常，服务未启动或端口错误\r\n\r\n",szHost,iPort);
		return 3;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"服务检查：数据库服务已启动\r\n");
	sock.Close();
	
	//4、采用TDS协议 尝试登录数据库
	//Socket close时会调用WaitForSingleObject等待socket的工作线程结束；
	//而socket的工作线程在结束前会调用ISocketListener对象的SocketClosed方法
	//SocketClosed中会向当前UI线程的窗体通过SendMessage输出关闭信息，SendMessage是一个阻塞式调用
	//而UI线程此时正被WaitForSingleObject阻塞，导致2个线程死锁程序无法响应;本线程打印输出并不会死锁阻塞。
	//因此需要关闭信息调试输出(仅仅对向窗体输出信息时才会导致此现象)
	LOGLEVEL ll=RW_LOG_SETLOGLEVEL(LOGLEVEL_NONE);
	DBConn dbconn;
	dbconn.Setdbinfo(szHost,iPort,dbname,szUser,szPswd);
	int iresult=dbconn.TestConnect();
	dbconn.Close();
	RW_LOG_SETLOGLEVEL(ll); //恢复
	if(iresult==1)
	{ 
		RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d 异常，请检查帐号密码\r\n\r\n",szHost,iPort);
		return 4; 
    }
	RW_LOG_PRINT(LOGLEVEL_ERROR,0,"帐号检查：数据库帐号密码有效\r\n");
	if(iresult==2){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d 异常，请检查数据库名\r\n\r\n",szHost,iPort);
		return 5;
	}
	RW_LOG_PRINT(LOGLEVEL_ERROR,"数据库 %s:%d ***正常***\r\n\r\n",szHost,iPort); 
	return 0;
}
