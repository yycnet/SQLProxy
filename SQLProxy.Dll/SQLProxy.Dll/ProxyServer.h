#ifndef _RPCSERVER_H_
#define _RPCSERVER_H_

#include <map>
#include <vector>
#include <string>

#include "SocketListener.h"
#include "Socket.h"
#include "dbclient.h"
#include "mytds.h"
#include "SQLAnalyse.h"

#define MAX_WAITREPLY_TIMEOUT 30*1000	//30s，等待回复最大超时时间
//SQL访问客户端对象
class CProxyServer;
class CSQLClient : public ISocketListener
{
public:
	bool m_bLoginDB;		 //当前客户端是否已成功登录主服务器
	unsigned short m_tdsver; //当前客户端使用的tds版本
	std::string m_tds_dbname; //当前访问的数据库 (dbserver可不设置数据库名对整个数据库实例集群)
public:
	CSQLClient(CProxyServer *proxysvr);
	~CSQLClient();
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket){ return; }
	int GetSelectedDBSvr() { return m_idxRandDBSvr; }
	void InitDBConn(unsigned char *login_packet,unsigned int nSize,tds_login *ptr_login);
	void SendQuery_mode1(QueryPacket *plastquery,bool bRollback);	 //iLoadMode!=0均衡负载模式处理接收的Query
	void SendQuery_mode0(QueryPacket *plastquery,bool bRollback);	//iLoadMode=0主从负载模式处理接收的Query
	void TransactPacket(void *pData, unsigned int nSize,bool bRollback);

	//直接转发数据到主服务器;msTimeOut!=0则等待主服务器返回;0执行成功 -1执行超时，其它Token+bit_flag
	int SendWaitRelpy(void *pData, unsigned int nSize,int msTimeOut);
	void SetMainDBSocket(CSocket *pSocketUser,CSocket *pRandDBSvr,int idxRandDBSvr,int idxLoadDBsvr);
private:
	//向当前主服务器发送写入SQL语句，如果执行成功则同步所有负载或备份服务器
	bool SendQueryAndSynDB(QueryPacket *plastquery);
	//负载或备份服务器同步执行，成功返回0，否则错误 -1超时
	int SendQueryToAll(QueryPacket *plastquery,unsigned int msTimeout);
	int  GetInValidDBsvr(); //获取无效未同步的数据库索引

	bool BeginTransaction(bool bIncludeMainDB,unsigned char sta);
	bool EndTransaction(bool bIncludeMainDB,bool bCommit);
	//向当前所有服务器同步写入更新操作，任何一个服务器执行失败则向主服务器返回
	bool SendQueryAndSynDB_rb(QueryPacket *plastquery);
	void SendCancelToAll_rb();
	int SendQueryToAll_rb(QueryPacket *plastquery,unsigned int msTimeout,bool bEndTrans);
private:
	CSocket *m_pSocketUser; //客户端的访问socket
	CSocket *m_pRandDBSvr;	//和主服务器连接的socket
	int		m_idxRandDBSvr;	//当前连接的主服务器索引
	int		m_idxLoadDBSvr;	//当前随机选择的负载服务器
	ReplyPacket m_lastreply; //保存最后一次等待的回复消息数据
	unsigned __int64 m_uTransactionID;	//当前事务的事务ID，==0则当前非事务处理
	std::deque<QueryPacket *> m_vecQueryData; //保存所有已发送等待主数据库回复的Query/RPC请求
	std::vector<QueryPacket *> m_vecTransData; //保存事务会话期间的所有Query/RPC请求
	std::vector<DBConn *> m_vecDBConn; //后台数据库访问连接对象，用于数据同步执行
	
	CProxyServer *m_proxysvr;
	//dbconn数据库连接断开通知回调
	static void dbconn_onStatus(int iEvent,CSQLClient *pclnt);
	//dbconn收到db服务器的数据Packet通知回调
	static void dbconn_onData(tds_header *ptdsh,unsigned char *pData,CSQLClient *pclnt);
};

typedef struct _TAccessList
{
	unsigned long ipAddr;	//访问ip
	unsigned long ipMask;	//网络掩码    if((clntip & ipMask)==ipAddr) 访问许可=bAcess
	bool bAcess;			//true 访问允许否则访问禁止
}TAccessAuth;
class CProxyServer : public ISocketListener
{
public: //控制参数
	int m_iLoadMode;	 //工作模式 0主从负载 1均衡负载模式 5 冗余备份模式，默认0
						//主从负载模式,查询操作随机选择负载服务器执行，写入操作执行成功会同步到其它负载服务器；
						//冗余备份或均衡负载模式，所有操作都在主服务器执行，写入操作执行成功会同步到其它负载服务器；
	int m_iSynDBTimeOut;//同步执行等待超时时间(s) <0不进行写入操作同步 =0等待模式时间 >0最多等待指定时间s
						//主从负载或冗余备份模式可不进行写入同步操作，有主服务器的SQL发布机制完成同步，其它负载服务器设置订阅
	bool m_bRollback;	//事务同步失败是否回滚，false-否
	void * m_dbservers;	//DB服务器集群对象指针(用于获取集群数据库的信息)
	SQLAnalyse *m_psqlAnalyse; //SQL语句分析打印对象(线程异步处理)
	HANDLE m_hSynMutex;	//串行化同步执行	
public:
	CProxyServer(void *dbservers);
	virtual ~CProxyServer();
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket);
	bool SocketAccepted(CSocket *pSocket);
	//设置后台绑定SQL数据库,szParam格式name=val name=val...
	//可设置设置参数 dbname=<数据库名> user=<帐号> pswd=<密码> dbhost=<ip:port,ip:port,...>
	void SetBindServer(const char *szParam);
	unsigned int GetDBNums(); //返回有效DB服务器个数
	int GetDBStatus(int idx,int proid);
	const char *GetLoadMode();
protected:
	CSocket *ConnectMainDB(int *pIdxMainDB,ISocketListener *pListener); //连接主服务器
	bool Parse_LoginPacket(CSocket *pSocket,unsigned char *pInbuffer,unsigned int nSize);
	
	unsigned int  m_nValidDBServer;	//当前有效的集群数据库个数
	int  m_idxSelectedMainDB;		//下一次连接应选择的主服务器索引
	std::vector<TAccessAuth> m_vecAcessList; //访问许可控制列表
};

#endif

