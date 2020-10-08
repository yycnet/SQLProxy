#pragma once 

#include <map>
#include <deque>
#include <vector>
#include "SQLPacket.h"

class SQLAnalyse
{
public:
	int m_iLogQuerys;	//是否记录错误恢复日志 0不记录 1记录 -1记录打印输出的Query Data(调试用)
public:
	SQLAnalyse(void *dbservers);
	~SQLAnalyse();
	int Size() const { return m_QueryLists.size(); }
	void SetFilter(const char *szParam);
	bool Active(void){ return !m_bStopped; }  //thread is active or not
	bool Start();
	void Stop();
	bool AddQuery(QueryPacket *pquery,void * pinstream,unsigned short tdsver);
	
	void Run(void);
private:
	bool m_bStopped;
	HANDLE m_hThread;
	HANDLE m_hMutex;
	std::deque<QueryPacket *> m_QueryLists; //要分析打印的查询列表
	void * m_dbservers;	//DB服务器集群对象指针(用于获取集群数据库的信息)

private: //SQL打印输出过滤参数
	bool m_bPrintSQL;
	int m_iSQLType; //1查询 2写入 4 事务
	int m_msExec; //仅仅打印输出大于等于指定执行时间的的SQL语句(ms),默认0
	int m_staExec; //仅仅打印输出指定执行状态的语句 1成功 2失败 
	unsigned long m_clntip; //仅仅输出指定客户端SQL语句
	void * m_pRegExpress; //正则表达式对象
	bool MatchQuerys(std::vector<QueryPacket *> &vecQuery);
	void PrintQuerys(std::vector<QueryPacket *> &vecQuery,int LogLevel);
	void LogQuerys(std::vector<QueryPacket *> &vecQuery,bool bSynErr);
};
