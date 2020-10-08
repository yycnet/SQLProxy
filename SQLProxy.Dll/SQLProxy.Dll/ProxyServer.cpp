
#include "stdafx.h"
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"
extern //defined in DebugLog.cpp
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);
static const char *szLoadBalanceMode[]={"主从负载","均衡负载","","","","容灾备份",""};
CProxyServer :: CProxyServer(void *dbservers)
{
	m_iLoadMode=0;
	m_idxSelectedMainDB=0;
	m_iSynDBTimeOut=0;
	m_bRollback=false;
	m_dbservers=dbservers;
	m_hSynMutex=NULL;
	m_nValidDBServer=0;
}

CProxyServer :: ~CProxyServer()
{	
	if(m_hSynMutex!=NULL) CloseHandle(m_hSynMutex);
}

const char * CProxyServer ::GetLoadMode()
{
	return szLoadBalanceMode[m_iLoadMode];
}
//返回有效DB服务器个数
unsigned int CProxyServer ::GetDBNums()
{
	unsigned int iNumsSocketSvrs=0; //SQL集群服务计数
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	for(;iNumsSocketSvrs<MAX_NUM_SQLSERVER;iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[iNumsSocketSvrs];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") 
			break; 
	}
	return iNumsSocketSvrs;
}

int CProxyServer ::GetDBStatus(int idx,int proid)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	if(idx==-1) return GetDBNums(); //返回有效SQL集群服务个数
	if(idx<0 || idx>=MAX_NUM_SQLSERVER)
		return 0;
	
	if(proid==0) //返回SQL服务是否正常
		return dbservers[idx].SetValid(-1);
	if(proid==1){ //返回SQL服务当前并发客户连接数
		if(m_iLoadMode==0 && dbservers[idx].IsValid() ) //主从负载模式,从服务器的连接始终未0
			return dbservers[m_idxSelectedMainDB].m_nSQLClients;
		return dbservers[idx].m_nSQLClients;
	}
	if(proid==2){ //返回后台同步执行数据库连接个数，正常应该是m_nSQLClients*(有效数据库服务器个数-1)
		if(m_iLoadMode==0 && dbservers[idx].IsValid() ) //主从负载模式
			return dbservers[m_idxSelectedMainDB].m_nDBConnNums;
		return dbservers[idx].m_nDBConnNums;
	}
	if(proid==3) //返回SQL服务当前负载处理量
		return dbservers[idx].m_nQueryCount;
	if(proid==4) //其中写操作次数
		return dbservers[idx].m_nWriteCount;
	return 0;
}
static std::string splitstring(const char *szString,char c,int idx)
{
	if(szString==NULL) return std::string();
	int nCount=0;
	const char *ptrBegin=NULL,*ptrEnd=NULL;
	while(true){
		ptrEnd=strchr(szString,c);
		if(nCount==idx){ 
			ptrBegin=szString; break; 
		}else if(nCount>idx) break;
		else nCount++;
		if(ptrEnd==NULL) break;
		szString=ptrEnd+1; //继续
	}
	if(ptrBegin==NULL) return std::string();
	return (ptrEnd==NULL)?std::string(ptrBegin):std::string(ptrBegin,(ptrEnd-ptrBegin));
}
//设置后台绑定SQL数据库,szParam格式name=val name=val...
//可设置设置参数 mode=<rand|order> dbname=<数据库名> user=<帐号> pswd=<密码> dbhost=<ip:port,ip:port,...>
void CProxyServer ::SetBindServer(const char *szParam)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	std::map<std::string,std::string> maps;
	std::map<std::string,std::string>::iterator it;
	if(SplitString(szParam,' ',maps)>0)
	{
		if((it=maps.find("rollback"))!=maps.end() )
		{
			m_bRollback=( (*it).second=="true" );
		}
		if((it=maps.find("waitreply"))!=maps.end() )
		{
			m_iSynDBTimeOut=atoi((*it).second.c_str());
		}
		//设置DB服务选择模式 rand：随机 order：顺序轮流
		if( (it=maps.find("mode"))!=maps.end() )
		{
			if((*it).second=="rand" || (*it).second=="order")
				m_iLoadMode=1; //均衡负载模式
			else if((*it).second=="master")
				m_iLoadMode=0;			//主从负载模式
			else if((*it).second=="backup")
				m_iLoadMode=5;			//容灾备份模式
			m_idxSelectedMainDB=0; //指定默认的主服务器索引
		}
		
		if( (it=maps.find("dbhost"))!=maps.end())
		{
			int nCount=0; //有效DB集群服务器计数
			const char *szdbsvr=(*it).second.c_str();
			while(true){
				while(*szdbsvr==' ') szdbsvr++; //去掉前导空格
				const char *ptr_e=strchr(szdbsvr,',');
				if(ptr_e) *(char *)ptr_e=0;

				dbservers[nCount].Init();
				dbservers[nCount].m_svrhost=splitstring(szdbsvr,':',0);
				std::string strtmp=splitstring(szdbsvr,':',1);
				if(strtmp!=""){
					dbservers[nCount].m_svrport=atoi(strtmp.c_str());
					dbservers[nCount].m_username=splitstring(szdbsvr,':',2);
					dbservers[nCount].m_userpwd=splitstring(szdbsvr,':',3);
				}else dbservers[nCount].m_svrport=1433;

				if(dbservers[nCount].m_svrport>0 && dbservers[nCount].m_svrhost!="")
					++nCount;
				if(ptr_e==NULL) break;
				*(char *)ptr_e=','; szdbsvr=ptr_e+1;
			}//?while(true)
			dbservers[nCount].m_svrhost="";
			dbservers[nCount].m_svrport=0;
		}//?if( (it=maps.find("dbhost"))!=maps.end()
		if( (it=maps.find("dbname"))!=maps.end() && (*it).second!="" )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_dbname =(*it).second;
			}
		}
		if( (it=maps.find("dbuser"))!=maps.end() && (*it).second!="" )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_username =(*it).second;
			}
		}
		if( (it=maps.find("dbpswd"))!=maps.end() )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_userpwd =(*it).second;
			}
		}
		if( (it=maps.find("logquerys"))!=maps.end() && 
			( (*it).second=="true" || (*it).second=="all") )
		{
			if((*it).second=="all") //记录符合条件打印输出的Query Data(调试用)
				m_psqlAnalyse->m_iLogQuerys=-1; 
			else if((*it).second=="true") 
				m_psqlAnalyse->m_iLogQuerys=1; //记录错误恢复Query Data
			else m_psqlAnalyse->m_iLogQuerys=0;
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] %s数据库未同步记录日志\r\n", (m_psqlAnalyse->m_iLogQuerys==0)?"关闭":"开启");
		}
		if((it=maps.find("accesslist"))!=maps.end() )
		{//格式 <true|false|1:0>|ip,ip,ip....
			bool bAccess_default=false; //默认禁止
			const char *ptrB=strchr((*it).second.c_str(),'|');
			if(ptrB!=NULL){
				*(char *)ptrB=0;
				if(_stricmp((*it).second.c_str(),"false")==0 || _stricmp((*it).second.c_str(),"0")==0 )
					bAccess_default=true; //禁止列表中的ip访问，默认其它允许
				*(char *)ptrB=':'; ptrB+=1; //跳过:
			}else ptrB=(*it).second.c_str();
			m_vecAcessList.clear();
			for(int idx=0;;idx++){
				std::string strip=splitstring(ptrB,',',idx);
				if(strip=="") break;
				unsigned long ipAddr=CSocket::Host2IP(strip.c_str());
				if(ipAddr==INADDR_NONE) continue;
				TAccessAuth aa; aa.bAcess=!bAccess_default;
				aa.ipAddr =ipAddr; aa.ipMask =0xffffffff; //255.255.255.255
				m_vecAcessList.push_back(aa);
			}
			if(m_vecAcessList.size()!=0){//添加默认访问规则
				TAccessAuth aa; aa.bAcess=bAccess_default;
				aa.ipAddr =0; aa.ipMask =0;
				m_vecAcessList.push_back(aa);
				RW_LOG_PRINT(LOGLEVEL_DEBUG,"%s列表中IP访问本服务, IP数:%d\r\n",(bAccess_default)?"禁止":"允许",m_vecAcessList.size()-1);
			}
			
		}//?if((it=maps.find("accesslist"))!=maps.end() )
		
		if((it=maps.find("syncond"))!=maps.end() )
		{
			if((*it).second=="false")
			{
				HANDLE hMutex=m_hSynMutex;
				if(hMutex!=NULL){
					WaitForSingleObject(hMutex, INFINITE);
					m_hSynMutex=NULL; CloseHandle(hMutex);
				}
			}else if((*it).second=="true")
			{
				if(m_hSynMutex==NULL) m_hSynMutex=CreateMutex(NULL, false, NULL);
			}
			/*(m_hSynMutex)?RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] 开启串行化同步执行！\r\n"):
						  RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] 关闭串行化同步执行！\r\n");*/
		}
	}//?if(SplitString(szParam,' ',maps)>0)
	//均衡负载模式必须程序同步，无法采用SQL的发布订阅机制同步
	if(m_iLoadMode==1 && m_iSynDBTimeOut<0) m_iSynDBTimeOut=0;
	if(m_iSynDBTimeOut<0) m_bRollback=false; //如果设置了禁止同步，则同步失败回滚无效
}

