
#include "stdafx.h"
#include <process.h>
#include <time.h>
#include <sstream>		//ostringstream
#include <assert.h>
#include <regex>
#pragma warning(disable:4996)

#include "mytds.h"
#include "mytdsproto.h"
#include "CDBServer.h"
#include "Socket.h"
#include "SQLAnalyse.h"
#include "DebugLog.h"
extern //defined in DebugLog.cpp
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);

static unsigned int __stdcall ThreadProc(void *lpParameter)
{
	((SQLAnalyse *)lpParameter)->Run();
	return 0;
}

SQLAnalyse :: SQLAnalyse(void *dbservers)
{
	m_bStopped = true;
	m_hThread=NULL;
	m_dbservers=dbservers;
	m_hMutex = CreateMutex(NULL, false, NULL);
	
	m_iLogQuerys=0; 
	m_bPrintSQL=false;
	m_iSQLType=0;
	m_msExec=0;
	m_staExec=0;
	m_clntip=INADDR_NONE;
	m_pRegExpress=NULL;
}
SQLAnalyse :: ~SQLAnalyse()
{
	Stop();
	if(m_pRegExpress!=NULL) delete (std::tr1::wregex *)m_pRegExpress;
	if(m_hMutex!=NULL) CloseHandle(m_hMutex);
}

//设置SQL打印输出过滤参数
void SQLAnalyse ::SetFilter(const char *szParam)
{
	std::map<std::string,std::string> maps;
	std::map<std::string,std::string>::iterator it;
	if(SplitString(szParam,' ',maps)>0)
	{
		m_bPrintSQL=false; //先关闭打印输出
		::Sleep(300); //延迟，避免正在使用std::tr1::wregex对象
		std::tr1::wregex *pRegExpress=(std::tr1::wregex *)m_pRegExpress;
		m_pRegExpress=NULL;
		m_clntip=INADDR_NONE; //不根据ip过滤SQL输出
		if( (it=maps.find("sqltype"))!=maps.end() )
			m_iSQLType=atoi((*it).second.c_str());
		if( (it=maps.find("msexec"))!=maps.end() )
			m_msExec=atoi((*it).second.c_str());
		if( (it=maps.find("staexec"))!=maps.end() )
			m_staExec=atoi((*it).second.c_str());
		if( (it=maps.find("ipfilter"))!=maps.end() )
		{
			const char *ptr=(*it).second.c_str();
			while(*ptr==' ') ptr++;
			if(*ptr!=0) m_clntip=CSocket::Host2IP(ptr);
		}
		if( (it=maps.find("sqlfilter"))!=maps.end() )
		{
			const char *ptr=(*it).second.c_str();
			while(*ptr==' ') ptr++;
			if(*ptr!=0){
				// 表达式选项 - 忽略大小写
				std::tr1::regex_constants::syntax_option_type fl = std::tr1::regex_constants::icase;
				std::wstring wstrFilter=MultibyteToWide((*it).second);
				m_pRegExpress=(new std::tr1::wregex(wstrFilter,fl));
			}
		}
		if(pRegExpress!=NULL) delete pRegExpress;
		m_bPrintSQL=true;
	}else m_bPrintSQL=false;
}

bool SQLAnalyse ::  Start()
{
	Stop();
	
	unsigned int id;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &id);  //begin thread
	m_bStopped=(m_hThread!=0 && (long)m_hThread!=-1)?false:true;
	if(m_bStopped) m_hThread=NULL;
	return (!m_bStopped);
}

void SQLAnalyse :: Stop()
{
	m_bStopped = true;
	if (m_hThread != NULL)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread); m_hThread=NULL;
	}
	std::deque<QueryPacket *>::iterator it=m_QueryLists.begin();
	for(;it!=m_QueryLists.end();it++)
		delete (*it);
	m_QueryLists.clear();
}

