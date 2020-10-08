

#include "stdafx.h"
#include <time.h> //localtime
#pragma warning(disable:4996)

#include "InputStream.h"
#include "mytds.h"
#include "DebugLog.h"

unsigned short GetUSVerByULVer(unsigned int tdsversion)
{
	if(tdsversion==0x70000000) return 0x700;
	else if(tdsversion==0x71000001) return 0x701;
	else if(tdsversion==0x72090002) return 0x702;
	else if(tdsversion==0x730B0003) return 0x703;	//SUPPORT_NBCROW
	else if(tdsversion==0x730A0003) return 0x703; // 7.3.A, no NBCROW
	else if(tdsversion>0x71000001) return 0x702;
	return 0x500;
}
unsigned int GetULVerByUSVer(unsigned short tdsv)
{
	if(tdsv==0x700) return 0x70000000;
	else if(tdsv==0x701) return 0x71000001;
	else if(tdsv==0x702) return 0x72090002;
	else if(tdsv==0x703) return 0x730A0003; // 7.3.A, no NBCROW
	else if(tdsv==0x700) return 0x70000000;
	return 0x71000001;
}
 /**
 * tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
 * the calling function is responsible for ensuring crypt_pass is at least 
 * 'len' characters
 */
static unsigned char *
tds7_crypt_pass(const unsigned char *clear_pass, size_t len, unsigned char *crypt_pass)
{
	size_t i;

	for (i = 0; i < len; i++)
		crypt_pass[i] = ((clear_pass[i] << 4) | (clear_pass[i] >> 4)) ^ 0xA5;
	return crypt_pass;
}
static unsigned char *
tds7_decrypt_pass(const unsigned char *crypt_pass, size_t len, unsigned char *clear_pass)
{
	size_t i;

	for (i = 0; i < len; i++)
		clear_pass[i] =((crypt_pass[i]^0xA5) << 4) | ((crypt_pass[i]^0xA5) >> 4);
	return clear_pass;
}
//将std::string转成std::wstring
static std::string WideToMultibyte(const wchar_t *szText,int iLen)
{
	if(szText==NULL || iLen==0) return std::string();
	int iBufLen=iLen*sizeof(short); 
	char *szBuf=new char[iBufLen+1];
	if(szBuf==NULL) return std::string();

	int lret=WideCharToMultiByte(CP_ACP,WC_COMPOSITECHECK|WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR,
					(wchar_t *)szText,iLen,szBuf,iBufLen,NULL,NULL);
	std::string strret(szBuf,lret);
	delete[] szBuf; return strret;
}
//将std::string转成std::wstring
std::wstring MultibyteToWide(const std::string& multibyte)
{
	size_t length = multibyte.length();
	if (length == 0)
	return std::wstring();

	wchar_t * wide = new wchar_t[multibyte.length()*2+2];
	if (wide == NULL) return std::wstring();
	int ret = MultiByteToWideChar(CP_ACP, 0, multibyte.c_str(), (int)multibyte.size(), wide, (int)length*2 - 1);
	wide[ret] = 0;
	std::wstring str(wide);
	delete[] wide; return str;
}

unsigned int write_tds_header(void * pinstream,tds_header &tdsh)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	*pstream=tdsh.type; pstream+=sizeof(tdsh.type);
	*pstream=tdsh.status; pstream+=sizeof(tdsh.status);
	
	*pstream=((tdsh.size & 0xff00)>>8);
	*(pstream+1)=(tdsh.size & 0x00ff);  pstream+=sizeof(tdsh.size);
	
	*pstream=((tdsh.channel & 0xff00)>>8);
	*(pstream+1)=(tdsh.channel & 0x00ff);  pstream+=sizeof(tdsh.channel);

	*pstream=tdsh.packet_number; pstream+=sizeof(tdsh.packet_number);
	*pstream=tdsh.window; pstream+=sizeof(tdsh.window);
	return pstream-(unsigned char *)pinstream;
}

