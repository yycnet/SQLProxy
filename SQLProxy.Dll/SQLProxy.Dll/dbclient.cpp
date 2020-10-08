#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "mytds.h"
#include "dbclient.h"
#include "DebugLog.h"
using namespace std;


static const TDS_UCHAR tds72_query_start[] = {
	/* total length */
	0x16, 0, 0, 0,
	/* length */
	0x12, 0, 0, 0,
	/* type */
	0x02, 0,
	/* transaction */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* request count */
	1, 0, 0, 0
};
//初始化并返回TDS登录参数信息
static void Init_tds_login_params(tds_login_param &tdsparam)
{
	tdsparam.tdsversion=0x701;	//默认采用的tds协议版本
	tdsparam.tds_ssl_login=0;	//登录时是否采用ssl机密机制 0，否 1是
	tdsparam.tds_block_size=4096;		//TDS 7.0+ use a default of 4096 as block size.
}
//根据设定block size发送packet，如果packet大小超过block size则分成多个packet发送
static bool SendOnePacket(CSocket *psocket,tds_header &tdsh,unsigned char *pinstream,int block_size)
{
	char tmpbuf[sizeof(tds_header)]; //临时备份恢复数据缓冲
	if(tdsh.size >block_size)
	{//分成多个packet发送
		unsigned short packetLen=tdsh.size-sizeof(tds_header); //不包含TDS头的数据长度
		unsigned short usMaxDataLen=block_size-sizeof(tds_header);
		while(packetLen!=0)
		{
			tds_header tdsh_new; ::memcpy(&tdsh_new,&tdsh,sizeof(tds_header));
			unsigned short usDataLen=(packetLen>usMaxDataLen)?usMaxDataLen:packetLen;
			packetLen-=usDataLen;
			tdsh_new.size=usDataLen+sizeof(tds_header);
			if( IS_LASTPACKET(tdsh.status) )
			{//原包为最后一个包，此时被分成多个包
				tdsh_new.status=(packetLen!=0)?0:0x01; //tdsh.status;
			}else tdsh_new.status=tdsh.status; //保持原包status不变

			::memcpy(tmpbuf,pinstream,sizeof(tds_header));  //备份
			write_tds_header(pinstream,tdsh_new);			//写入新的TDS的协议头
			int iSendRet=psocket->Send(pinstream, tdsh_new.size);
			::memcpy(pinstream,tmpbuf,sizeof(tds_header));  //恢复
			if(iSendRet<=0) return false; //发送失败
			pinstream=pinstream+tdsh_new.size-sizeof(tds_header);
		}//?while(packetLen!=0)
		return true;
	}
	return (psocket->Send(pinstream, tdsh.size)>0);
}
static bool SendOnePacket(CSocket *psocket,unsigned char *pinstream,int block_size)
{
	tds_header tdsh; parse_tds_header(pinstream,tdsh);
	return SendOnePacket(psocket,tdsh,pinstream,block_size);
}

DBConn :: DBConn()
{
	m_psocket=NULL;
	m_dbconn=NULL;
	m_uTransactionID=0;
	m_pfnOnStatus=NULL;
	m_pfnOnData=NULL;
	m_lUserParam=0;
	Init_tds_login_params(m_params);
}

DBConn :: ~DBConn()
{
	Close();	//关闭数据库连接
}

void DBConn::Setdbinfo(const char *szHost,int iport,const char *szDbname,const char *szUsername,const char *szUserpwd)
{
	if(szHost) m_hostname.assign(szHost);
	m_hostport=iport;
	if(szDbname) m_params.dbname.assign(szDbname);
	if(szUsername) m_params.dbuser.assign(szUsername);
	if(szUserpwd)  m_params.dbpswd.assign(szUserpwd);
}