bool SQLAnalyse::AddQuery(QueryPacket *pquery,void * pinstream,unsigned short tdsver)
{
	if(m_bStopped) return false;
	//if( -1==m_iLogQuerys &&	m_bPrintSQL)
	//	NULL; //如果指定记录所有sql且指定了打印则添加到分析队列
	//else 
	if(!m_bPrintSQL && pquery->m_idxSynDB==0 )
		return false; //如果不打印SQL且没有未同步的数据库
		
	if(pinstream!=NULL)
		pquery->m_staExec=parse_reply(pinstream,tdsver,&pquery->m_nAffectRows);
	WaitForSingleObject(m_hMutex, INFINITE);
	if(m_QueryLists.size()>3000){
		std::deque<QueryPacket *>::iterator it=m_QueryLists.begin();
		for(;it!=m_QueryLists.end();it++)
			delete (*it);
		m_QueryLists.clear();
	}
	m_QueryLists.push_back(pquery);
	ReleaseMutex(m_hMutex);
	return true;
}

//只有连接的主服务器执行成功的非查询语句才会进行数据库同步
//vecQuery - 主服务器执行成功的Query (包括查询语句)
void SQLAnalyse::LogQuerys(std::vector<QueryPacket *> &vecQuery,bool bSynErr)
{
	QueryPacket *pquery=NULL;
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	if(bSynErr){ //仅仅记录同步失败的Querys
		for(unsigned int idx=0;idx<vecQuery.size();idx++)
		{
			pquery=vecQuery[idx];
			if(pquery->m_idxSynDB==0) continue; //无需同步的或都同步成功Query		
			for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
			{
				CDBServer &sqlsvr=dbservers[n];
				if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") break; 
				if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
				sqlsvr.SaveQueryLog(pquery->GetQueyDataPtr(),pquery->GetQueryDataSize());
			}
		}
		return;
	}
	//记录符合条件打印输出的Query Data(调试用)
	for(unsigned int idx=0;idx<vecQuery.size();idx++)
	{
		pquery=vecQuery[idx];
		CDBServer &sqlsvr=dbservers[pquery->m_idxDBSvr];
		sqlsvr.SaveQueryLog(pquery->GetQueyDataPtr(),pquery->GetQueryDataSize());
	}
}

