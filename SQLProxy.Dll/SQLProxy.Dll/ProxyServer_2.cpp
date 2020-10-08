
//同步执行，如果失败则回滚
#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"

static const unsigned char szBeginTrans[]=
{
	0x0E,0x01,0x00,0x22,0x00,0x00,0x01,0x00,
	0x16,0x00,0x00,0x00,0x12,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x00,0x00,0x00,0x05,0x00,
	0x02,0x00 
};
static const unsigned char szEndTrans[]=
{
	0x0E,0x01,0x00,0x22,0x00,0x00,0x01,0x00,
	0x16,0x00,0x00,0x00,0x12,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x00,0x00,0x00,0x07,0x00,
	0x00,0x00 
};
//请求取消数据包
static const unsigned char szCancelReq[]=
{
	0x06,0x01,0x00,0x08,0x00,0x00,0x00,0x00
};
//取消确认回复数据包
static const unsigned char szCancelRsp[]=
{
	0x04,0x01,0x00,0x15,0x00,0x33,0x01,0x00,
	0xFD,0x20,0x00,0xFD,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00
};
static const unsigned char szErrPacket[]=
{
	0x04,0x01,0x00,0x15,0x00,0x33,0x01,0x00,
	0xFD,0x02,0x00,0xFD,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00
};


//获取返回的TransID；事务请求回应消息格式:
//<TDS header 8字节> + <E3>+<2字节长度>+<1字节类型 08为transID>+<1字节大小>+<内容>
static unsigned __int64 GetTransIDFromReply(unsigned char *szReply)
{
	if(szReply==NULL) return 0;
	unsigned char *ptrBegin=szReply+sizeof(tds_header);
	while(*ptrBegin++==0xE3)
	{
		unsigned short usLen=*(unsigned short *)ptrBegin; ptrBegin+=2;
		if(usLen==0x0B)
		{
			if(*ptrBegin==0x08) //0x08 0x08 <后续为8字节ID> 0x00
				return *(unsigned __int64 *)(ptrBegin+2);
		}
		ptrBegin+=usLen;
	}
	return 0;
}

#define MAX_SYNCOND_TIMEOUT 10*1000	//10s，等待回复最大超时时间
//向所有服务器发送开始事务请求，并获取服务器返回的事务ID
bool CSQLClient :: BeginTransaction(bool bIncludeMainDB,unsigned char sta)
{
	unsigned int idx=0;
	QueryPacket query(m_idxRandDBSvr,0x0100007f,64);
	query.SetQueryData((unsigned char *)&szBeginTrans[0],sizeof(szBeginTrans));
	unsigned char *pquery=query.GetQueyDataPtr();
	*(pquery+1)=sta;
	
	HANDLE hMutex=m_proxysvr->m_hSynMutex;	//串行化同步执行
	try{
		if(hMutex && WaitForSingleObject(hMutex, MAX_SYNCOND_TIMEOUT)==WAIT_TIMEOUT)
			throw -2 ;
		m_lastreply.ClearData(); //清空回复
		m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //设置等待消息回复标识
		if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
				throw -3;	//向主服务器发送开始事务请求
		//if(bIncludeMainDB){ //向主服务器发送开始事务请求
		//	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //设置等待消息回复标识
		//	if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
		//		throw -3;
		//}else m_pRandDBSvr->SetUserTag(0);

		bool bSuccess=true; //任何一个需同步的负载服务器事务获取失败都直接结束客户端连接，避免出现事务嵌套
		long msTimeout=MAX_WAITREPLY_TIMEOUT; //最大超时等待时间ms
		//向所有的负载或备份服务器发送,0成功 -1超时
		int iresult=SendQueryToAll_rb(&query,msTimeout,false);
		if(iresult==0){
		for(idx=0;idx<m_vecDBConn.size();idx++)
		{//获取返回的TransID
			if(idx==m_idxRandDBSvr) continue; //主DB服务器无需数据同步
			DBConn * pconn=m_vecDBConn[idx];
			if(pconn==NULL || !pconn->IsConnected()) continue;
			ReplyPacket &reply=pconn->GetLastReply();
			pconn->m_uTransactionID =(reply.GetReplyDataSize()!=0)?GetTransIDFromReply(reply.GetReplyDataPtr()):0;
			if(pconn->m_uTransactionID!=0) continue;
			bSuccess=false; break;
		} }else bSuccess=false;
		if(bSuccess) // && bIncludeMainDB)
		{//获取主服务器的事务回应
			m_uTransactionID=(m_lastreply.GetReplyDataSize()!=0)?GetTransIDFromReply(m_lastreply.GetReplyDataPtr()):0;
			bSuccess=(m_uTransactionID!=0);
		}
		if(!bSuccess) throw iresult;
		if(bIncludeMainDB) //向主服务器返回开始事务请求的返回
			m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		 //m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
	}catch( int iresult)
	{
		if(hMutex) ReleaseMutex(hMutex);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 获取开始事务请求响应失败 (%d)！\r\n",iresult);
		m_pSocketUser->Close(true);	return false;//强制关闭客户端连接
	}
	return true;
}

