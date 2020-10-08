
#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"

//dbconn数据库连接断开通知回调；iEvent==0已连接 ==1正常断开 ==2异常断开
void CSQLClient :: dbconn_onStatus(int iEvent,CSQLClient *pclnt)
{
	CDBServer *dbservers=(CDBServer *)pclnt->m_proxysvr->m_dbservers;
	int idxRandDBSvr=pclnt->m_idxRandDBSvr;
	switch(iEvent)
	{
	case 0:
		dbservers[idxRandDBSvr].m_nDBConnNums++;
		break;
	case 1:
		dbservers[idxRandDBSvr].m_nDBConnNums--;
		break;
	case 2:
		dbservers[idxRandDBSvr].m_nDBConnNums--;
		//强制关闭客户端连接，避免同步错误
		if(pclnt->m_pSocketUser!=NULL) pclnt->m_pSocketUser->Close(true);
		break;
	}	
}
//dbconn收到db服务器的数据通知回调,将某个数据库的响应数据转发到客户端，转发完毕后自动关闭
void CSQLClient :: dbconn_onData(tds_header *ptdsh,unsigned char *pData,CSQLClient *pclnt)
{	
	if(ptdsh->type ==TDS_REPLY && IS_LASTPACKET(ptdsh->status) )
	{//回复Packet且为最后一个Packet
		int idxLoadDBSvr=pclnt->m_idxLoadDBSvr;
		DBConn * pconn=(idxLoadDBSvr>=0)?pclnt->m_vecDBConn[idxLoadDBSvr]:NULL;
		if(pconn) //关闭当前负载服务器的数据转发
		pconn->SetEventFuncCB((FUNC_DBDATA_CB *)NULL);

		std::deque<QueryPacket *> &vecQueryData=pclnt->m_vecQueryData;
		if(!vecQueryData.empty() )
		{//如果当前有正等待服务器响应的Query/RPC请求队列则处理，取出最早的Query/RPC请求并从等待队列中移除
			QueryPacket *plastquery=vecQueryData.front();
			vecQueryData.pop_front();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
			//plastquery->m_staExec=parse_reply(pData,pclnt->m_tdsver,&plastquery->m_nAffectRows);
			if(!pclnt->m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,pData,pclnt->m_tdsver)) delete plastquery;
		}
	}//if(tdsh.type ==TDS_REPLY && ) 
	//将dbconn连接数据库的返回信息转发到客户端
	pclnt->m_pSocketUser->Send(pData,ptdsh->size);
}

CSQLClient :: CSQLClient(CProxyServer *proxysvr)
: m_proxysvr(proxysvr)
{
	m_pSocketUser=NULL;
	m_pRandDBSvr=NULL;
	m_idxRandDBSvr=0;
	m_idxLoadDBSvr=0;
	m_bLoginDB=false;
	m_tdsver=0;
	m_uTransactionID=0;
}

CSQLClient :: ~CSQLClient()
{
	//首先关闭连接，避免m_pRandDBSvr的ISocketListener::SocketReceived一直在等待db reply，线程无法结束
	for(unsigned int i=0;i<m_vecDBConn.size();i++)
	{
		DBConn *pconn=m_vecDBConn[i];
		if(pconn!=NULL) pconn->Close();
	}

	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	if(m_pRandDBSvr!=NULL){
		dbservers[m_idxRandDBSvr].m_nSQLClients--;
		delete m_pRandDBSvr;
	}
	//释放连接对象
	for(unsigned int i=0;i<m_vecDBConn.size();i++)
	{
		DBConn *pconn=m_vecDBConn[i];
		if(pconn!=NULL) delete pconn;
	}
	m_vecDBConn.clear();

	for(unsigned int i=0;i<m_vecQueryData.size();i++)
		delete m_vecQueryData[i];
	for(unsigned int i=0;i<m_vecTransData.size();i++)
		delete m_vecTransData[i];
}