void SQLAnalyse::PrintQuerys(std::vector<QueryPacket *> &vecQuery,int LogLevel)
{
	QueryPacket *pquery=NULL;
	std::ostringstream outstream;
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	//-1记录符合条件打印输出的Query Data(调试用)
	if(-1==m_iLogQuerys) LogQuerys(vecQuery,false);

	if(vecQuery[0]->m_bTransaction)
	{
		pquery=vecQuery[0]; int idxDBSvr=pquery->m_idxDBSvr;
		bool bNotSelectSQL=false; //是否有需要同步的非查询语句
		RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d 开始事务处理...\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport);
		for(unsigned int i=0;i<vecQuery.size();i++){
			vecQuery[i]->PrintQueryInfo(outstream); bNotSelectSQL |=vecQuery[i]->IsNotSelectSQL(false); }
		if(pquery->m_staExec==0){
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d 事务处理结束 '提交'，耗时: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_msExec);
		}else if(pquery->m_staExec==0x08){//事务回滚 Rollback
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d 事务处理结束 '回滚'，耗时: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_msExec);
		}else{
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d 事务处理结束 '0x%02X'，耗时: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_staExec,pquery->m_msExec);
		}
		if(pquery->m_nSynDB!=0)
		{//需要同步的数据库个数和同步成功的个数
			unsigned int nSynDB=((pquery->m_nSynDB & 0xffff0000)>>16),nSynDB_OK=(pquery->m_nSynDB & 0x0000ffff); 
			if(pquery->m_staSynExec!=0){
				if(pquery->m_staSynExec==-1)
					RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行失败, 响应超时 - %d/%d\r\n",nSynDB_OK,nSynDB);
				else if(pquery->m_staSynExec==-2)
					RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行失败, 连接已断开 - %d/%d\r\n",nSynDB_OK,nSynDB);
				else{
					unsigned short tds_token=(pquery->m_staSynExec & 0x0000ffff),bit_flags=((pquery->m_staSynExec & 0xffff0000)>>16);
					RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行失败, Token=%02X %02X - %d/%d\r\n",tds_token, bit_flags,nSynDB_OK,nSynDB);
				}
			}else 
				RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行成功，耗时:%d ms - %d/%d\r\n",pquery->m_msSynExec,nSynDB_OK,nSynDB);
			if(pquery->m_idxSynDB!=0) //有同步失败的数据库，某位置1则说明该服务器同步失败
			{
				RW_LOG_PRINT(outstream,0,"[SQLProxy] 未同步或同步失败的数据库:");
				for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
				{
					CDBServer &sqlsvr=dbservers[n];
					if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="" ) break; 
					if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
					RW_LOG_PRINT(outstream," %s:%d",sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
				}
				RW_LOG_PRINT(outstream,0,"\r\n");
			}//?if(pquery->m_idxSynDB!=0) //有同步失败的数据库，某位置1则说明该服务器同步失败
		}
	}else //非事务处理
	{ 
		for(unsigned int idx=0;idx<vecQuery.size();idx++)
		{
			pquery=vecQuery[idx]; int idxDBSvr=pquery->m_idxDBSvr;
			bool bNotSelectSQL=pquery->IsNotSelectSQL(false);
			pquery->PrintQueryInfo(outstream);
			unsigned short tds_token=(pquery->m_staExec & 0x0000ffff),bit_flags=((pquery->m_staExec & 0xffff0000)>>16);
			if(pquery->m_staExec==0)
			{//写入查询成功
				if(pquery->m_nAffectRows!=(unsigned int)-1)
					RW_LOG_PRINT(outstream,"[SQLProxy] 服务器(%s:%d) %s执行成功，耗时: %d ms，受影响(返回)行数: %d\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"写入":"查询",pquery->m_msExec,pquery->m_nAffectRows);
				else
					RW_LOG_PRINT(outstream,"[SQLProxy] 服务器(%s:%d) %s执行成功，耗时: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"写入":"查询",pquery->m_msExec);
			}else if(pquery->m_staExec==-1)
				RW_LOG_PRINT(outstream,"[SQLProxy] 服务器(%s:%d) %s执行失败, 响应超时\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							 dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"写入":"查询");
			else //写入查询失败
				RW_LOG_PRINT(outstream,"[SQLProxy] 服务器(%s:%d) %s执行失败,Token=%02X %02X\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"写入":"查询",tds_token, bit_flags);
			if(pquery->m_nSynDB!=0) 
			{ //需要同步的数据库个数和同步成功的个数
				unsigned int nSynDB=((pquery->m_nSynDB & 0xffff0000)>>16),nSynDB_OK=(pquery->m_nSynDB & 0x0000ffff); 
				if(pquery->m_staSynExec!=0){
					if(pquery->m_staSynExec==-1)
						RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行失败, 响应超时 - %d/%d\r\n",nSynDB_OK,nSynDB);
					else{
						tds_token=(pquery->m_staSynExec & 0x0000ffff),bit_flags=((pquery->m_staSynExec & 0xffff0000)>>16);
						RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行失败, Token=%02X %02X - %d/%d\r\n",tds_token, bit_flags,nSynDB_OK,nSynDB);
					}
				}else 
					RW_LOG_PRINT(outstream,"[SQLProxy] 数据库同步执行成功，耗时:%d ms - %d/%d\r\n",pquery->m_msSynExec,nSynDB_OK,nSynDB);
				if(pquery->m_idxSynDB!=0) //有同步失败的数据库，某位置1则说明该服务器同步失败
				{
					RW_LOG_PRINT(outstream,0,"[SQLProxy] 未同步或同步失败的数据库:");
					for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
					{
						CDBServer &sqlsvr=dbservers[n];
						if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="" ) break; 
						if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
						RW_LOG_PRINT(outstream," %s:%d",sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
					}
					RW_LOG_PRINT(outstream,0,"\r\n");
				}//?if(pquery->m_idxSynDB!=0) //有同步失败的数据库，某位置1则说明该服务器同步失败
			}
		}//?for(idx=0;idx<vecQuery.size();idx++)
	}
	
	long outlen=outstream.tellp(); //打印解析输出的信息
	if(outlen>0) RW_LOG_PRINT((LOGLEVEL)LogLevel,outlen,outstream.str().c_str());
}

bool SQLAnalyse::MatchQuerys(std::vector<QueryPacket *> &vecQuery)
{
	if(m_clntip!=INADDR_NONE){ //指定了客户端ip
		if(vecQuery[0]->m_clntip!=m_clntip) return false;
	}
	if(m_pRegExpress==NULL) return true;
	std::tr1::wregex *pRegExpress=(std::tr1::wregex *)m_pRegExpress;
	std::tr1::wsmatch ms; //保存查找结果
	for(unsigned int idx=0;idx<vecQuery.size();idx++)
	{
		std::wstring w_strsql=vecQuery[idx]->GetQuerySQL(NULL);
		if(regex_search(w_strsql, ms, *pRegExpress))
			return true; //查找到
	}
	return false;
}

void SQLAnalyse::Run(void)
{
	QueryPacket *pquery=NULL;
	std::vector<QueryPacket *> vecQuery;
	while (!m_bStopped)
	{
		WaitForSingleObject(m_hMutex, INFINITE);
		while(!m_QueryLists.empty()){
			pquery=m_QueryLists.front();
			m_QueryLists.pop_front();
			if(pquery==NULL) break;
			vecQuery.push_back(pquery);
			if(pquery->m_nMorePackets==0) break; //要连续处理的最后一个包
		}
		ReleaseMutex(m_hMutex);
		if(vecQuery.empty()) { ::Sleep(100); continue; }
		
		pquery=vecQuery[0];
		if(pquery->m_staSynExec!=0) //同步执行失败的始终打印输出(ERROR/INFO)
			PrintQuerys(vecQuery,(pquery->m_staSynExec==-1)?LOGLEVEL_INFO:LOGLEVEL_ERROR);
		else if(m_bPrintSQL)
		{	
			bool bPrintQuery=false;
			bool bNotSelectSQL=pquery->IsNotSelectSQL(false);
			if( m_iSQLType==0) bPrintQuery=true;
			else if( (m_iSQLType & 4)!=0) //指定了输出事务
			{//输出所有事务语句和符合条件的语句
				if( pquery->m_bTransaction ||  //事务
					( (bNotSelectSQL && (m_iSQLType & 2)!=0 ) ||		//写入
					 (!bNotSelectSQL && (m_iSQLType & 1)!=0 ) 			//查询
					) ) bPrintQuery=true;
			}else{ //没有指定输出事务，仅仅输出符合条件的非事务语句
				if(!pquery->m_bTransaction &&  //非事务
				   ( (bNotSelectSQL && (m_iSQLType & 2)!=0 ) ||			//写入
					 (!bNotSelectSQL && (m_iSQLType & 1)!=0 ) 			//查询
					) ) bPrintQuery=true;
			}
			
			if(bPrintQuery)	
			{
				if(pquery->m_msExec>=m_msExec)						//执行时间
				{
					if( m_staExec==0 ||								//执行状态 全部
					   (pquery->m_staExec==0 && (m_staExec & 1)!=0) ||  //成功
					   (pquery->m_staExec!=0 && (m_staExec & 2)!=0) )   //失败
					{
//======================================打印输出 Beign================================================================
							if(MatchQuerys(vecQuery)) PrintQuerys(vecQuery,LOGLEVEL_ERROR);
//======================================打印输出  End ================================================================
					}//打印执行成功或失败的语句
				}//打印指定执行时间的SQL
			}//打印指定类型的SQL  查询，写入，事务
		}//?else if(m_bPrintSQL)
		//只有连接的主服务器执行成功的非查询语句才会进行数据库同步，对于未同步或同步错误的SQL记录
		if(0==pquery->m_staExec && 1==m_iLogQuerys) LogQuerys(vecQuery,true);
		for(unsigned int idx=0;idx<vecQuery.size();idx++) delete vecQuery[idx];
		vecQuery.clear();
	}//?while (!m_bStopped)
}

//=================ReplyPacket class==========================================
ReplyPacket::ReplyPacket()
{
	m_tdsver=0;
	m_preplydata=NULL;
	m_datasize=m_nAllocated=0;

	m_iresult=0;
	m_nAffectRows=0;
}
ReplyPacket::~ReplyPacket()
{
	if(m_preplydata!=NULL)
		delete[] m_preplydata;
}
void ReplyPacket::SetReplyData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_preplydata && m_nAllocated<nsize)
	{
		delete[] m_preplydata; m_preplydata=NULL;
	}
	if(m_preplydata==NULL){
		m_datasize=0;
		m_nAllocated=(nsize<4096)?4096:nsize;
		if( (m_preplydata=new unsigned char[m_nAllocated])==NULL ) 
			return;
	}
	::memcpy(m_preplydata,ptrData,nsize);
	m_datasize=nsize;
}
void ReplyPacket::AddReplyData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_preplydata && 
		(m_nAllocated-m_datasize)>= nsize )
	{
		::memcpy(m_preplydata+m_datasize,ptrData,nsize);
		m_datasize+=nsize; return;
	}
	
	m_nAllocated+=nsize;
	unsigned char *preplydata=new unsigned char[m_nAllocated];
	if(preplydata==NULL) return;
	if(m_preplydata!=NULL) ::memcpy(preplydata,m_preplydata,m_datasize);
	::memcpy(preplydata+m_datasize,ptrData,nsize);
	
	delete[] m_preplydata;
	m_preplydata=preplydata; m_datasize+=nsize;	
}

