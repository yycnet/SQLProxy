
#pragma once 
#include <iostream>

//TDS Query语句分析打印类
class QueryPacket
{
public:
	int m_idxDBSvr;		//随机选择执行此Query的服务器索引
	unsigned long m_clntip; //访问客户端ip地址
	time_t m_atExec;	//开始执行时刻
	long m_msExec;		//执行时间ms
	unsigned long m_nAffectRows; //受影响行数或返回记录数
	int m_staExec;		//执行状态 0 成功,否则失败状态码。
	int m_staSynExec;	//同步执行状态 0 成功；否则错误码 (-1同步超时...)
						//同步执行状态仅仅针对执行同步的DB的返回状态，如果某个db标志为错误状态并不进行同步
	long m_msSynExec;	//同步时间ms
	unsigned int m_idxSynDB; //同步失败或未进行同步的数据库索引
	unsigned int m_nSynDB; //高2字节需要同步的DB，低2字节同步成功的DB
	bool m_bTransaction; //是否为事务packet
	int m_nMorePackets; //指明后续还有N个packet是一个连续的事务处理包
public:
	QueryPacket(int idxDBSvr,unsigned long ipAddr,unsigned int nAlloc=0);
	~QueryPacket();
	unsigned int GetQueryDataSize() { return m_datasize; }
	unsigned char *GetQueyDataPtr(){ return m_pquerydata; }
	void SetQueryData(unsigned char *ptrData,unsigned int nsize);
	void AddQueryData(unsigned char *ptrData,unsigned int nsize);
	bool IsNotSelectSQL(bool bParseQuery);
	std::wstring GetQuerySQL(void *pvecsql_params); //返回sql语句字符串以及参数信息(可选)
	void PrintQueryInfo(std::ostream &os);

private:
	//一条完整的Query可能由多个packet组成，最后一个packet头的status标志为非0(一般为1)
	unsigned char *m_pquerydata;	//保存一个完整的packets数据，可能包含多个packet
	unsigned int m_datasize;		//m_pquerydata指向的数据大小
	unsigned int m_nAllocated;		//m_pquerydata指向的缓冲空间大小
	bool	m_bNotSelectSQL;	//保存是否非查询语句的分析结果
};

//异步等待回复标识
#define WAITREPLY_FLAG			1	//等待执行回应，但不截获回应，回应正常发送到客户端
#define WAITREPLY_NOSEND_FLAG	2	//等待执行回应，截获回应不发送给客户端(only for同步失败数据回滚模式)
//TDS响应回复Packet
class ReplyPacket
{
public:
	unsigned short m_tdsver;	//当前使用的tds版本
	int m_iresult; //回复消息解析结果：0执行成功 ,其它 Token+bit_flag
	unsigned long m_nAffectRows; //执行成功，返回受影响的行数
public:
	ReplyPacket();
	~ReplyPacket();
	unsigned int GetReplyDataSize() { return m_datasize; }
	unsigned char *GetReplyDataPtr(){ return m_preplydata; }
	//清空接收数据
	void ClearData() { m_datasize=0; m_iresult=0; m_nAffectRows=0;}	
	void SetReplyData(unsigned char *ptrData,unsigned int nsize);
	void AddReplyData(unsigned char *ptrData,unsigned int nsize);
	
private:
	unsigned char *m_preplydata;	//保存一个完整的响应回复消息
	unsigned int m_datasize;		//m_preplydata指向的数据大小
	unsigned int m_nAllocated;		//m_pquerydata指向的缓冲空间大小
};