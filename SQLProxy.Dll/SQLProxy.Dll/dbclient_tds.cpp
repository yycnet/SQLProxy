#include "stdafx.h"
#include <process.h>	//_getpid
#pragma warning(disable:4996)

#include "Socket.h"
#include "mytds.h"
#include "dbclient.h"
#include "DebugLog.h"
using namespace std;

#define TDS_PUT_BYTE(buf,x) { *(buf)=(x); (buf)+=1; }
#define TDS_PUT_SHORT(buf,x) { *(buf)=(((x)&0xff00)>>8); *((buf)+1)=((x)&0xff); (buf)+=2; }
#define TDS_PUT_LONG(buf,x) { *(buf)=(((x)&0xff000000)>>24); *((buf)+1)=(((x)&0x00ff0000)>>16); *((buf)+2)=(((x)&0x0000ff00)>>8); *((buf)+3)=((x)&0x000000ff); (buf)+=4; }
#define TDS_PUT(buf,x) { ::memcpy(buf,&(x),sizeof(x)); (buf)+=sizeof(x); }
#define TDS_PUT_PID(buf) { int pid=_getpid(); TDS_PUT(buf,pid); }
#define TDS_GET_SHORT(buf,x){ (x)=(*(buf))*256+(*((buf)+1)); (buf)+=2; }
#define TDS_GET_LONG(buf,x) { (x)=((*(buf))<<24)+((*((buf)+1))<<16)+((*((buf)+2))<<8)+(*((buf)+3)); (buf)+=4;}

#define MAX_REPLY_TIMEOUT 20		//20s，最大超时等待时间 s

int tds_prelogin_package(unsigned char *buf,unsigned int buflen,tds_login_param &tdsparam)
{
	static const char InstanceName[]="MSSQLServer";
	#define SET_BYTES(buf,idx,n1,n2) { *(buf)=(idx); *((buf)+1)=((n1)>>8); *((buf)+2)=((n1)&0xff); *((buf)+3)=((n2)>>8); *((buf)+4)=((n2)&0xff); (buf)+=5; }
	static const TDS_UCHAR netlib8[] = { 8, 0, 1, 0x55, 0, 0 };
	static const TDS_UCHAR netlib9[] = { 9, 0, 0,    0, 0, 0 };
	static const TDS_UCHAR netlib10[] = { 0x10, 0, 0,0, 0, 0 };
	
	if(buflen<(sizeof(tds_header)+26+6+sizeof(InstanceName)+6) )
		return 0; //最小缓冲大小
	unsigned char *szPacket=buf;
	int len_package=0;
	tds_header tdsh; 
	::memset(&tdsh,0,sizeof(tds_header));
	tdsh.type =TDS71_PRELOGIN;
	tdsh.status =0x01;
	len_package+=sizeof(tds_header);
	szPacket=buf+len_package;

	int instance_name_len=sizeof(InstanceName);
	//idx序号 n1信息起始位置 n2信息字节大小
	unsigned int start_pos =(!IS_TDS72_PLUS(tdsparam.tdsversion))? 21:26;
	SET_BYTES(szPacket,0,start_pos,6);		/* netlib version */
	SET_BYTES(szPacket,1,start_pos+6,1);	/* encryption */
	SET_BYTES(szPacket,2,start_pos+6+1,instance_name_len);	/* instance */
	SET_BYTES(szPacket,3,start_pos + 6 + 1 + instance_name_len,4);	/* process id */
	if(IS_TDS72_PLUS(tdsparam.tdsversion)) //TDS协议版本>=7.2
		SET_BYTES(szPacket,4,start_pos + 6 + 1 + instance_name_len + 4,1);/* MARS enables */
	*szPacket++=0xff;
	len_package+=start_pos;
	
	if(IS_TDS73_PLUS(tdsparam.tdsversion)){ //TDS协议版本>=7.3
		::memcpy(szPacket,netlib10,sizeof(netlib10)); 
		szPacket+=sizeof(netlib10); len_package+=sizeof(netlib10);
	}else if(IS_TDS72_PLUS(tdsparam.tdsversion)){ //TDS协议版本>=7.2
		::memcpy(szPacket,netlib9,sizeof(netlib9)); 
		szPacket+=sizeof(netlib9); len_package+=sizeof(netlib9);
	}else{
		::memcpy(szPacket,netlib8,sizeof(netlib8)); 
		szPacket+=sizeof(netlib8); len_package+=sizeof(netlib8);
	}
	if(tdsparam.tds_ssl_login==0)
		 *szPacket++=2;
	else *szPacket++=0;	//登录时采用 SSL加密
	len_package+=1;
	::memcpy(szPacket,InstanceName,instance_name_len);	/* instance */
	szPacket+=instance_name_len; len_package+=instance_name_len;
	
	/* pid */
	TDS_PUT_PID(szPacket); len_package+=4;
	/* MARS (1 enabled) */
	if(IS_TDS72_PLUS(tdsparam.tdsversion)){ *szPacket++=0; len_package+=1; }

	tdsh.size=len_package;
	write_tds_header(buf,tdsh);
	return len_package;
}