void CSQLClient :: SetMainDBSocket(CSocket *pSocketUser,CSocket *pRandDBSvr,int idxRandDBSvr,int idxLoadDBsvr)
{
	m_pSocketUser=pSocketUser;
	m_pRandDBSvr=pRandDBSvr;
	m_idxRandDBSvr=idxRandDBSvr;
	m_idxLoadDBSvr=idxLoadDBsvr;
}
//初始化数据库连接对象并连接后台集群数据库
void CSQLClient :: InitDBConn(unsigned char *login_packet,unsigned int nSize,tds_login *ptr_login)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	unsigned int i,iNumsSocketSvrs=0;//SQL集群服务计数
	//初始化数据库连接对象
	for(i=0;i<MAX_NUM_SQLSERVER;i++,iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[i];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") break;
		if(sqlsvr.IsValid()){
			DBConn *pconn=new DBConn;
			if(pconn){
				tds_login_param &loginparams=pconn->LoginParams();
				std::string dbuser=sqlsvr.m_username,dbpswd=sqlsvr.m_userpwd,dbname=sqlsvr.m_dbname;
				loginparams.tdsversion =this->m_tdsver; //采用和客户端一致的登录协议
				if(ptr_login && dbuser==""){  //如果没有指定数据登录帐号密码则采用客户端登录协议中的帐号密码
					dbuser=ptr_login->username; dbpswd=ptr_login->userpswd; }
				if(ptr_login && dbname=="") dbname=ptr_login->dbname;
				if(ptr_login) loginparams.tds_block_size=ptr_login->psz; //采用和客户端一致的block size设置
				pconn->Setdbinfo(sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport,dbname.c_str(),dbuser.c_str(),dbpswd.c_str());	
			}
			m_vecDBConn.push_back(pconn);
		}else m_vecDBConn.push_back(NULL);
	}//?for(i=0;i<MAX_NUM_SQLSERVER;i++)
	
	for(i=0;i<iNumsSocketSvrs;i++)
	{
		bool bconn=false;
		DBConn *pconn=m_vecDBConn[i];
		if(pconn==NULL || i==m_idxRandDBSvr) continue; 
		if(login_packet!=NULL && nSize!=0)
		{
			tds_login_param &params=pconn->LoginParams();
			if( ptr_login!=NULL && //首先判断当前登录帐号密码和设定的是否一致，如果不一致则重新更改登录数据包里的登录信息
				(_stricmp(params.dbuser .c_str(),ptr_login->username.c_str())!=0 || params.dbpswd!=ptr_login->userpswd) )
			{
				unsigned char NewLoginPacket[512]; 
				unsigned int NewSize=write_login_new(NewLoginPacket,512,*ptr_login,NULL,params.dbuser .c_str(),params.dbpswd.c_str());
				if(NewSize!=0) bconn=pconn->Connect_tds(NewLoginPacket,NewSize);
			}else //直接转发原始登录数据包进行登录
				bconn=pconn->Connect_tds(login_packet,nSize);
		}else  //根据设定的帐号密码信息通过dblib连接访问数据库
			bconn=pconn->Connect();
		if(bconn){
			CSQLClient::dbconn_onStatus(0,this);
			pconn->SetEventFuncCB((FUNC_DBSTATUS_CB *)CSQLClient::dbconn_onStatus,(INTPTR)this);
		}else dbservers[i].SetValid(1); //标明服务器无效，避免再次连接
	}	
}