//有一个客户端连接关闭
void CProxyServer :: SocketClosed(CSocket *pSocket)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	pSocket->SetUserTag(0);
	if(pclnt!=NULL){
		int idxSelectedDBSvr=pclnt->GetSelectedDBSvr();
		RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] one client(%s:%d) closed!\r\n\t DB server: %d - %s:%d\r\n",
						pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),idxSelectedDBSvr+1,
						dbservers[idxSelectedDBSvr].m_svrhost.c_str(),dbservers[idxSelectedDBSvr].m_svrport);
		delete pclnt;
	}
}
//有一个客户端连接进来
bool CProxyServer :: SocketAccepted(CSocket *pSocket)
{
	bool bAccess=true; //判断是否允许访问
	unsigned long clntip=pSocket->GetRemoteAddr();
	for(unsigned int idx=0;idx<m_vecAcessList.size();idx++)
	{
		if( (clntip & m_vecAcessList[idx].ipMask)!=m_vecAcessList[idx].ipAddr )
			continue;
		bAccess=m_vecAcessList[idx].bAcess; break;
	}
	if(!bAccess){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] one client(%s:%d) connected - 拒绝访问!\r\n",pSocket->GetRemoteAddress(),pSocket->GetRemotePort());
		return false; //拒绝访问，关闭连接
	}
	
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	int idxLoadDBsvr,idxSelectedDBSvr=-1; //当前成功连接的主服务器索引
	CSQLClient *pclnt=new CSQLClient(this);
	if(pclnt==NULL) return false;
	CSocket *pMainDBSocket=ConnectMainDB(&idxSelectedDBSvr,pclnt);
	if(pMainDBSocket==NULL){ delete pclnt; return false; }
	RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] one client(%s:%d) connected!\r\n\t DB server: %d - %s:%d\r\n",
							pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),idxSelectedDBSvr+1,
							dbservers[idxSelectedDBSvr].m_svrhost.c_str(),dbservers[idxSelectedDBSvr].m_svrport);
	idxLoadDBsvr=(rand() % m_nValidDBServer); //随机选择一台主从查询服务器的索引，避免每个客户都从第一个开始
	pclnt->m_tds_dbname=dbservers[idxSelectedDBSvr].m_dbname;
	pclnt->SetMainDBSocket(pSocket,pMainDBSocket,idxSelectedDBSvr,idxLoadDBsvr);
	pSocket->SetUserTag((int)pclnt);
	return true;
}