unsigned int parse_tds_header(void * pinstream,tds_header &tdsh)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	tdsh.type=*pstream;
	pstream+=sizeof(tdsh.type);

	tdsh.status=*pstream;
	pstream+=sizeof(tdsh.status);
	
	tdsh.size=(*pstream)*256+ (*(pstream+1));
	pstream+=2;
	//tdsh.size=*(unsigned short *)pstream; 
	//pstream+=IInputStream::htonb(tdsh.size);
	
	tdsh.channel=0; pstream+=2;
	//实际为SPID (the process ID on the server, corresponding to the current connection)
	//This information is sent by the server to the client and is useful for identifying which thread on the server sent the TDS packet. It is provided for debugging purposes. 
	//The client MAY send the SPID value to the server. If the client does not, then a value of 0x0000 SHOULD be sent to the server.
	//tdsh.channel=*(unsigned short *)pstream; 
	//pstream+=IInputStream::htonb(tdsh.channel);
	
	tdsh.packet_number=*pstream;
	pstream+=sizeof(tdsh.packet_number);

	tdsh.window=*pstream;
	pstream+=sizeof(tdsh.window);
	return pstream-(unsigned char *)pinstream;
}
//PreLogin - 预登录包
//<TDS header 8字节> + <信息定义 5字节>...<信息定义 5字节>+信息n
//信息定义： <1字节序号0...4> + <2字节偏移地址,相对信息定义起始位置>+<2字节信息长度>
//序号0 netlib信息 1加密信息 2...
unsigned char * parse_prelogin_packet(void * pinstream,unsigned char idx)
{
	unsigned char *szPacket=(unsigned char *)pinstream+sizeof(tds_header);
	szPacket+=(idx*5);
	unsigned char n=*szPacket++;
	if(n!=idx) return NULL;
	unsigned short startpos=(*szPacket)*256+(*(szPacket+1)); szPacket+=2;
	//unsigned short ilen=(*szPacket)*256+(*(szPacket+1)); szPacket+=2;
	return ((unsigned char *)pinstream+sizeof(tds_header)+startpos);
}
//根据netlib信息初步判断tds协议版本 (并不准确)
unsigned short GetNetLib_PreLogin(void * pinstream)
{
	unsigned char *szPacket=parse_prelogin_packet(pinstream,0);
	//netlib实际是6字节表示,此处禁用4字节初步判断
	unsigned int netlib=(szPacket)?(*(unsigned int *)szPacket):0;
	if(netlib==0x00000010) // >=TDS7.3
		return 0x703;
	if(netlib==0x00000009) // >=TDS7.2
		return 0x702;
	return 0x701;
}
//解析回复packet获取执行结果,返回0执行成功，否则执行失败 (低2字节Token，高2字节bit_falgs)
unsigned int parse_reply(void * pinstream,unsigned short tdsver,unsigned long *nAffectRows)
{
	unsigned int iResult=0;
	unsigned short nsize=*(unsigned short *)((char *)pinstream+2); 
	IInputStream::htonb(nsize); //packet字节大小
	/*所有的回复，最后都是1字节标志+12字节的Process Done信息 (7.1及以下是8字节)
	INT16       INT16     INT32                  INT64
	+-----------+---------+----------------------+---------------------+
	| bit flags | unknown | row count (TDS 7.1-) | row count (TDS 7.2) |
	+-----------+---------+----------------------+---------------------+ */
	//登录package中包含tds版本，譬如 { 0x01, 0x00, 0x00, 0x71}表示TDS7.1 //see login.c tds7_send_login
	unsigned int done_offset=(tdsver==0 || IS_TDS72_PLUS(tdsver))?13:9; 
	unsigned char *ptr_done=(unsigned char *)pinstream+nsize-done_offset;
	unsigned char tds_token=*ptr_done;
	if(is_end_token(tds_token)){
		int bit_flags=*(unsigned short *)(ptr_done+1);
		if((bit_flags & TDS_DONE_CANCELLED) == 0 && (bit_flags & TDS_DONE_ERROR) == 0)
		{
			iResult=0;
			if(bit_flags==0x10 && nAffectRows!=NULL) //0x10 row count is valid
				*nAffectRows=*(unsigned int *)(ptr_done+5);
		}else iResult=tds_token+ ((bit_flags<<16) & 0xffff0000);
	}else iResult=tds_token;
	return iResult;
}