//接收处理SQL服务器返回的数据
bool CSQLClient :: SocketReceived(CSocket *pSocket)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	//IOutputStream *pOutStream=m_pSocketUser->GetOutputStream();
	CSocketStream *pInStream= (CSocketStream *)pSocket->GetInputStream();
	bool bNewReply=true; //标记是否开始一个信息的回复packet，每个reply可能包含多个packets
	long lUserTag=0;	//如果==WAITREPLY_FLAG说明有异步等待回复的处理
	while(true) //循环处理所有已接收的数据
	{
		unsigned int nSize=pInStream->GetInputSize(); //输入未处理数据大小
		unsigned int offset=pInStream->GetReadPos();
		unsigned char *pInbuffer=(unsigned char *)pInStream->GetInput()+offset; //未处理数据起始指针
		if(nSize==0) break; //数据已处理完毕
		if(nSize<sizeof(tds_header)) break; //数据未接收完毕,继续接收

		tds_header tdsh;
		unsigned char *pinstream=pInbuffer;
		pinstream+=parse_tds_header(pinstream,tdsh);
		if(tdsh.type<0x01 || tdsh.type >0x1f){	
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 严重错误 - 无效的协议数据，t=0x%02X s=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); break;
		}
		if(tdsh.size>nSize) break; //数据未接收完毕,继续接收
		//接收到到一个完整packet，解析处理 begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d reply data from %s:%d\r\n\t   type=0x%x status=%d size=%d\r\n",
							nSize,pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),tdsh.type ,tdsh.status ,tdsh.size);
		if( tdsh.type ==TDS_REPLY)
		{//回复消息
			if( (lUserTag=pSocket->GetUserTag())!=0)
			{//有正等待回复结果的处理
				(bNewReply)?m_lastreply.SetReplyData(pInbuffer,tdsh.size):
							m_lastreply.AddReplyData(pInbuffer,tdsh.size);
				if( IS_LASTPACKET(tdsh.status) ){ //回复消息的最后一个packet
					m_lastreply.m_iresult=(int)parse_reply(pInbuffer,m_tdsver,&m_lastreply.m_nAffectRows);
					pSocket->SetUserTag(0); //已接收到回复，取消有正等待回复结果的处理标识
					bNewReply=true; //开始一个新的回复接收处理
				}else bNewReply=false; //后续还有该回复的packet待接收
				if(WAITREPLY_NOSEND_FLAG==lUserTag) //回应消息不转发
				{
					//移动数据处理指针，跳过已处理数据大小
					pInStream->Skip(tdsh.size); continue;
				}
			}//?if(pSocket->GetUserTag()!=0)
			else if( IS_LASTPACKET(tdsh.status) && !m_vecQueryData.empty())
			{//当前有正等待服务器响应的Query/RPC请求队列则处理且这是回复的最后一个packet
				//取出最早的Query/RPC请求并从等待队列中移除
				QueryPacket *plastquery=m_vecQueryData.front();// m_vecQueryData[0];
				m_vecQueryData.pop_front(); //.erase(m_vecQueryData.begin());
				plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
				//plastquery->m_staExec=parse_reply(pInbuffer,m_tdsver,&plastquery->m_nAffectRows);
				if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,pInbuffer,m_tdsver)) delete plastquery;
			}
		}//?if( tdsh.type ==TDS_REPLY)
		//接收到到一个完整packet，解析处理  end ====================================
		nSize=tdsh.size; //已处理字节
		pInStream->Skip(nSize); //移动数据处理指针，跳过已处理数据大小
		m_pSocketUser->Send(pInbuffer,nSize);
	}//?while(true) 
	return true;
}

//直接转发数据到主服务器；返回0- 成功等到回应且执行成功；否则失败 -1超时
//msTimeOut==0 不等待返回仅发送否则等待指定的ms
int CSQLClient :: SendWaitRelpy(void *pData, unsigned int nSize,int msTimeOut)
{
	if(msTimeOut==0){ //不等待返回,仅仅发送
		m_pRandDBSvr->Send(pData,nSize);
		return 0;
	}
	int iresult=-1; //超时
	DWORD dwTimeOut=(msTimeOut>0)?msTimeOut:(300*1000);
	if(pData!=NULL) m_pRandDBSvr->SetUserTag(WAITREPLY_FLAG);
	if(pData==NULL || m_pRandDBSvr->Send(pData,nSize)>0)
	{
		long tStartTime=clock();
		while(true)
		{
			if( (clock()-tStartTime)>dwTimeOut ) break; //超时
			if( m_pRandDBSvr->GetUserTag()==0 )
			{//已经接收到回复数据
				iresult=m_lastreply.m_iresult;
				break;
			}
			::Sleep(1); //延时,继续
		}
	}else iresult=DBSTATUS_ERROR; //网络连接错误
	m_pRandDBSvr->SetUserTag(0);	return iresult;
}