//向所有服务器发送事务提交或回滚操作
bool CSQLClient :: EndTransaction(bool bIncludeMainDB,bool bCommit)
{
	unsigned int idx=0;	
	QueryPacket query(m_idxRandDBSvr,0x0100007f,64);
	query.SetQueryData((unsigned char *)&szEndTrans[0],sizeof(szEndTrans));
	unsigned char *pquery=query.GetQueyDataPtr();
	unsigned char *pcommit=pquery+sizeof(tds_header)+TDS9_QUERY_START_LEN;
	*pcommit=(bCommit)?0x07:0x08;
	
	HANDLE hMutex=m_proxysvr->m_hSynMutex;	//串行化同步执行
	try{
		m_lastreply.ClearData(); //清空回复
		unsigned __int64 *pTransID=(unsigned __int64 *)( pquery+sizeof(tds_header)+4+4+2);
		*pTransID=m_uTransactionID;
		m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG);
		if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
			throw -3; //向主服务器发送
		//if(bIncludeMainDB){ //向主服务器发送
		//	unsigned __int64 *pTransID=(unsigned __int64 *)( pquery+sizeof(tds_header)+4+4+2);
		//	*pTransID=m_uTransactionID;
		//	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG);
		//	if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
		//		throw -3;
		//}else m_pRandDBSvr->SetUserTag(0);

		long msTimeout=MAX_WAITREPLY_TIMEOUT; //最大超时等待时间ms
		//向所有的负载或备份服务器发送
		int iresult=SendQueryToAll_rb(&query,msTimeout,true);
		if(bIncludeMainDB) ////向主服务器返回结束事务的回复	
			m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		m_uTransactionID=0;
		for(idx=0;idx<m_vecDBConn.size();idx++)
			if(m_vecDBConn[idx]!=NULL) m_vecDBConn[idx]->m_uTransactionID=0;
		if(iresult!=0) throw iresult;
	}catch(int iresult)
	{
		if(hMutex) ReleaseMutex(hMutex);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 获取结束事务请求响应失败 (%d)！\r\n",iresult);
		//强制关闭客户端连接
		m_pSocketUser->Close(true); return false;
	}
	if(hMutex) ReleaseMutex(hMutex); return true;
}

//向当前所有服务器同步写入更新操作，任何一个服务器执行失败则向主服务器返回
bool CSQLClient :: SendQueryAndSynDB_rb(QueryPacket *plastquery)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //设置等待消息回复标识
	if(m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize())<=0)
	{//向主服务器发送失败，直接回应执行错误消息
		m_pSocketUser->Send((void*)szErrPacket,sizeof(szErrPacket));
		m_pRandDBSvr->SetUserTag(0); return false; 
	}
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //主DB服务器无需数据同步
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn && pconn->IsConnected()) //需要同步的肯定都是写入更新语句
			dbservers[idx].QueryCountAdd(true); //执行计数
	}
	
	long msTimeout=MAX_WAITREPLY_TIMEOUT; //最大超时等待时间ms
	int iresult=SendQueryToAll_rb(plastquery,msTimeout,false);
	if(iresult==0){ //所有负载或备份服务器执行成功，则获取主服务器的返回并转发到客户端
		m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		plastquery->m_staExec=m_lastreply.m_iresult; //主服务器执行结果 0成功 -1超时...
		plastquery->m_nAffectRows=m_lastreply.m_nAffectRows;
		return true;
	}
	if(iresult==-1){ //超时，可能事务死锁了
		plastquery->m_staExec=-1; //执行结果超时
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] 事务同步执行超时,释放连接！\r\n");
		if(m_proxysvr->m_hSynMutex) ReleaseMutex(m_proxysvr->m_hSynMutex);
		//强制关闭客户端连接
		m_pSocketUser->Close(true); return false;
	}

	//否则同步执行失败或超时，则向所有未执行完毕超时的服务器发送取消执行命令
	SendCancelToAll_rb();
	DBConn * pconn=m_vecDBConn[iresult-1];
	ReplyPacket &lastreply=pconn->GetLastReply();//向客户端回复
	m_pSocketUser->Send(lastreply.GetReplyDataPtr(),lastreply.GetReplyDataSize());
	plastquery->m_staExec=lastreply.m_iresult; //执行结果失败
	return false;
}

