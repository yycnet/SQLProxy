
//TDS 登录处理
#include "stdafx.h"
#include <process.h>
#pragma warning(disable:4996)

#include "Socket.h"
#include "mytds.h"
#define IS_TDS72_PLUS(x) ((x)>=0x702)
#define IS_TDS73_PLUS(x) ((x)>=0x703)
#define TDS_PUT_BYTE(buf,x) { *(buf)=(x); (buf)+=1; }
#define TDS_PUT_SHORT(buf,x) { *(buf)=(((x)&0xff00)>>8); *((buf)+1)=((x)&0xff); (buf)+=2; }
#define TDS_PUT_LONG(buf,x) { *(buf)=(((x)&0xff000000)>>24); *((buf)+1)=(((x)&0x00ff0000)>>16); *((buf)+2)=(((x)&0x0000ff00)>>8); *((buf)+3)=((x)&0x000000ff); (buf)+=4; }
#define TDS_PUT(buf,x) { ::memcpy(buf,&(x),sizeof(x)); (buf)+=sizeof(x); }
#define TDS_PUT_PID(buf) { int pid=_getpid(); TDS_PUT(buf,pid); }
#define TDS_GET_SHORT(buf,x){ (x)=(*(buf))*256+(*((buf)+1)); (buf)+=2; }
#define TDS_GET_LONG(buf,x) { (x)=((*(buf))<<24)+((*((buf)+1))<<16)+((*((buf)+2))<<8)+(*((buf)+3)); (buf)+=4;}

/* mssql login options flags */
enum option_flag1_values {
	TDS_BYTE_ORDER_X86		= 0, 
	TDS_CHARSET_ASCII		= 0, 
	TDS_DUMPLOAD_ON 		= 0, 
	TDS_FLOAT_IEEE_754		= 0, 
	TDS_INIT_DB_WARN		= 0, 
	TDS_SET_LANG_OFF		= 0, 
	TDS_USE_DB_SILENT		= 0, 
	TDS_BYTE_ORDER_68000	= 0x01, 
	TDS_CHARSET_EBDDIC		= 0x02, 
	TDS_FLOAT_VAX		= 0x04, 
	TDS_FLOAT_ND5000		= 0x08, 
	TDS_DUMPLOAD_OFF		= 0x10,	/* prevent BCP */ 
	TDS_USE_DB_NOTIFY		= 0x20, 
	TDS_INIT_DB_FATAL		= 0x40, 
	TDS_SET_LANG_ON		= 0x80
};

enum option_flag2_values {
	TDS_INIT_LANG_WARN		= 0, 
	TDS_INTEGRATED_SECURTY_OFF	= 0, 
	TDS_ODBC_OFF		= 0, 
	TDS_USER_NORMAL		= 0,	/* SQL Server login */
	TDS_INIT_LANG_REQUIRED	= 0x01, 
	TDS_ODBC_ON			= 0x02, 
	TDS_TRANSACTION_BOUNDARY71	= 0x04,	/* removed in TDS 7.2 */
	TDS_CACHE_CONNECT71		= 0x08,	/* removed in TDS 7.2 */
	TDS_USER_SERVER		= 0x10,	/* reserved */
	TDS_USER_REMUSER		= 0x20,	/* DQ login */
	TDS_USER_SQLREPL		= 0x40,	/* replication login */
	TDS_INTEGRATED_SECURITY_ON	= 0x80
};
enum option_flag3_values {	/* TDS 7.3+ */
	TDS_RESTRICTED_COLLATION	= 0, 
	TDS_CHANGE_PASSWORD		= 0x01, 
	TDS_SEND_YUKON_BINARY_XML	= 0x02, 
	TDS_REQUEST_USER_INSTANCE	= 0x04, 
	TDS_UNKNOWN_COLLATION_HANDLING	= 0x08, 
	TDS_ANY_COLLATION		= 0x10
};
//TDS登录需要的参数信息
typedef struct tds_login_param
{
//PreLogin Packet设置参数
	unsigned short tdsversion;
	unsigned char tds_ssl_login;
//Login Packet设置参数
	unsigned int tds_block_size;
	std::string hostname;	//客户端主机名
	std::string dbname;		//数据库名以及访问帐号和密码
	std::string dbuser;
	std::string dbpswd;
}tds_login_param;

//初始化并返回TDS登录参数信息
tds_login_param Init_tds_login_params()
{
	char szbuf[128];
	tds_login_param tdsparam;
	tdsparam.tdsversion=0x701;	//采用的tds协议版本
	tdsparam.tds_ssl_login=0;	//登录时是否采用ssl机密机制 0，否 1是

	tdsparam.tds_block_size=4096;		//TDS 7.0+ use a default of 4096 as block size.
	::memset(szbuf,0,sizeof(szbuf));
	gethostname(szbuf, sizeof(szbuf));
	szbuf[sizeof(szbuf) - 1] = '\0';	/* make sure it's truncated */
	tdsparam.hostname.assign(szbuf);
	return tdsparam;
}