inline int  CSQLClient :: GetInValidDBsvr()
{
	int idxSynDB=0;
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //主DB服务器
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected())
			idxSynDB |=(0x01<<idx); //标记此数据库未进行同步
	}
	return idxSynDB;
}
void CSQLClient :: TransactPacket(void *pData, unsigned int nSize,bool bRollback)
{
	unsigned char *pinstream=(unsigned char *)pData+sizeof(tds_header);
	pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
	unsigned short usTransFlag=*(unsigned short *)pinstream;
	if(usTransFlag==0x05)
	{//开始事务处理请求
		//如果开启同步失败回滚则向所有服务器发送开始事务请求
		unsigned char sta=*((unsigned char *)pData+1);
		if(!bRollback){//直接向主服务器转发事务处理请求
			m_uTransactionID=1; //置事务处理标志，==0非事务处理
			m_pRandDBSvr->Send(pData,nSize);
		}else BeginTransaction(true,sta);
		return;
	}
	//否则事务处理结束 0x07事务提交 0x08事务回滚=======================
	int idxSynDB=-1,iTransmit=0;	//事务同步执行是否成功
	QueryPacket *plastquery=(m_vecTransData.size()!=0)?m_vecTransData[0]:NULL;
	unsigned int msExecTransTime=(plastquery)?(clock()-plastquery->m_msExec):0; //事务执行时间
	//如果是支持回滚状态，Query/RPC处理时已经对写入更新做了同步操作，因此事务提交时无需再进行数据同步处理
	if(!bRollback)
	{ //如果没有开启同步失败回滚，则在事务成功后才进行写入更新操作的同步
		m_pRandDBSvr->Send(pData,nSize);//直接向主服务器转发事务消息,尽可能快的结束事务
		if(usTransFlag==0x07 && m_proxysvr->m_iSynDBTimeOut>=0)
		{ //只有事务处理成功提交且允许同步才进行负载数据库同步执行操作
			unsigned int msExecTime=(m_proxysvr->m_iSynDBTimeOut>0)?(m_proxysvr->m_iSynDBTimeOut*1000):0;
			for(unsigned int i=0;i<m_vecTransData.size();i++)
			{
				QueryPacket *plastquery=m_vecTransData[i];
				if(plastquery!=NULL && plastquery->IsNotSelectSQL(false) )
				{//仅仅同步写入更新操作语句
					iTransmit=SendQueryToAll(plastquery,msExecTime);
					if(iTransmit!=0 && iTransmit!=-1) break; //0成功 -1超时
				}
			}
		}
	}else{ //如果开启同步失败回滚则向所有服务器发送开始事务结束消息
		EndTransaction(true,(usTransFlag==0x07));
		idxSynDB=(usTransFlag==0x07)? GetInValidDBsvr():0;
	}
	m_uTransactionID=0; //置事务处理结束标志
	//保存事务SQL操作信息到打印分析队列
	for(unsigned int i=0;i<m_vecTransData.size();i++)
	{
		plastquery=m_vecTransData[i];
		plastquery->m_bTransaction=true;
		plastquery->m_msExec=msExecTransTime; //事务执行时间
		plastquery->m_staExec =(usTransFlag==0x07)?0:usTransFlag; //事务执行状态
		plastquery->m_staSynExec =iTransmit;	//事务同步执行状态
		if(idxSynDB!=-1) plastquery->m_idxSynDB=idxSynDB;
		plastquery->m_nMorePackets=m_vecTransData.size()-i-1;
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}
	m_vecTransData.clear(); return;	
}

