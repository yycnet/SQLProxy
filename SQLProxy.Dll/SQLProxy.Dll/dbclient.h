#pragma once 

#include <string>
#include "Socket.h"
#include "SQLPacket.h"
#include "x64_86_def.h" //INTPTR定义

enum
{
	DBSTATUS_ERROR=-2,	//未连接
	DBSTATUS_PROCESS=-1, //正在处理中
	DBSTATUS_SUCCESS=0,	//执行成功
};
//TDS登录需要的参数信息
typedef struct tds_login_param
{
//PreLogin Packet设置参数
	unsigned short tdsversion;
	unsigned char tds_ssl_login;	//暂不支持设置ssl方式
//Login Packet设置参数
	unsigned int tds_block_size;
	std::string dbname;		//数据库名以及访问帐号和密码
	std::string dbuser;
	std::string dbpswd;
}tds_login_param;

//数据库连接断开通知事件;iEvent==0已连接 ==1正常断开 ==2异常断开
typedef void (FUNC_DBSTATUS_CB)(int iEvent,INTPTR userParam);
typedef void (FUNC_DBDATA_CB)(void *ptdsh,unsigned char *pData,INTPTR userParam);
class DBConn : public ISocketListener
{
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket);
	
public:
	DBConn();
	~DBConn();
	tds_login_param &LoginParams(){ return m_params; }
	void Setdbinfo(const char *szHost,int iport,const char *szDbname,const char *szUsername,const char *szUserpwd);
	void SetEventFuncCB(FUNC_DBSTATUS_CB *pfn,INTPTR userParam){	m_pfnOnStatus=pfn;m_lUserParam=userParam; }
	void SetEventFuncCB(FUNC_DBDATA_CB *pfn) { m_pfnOnData=pfn; }
	CSocket *GetSocket(){ return m_psocket; }
	ReplyPacket &GetLastReply() { return m_lastreply; }
	bool IsConnected(){ return (m_psocket!=NULL && m_psocket->Active()); }
	void Close(); //关闭数据库连接
	bool Connect(); //连接SQL数据库服务器
	int TestConnect(); //连接测试，成功返回0否则失败
	bool Connect_tds(unsigned char *login_packet,unsigned int nSize);
	//查询执行状态；DBSTATUS_PROCESS正在执行中否则执行完毕返回执行结果 0成功其它Token+bit_flag
	int GetStatus_Asyn(){ //DBSTATUS_ERROR 连接错误 
		if(m_psocket && m_psocket->Active() )
		{
			if(m_psocket->GetUserTag()!=0) return DBSTATUS_PROCESS; //正在处理中等待回复
			//否则已经接收到回复消息，m_lastreply保存完整的回复消息数据
			return m_lastreply.m_iresult; //直接返回回复结果
		}
		return DBSTATUS_ERROR;
	}
	bool ExcuteData(unsigned short tdsv,unsigned char *ptrData,int iLen);
	//执行指定的sql并等待执行结果，返回真执行成功
	bool ExcuteSQL(const char *szsql,int lTimeout);
	//发送数据并等待执行返回结果 lTimeout:超时时间s； 0执行成功 -1执行超时，其它Token+bit_flag
	int WaitForResult(void *pdata,unsigned int nsize,int lTimeout);
	unsigned __int64 m_uTransactionID;	//当前事务的m_uTransactionID

private:
	//采用0x702以下协议发送query data。如果query data包含START_QUERY信息则要去掉
	bool ExcuteData_TDS72SUB(unsigned char *ptrData,int iLen);
	//采用0x702及以上协议发送query data。如果query data不包含START_QUERY信息则要添加上
	bool ExcuteData_TDS72PLUS(unsigned char *ptrData,int iLen);

	std::string m_hostname;
	int m_hostport;
	tds_login_param m_params;
	ReplyPacket m_lastreply; //保存最后一次等待的回复消息数据

	CSocket *m_psocket;
	void *m_dbconn;
	//数据库连接断开通知事件
	FUNC_DBSTATUS_CB *m_pfnOnStatus;
	FUNC_DBDATA_CB *m_pfnOnData;
	INTPTR m_lUserParam;
};