bool CProxyServer :: SocketReceived(CSocket *pSocket)
{
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	if(pclnt==NULL) return false;
	IOutputStream *pOutStream=pSocket->GetOutputStream();
	CSocketStream *pInStream=(CSocketStream *) pSocket->GetInputStream();
	QueryPacket * plastquery=NULL;

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
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 严重错误 - 无效的协议数据，T=0x%02X S=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); return false; //关闭socket
		}
		if(!pclnt->m_bLoginDB){ //未成功登录主服务器
			if(tdsh.type ==0x17 || tdsh.type ==TDS7_LOGIN)
			{//0x17实际为ssl加密的登录数据包 (TDS协议中只有登录包采用ssl加密，其它数据包不加密)
			 //如果采用ssl加密方式则一个预登录包后续2个ssl握手协议交互包(type也是0x12)，然后是加密的登录包(type=0x17)
				if(tdsh.type ==TDS7_LOGIN) nSize=tdsh.size; //已处理字节
				if(!Parse_LoginPacket(pSocket,pInbuffer,nSize))
					return false;	
			}else{
				if(tdsh.type ==TDS71_PRELOGIN && *pinstream==0) //预登录包 0x12
				{//采用ssl加密登录，则ssl的的2次连接握手 TDS Heaer的type也是0x12,因此要注意保护
					unsigned char *lpEncry=parse_prelogin_packet(pInbuffer,1);
					if(lpEncry!=NULL) *lpEncry=0x02; //设置为不加密登录信息
					pclnt->m_tdsver=GetNetLib_PreLogin(pInbuffer);
				}
				nSize=tdsh.size; //登录前的所有数据都原封不动转发
				pclnt->SendWaitRelpy(pInbuffer,nSize,0); //转发数据,不等待返回
			}
			//移动数据处理指针，跳过已处理数据大小
			pInStream->Skip(nSize);  continue;
		}//?if(!pclnt->m_bLoginDB){ //未成功登录主服务器

		//否则成功登录主服务器
		if(tdsh.size>nSize) break; //数据未接收完毕,继续接收
		//接收到到一个完整packet，解析处理 begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d request data from %s:%d\r\n\t   type=0x%x status=%d size=%d\r\n",
						nSize,pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),tdsh.type ,tdsh.status ,tdsh.size);
		
		//优化: 如果只有一个有效的数据库，设置回滚false，减少不必要的调用和处理
		bool bRollback=(m_nValidDBServer>1)?m_bRollback:false; 
		if(tdsh.type ==TDS_QUERY || tdsh.type ==TDS_RPC)
		{//处理SQL操作消息 Query和RPC，RPC为带参数SQL
			if(plastquery==NULL)
			{
				if( (plastquery=new QueryPacket(pclnt->GetSelectedDBSvr(),pSocket->GetRemoteAddr()) )!=NULL )
				{
					plastquery->SetQueryData(pInbuffer,tdsh.size);
					plastquery->IsNotSelectSQL(true); //判断是否为非select语句
				}
			}else plastquery->AddQueryData(pInbuffer,tdsh.size);
			if( !IS_LASTPACKET(tdsh.status) || plastquery==NULL) goto PACKET_DONE;//没有接收到一条完整SQL操作语句

			if(0!=m_iLoadMode)//冗余备份或均衡负载模式，所有操作都在主服务器执行，写入操作执行成功会同步到其它负载服务器；
				pclnt->SendQuery_mode1(plastquery,bRollback);
			else //==0 主从负载模式,查询操作随机选择负载服务器执行，写入操作执行成功会同步到其它负载服务器；
				pclnt->SendQuery_mode0(plastquery,bRollback);
			plastquery=NULL; //已处理，无需释放
		}//?if(tdsh.type ==TDS_QUERY || tdsh.type ==TDS_RPC)
		else if(tdsh.type ==TDS7_TRANS) //事务处理消息
			pclnt->TransactPacket(pInbuffer,tdsh.size,bRollback);
		else //其它消息，仅仅转发到主服务器
			pclnt->SendWaitRelpy(pInbuffer,tdsh.size,0); //转发数据,不等待返回