//向所有有效的集群服务器转发query，以便数据库保持同步; lTimeout 超时时间ms
//任何一个数据库同步执行失败就立即返回；成功返回0, -1超时;否则同步失败的数据索引序号(下标1开始)
int CSQLClient :: SendQueryToAll_rb(QueryPacket *plastquery,unsigned int msTimeout,bool bEndTrans)
{
	unsigned int idx=0;
	int iresult,iNumsWaitforReply=1; //需要同步的数据库个数，包含主服务器
	std::vector<DBConn *> vecWaitforReply; //等待执行返回的conn队列
	for(idx=0;idx<m_vecDBConn.size();idx++) vecWaitforReply.push_back(NULL);
	for(idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue;
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected()) continue;
		if(bEndTrans && pconn->m_uTransactionID==0) continue;
		pconn->GetLastReply().ClearData();
		//已经连接数据库,异步执行数据同步Query
		if( pconn->ExcuteData(m_tdsver,plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize()) )
		{	
			vecWaitforReply[idx]=pconn; iNumsWaitforReply++; 
		}//异步等待执行返回
	}
	if(iNumsWaitforReply==0) return 0; //没有需要同步的数据库
	long tStartTime=clock();
	while(true)
	{
		iresult=iNumsWaitforReply=0; //等待执行返回的连接个数
		if( m_pRandDBSvr->GetUserTag()!=0 ) iNumsWaitforReply++; //主服务器未执行完毕
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn==NULL) continue;
			iresult=pconn->GetStatus_Asyn();
			if(iresult!=DBSTATUS_PROCESS){ //已执行完毕，判断是否执行成功
				vecWaitforReply[idx]=NULL;
				if(iresult!=DBSTATUS_SUCCESS) 
				{//任一服务器同步执行失败则返回
					iresult=(idx+1);
					iNumsWaitforReply=0; break;
				}
			}else iNumsWaitforReply++; //未执行完毕，继续等待返回
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply==0) break;
		if( (clock()-tStartTime)<msTimeout){ ::Sleep(1); continue; }
		iresult=-1; break; //超时
	}//?while(true)
	return iresult;
}
void CSQLClient :: SendCancelToAll_rb()
{
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{//对于所有正在处理中的数据库发送cancel
		if(idx==m_idxRandDBSvr) continue;
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected()) continue;
		int iresult=pconn->GetStatus_Asyn();
		if(iresult==DBSTATUS_PROCESS) //正在处理中,发送cancel消息
			pconn->ExcuteData(m_tdsver,(unsigned char *)szCancelReq,sizeof(szCancelReq));
	}
	if(m_pRandDBSvr->GetUserTag()!=0)
		m_pRandDBSvr->Send((void *)szCancelReq,sizeof(szCancelReq));
}

/*
#define SETDB_EXECFLAG(dbflags,idx,fl) { \
	unsigned int ii=idx<<1; \
	(dbflags) &=~(3<<ii); \
	(dbflags) |=( (fl)<<ii ); \
}
#define GETDB_EXECFLAG(dbflags,idx,fl) { \
	unsigned int ii=idx<<1; \
	fl= ( (dbflags)&(3<<ii) )>>ii; \
}
unsigned int CSQLClient :: WaitForReplys(std::vector<DBConn *> &vecWaitforReply,unsigned int msTimeout,unsigned int *pdbIndex)
{
	unsigned int idx,dbIndex=0,numErrDB=0;
	for(idx=0;idx<vecWaitforReply.size();idx++) //初始化服务器执行标识
		if(vecWaitforReply[idx]!=NULL) SETDB_EXECFLAG(dbIndex,idx,0x10); 
	long tStartTime=clock();
	while(true)
	{
		int iNumsWaitforReply=0; //等待执行返回的连接个数
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn==NULL) continue; //无需等待
			int iresult=pconn->GetStatus_Asyn();
			if(iresult!=DBSTATUS_PROCESS)
			{ //已执行完毕，判断是否执行成功
				if(iresult==DBSTATUS_ERROR)
				{//连接无效
					SETDB_EXECFLAG(dbIndex,idx,0x11); 
					numErrDB++; //同步失败服务器计数
				}else if(iresult!=DBSTATUS_SUCCESS)
				{ //同步执行失败
					SETDB_EXECFLAG(dbIndex,idx,0x01); //标明该服务器失败
					numErrDB++; //同步失败服务器计数
				}else //标明该服务器成功
					SETDB_EXECFLAG(dbIndex,idx,0x00);
				vecWaitforReply[idx]=NULL;
			}else //未执行完毕，继续等待返回
				iNumsWaitforReply++; 	
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply==0) break;
		if( (clock()-tStartTime)<msTimeout){::Sleep(1); continue; }
		numErrDB+=iNumsWaitforReply; break; //超时
	}//?while(true)
	if(pdbIndex!=NULL) *pdbIndex=dbIndex;
	return numErrDB;
} 
*/