//连接数据库
bool DBConn::Connect_tds(unsigned char *login_packet,unsigned int nSize)
{
	Close();
	if( (m_psocket=new CSocket(SOCK_STREAM))==NULL ) 
		return false;
	if(!m_psocket->Connect(m_hostname.c_str(),m_hostport,this))
        return false;
	int lTimeout=MAX_REPLY_TIMEOUT; //最大超时等待时间s
	unsigned char szPacket[256]; int iPacketlen;
	iPacketlen=tds_prelogin_package(szPacket,256,m_params);
	//登录包返回的的bit_flag和正常查询包不一致，因此不能用==0判断返回是否成功
	if(WaitForResult(szPacket,iPacketlen,lTimeout)==DBSTATUS_ERROR) 
		return false; //发送预登录包失败

	if(WaitForResult(login_packet,nSize,lTimeout)!=0)
	{//登录失败
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] Connect to MS SQL SERVER (%s:%d) fail\r\n",m_hostname.c_str(),m_hostport);
		Close(); return false;
	}
	RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] Conect to SQL SERVER (%s:%d) success, Ver=0x%x\r\n",m_hostname.c_str(),m_hostport,m_params.tdsversion);
	m_lastreply.m_tdsver=m_params.tdsversion;
	if(m_pfnOnStatus) (*m_pfnOnStatus)(0,m_lUserParam);
	return true;
}

//关闭数据库连接
void DBConn::Close()
{
	CSocket *psocket=m_psocket;
	m_dbconn=NULL; m_psocket=NULL;

	if(psocket!=NULL)delete psocket;
}

//连接数据库;
bool DBConn::Connect()
{
	Close();
	if( (m_psocket=new CSocket(SOCK_STREAM))==NULL ) 
		return false;
	if(!m_psocket->Connect(m_hostname.c_str(),m_hostport,this))
        return false;
	
	int lTimeout=MAX_REPLY_TIMEOUT; //最大超时等待时间s
	unsigned char szPacket[256]; int iPacketlen;
	iPacketlen=tds_prelogin_package(szPacket,256,m_params);
	//登录包返回的的bit_flag和正常查询包不一致，因此不能用==0判断返回是否成功
	if(WaitForResult(szPacket,iPacketlen,lTimeout)==DBSTATUS_ERROR) 
		return false; //发送预登录包失败
	
	char tmpbuf[64];
	tds_login login;//发送登录包
	login.version =GetULVerByUSVer(m_params.tdsversion);
	login.psz =m_params.tds_block_size;
	login.client_version =0xf8f28306;
	login.client_pid =0x07d4;
	login.connection_id=0x0;
	login.opt_flag1=0xf0; login.opt_flag2=0x01; 
	login.sql_flag=0x00;  login.res_flag=0x00; 
	login.time_zone=0xFFFFFF88; 
	login.collation=0x0436;
	login.hostname =m_hostname;
	login.username=m_params.dbuser;
	login.userpswd=m_params.dbpswd ;
	login.appname="";
	sprintf(tmpbuf,"%d:%d",m_hostname,m_hostport);
	login.servername.assign(tmpbuf);
	login.remotepair="";
	login.libraryname="DB-Library";
	login.language="us_english";
	login.dbname=m_params.dbname;

	iPacketlen=write_login_packet(NULL,login)+sizeof(tds_header);
	tds_header tdsh; tdsh.type =TDS7_LOGIN;
	tdsh.status =0x01; tdsh.size =iPacketlen; 
	tdsh.channel=0; tdsh.packet_number=tdsh.window=0;
	write_tds_header(szPacket,tdsh);
	write_login_packet(szPacket,login);
	
	if(WaitForResult(szPacket,iPacketlen,lTimeout)!=0) //发送登录包
	{//登录失败
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] Connect to MS SQL SERVER (%s:%d) fail\r\n",m_hostname.c_str(),m_hostport);
		Close();return false;
	}
	RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] Conect to SQL SERVER (%s:%d) success, Ver=0x%x\r\n",m_hostname.c_str(),m_hostport,m_params.tdsversion);
	m_lastreply.m_tdsver=m_params.tdsversion;
	if(m_pfnOnStatus) (*m_pfnOnStatus)(0,m_lUserParam);
	return true;
}
//连接测试；成功返回0，否则失败 1登录失败(帐号或密码错误) 2数据库错误
int DBConn::TestConnect()
{
	std::string dbname=m_params.dbname;
	m_params.dbname=""; //禁止Connect时指定数据库，connect仅仅登录
	if(!Connect()) return 1;

	if(dbname=="") return 0;
	//打开指定的数据库,测试数据库是否存在
	int lTimeout=MAX_REPLY_TIMEOUT; //最大超时等待时间s
	char szsql[64]; sprintf(szsql,"use [%s]",dbname.c_str());
	return ExcuteSQL(szsql,lTimeout)?0:2;
}