static 
void getstr_login(void * pinstream,unsigned char *szPacket,std::string &strret)
{
	unsigned short pos=*(unsigned short *)szPacket; szPacket+=sizeof(unsigned short);
	unsigned short len=*(unsigned short *)szPacket; szPacket+=sizeof(unsigned short);
	if(pos==0 || len==0) return;
	wchar_t *szText=(wchar_t * )((unsigned char *)pinstream+sizeof(tds_header)+pos);
	strret=WideToMultibyte(szText,len);
}
static 
void getpswd_login(void * pinstream,unsigned char *szPacket,std::string &strret)
{
	unsigned short pos=*(unsigned short *)szPacket; szPacket+=sizeof(unsigned short);
	unsigned short len=*(unsigned short *)szPacket; szPacket+=sizeof(unsigned short);
	if(pos==0 || len==0) return;
	wchar_t *szText=(wchar_t * )((unsigned char *)pinstream+sizeof(tds_header)+pos);
	std::wstring strText(szText,len);
	tds7_decrypt_pass((unsigned char *)szText,len*sizeof(wchar_t),(unsigned char *)strText.c_str());
	strret=WideToMultibyte(strText.c_str(),len);
}
static 
void setstr_login(void * pinstream,unsigned char *szPacket,const std::string &AnsiStr,unsigned short pos)
{
	*(unsigned short *)szPacket=pos; szPacket+=sizeof(unsigned short);
	*(unsigned short *)szPacket=AnsiStr.length(); szPacket+=sizeof(unsigned short);
	if(AnsiStr=="") return;

	std::wstring wstr=MultibyteToWide(AnsiStr);
	wchar_t *szText=(wchar_t * )((unsigned char *)pinstream+sizeof(tds_header)+pos);
	::memcpy(szText,wstr.c_str(),AnsiStr.length()*sizeof(wchar_t));
}
static 
void setpswd_login(void * pinstream,unsigned char *szPacket,const std::string &AnsiStr,unsigned short pos)
{
	*(unsigned short *)szPacket=pos; szPacket+=sizeof(unsigned short);
	*(unsigned short *)szPacket=AnsiStr.length(); szPacket+=sizeof(unsigned short);
	if(AnsiStr=="") return;

	std::wstring wstr=MultibyteToWide(AnsiStr);
	wchar_t *szText=(wchar_t * )((unsigned char *)pinstream+sizeof(tds_header)+pos);
	tds7_crypt_pass((const unsigned char *)wstr.c_str(),AnsiStr.length()*sizeof(wchar_t),(unsigned char *)szText);
}
//解析login packet信息
unsigned int parse_login_packet(void * pinstream,tds_login &login)
{
	unsigned char *szPacket=(unsigned char *)pinstream+sizeof(tds_header);
	login.tlen=*(unsigned int *)szPacket; szPacket+=sizeof(login.tlen);
	login.version =*(unsigned int *)szPacket; szPacket+=sizeof(login.version);
	login.psz =*(unsigned int *)szPacket; szPacket+=sizeof(login.psz);
	login.client_version =*(unsigned int *)szPacket; szPacket+=sizeof(login.client_version);
	login.client_pid =*(unsigned int *)szPacket; szPacket+=sizeof(login.client_pid);
	login.connection_id =*(unsigned int *)szPacket; szPacket+=sizeof(login.connection_id);
	login.opt_flag1=*szPacket++;
	login.opt_flag2=*szPacket++;
	login.sql_flag=*szPacket++;
	login.res_flag=*szPacket++;
	login.time_zone =*(unsigned int *)szPacket; szPacket+=sizeof(login.time_zone);
	login.collation =*(unsigned int *)szPacket; szPacket+=sizeof(login.collation);
	
	getstr_login(pinstream,szPacket,login.hostname); szPacket+=4;
	getstr_login(pinstream,szPacket,login.username); szPacket+=4;
	getpswd_login(pinstream,szPacket,login.userpswd); szPacket+=4;
	getstr_login(pinstream,szPacket,login.appname); szPacket+=4;
	getstr_login(pinstream,szPacket,login.servername); szPacket+=4;
	getstr_login(pinstream,szPacket,login.remotepair); szPacket+=4;
	getstr_login(pinstream,szPacket,login.libraryname); szPacket+=4;
	getstr_login(pinstream,szPacket,login.language); szPacket+=4;
	getstr_login(pinstream,szPacket,login.dbname); szPacket+=4;
	::memcpy(login.clt_mac,szPacket,sizeof(login.clt_mac)); szPacket+=sizeof(login.clt_mac);
	getstr_login(pinstream,szPacket,login.authnt); szPacket+=4;
	//后续跟着4字节 next position (same of total packet size) + 0 
	return login.tlen;
}
//返回login packet size
unsigned int write_login_packet(void * pinstream,tds_login &login)
{
	login.authnt="";
	unsigned char *szPacket=(unsigned char *)pinstream+sizeof(tds_header);
	unsigned short tdsv=GetUSVerByULVer(login.version);
	unsigned short pos_str=(IS_TDS72_PLUS(tdsv))?(86+8):86; //字符串写入的起始位置
	unsigned short usSize=pos_str;
	usSize+=login.hostname.length()*sizeof(wchar_t);
	usSize+=login.username.length()*sizeof(wchar_t);
	usSize+=login.userpswd.length()*sizeof(wchar_t);
	usSize+=login.appname.length()*sizeof(wchar_t);
	usSize+=login.servername.length()*sizeof(wchar_t);
	usSize+=4;//remotepair
	usSize+=login.libraryname.length()*sizeof(wchar_t);
	usSize+=login.language.length()*sizeof(wchar_t);
	usSize+=login.dbname.length()*sizeof(wchar_t);
	usSize+=login.authnt.length()*sizeof(wchar_t);
	if(pinstream==NULL) return  usSize; //仅仅返回Login Packet大小，以便分配空间
	
	login.tlen=usSize; //更新为实际Login Packet大小
	//开始写入字符串数据的位置，以及字符串数据总长度
	::memcpy(szPacket,&login.tlen,sizeof(login.tlen)); szPacket+=sizeof(login.tlen);
	::memcpy(szPacket,&login.version,sizeof(login.version)); szPacket+=sizeof(login.version);
	::memcpy(szPacket,&login.psz,sizeof(login.psz)); szPacket+=sizeof(login.psz);
	::memcpy(szPacket,&login.client_version,sizeof(login.client_version)); szPacket+=sizeof(login.client_version);
	::memcpy(szPacket,&login.client_pid,sizeof(login.client_pid)); szPacket+=sizeof(login.client_pid);
	::memcpy(szPacket,&login.connection_id,sizeof(login.connection_id)); szPacket+=sizeof(login.connection_id);
	*szPacket=login.opt_flag1; szPacket++;
	*szPacket=login.opt_flag2; szPacket++;
	*szPacket=login.sql_flag; szPacket++;
	*szPacket=login.res_flag; szPacket++;
	::memcpy(szPacket,&login.time_zone,sizeof(login.time_zone)); szPacket+=sizeof(login.time_zone);
	::memcpy(szPacket,&login.collation,sizeof(login.collation)); szPacket+=sizeof(login.collation);
	
	setstr_login(pinstream,szPacket,login.hostname,pos_str); szPacket+=4;
	pos_str+=login.hostname.length()*sizeof(wchar_t);
	setstr_login(pinstream,szPacket,login.username,pos_str); szPacket+=4;
	pos_str+=login.username.length()*sizeof(wchar_t);
	setpswd_login(pinstream,szPacket,login.userpswd,pos_str); szPacket+=4;
	pos_str+=login.userpswd.length()*sizeof(wchar_t);
	setstr_login(pinstream,szPacket,login.appname,pos_str); szPacket+=4;
	pos_str+=login.appname.length()*sizeof(wchar_t);
	setstr_login(pinstream,szPacket,login.servername,pos_str); szPacket+=4;
	pos_str+=login.servername.length()*sizeof(wchar_t);
	*(unsigned short *)szPacket=pos_str; //remotepair
	*(unsigned int *)(szPacket+2)=0; szPacket+=4; 
	
	setstr_login(pinstream,szPacket,login.libraryname,pos_str); szPacket+=4;
	pos_str+=login.libraryname.length()*sizeof(wchar_t);
	setstr_login(pinstream,szPacket,login.language,pos_str); szPacket+=4;
	pos_str+=login.language.length()*sizeof(wchar_t);
	setstr_login(pinstream,szPacket,login.dbname,pos_str); szPacket+=4;
	pos_str+=login.dbname.length()*sizeof(wchar_t);
	::memcpy(szPacket,login.clt_mac,sizeof(login.clt_mac)); szPacket+=sizeof(login.clt_mac);
	setstr_login(pinstream,szPacket,login.authnt,pos_str); szPacket+=4;
	pos_str+=login.authnt.length()*sizeof(wchar_t);
	*(unsigned short *)szPacket=pos_str; szPacket+=sizeof(unsigned short);
	*(unsigned short *)szPacket=0; szPacket+=sizeof(unsigned short);
	if(IS_TDS72_PLUS(tdsv)){ //后续8字节 
		*(unsigned short *)szPacket=pos_str; szPacket+=sizeof(unsigned short);
		*(unsigned short *)szPacket=0; szPacket+=sizeof(unsigned short);
		*(unsigned int *)szPacket=0; szPacket+=sizeof(unsigned int);
	}
	return pos_str; //返回login packet size
}
//采用新的登录帐号密码替换原始登录包中的帐号密码信息，生成新的登录数据包
unsigned int write_login_new(unsigned char *pInBuffer,unsigned int buflen,tds_login &login,const char *szName,const char *szUser,const char *szPswd)
{
	std::string dbuser=login.username,dbpswd=login.userpswd,dbname=login.dbname;
	if(szName!=NULL) login.dbname.assign(szName);
	if(szUser!=NULL) login.username.assign(szUser);
	if(szPswd!=NULL) login.userpswd.assign(szPswd);
	unsigned int NewSize=write_login_packet(NULL,login)+sizeof(tds_header);
	if(NewSize<buflen)
	{
		tds_header tdsh; tdsh.type =TDS7_LOGIN;
		tdsh.status =0x01; tdsh.size =NewSize; 
		tdsh.channel=0; tdsh.packet_number=tdsh.window=0;
		write_tds_header(pInBuffer,tdsh);
		write_login_packet(pInBuffer,login);
	}else NewSize=0;
	login.username=dbuser,login.userpswd=dbpswd,login.dbname=dbname;
	return NewSize;
}
//处理TDS_SP_CALLED信息,==0发生错误，否则返回解析处理字节数
//0xFFFF+INT16(调用编号)+INT16(Flags)+INT16+INT8(类型Type) + INT16(字节长度) +INT8[5](collate info TDS7.1+)+ INT16(字节长度)
unsigned int tds_sp_called(void * pinstream)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	
	//首先判断头2字节 rpc name长度
	unsigned short nameLen=*((unsigned short *)pstream);
	pstream+=sizeof(nameLen);

	if( nameLen==0xffff ) //nameLen=0xffff，则后续2字节为rpc过程编号
	{//后续SP为调用编号 2字节；0x000A为 TDS_SP_EXECUTESQL sp_executesql...
		unsigned short procid=*(unsigned short *)pstream; pstream+=sizeof(unsigned short);
		//后续为2字节的Flag； 0x1 1 recompile procedure (TDS 7.0+/TDS 5.0) , 0x02 no metadat (TDS 7.0+)
		pstream+=2;		//example 0x02 0x00
		//后续2字节含义不明
		pstream+=2;		//example 0x00 0x00
		//后续一字节为 TDS_SERVER_TYPE： /* 0xE7 */ XSYBNVARCHAR = 231
		unsigned char type=*pstream++;
		
		pstream+=2; //后续2字节query语句字节长度
		pstream+=5; //skip collate info ((TDS 7.0+)
		pstream+=2;//最后2字节也是后续query语句字节长度
	}else
	{//后续为调用哪个过程名称 unicode编码,譬如"sp_executesql"
	}
	return pstream-(unsigned char *)pinstream;
}
