// export.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <time.h>
#include <process.h>
#pragma warning(disable:4996)
#pragma warning(disable:4244)

#include "export.h"
#include "dbclient.h"
#include "CDBServer.h"
#include "ServerSocket.h"
#include "ProxyServer.h"
#include "DebugLog.h"

const char * _PROJECTNAME_="SQLProxy"; //DebugLog日志输出标识
#define SQL_PORT 1433
static CDBServer g_dbservers[MAX_NUM_SQLSERVER];
static CProxyServer  g_proxysvr(g_dbservers);
static SQLAnalyse	g_sqlanalyse(g_dbservers);
static CServerSocket g_socketsvr;

extern "C"
SQLPROXY_API void _stdcall SetLogLevel(HWND hWnd,int LogLevel)
{
	if(LogLevel>=LOGLEVEL_DEBUG && LogLevel<=LOGLEVEL_NONE)
		RW_LOG_SETLOGLEVEL((LOGLEVEL)LogLevel);
	if((int)hWnd==1){
		RW_LOG_SETSTDOUT();
	}else if(hWnd!=NULL){
		 RW_LOG_SETHWND((long)hWnd);
	}else RW_LOG_SETDVIEW();
}

//设置集群数据库信息、启动运行参数以及SQL打印输出过滤参数
extern "C"
SQLPROXY_API void _stdcall SetParameters(int SetID,const char *szParam)
{
	if(SetID==1)
		g_proxysvr.SetBindServer(szParam);
	else if(SetID==2)
		g_sqlanalyse.SetFilter(szParam);
}

//获取后台某集群SQL服务状态 idx==-1获取SQL集群服务个数 ==-8待分析的Query数 ==-9当前工作模式名称==-10返回工作模式
//proid==0SQL服务是否运行正常 ==1返回SQL服务当前并发客户连接数 ==3返回SQL服务当前负载处理量 ==4写入处理量 
extern "C"
SQLPROXY_API INTPTR _stdcall GetDBStatus(int idx,int proid)
{
	if(idx==-8) return g_sqlanalyse.Size();
	if(idx==-9) return (INTPTR)g_proxysvr.GetLoadMode();
	if(idx==-10) return g_proxysvr.m_iLoadMode;

	return (INTPTR)g_proxysvr.GetDBStatus(idx,proid);
}

//启动SQLProxy代理服务
extern "C"
SQLPROXY_API bool _stdcall StartProxy(int iport)
{
	g_proxysvr.m_psqlAnalyse=&g_sqlanalyse;
	g_socketsvr.SetListener(&g_proxysvr);
	g_sqlanalyse.Start();
	if(iport<=0) iport=SQL_PORT;
	if (!g_socketsvr.Open(iport,0) )
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"SQL数据库集群服务启动失败，端口(%d)\r\n",iport);
		return false;
	}
	hostent *localHost = gethostbyname("");
	RW_LOG_PRINT(LOGLEVEL_ERROR,0,"SQLProxy Copyright (C) 2014 yyc; email:yycnet@163.com\r\n");
	RW_LOG_PRINT(LOGLEVEL_ERROR,"SQL数据库集群服务已启动 (%s:%d)\r\n",inet_ntoa (*(struct in_addr *)*localHost->h_addr_list),iport);
	RW_LOG_PRINT(LOGLEVEL_ERROR,"此集群共 %d 台后台数据库，当前选择模式为: %s;",g_proxysvr.GetDBNums(),g_proxysvr.GetLoadMode());
	if(g_proxysvr.m_iSynDBTimeOut>=0){
		if(g_proxysvr.m_bRollback){
			(g_proxysvr.m_hSynMutex!=NULL)?
				RW_LOG_PRINT(LOGLEVEL_ERROR,0," 启用同步失败回滚;"):
				RW_LOG_PRINT(LOGLEVEL_ERROR,0," 开启同步失败回滚;");
		}
		RW_LOG_PRINT(LOGLEVEL_ERROR,  " 同步超时: %dms\r\n", g_proxysvr.m_iSynDBTimeOut*1000);
	}else RW_LOG_PRINT(LOGLEVEL_ERROR,0," 同步禁止\r\n");
	return true;
}
extern "C"
SQLPROXY_API void _stdcall StopProxy()
{
	//Socket close时会调用WaitForSingleObject等待socket的工作线程结束；
	//而socket的工作线程在结束前会调用ISocketListener对象的SocketClosed方法
	//SocketClosed中会向当前UI线程的窗体通过SendMessage输出关闭信息，SendMessage是一个阻塞式调用
	//而UI线程此时正被WaitForSingleObject阻塞，导致2个线程死锁程序无法响应;
	//因此需要关闭信息调试输出(仅仅对向窗体输出信息时才会导致此现象)
	LOGLEVEL ll=RW_LOG_SETLOGLEVEL(LOGLEVEL_NONE);
	g_socketsvr.Close();
	RW_LOG_SETLOGLEVEL(ll); //恢复
	g_sqlanalyse.Stop();
	for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
		CDBServer &dbsvr=g_dbservers[idx];
		if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
		dbsvr.CloseQueryLog();
	}
}

static unsigned int __stdcall ExecFileData(void *args);
extern "C"
SQLPROXY_API int _stdcall ProxyCall(int callID,INTPTR lParam)
{
	switch(callID)
	{
	case 1:
		return ExecFileData((void *)lParam);
	}
	return 0;
}