//=================QueryPacket class==========================================
QueryPacket::QueryPacket(int idxDBSvr,unsigned long ipAddr,unsigned int nAlloc)
: m_idxDBSvr(idxDBSvr),m_clntip(ipAddr)
{
	
	m_bNotSelectSQL=true;
	m_atExec=0;	//开始执行时刻(s)
	m_msExec=0;	//执行时间ms,默认初始化为开始时刻(ms)
	m_msSynExec=0;
	m_nAffectRows=(unsigned int)-1;
	m_staExec=0;		//执行状态 0 成功,否则失败状态码。
	m_staSynExec=0;	//同步执行状态 0 成功
	m_nSynDB=m_idxSynDB=0;
	m_bTransaction=false; //是否为事务packet
	m_nMorePackets=0; //指明后续还有N个packet是一个连续的事务处理包	
	
	if( (m_nAllocated=nAlloc)!=0) 
	{
		if( (m_pquerydata=new unsigned char[m_nAllocated])==NULL )
			m_nAllocated=0;
	}else m_pquerydata=NULL;
	m_datasize=0;
}

QueryPacket::~QueryPacket()
{
	if(m_pquerydata!=NULL)
		delete[] m_pquerydata;
}

void QueryPacket::SetQueryData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_pquerydata && m_nAllocated<nsize)
	{
		delete[] m_pquerydata; m_pquerydata=NULL;
	}
	if(m_pquerydata==NULL){
		m_datasize=0;
		//m_nAllocated=( *(ptrData+1)!=0)?nsize:8*1024; //yyc removed 2017-11-05
		//yyc modify 2017-11-05 上述判断有问题，应该根据结束包标志为判断而不是字节判断
		m_nAllocated=IS_LASTPACKET(*(ptrData+1))?nsize:8*1024;
		if(m_nAllocated<nsize) m_nAllocated=nsize;
		if( (m_pquerydata=new unsigned char[m_nAllocated])==NULL )
			return;
	}
	m_datasize=nsize;
	::memcpy(m_pquerydata,ptrData,nsize);
	m_atExec=time(NULL);	 //记录开始开始执行时刻(s)
	m_msExec=clock(); //执行时间ms,默认初始化为开始时刻(ms)
}
void QueryPacket::AddQueryData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_pquerydata && 
		(m_nAllocated-m_datasize)>= nsize )
	{
		::memcpy(m_pquerydata+m_datasize,ptrData,nsize);
		m_datasize+=nsize; return;
	}
	
	m_nAllocated+=nsize;
	unsigned char *pquerydata=new unsigned char[m_nAllocated];
	if(pquerydata==NULL) return;
	if(m_pquerydata!=NULL) ::memcpy(pquerydata,m_pquerydata,m_datasize);
	::memcpy(pquerydata+m_datasize,ptrData,nsize);
	
	delete[] m_pquerydata;
	m_pquerydata=pquerydata;
	m_datasize+=nsize; return;	
}

