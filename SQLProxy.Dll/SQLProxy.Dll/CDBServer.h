
#pragma once 

#include <string>

#define MAX_NUM_SQLSERVER 10 //最多允许集群的后端SQL服务器个数
class CDBServer //保存SQL集群服务器信息
{
	bool OpenQueryLog(const char *szLogFile); //打开语句记录文件，以便记录Query/RPC语句
public:
	CDBServer();
	~CDBServer();
	void Init(); //初始化信息
	int SetValid(int iflag){//设置/返回服务器状态
		if(iflag>=0) m_iValid=iflag;
		return m_iValid;
	}
	bool IsValid() { return (m_iValid!=1); } //服务器是否可连接访问
	void CloseQueryLog();
	void SaveQueryLog(unsigned char *ptrData,unsigned int nsize);
	void QueryCountAdd(bool bNotSelect){
		//WaitForSingleObject(m_hMutex, INFINITE);
		++m_nQueryCount;
		if(bNotSelect) ++m_nWriteCount;
		//ReleaseMutex(m_hMutex);
	}
	std::string m_svrhost;
	int m_svrport;
	std::string m_dbname;
	std::string m_username;
	std::string m_userpwd;
	
	unsigned int m_nSQLClients; //当前服务器上连接客户端数
	unsigned int m_nQueryCount; //当前服务器上执行总query次数
	unsigned int m_nWriteCount; //其中写操作次数
	unsigned int m_nDBConnNums; //后台同步执行数据库连接个数，正常应该是m_nSQLClients*(有效数据库服务器个数-1)
private:
	int m_iValid; //是否有效 0有效 1数据库连接错误 2数据库同步异常
	FILE * m_fpsave;
	HANDLE m_hMutex;
};