//发送SQL语句，成功返回真;lTimeout:超时时间 s
bool DBConn::ExcuteSQL(const char *szsql,int lTimeout)
{
	std::string strsql(szsql);
	std::wstring wstrsql=MultibyteToWide(strsql);
	int iPacketlen=sizeof(tds_header)+strsql.length()*sizeof(wchar_t);
	if(IS_TDS72_PLUS(m_params.tdsversion)) iPacketlen+=TDS9_QUERY_START_LEN;

	unsigned char *szPacket=new unsigned char[iPacketlen];
	if(szPacket==NULL) return false;
	tds_header tdsh; tdsh.type =TDS_QUERY;
	tdsh.status =0x01; tdsh.size =iPacketlen; 
	tdsh.channel=0; tdsh.packet_number=tdsh.window=0;
	unsigned int buflen=write_tds_header(szPacket,tdsh);
	if(IS_TDS72_PLUS(m_params.tdsversion))
	{
		::memcpy(szPacket+buflen,tds72_query_start,TDS9_QUERY_START_LEN);
		buflen+=TDS9_QUERY_START_LEN;
	}
	::memcpy(szPacket+buflen,wstrsql.c_str(),strsql.length()*sizeof(wchar_t));
	buflen+=strsql.length()*sizeof(wchar_t);
	
	int iresult=WaitForResult(szPacket,tdsh.size,lTimeout); 
	delete[] szPacket; return (iresult==0);
}
//tdsv - 当前要执行发送数据的版本
bool DBConn::ExcuteData(unsigned short tdsv,unsigned char *ptrData,int iLen)
{
	if(tdsv==m_params.tdsversion){
		//query data版本和当前连接数据库使用的TDS版本一致，直接转发
		int nReadCount=0; tds_header tdsh;
		while(nReadCount<iLen)
		{
			unsigned char *pinstream=ptrData+nReadCount;
			pinstream+=parse_tds_header(pinstream,tdsh);
			if(IS_TDS9_QUERY_START(pinstream)){ //更改事务的Transaction ID为当前Transaction ID
				//实际测试发现(0x703版本)如果是事务Query，status为0x01为结束包标志 非事务为0x09.
				unsigned char *psta=pinstream-sizeof(tds_header)+1;
				if(m_uTransactionID!=0 && (tdsh.type==TDS_QUERY || tdsh.type==TDS_RPC) )
					*psta &=0xF7; //取消RESETCONNECTION位(0x08)
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pinstream+4+4+2); 
				*pTransID=m_uTransactionID;
			} 
			m_psocket->SetUserTag(WAITREPLY_FLAG); //设置等待消息回复标识
			if(m_psocket->Send(pinstream-sizeof(tds_header), tdsh.size)<=0)
			{ m_psocket->SetUserTag(0);	return false; } //发送失败
			nReadCount+=tdsh.size;
		}
		return true;
	}//?if(tdsv==tdsparam.tdsversion)
	//否则采用当前连接数据的协议版本发送Query 数据
	return IS_TDS72_PLUS(m_params.tdsversion)?ExcuteData_TDS72PLUS(ptrData,iLen):ExcuteData_TDS72SUB(ptrData,iLen);
}