bool QueryPacket::IsNotSelectSQL(bool bParseQuery)
{
	//if(m_datasize==0 || m_pquerydata==NULL) return false;
	if(m_pquerydata==NULL) return false;
	if(!bParseQuery) return m_bNotSelectSQL;
	wchar_t * qry_strsql=NULL; //查询语句或存储过程名称
	
	tds_header tdsh; 
	unsigned char *pinstream=m_pquerydata;
	pinstream+=parse_tds_header(pinstream,tdsh);
	pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
	if(tdsh.type ==TDS_QUERY)
	{
		qry_strsql=(wchar_t *)pinstream; //查询语句起始
	}else if(tdsh.type ==TDS_RPC)
	{
		unsigned short qry_strsql_len=0;//查询语句或存储过程名称字节长度
		unsigned short sp_flag=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
		if(sp_flag==0xFFFF){
			//0xFFFF+INT16(调用编号)+INT16(Flags)+INT16+INT8(类型Type) + INT16(字节长度) +INT8[5](collate info TDS7.1+)+ INT16(字节长度)
			pinstream+=0x0E; //skip 14字节
			qry_strsql_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
			qry_strsql=(wchar_t *)pinstream; //查询语句起始
		}else
		{	//sp_flag为存储过程名称UCS长度，非字节长度
			qry_strsql_len=sp_flag*sizeof(short); //存储过程名称字节长度
			qry_strsql=(wchar_t *)pinstream; //存储过程名称起始
		}
	}
	if(qry_strsql==NULL) return false;
	while(*qry_strsql==L' ') qry_strsql++; //去掉前导空格
	return m_bNotSelectSQL=(_wcsnicmp(qry_strsql,L"select ",7)!=0);
}
//返回sql语句字符串(unicode编码)
static void  GetSqlParamDef(void *ptr_param,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params);
static void GetSqlParamVal(void *ptr_param_1,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params);
std::wstring QueryPacket::GetQuerySQL(void *pvecsql_params)
{
	if(m_datasize==0 || m_pquerydata==NULL) 
		return std::wstring();
	wstring w_strsql; //保存sql语句(unicode编码)

	unsigned int nCount=0;
	unsigned char *ptrData=m_pquerydata;
	while(nCount<m_datasize)
	{
		tds_header tdsh; 
		unsigned char *pinstream=ptrData;
		pinstream+=parse_tds_header(pinstream,tdsh);
		pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
		if(tdsh.type ==TDS_QUERY)
		{
			wchar_t * qry_strsql=(wchar_t *)pinstream; //查询语句起始
			//查询语句长度(字节长度)
			unsigned int qry_strsql_len=tdsh.size-(pinstream-ptrData);
			w_strsql.append(qry_strsql,(qry_strsql_len>>1) );
		}else if(tdsh.type ==TDS_RPC)
		{
			wchar_t * qry_strsql=NULL;	//查询语句或存储过程名称
			unsigned short qry_strsql_len=0;//查询语句或存储过程名称字节长度
			unsigned short qry_param_def_len=0; //参数类型定义字符串字节长度
			//参数描述格式为 @参数名 类型(长度),@参数名 类型(长度),...譬如 @a1 char(10),@a2 varchar(20), @a3 int...
			unsigned char * qry_param_def=NULL;	//unicode 字符串
			unsigned short qry_param_val_len=0; //参数值定义部分字节长度
			//格式:INT8(参数名长度) + 参数名(UCS) + INT8(Flags) + INT8(参数类型Type) + INT8(定义字节长度) + INT8(option)+....
			unsigned char * qry_param_val=NULL;

			unsigned short sp_flag=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
			if(sp_flag==0xFFFF){
				//0xFFFF+INT16(调用编号)+INT16(Flags)+INT16+INT8(类型Type) + INT16(字节长度) +INT8[5](collate info TDS7.1+)+ INT16(字节长度)
				pinstream+=0x0E; //skip 14字节
				qry_strsql_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
				qry_strsql=(wchar_t *)pinstream; //查询语句起始
				//跳过sql语句部分和空结束符'\0'
				pinstream+=(qry_strsql_len+0x02);
				//参数定义描述信息0x0A字节; INT8(类型Type) + INT16(字节长度) +INT8[5](collate info TDS7.1+)+ INT16(字节长度)
				pinstream+=0x08; //skip
				qry_param_def_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
				qry_param_def=pinstream;
				pinstream+=qry_param_def_len; //skip 参数类型定义部分
				//后续为参数的值信息
				qry_param_val=pinstream;
				qry_param_val_len=tdsh.size-(pinstream-ptrData);
			}else
			{	//sp_flag为存储过程名称UCS长度，非字节长度
				qry_strsql_len=sp_flag*sizeof(short); //存储过程名称字节长度
				qry_strsql=(wchar_t *)pinstream; //存储过程名称起始
				//跳过sql语句部分和空结束符'\0'
				pinstream+=(qry_strsql_len+0x02);
				//后续为参数的值信息
				qry_param_val=pinstream;
				qry_param_val_len=tdsh.size-(pinstream-ptrData);
			}
			assert(qry_strsql_len<0x7fff);
			w_strsql.append(qry_strsql,(qry_strsql_len>>1) );
			if(pvecsql_params!=NULL)
			{//保存传参sql语句的参数信息
				std::vector<tds_sql_param *> &vecsql_params=*(std::vector<tds_sql_param *> *)pvecsql_params;
				if(qry_param_def!=NULL)
					GetSqlParamDef(qry_param_def,qry_param_def_len,vecsql_params);
				if(qry_param_val!=NULL)
					GetSqlParamVal(qry_param_val,qry_param_val_len,vecsql_params);
			}//?if(pvecsql_params!=NULL)
		}

		nCount+=tdsh.size;
		ptrData+=tdsh.size;
	}//?while(nCount<m_datasize)
	return w_strsql;
}