//均衡负载模式，所有的SQL操作都在主服务器执行。如果是写入操作则同步到其它负载服务器执行
void CSQLClient :: SendQuery_mode1(QueryPacket *plastquery,bool bRollback)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	bool bNotSelectSQL=plastquery->IsNotSelectSQL(false);
	//所有操作肯定在主服务器执行，因此主服务器计数+1
	dbservers[m_idxRandDBSvr].QueryCountAdd(bNotSelectSQL); //主服务器执行计数

	if(m_uTransactionID!=0){ //当前是事务处理过程,转发主服务器执行
		m_vecTransData.push_back(plastquery); //保存QueryPacket对象
		if(bRollback && bNotSelectSQL) //如果非查询语句且当前是同步失败回滚模式
			SendQueryAndSynDB_rb(plastquery);
		else m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}else if(bNotSelectSQL) //写入更新操作
	{
		if(bRollback && IS_TDS72_PLUS(m_tdsver))
		{
			if(BeginTransaction(false,0x01))
			{
				unsigned char *pquery=plastquery->GetQueyDataPtr();
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pquery+sizeof(tds_header)+4+4+2); 
				*pTransID=m_uTransactionID;
				*(pquery+1) &=0xF7; //取消RESETCONNECTION位(0x08)
			}
			bool bCommit=SendQueryAndSynDB_rb(plastquery);
			EndTransaction(false,bCommit);
			if(bCommit) plastquery->m_idxSynDB= GetInValidDBsvr();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
		}else //向当前主服务器发送写入SQL语句，如果执行成功则同步所有负载或备份服务器
			SendQueryAndSynDB(plastquery);
		//保存到Query data到打印分析队列，如果失败则删除释放
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}else{ //查询操作，转发数据,不等待返回
		m_vecQueryData.push_back(plastquery); //保存QueryPacket对象
		m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}
}
//如果是查询操作则随机转发到负载服务器执行否则转发主服务器执行
void CSQLClient :: SendQuery_mode0(QueryPacket *plastquery,bool bRollback)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	bool bNotSelectSQL=plastquery->IsNotSelectSQL(false);
	//事务和写入更新操作在主服务器执行，查询操作随机选择一台负载服务器执行
	if(m_uTransactionID!=0){ //当前是事务处理过程,转发主服务器执行
		dbservers[m_idxRandDBSvr].QueryCountAdd(bNotSelectSQL); //执行计数
		m_vecTransData.push_back(plastquery); //保存QueryPacket对象
		if(bRollback && bNotSelectSQL) //如果非查询语句且当前是同步失败回滚模式
			SendQueryAndSynDB_rb(plastquery);
		else m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}else if(bNotSelectSQL) //写入更新操作
	{
		dbservers[m_idxRandDBSvr].QueryCountAdd(true); //执行计数
		if(bRollback && IS_TDS72_PLUS(m_tdsver))
		{
			if(BeginTransaction(false,0x01))
			{
				unsigned char *pquery=plastquery->GetQueyDataPtr();
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pquery+sizeof(tds_header)+4+4+2); 
				*pTransID=m_uTransactionID;
				*(pquery+1) &=0xF7; //取消RESETCONNECTION位(0x08)
			}
			bool bCommit=SendQueryAndSynDB_rb(plastquery);
			EndTransaction(false,bCommit);
			if(bCommit) plastquery->m_idxSynDB= GetInValidDBsvr();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
		}else //向当前主服务器发送写入SQL语句，如果执行成功则同步所有负载或备份服务器
			SendQueryAndSynDB(plastquery);
		//保存到Query data到打印分析队列，如果失败则删除释放
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}else
	{//查询操作，随机选择一台负载服务器执行
		CSocket *psocket=m_pRandDBSvr;
		for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
		{
			m_idxLoadDBSvr=(m_idxLoadDBSvr+1) % m_vecDBConn.size();
			if(m_idxLoadDBSvr==m_idxRandDBSvr) break;
			DBConn * pconn=m_vecDBConn[m_idxLoadDBSvr];
			if(pconn==NULL || !pconn->IsConnected()) continue;
			pconn->SetEventFuncCB((FUNC_DBDATA_CB *)CSQLClient::dbconn_onData);
			psocket=pconn->GetSocket(); break;
		}
		plastquery->m_idxDBSvr =m_idxLoadDBSvr; //更改执行SQL的数据库索引
		dbservers[m_idxLoadDBSvr].QueryCountAdd(false); //执行计数
		m_vecQueryData.push_back(plastquery); //保存QueryPacket对象
		psocket->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}
}