static unsigned int write_tds_header(void * pinstream,tds_header &tdsh)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	TDS_PUT_BYTE(pstream,tdsh.type);
	TDS_PUT_BYTE(pstream,tdsh.status);
	
	TDS_PUT_SHORT(pstream,tdsh.size);
	TDS_PUT_SHORT(pstream,tdsh.channel);

	TDS_PUT_BYTE(pstream,tdsh.packet_number);
	TDS_PUT_BYTE(pstream,tdsh.window);
	return pstream-(unsigned char *)pinstream;
}
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

//解析prelogin包的返回的是否加密信息，序号1
int tds_prelogin_reply(unsigned char *buf,unsigned int buflen)
{
	if(buflen<(sizeof(tds_header)+10)) return -1; //数据未接收完
	if(*buf!=TDS_REPLY && *(buf+1)!=0x01) return -2; //不是有效的响应包

	//跳过TDS协议头
	unsigned char *szPacket=buf+sizeof(tds_header);
	//下面跟着每个序号的定义，每个定义5字节; <序号1字节> <信息的起始位置2字节> <信息大小2字节>
	szPacket+=5; //跳过序号0 的定义，加密信息定义为序号1
	unsigned char idx=*szPacket++;
	unsigned short startpos=(*szPacket)*256+(*(szPacket+1));
	szPacket+=sizeof(unsigned short);
	unsigned short ilen=(*szPacket)*256+(*(szPacket+1));
	szPacket+=sizeof(unsigned short);
	//定位到加密信息描述自己位置
	if(buflen<(sizeof(tds_header)+startpos)) return -1; //数据未接收完
	szPacket=buf+sizeof(tds_header)+startpos;
	//02不加密 //00加密
	return *szPacket;
}


int tds_login_package(unsigned char *buf,unsigned int buflen,tds_login_param &tdsparam)
{
	//固定信息
	static const unsigned char client_progver[] = {6, 0x83, 0xf2, 0xf8 };
	static const unsigned char connection_id[] = { 0x00, 0x00, 0x00, 0x00 };
	static const unsigned char time_zone[] = { 0x88, 0xff, 0xff, 0xff }; 
	static const unsigned char collation[] = { 0x36, 0x04, 0x00, 0x00 }; 
	static const unsigned char sql_type_flag = 0x00;
	static const unsigned char reserved_flag = 0x00;
	unsigned char option_flag1 = TDS_SET_LANG_ON | TDS_USE_DB_NOTIFY | TDS_INIT_DB_FATAL;
	unsigned char option_flag2 = TDS_INIT_LANG_REQUIRED | TDS_ODBC_ON;
	unsigned char option_flag3 = TDS_UNKNOWN_COLLATION_HANDLING;

	int block_size = tdsparam.tds_block_size;
	unsigned int tdsversion=GetULVerByUSVer(tdsparam.tdsversion );
	int login_size=0; //登录数据包大小(不包含TDS头)
	
	if(buflen<(sizeof(tds_header)+login_size) )
		return 0; //最小缓冲大小
	
	unsigned char *szPacket=buf;
	int len_package=0;
	tds_header tdsh; 
	::memset(&tdsh,0,sizeof(tds_header));
	tdsh.type =TDS7_LOGIN;
	tdsh.status =0x01;
	len_package+=sizeof(tds_header);
	szPacket=buf+len_package;

	TDS_PUT(szPacket,login_size); len_package+=sizeof(login_size);
	TDS_PUT(szPacket,tdsversion); len_package+=sizeof(tdsversion);
	TDS_PUT(szPacket,block_size); len_package+=sizeof(block_size);
	TDS_PUT(szPacket,client_progver); len_package+=sizeof(client_progver);
	TDS_PUT_PID(szPacket); len_package+=4; /* pid */
	TDS_PUT(szPacket,connection_id); len_package+=sizeof(connection_id);
	*szPacket=option_flag1;  szPacket++;
	*szPacket=option_flag2;  szPacket++;
	*szPacket=sql_type_flag; szPacket++;
	*szPacket++=(IS_TDS73_PLUS(tdsparam.tdsversion))?option_flag3:0;
	
	TDS_PUT(szPacket,time_zone); len_package+=sizeof(time_zone);
	TDS_PUT(szPacket,collation); len_package+=sizeof(collation);

	tdsh.size=len_package; //login_size+sizeof(tds_header)
	write_tds_header(buf,tdsh);
	return len_package;
}