//采用0x702以下协议发送query data。如果query data包含START_QUERY信息则要去掉
bool DBConn::ExcuteData_TDS72SUB(unsigned char *ptrData,int iLen)
{
	int block_size=m_params.tds_block_size;
	char tmpbuf[sizeof(tds_header)]; //临时备份恢复数据缓冲
	tds_header tdsh_old,tdsh_new;
	int nReadCount=0;
	while(nReadCount<iLen)
	{
		unsigned char *pinstream=ptrData+nReadCount;
		pinstream+=parse_tds_header(pinstream,tdsh_old);
		//判断是否包含START_QUERY,如果包含则跳过去掉
		int nSkipBytes=(IS_TDS9_QUERY_START(pinstream))?TDS9_QUERY_START_LEN:0;
		::memcpy(&tdsh_new,&tdsh_old,sizeof(tds_header));
		tdsh_new.status=(tdsh_old.status!=0x0)?0x01:0;
		tdsh_new.size=tdsh_old.size-nSkipBytes;
		
		m_psocket->SetUserTag(WAITREPLY_FLAG); //设置等待消息回复标识
		pinstream=pinstream+nSkipBytes-sizeof(tds_header); //新的packet起始位置(包含TDS头)
		::memcpy(tmpbuf,pinstream,sizeof(tds_header));  //备份
		write_tds_header(pinstream,tdsh_new);			//写入新的TDS7.1的协议头
		bool bret=SendOnePacket(m_psocket,tdsh_new,pinstream,block_size);
		::memcpy(pinstream,tmpbuf,sizeof(tds_header));  //恢复

		if(!bret){ m_psocket->SetUserTag(0); return false;} //发送失败
		nReadCount+=tdsh_old.size;
	}//?while(nReadCount<iLen)
	return true;
}
//采用0x702及以上协议发送query data。如果query data不包含START_QUERY信息则要添加上
bool DBConn::ExcuteData_TDS72PLUS(unsigned char *ptrData,int iLen)
{
	tds_header tdsh_old,tdsh_new;
	int nReadCount=0,block_size=m_params.tds_block_size;
	while(nReadCount<iLen)
	{
		unsigned char *ptrBuffer=NULL;
		unsigned char *pinstream=ptrData+nReadCount;
		pinstream+=parse_tds_header(pinstream,tdsh_old);
		if( !IS_TDS9_QUERY_START(pinstream) )
		{//不包含包含STRAT QUERY信息
			ptrBuffer=(unsigned char *)::malloc(tdsh_old.size+TDS9_QUERY_START_LEN);
			if(ptrBuffer==NULL) return false;
			::memcpy(&tdsh_new,&tdsh_old,sizeof(tds_header));
			tdsh_new.size =tdsh_old.size+TDS9_QUERY_START_LEN;
			int buflen=write_tds_header(ptrBuffer,tdsh_new);
			::memcpy(ptrBuffer+buflen,tds72_query_start,TDS9_QUERY_START_LEN);
			TDS_UINT8 *pTransID=(TDS_UINT8 *)(ptrBuffer+buflen+4+4+2); 
			*pTransID=m_uTransactionID;//更改事务的Transaction ID为当前Transaction ID
			buflen+=TDS9_QUERY_START_LEN;
			::memcpy(ptrBuffer+buflen,pinstream,tdsh_old.size-sizeof(tds_header));
		}else{ //包含STRAT QUERY信息
			TDS_UINT8 *pTransID=(TDS_UINT8 *)( pinstream+4+4+2); 
			*pTransID=m_uTransactionID;//更改事务的Transaction ID为当前Transaction ID;0非事务
			pinstream=pinstream-sizeof(tds_header);
		}
		unsigned char *psta=(ptrBuffer!=NULL)?(ptrBuffer+1):(pinstream+1);
		if(m_uTransactionID!=0 && (tdsh_old.type==TDS_QUERY || tdsh_old.type==TDS_RPC) )
					*psta &=0xF7; //取消RESETCONNECTION位(0x08)

		m_psocket->SetUserTag(WAITREPLY_FLAG); //设置等待消息回复标识
		bool bret=(ptrBuffer!=NULL)?SendOnePacket(m_psocket,tdsh_new,ptrBuffer,block_size):
									SendOnePacket(m_psocket,tdsh_old,pinstream,block_size);
		if(ptrBuffer!=NULL) ::free(ptrBuffer);
		if(!bret){ m_psocket->SetUserTag(0); return false;} //发送失败
		nReadCount+=tdsh_old.size;
	}//?while(nReadCount<iLen)
	return true;
}