PACKET_DONE:
		//接收到到一个完整packet，解析处理  end ====================================
		pInStream->Skip(tdsh.size); //移动数据处理指针，跳过已处理数据大小
	}//?while(true)
	//if(plastquery!=NULL) delete plastquery;
	if(plastquery!=NULL){//yyc modify 2017-11-05 如果此对象有效，说明此次接收的数据不是一个完整的sql请求数据包
	//因此要恢复已处理的接收数据，让上层socket继续接收而不是直接释放，否则上层再次接收到数据进入此处理过程时此前接收的数据已经被释放掉了
		pInStream->UnSkip(plastquery->GetQueryDataSize()); delete plastquery;
	}
	return true;
}

//向主服务器转发登录数据包，如果是未加密登录数据包则解析登录信息；登录成功后连接登录负载服务器
bool CProxyServer :: Parse_LoginPacket(CSocket *pSocket,unsigned char *pInbuffer,unsigned int nSize)
{
	CDBServer *dbservers=(CDBServer *)m_dbservers;
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	if(pclnt==NULL) return false;
	int iLoginOK=0,idxSelectedDBSvr=pclnt->GetSelectedDBSvr();
	unsigned int lTimeout=MAX_WAITREPLY_TIMEOUT; //登录数据包响应回复最大等待延迟(s)
	unsigned char type=*pInbuffer; //登录数据包类型(TDS7_LOGIN 未加密的)
	tds_login login, *ptr_login=NULL;
	if(type ==TDS7_LOGIN){
		parse_login_packet(pInbuffer,login); ptr_login=&login;
		pclnt->m_tdsver =GetUSVerByULVer(login.version);
		RW_LOG_PRINT(LOGLEVEL_DEBUG,"[SQLProxy] Access '%s' DB using %s:****** ;ver=0x%x block=%d\r\n",login.dbname.c_str(),
									login.username.c_str(),pclnt->m_tdsver,login.psz);//login.userpswd.c_str()
		
		CDBServer &sqlsvr=dbservers[idxSelectedDBSvr];
		std::string dbuser=sqlsvr.m_username,dbpswd=sqlsvr.m_userpwd,dbname=sqlsvr.m_dbname;
		if(dbname==""){ dbname=login.dbname; pclnt->m_tds_dbname=dbname; }
		if(dbuser==""){ dbuser=login.username; dbpswd=login.userpswd; }
		//如果指定了数据库名，则判断当前连接数据库和设定的数据库是否一致
		if(_stricmp(dbname.c_str(),login.dbname.c_str())!=0)
		{
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] '%s' 拒绝访问 - 访问的数据库和设定的不一致\r\n",login.dbname.c_str());
			return false;
		}
		if(_stricmp(dbuser.c_str(),login.username.c_str())!=0 || dbpswd!=login.userpswd)
		{//更改登录Packet中登录信息然后登录
			unsigned char NewLoginPacket[512]; 
			unsigned int NewSize=write_login_new(NewLoginPacket,512,login,NULL,dbuser.c_str(),dbpswd.c_str());
			iLoginOK=(NewSize==0)?0x0101:
					  pclnt->SendWaitRelpy(NewLoginPacket,NewSize,lTimeout);
		}else //否则转发原始登录数据包信息进行登录
			iLoginOK=pclnt->SendWaitRelpy(pInbuffer,nSize,lTimeout); //转发数据,等待返回
	}else //否则直接转发原始加密数据包进行登录
		iLoginOK=pclnt->SendWaitRelpy(pInbuffer,nSize,lTimeout); //转发数据,等待返回
	if(iLoginOK!=0 && iLoginOK!=-1){ //连接数据库失败
		unsigned short tds_token=(iLoginOK & 0x0000ffff),bit_flags=((iLoginOK & 0xffff0000)>>16);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 连接数据库失败 - Token=%02X %02X\r\n",tds_token, bit_flags);
		return false;
	}
	//如果是未加密的登录包，则优先采用原始登录包进行负载服务器的登录，尽量保证主服务器和负载服务器的登录设置一致
	unsigned char *login_packet=(type ==TDS7_LOGIN)?pInbuffer:NULL;
	pclnt->InitDBConn(login_packet,nSize,ptr_login); //此时才开始初始化后台数据库连接并启动工作线程
	return (pclnt->m_bLoginDB=true);
}