void QueryPacket::PrintQueryInfo(std::ostream &os)
{
	if(m_datasize==0 || m_pquerydata==NULL)
		return;
	//保存传参sql语句的参数信息
	std::vector<tds_sql_param *> vecsql_params; 
	//保存sql语句(unicode编码)
	wstring w_strsql=GetQuerySQL(&vecsql_params); 
	if(w_strsql[0]==0) return;
	
	struct tm * ltime=localtime(&m_atExec);
	char szDateTime[64]; //打印sql语句执行时间
	sprintf(szDateTime,"[%04d-%02d-%02d %02d:%02d:%02d] from %s %s\r\n",
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour,
		ltime->tm_min, ltime->tm_sec,CSocket::IP2A(m_clntip),
		IsNotSelectSQL(false)?"*":"");
	os<<szDateTime;
	printUnicodeStr(w_strsql.c_str(),w_strsql.length(),os,true);
	
	std::vector<tds_sql_param *>::iterator it=vecsql_params.begin();
	//打印参数 @参数名 参数类型(长度) 值\r\n
	for(;it!=vecsql_params.end();it++)
	{
		tds_sql_param *p=*it;
		if(p!=NULL) print_sqlparam(*p,os);
	}
}

//参数描述格式为 @参数名 类型(长度),@参数名 类型(长度),...譬如 @a1 char(10),@a2 varchar(20),...
//参数名定义为unicode字符串，param_len长度为字节长度
static void  GetSqlParamDef(void *ptr_param,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params)
{
	unsigned int nCount=0;
	wchar_t *param=(wchar_t *)ptr_param;
	wchar_t *ptr_next=param;
	while(true)
	{
		if(*ptr_next==L',' || 
			nCount>=param_len )
		{
			tds_sql_param *p=new tds_sql_param;
			if(p==NULL) break; //内存分配失败
			unsigned int nSize=(ptr_next-param);
			if(parse_sqlparam_def(param,nSize,*p)==0){ delete p; break;}
			vecsql_params.push_back(p);
			
			if(nCount>=param_len) break;
			param=ptr_next+1;
		}
		ptr_next++; nCount+=sizeof(wchar_t);
	}
}

//格式: 1字节 参数名长度+参数名+9字节(未知含义)+2字节值长度+值
//param_len长度为字节长度
static void GetSqlParamVal(void *ptr_param_1,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params)
{
	unsigned int nCount=0;
	unsigned char *ptr_param=(unsigned char *)ptr_param_1;
	while(nCount<param_len)
	{
		tds_sql_param *p1=new tds_sql_param;
		if(p1==NULL) return;
		unsigned int nSkip=parse_sqlparam_val(ptr_param,*p1);
		if(nSkip==0){ delete p1; break; }
		nCount+=nSkip; ptr_param+=nSkip;

		std::vector<tds_sql_param *>::iterator it=vecsql_params.begin();
		for(;it!=vecsql_params.end();it++)
		{
			tds_sql_param *p0=*it;
			if(p0 && p0->name==p1->name)
			{
				p1->type_name=p0->type_name;
				*it=p1; delete p0;
				break;
			}
		}
		if(it==vecsql_params.end())
			vecsql_params.push_back(p1);
	}//?while(nCount<param_len)
}