//向当前主服务器发送写入SQL语句，如果执行成功则同步所有负载或备份服务器
bool CSQLClient :: SendQueryAndSynDB(QueryPacket *plastquery)
{
	int msTimeout=MAX_WAITREPLY_TIMEOUT; //最多等待30s
	int iresult=SendWaitRelpy(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize(),msTimeout);
	plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
	plastquery->m_staExec=iresult; //执行结果 0成功 -1超时...
	if(iresult==0) plastquery->m_nAffectRows=m_lastreply.m_nAffectRows;
	if(iresult!=0) return false; //执行失败
	if(m_proxysvr->m_iSynDBTimeOut<0) return true; //无需同步
	
	//设定同步超时时间,最多等待指定的ms
	unsigned int msExecTime=(m_proxysvr->m_iSynDBTimeOut>0)?(m_proxysvr->m_iSynDBTimeOut*1000):0;
	plastquery->m_staSynExec=SendQueryToAll(plastquery,msExecTime);	//向所有的集群服务器转发query
	return true;
}
//向所有有效的集群服务器转发query，以便数据库保持同步; lTimeout 超时时间ms
//直到所有同步执行完毕或超时才返回；成功返回0,-1超时,否则最后一个同步执行失败返回的错误码
int CSQLClient :: SendQueryToAll(QueryPacket *plastquery,unsigned int msTimeout)
{
	unsigned int idx=0,nSynDB=0,nSynDB_OK=0; //需要同步的数据库个数和同步成功的个数
	std::vector<DBConn *> vecWaitforReply; //等待执行返回的conn队列
	for(idx=0;idx<m_vecDBConn.size();idx++) vecWaitforReply.push_back(NULL);
	
	int  iSuccess =0;
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	for(idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //主DB服务器无需数据同步
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn && pconn->IsConnected())
		{//已经连接数据库,异步执行数据同步Query
			if( pconn->ExcuteData(m_tdsver,plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize()) )
			{	vecWaitforReply[idx]=pconn; nSynDB++; 
				//需要同步的肯定都是写入更新语句
				dbservers[idx].QueryCountAdd(true); //执行计数
			}//异步等待执行返回
		}else
			plastquery->m_idxSynDB |=(0x01<<idx); //标记此数据库未进行同步
	}
	if(nSynDB==0) return iSuccess; //没有需要同步的数据库
	
	//等待转发Query执行完毕，并判断是否执行成功;最小超时时间为5S
	//time_t tStartTime=GetTickCount(); 
	long tStartTime=clock();
	long ulMaxWaitforReply=(msTimeout==0)?(MAX_WAITREPLY_TIMEOUT):msTimeout;
	while(true)
	{
		int iNumsWaitforReply=0; //等待执行返回的连接个数
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn){
				int iStatus=pconn->GetStatus_Asyn();
				if(iStatus!=DBSTATUS_PROCESS){ //已执行完毕，判断是否执行成功
					if(iStatus!=DBSTATUS_SUCCESS){ //执行失败
						iSuccess=iStatus;
						plastquery->m_idxSynDB |=(0x01<<idx); //标记此数据库同步错误
						dbservers[idx].SetValid(2);
						m_vecDBConn[idx]=NULL; delete pconn; 
					}else//数据同步执行成功
						nSynDB_OK++; 
					vecWaitforReply[idx]=NULL;
				}else iNumsWaitforReply++; //未执行完毕，继续等待返回
			}
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply<=0) break;
		if( (clock()-tStartTime)<ulMaxWaitforReply){ ::Sleep(1); continue; }
		iSuccess=-1; break; //超时
	}//?while(true)
	plastquery->m_msSynExec=(int)(clock()-tStartTime);
	plastquery->m_nSynDB =nSynDB_OK+((nSynDB<<16) & 0xffff0000);
	return iSuccess;
}