//打开记录文件并执行，打开成功返回0，否则失败
//!!!此函数sqlAnalyse对象的工作线程和ISocketListener的接收数据处理线程都会进行日志输出
//因此如果日志是先窗体输出且当前调用时在窗体的UI线程调用，则会产生死锁，因此需要开启独立的线程调用
static unsigned int __stdcall ExecFileData(void *args)
{
	std::string fname; fname.assign((const char *)args);
	const char *queryfile=fname.c_str();
	FILE *fp=::fopen(queryfile,"rb");
	if(fp==NULL){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 打开记录文件失败 - %s\r\n",queryfile); 
		return 1;
	}
	//数据库信息、访问帐号以及默认TDS版本
	std::string hostname,dbname,dbuser,dbpswd;
	unsigned short hostport=0, tdsver=0;
	int block_size=8192;

	char tmpbuf[128]; unsigned short usLen=0;
	::fread(tmpbuf,1,4,fp);
	if(strcmp(tmpbuf,"sql")!=0){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 错误的记录文件格式 - %s\r\n",queryfile); 
		::fclose(fp); return 2;
	}
	::fread(tmpbuf,1,2,fp);						//host
	usLen=*(unsigned short *)tmpbuf;
	if(usLen>0x7f) { ::fclose(fp); return 3; }
	::fread(tmpbuf,1,usLen,fp);
	hostname.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//port
	hostport=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,2,fp);						//dbname
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbname .assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//username
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbuser.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//userpswd
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbpswd.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//tdsv
	tdsver=*(unsigned short *)tmpbuf; //默认协议版本
	if(tdsver<0x700 || tdsver>0x703) tdsver=0x703;

	DBConn dbconn; 
	int lTimeout=15; //Query执行结果最大超时等待时间s
	int numSuccess=0,numFailed=0; //执行计数
	//设定数据库连接登录信息
	tds_login_param &loginparams=dbconn.LoginParams();
	loginparams.tdsversion =tdsver; 
	loginparams.tds_block_size=block_size;
	dbconn.Setdbinfo(hostname.c_str(),hostport,dbname.c_str(),dbuser.c_str(),dbpswd.c_str());
	if(!dbconn.Connect()) { ::fclose(fp); return 4; }
	
	int idxRandDBSvr=MAX_NUM_SQLSERVER-1; //用于SQL分析时打印输出
	g_dbservers[idxRandDBSvr].Init();
	g_dbservers[idxRandDBSvr].m_svrhost=hostname;
	g_dbservers[idxRandDBSvr].m_svrport=hostport;
	g_dbservers[idxRandDBSvr].m_dbname=dbname;
	g_dbservers[idxRandDBSvr].m_username=dbuser;
	g_dbservers[idxRandDBSvr].m_userpwd=dbpswd;

	bool bActive=g_sqlanalyse.Active();
	if(!bActive) g_sqlanalyse.Start(); //启动SQL分析打印对象

	//循环获取query data并执行=================begin================
	QueryPacket * plastquery=NULL;
	unsigned char *poutbuf=new unsigned char [block_size]; 
	while(!::feof(fp) && poutbuf!=NULL)
	{
		size_t nReadBytes=::fread(poutbuf,1,sizeof(tds_header),fp);
		if(nReadBytes<sizeof(tds_header)) break; //发生错误
		bool bLastpacket=(*(poutbuf+1)!=0x0); //是否为Query的最后一个包
		unsigned short nSize= *(poutbuf+2)*256+*(poutbuf+3);
		if(nSize>block_size) break;
		nReadBytes=fread(poutbuf+sizeof(tds_header),1,(nSize-sizeof(tds_header)),fp);
		if(nReadBytes<(nSize-sizeof(tds_header)) ) 
			break; //发生错误
		if(plastquery==NULL){
			plastquery=new QueryPacket(idxRandDBSvr,0x0100007f);
			if(plastquery==NULL) break;
			plastquery->SetQueryData(poutbuf,nSize);
			plastquery->IsNotSelectSQL(true); //判断是否为非select语句
		}else plastquery->AddQueryData(poutbuf,nSize);
		if(!dbconn.IsConnected() || !dbconn.ExcuteData(0,poutbuf,nSize)) break;
		if(bLastpacket){//Query的最后一个Packet,等待服务器响应
			//Query执行结果，-1超时
			plastquery->m_staExec=dbconn.WaitForResult(NULL,0,lTimeout);
			plastquery->m_msExec=clock()-plastquery->m_msExec; //执行时间(ms)
			(plastquery->m_staExec==0)?++numSuccess:++numFailed;
			if(!g_sqlanalyse.AddQuery(plastquery,NULL,0)) delete plastquery;
			plastquery=NULL; //添加到SQL语句分析打印队列中
		}
	}//?while(!::feof(fp) && poutbuf!=NULL)
	delete[] poutbuf;
	if(plastquery!=NULL) delete plastquery;
	//循环获取query data并执行=================End================
	while(g_sqlanalyse.Size()!=0) ::Sleep(100); //等待直到打印分析完毕
	RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 打开记录文件 %s 成功\r\n发送 Query Data to %s:%d 执行成功：%d 执行失败：%d\r\n",
								queryfile,hostname.c_str(),hostport,numSuccess,numFailed);
	
	dbconn.Close(); 
	if(!bActive) g_sqlanalyse.Stop(); //停止SQL分析打印对象
	::fclose(fp); return 0;
}