void DBConn :: SocketClosed(CSocket *pSocket)
{
	int iEvent=(m_psocket==NULL)?1:2; //区分正常关闭还是异常
	LOGLEVEL ll=(m_psocket==NULL)?LOGLEVEL_INFO:LOGLEVEL_ERROR; 
	RW_LOG_PRINT(ll,"[SQLProxy] Connection to SQL SERVER(%s:%d) has been closed\r\n",m_hostname.c_str(),m_hostport);
	if(m_pfnOnStatus) (*m_pfnOnStatus)(iEvent,m_lUserParam);
}
//接收处理SQL服务器返回的数据
bool DBConn :: SocketReceived(CSocket *pSocket)
{
	CSocketStream *pInStream= (CSocketStream *)pSocket->GetInputStream();
	bool bNewReply=true; //标记是否开始一个信息的回复packet，每个reply可能包含多个packets
	long lUserTag=0;	//如果==WAITREPLY_FLAG说明有异步等待回复的处理，其它则为cond的同步等待对象地址
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
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] 严重错误 - 无效的协议数据，T=0x%02X s=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); break;
		}
		if(tdsh.size>nSize) break; //数据未接收完毕,继续接收
		//接收到到一个完整packet，解析处理 begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d data from DB Server\r\n\t   type=0x%x status=%d size=%d\r\n",
								nSize,tdsh.type ,tdsh.status ,tdsh.size);
		
		if(m_pfnOnData) //设置了数据到达回调，直接转发
			(*m_pfnOnData)(&tdsh,pInbuffer,m_lUserParam);
		else if(tdsh.type ==TDS_REPLY)
		{//回复消息
			if( (lUserTag=pSocket->GetUserTag())!=0) //有正等待回复结果的处理
			{
				(bNewReply)?m_lastreply.SetReplyData(pInbuffer,tdsh.size):
							m_lastreply.AddReplyData(pInbuffer,tdsh.size);
				if( IS_LASTPACKET(tdsh.status) )
				{//回复的最后一个packet
					m_lastreply.m_iresult=(int)parse_reply(pInbuffer,m_params.tdsversion,&m_lastreply.m_nAffectRows);
					pSocket->SetUserTag(0); //已接收到回复，取消有正等待回复结果的处理标识
					bNewReply=true; //开始一个新的回复接收处理
				}else bNewReply=false; //后续还有该回复的packet待接收
			}//?if(pSocket->GetUserTag()!=0)
		}//回复消息处理=====================
		
		//接收到到一个完整packet，解析处理  end ====================================
		pInStream->Skip(tdsh.size); //移动数据处理指针，跳过已处理数据大小
	}//while(true)
	return true;
}

//发送数据并等待执行返回结果 lTimeout(s):超时时间 =0不等待 <0无限等待否则等待指定s
//返回0成功 -2发送失败 -1响应超时 其它响应错误码
int DBConn::WaitForResult(void *pdata,unsigned int nsize,int lTimeout)
{
	if(lTimeout==0){ //不等待返回
		m_psocket->SetUserTag(WAITREPLY_FLAG); //设置等待消息回复标识
		if(m_psocket->Send(pdata,nsize)>0) return 0;
		m_psocket->SetUserTag(0); return DBSTATUS_ERROR;
	}//否则等待返回lTimeout<0 无限等待
	
	int iresult=-1; //超时
	DWORD msTimeOut=(lTimeout>0)?(lTimeout*1000):(300*1000);
	if(pdata!=NULL) m_psocket->SetUserTag(WAITREPLY_FLAG);
	if(pdata==NULL || m_psocket->Send(pdata,nsize)>0)
	{
		long tStartTime=clock();
		while(true)
		{
			iresult=-1; //超时
			if( (clock()-tStartTime)>msTimeOut ) break;
			if( (iresult=this->GetStatus_Asyn()) !=DBSTATUS_PROCESS) 
				break;
			::Sleep(1); //延时,继续
		}
	}else iresult=DBSTATUS_ERROR; //网络连接错误
	m_psocket->SetUserTag(0);	return iresult;

/*	//多线程，有可能cond.wait等待前数据已经到达，导致无法获得响应
	clsCond cond; int iresult=DBSTATUS_ERROR;
	DWORD msTimeOut=(lTimeout>0)?(lTimeout*1000):0;
	m_psocket->SetUserTag((int)&cond);
	if( m_psocket->Send(pdata,nsize)>0)
		iresult=(cond.wait(msTimeOut))?(m_lastreply.m_iresult):-1;
	m_psocket->SetUserTag(0);	return iresult; */
}