//连接主服务器
CSocket * CProxyServer :: ConnectMainDB(int *pIdxMainDB,ISocketListener *pListener)
{
	CSocket *pSocket = new CSocket(SOCK_STREAM);
	if(pSocket==NULL) return NULL;

	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	int idxSelected=m_idxSelectedMainDB; //当前应选择的主服务器索引
	unsigned int i,iNumsSocketSvrs=0; //SQL数据库集群的个数
	unsigned int iNumsValidSocketSvrs=0; //有效数据库集群个数
	for(;iNumsSocketSvrs<MAX_NUM_SQLSERVER;iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[iNumsSocketSvrs];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") 
			break;
		if(sqlsvr.IsValid()) iNumsValidSocketSvrs++;
	}
	m_nValidDBServer=iNumsValidSocketSvrs;

	for(i=0;i<iNumsSocketSvrs;i++)
	{
		CDBServer &sqlsvr=dbservers[idxSelected];
		if( sqlsvr.IsValid() && 
			pSocket->Connect(sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport,pListener) )
		{ //服务器有效且连接成功
			sqlsvr.m_nSQLClients++;
			//如果是均衡负载模式，则主服务器索引++,下次连接选择下一个
			m_idxSelectedMainDB=(m_iLoadMode==1)?((idxSelected+1) % iNumsSocketSvrs):idxSelected;
			if(pIdxMainDB) *pIdxMainDB=idxSelected; //返回当前选择的主服务器索引
			return pSocket; 
		}
		//如果当前服务器无效或者服务器连接失败，则选择下一个做主服务器
		idxSelected=(idxSelected+1) % iNumsSocketSvrs;
		if(!sqlsvr.IsValid()) continue; //该服务器已标记异常
		sqlsvr.SetValid(1);//否则设置此SQL服务器异常
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] Failed to connect %d - %s:%d\r\n",idxSelected,sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
	}//?for(i=0;i<iNumsSocketSvrs;i++)
	delete pSocket; return NULL;
}
