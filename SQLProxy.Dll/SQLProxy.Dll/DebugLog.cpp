/*******************************************************************
   *	DebugLog.cpp
   *    DESCRIPTION:日志记录实现，用于程序输出打印各种信息
   *			可以输出到控制台，文件，窗口或DebugView
   *    AUTHOR:yyc
   *
   *    如果在工程里定义了禁止DebugLog输出，则编译时会自动忽略所有的Debug输出
   *
   *******************************************************************/


#include "stdafx.h"
#include <windows.h> //包含windows的头文件
//如果使用预编译头，则 stdafx.h必须放在第一行，否则报"fatal error C1020: 意外的 #endif"
//如果使用了预编译头...那么VC会在所有CPP文件里寻找 #include   "stdafx.h " 
//在找到这行代码之前 VC忽略所有代码... 

#ifndef __NO_DEBUGLOG__

#pragma warning(disable:4996)
#pragma warning(disable:4786)
#include "DebugLog.h"

#include <ctime>
#include <cstdarg>
#include <string>
#include <map>

#ifdef UNICODE
#define strlowcase _wcslwr
#define strcasecmp _wcsicmp
#define strncasecmp _wcsnicmp
#define vsnprintf _vsnwprintf
#define stringlen wcslen
#define strprintf swprintf
#define fileopen _wfopen
#else
#define strlowcase _strlwr
#define strcasecmp _stricmp
#define strncasecmp _strnicmp	
#define vsnprintf  _vsnprintf	//_vsnprintf_s
#define stringlen strlen
#define strprintf sprintf	//sprintf_s
#define fileopen fopen
#endif
#define getloctime localtime

extern const char * _PROJECTNAME_;
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);
DebugLogger DebugLogger::m_logger;//静态日志类实例化

//获取调试信息设置，优先从命令行参数获取，否则从系统环境变量获取
//[M=<模块> L=<级别> [T=<view|file>] [FILE=<输出到指定的文件>] [PATH=<指明日志默认输出路径>]
bool GetDebugLogSetting(char *szBuf,int buflen)
{
	const char * szCmd=::GetCommandLine();
	if(szCmd!=NULL){
		const char *ptr=strstr(szCmd," -DEBUGLOG");
		if(ptr!=NULL){
			const char *ptrb=NULL,*ptre=NULL;
			ptr+=10;
			while(true){
				char c=*ptr++;
				if(c==0) break;
				if(c!='\'') continue;
				ptrb=ptr; break;
			}
			ptr=ptrb;
			while(ptrb!=NULL){
				char c=*ptr++;
				if(c==0) break;
				if(c!='\'') continue;
				ptre=ptr-1; break;
			}
			if(ptrb && ptre && ptre>ptrb)
			{
				strncpy(szBuf,ptrb,ptre-ptrb);
				return true;
			}
			return false;
		}
	}//?if(szCmd!=NULL)
	
	//否则获取环境变量设置的调试信息输出参数
	//根据系统环境变量DEBUGLOG决定是否进行调试输出
	buflen=::GetEnvironmentVariable("DEBUGLOG",szBuf,buflen-1);
	if(buflen<=0) return false;
	szBuf[buflen]=0; return true;
}
DebugLogger::DebugLogger()
{
	const char *szModName=_PROJECTNAME_;
	m_hout=(INTPTR)stdout;
	m_logtype=LOGTYPE_NONE;
	m_loglevel=LOGLEVEL_WARN;
	m_bOutputTime=false;
	m_fileopenType[0]='w';
	m_fileopenType[1]='b';
	m_fileopenType[2]=m_fileopenType[3]=0;
	
	char buf[256]; int buflen=256;
	if( !GetDebugLogSetting(buf,256))
		return; //根据系统环境变量DEBUGLOG决定是否进行调试输出
	std::map<std::string,std::string> maps;
	if(SplitString(buf,' ',maps)<=0) return;
	std::map<std::string,std::string>::iterator it;
	
	//[M=<模块> L=<级别> [T=<view|file>] [FILE=<输出到指定的文件>] [PATH=<指明日志默认输出路径>]
	if( (it=maps.find("m"))!=maps.end()) //M=日志输出模块
	{//是否指定仅仅输出此模块的日志信息
		if( (*it).second!="" && 
			strcasecmp((*it).second.c_str(),szModName)!=0)
			return; //此模块不输出调试日志
	}

	if( (it=maps.find("l"))!=maps.end())  //L=日志输出级别
	{//设置日志输出级别
		m_logtype=LOGTYPE_DVIEW;
		int l=atoi((*it).second.c_str());
		if(l>=LOGLEVEL_DEBUG && l<=LOGLEVEL_NONE)
			m_loglevel=(LOGLEVEL)l;
	}
	if( (it=maps.find("t"))!=maps.end())  //T=日志输出类型
	{//设置日志输出形式
		if(strcasecmp((*it).second.c_str(),"view")==0)
			m_logtype=LOGTYPE_DVIEW;
		else if(strcasecmp((*it).second.c_str(),"file")==0)
			m_logtype=LOGTYPE_FILE;
	}
	std::string strFileName,strPath;
	if( (it=maps.find("path"))!=maps.end() )  //PATH=日志输出到指定的目录
	{
		strPath=(*it).second;
		if(strPath!="" && strPath[strPath.length()-1]!='\\') 
			strPath.append("\\");
	}
	if( (it=maps.find("file"))!=maps.end() )  //FILE=日志输出到指定的文件
		strFileName=(*it).second;
	if( m_logtype==LOGTYPE_FILE || strFileName!="")	
	{//设置日志输出到文件
		m_logtype=LOGTYPE_FILE;
		if(strFileName==""){ //随机文件名
			time_t tNow=time(NULL);
			struct tm * ltime=getloctime(&tNow);
			strprintf(buf,"%sDLog_%04d%02d%02d%02d%02d%02d_%s.txt",
				strPath.c_str(),
				(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, 
				ltime->tm_hour, ltime->tm_min, ltime->tm_sec,
				szModName); //必须初始化随机种子，否则没意义(都是同一值) rand()
			strFileName.assign(buf);						
		}else if(strstr(strFileName.c_str(),":\\")==NULL) //指定相对输出文件名
			strFileName=strPath+strFileName;	
		m_hout=(INTPTR)fileopen(strFileName.c_str(),m_fileopenType);
	}
	
	m_hMutex = CreateMutex(NULL, false, NULL);
}

DebugLogger::~DebugLogger()
{
	if(m_logtype==LOGTYPE_FILE && m_hout) ::fclose((FILE *)m_hout);
	if(m_hMutex!=NULL) CloseHandle(m_hMutex);

}

LOGTYPE DebugLogger::setLogType(LOGTYPE lt,INTPTR lParam)
{
	if(m_logtype==LOGTYPE_FILE && m_hout) ::fclose((FILE *)m_hout);
	m_hout=0; m_logtype=LOGTYPE_NONE;
	switch(lt)
	{
		case LOGTYPE_STDOUT:
			m_hout=(INTPTR)stdout;
			m_logtype=LOGTYPE_STDOUT;
			break;
		case LOGTYPE_FILE:
			if( (m_hout=(INTPTR)fileopen((LPCTSTR)lParam,m_fileopenType)) )
				m_logtype=LOGTYPE_FILE;
			break;
		case LOGTYPE_HWND:
			m_hout=lParam;
			m_logtype=LOGTYPE_HWND;
			break;
		case LOGTYPE_DVIEW:
			m_hout=1;
			m_logtype=LOGTYPE_DVIEW;
			break;
	}//?switch
	return m_logtype;
}

void DebugLogger::printTime(std::ostream &os)
{
	time_t tNow=time(NULL);
	struct tm * ltime=localtime(&tNow);
	TCHAR buf[64];
	size_t len=strprintf(buf,TEXT("[%04d-%02d-%02d %02d:%02d:%02d] - \r\n"),
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour,
		ltime->tm_min, ltime->tm_sec);
	buf[len]=0;
	os<<buf; return;
}
void DebugLogger::dump(std::ostream &os,size_t len,LPCTSTR buf)
{
	if(buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime(os);//打印时间
	if(len>0) os<<buf;
}
void DebugLogger::dump(std::ostream &os,LPCTSTR fmt,...)
{
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	va_end(args);
	if(len==-1){//写入数据超过了给定的缓冲区空间大小
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime(os);//打印时间
	if(len>0) os<<buffer;
}

void DebugLogger::dumpBinary(std::ostream &os,const char *buf,size_t len)
{
	if(buf==NULL || len==0) return;
	if(m_bOutputTime) printTime(os);//打印时间
	
	char szBuffer[128]; int lBufLen=0;
	int i,j,lines=(len+15)/16; //计算共多少行
	size_t count=0;//打印字符计数
	sprintf(szBuffer,"Output Binary data,size=%d, lines=%d\r\n",len,lines);
	os<<szBuffer;  lBufLen=0;
	for(i=0;i<lines;i++)
	{
		count=i*16;
		for(j=0;j<16;j++)
		{
			if((count+j)<len)
				lBufLen+=sprintf(szBuffer+lBufLen,"%02X ",(unsigned char)buf[count+j]);
			else
				lBufLen+=sprintf(szBuffer+lBufLen,"   ");
		}
		lBufLen+=sprintf(szBuffer+lBufLen,"; ");
		for(j=0;j<16 && (count+j)<len;j++)
		{
			char c=(char)buf[count+j];
			if(c<32 || c>=127) c='.';
			lBufLen+=sprintf(szBuffer+lBufLen,"%c",c);
		}
		lBufLen+=sprintf(szBuffer+lBufLen,"\r\n");
		os<<szBuffer;  lBufLen=0;
	}//?for(...
	return;
}
void DebugLogger::printTime()
{
	if(m_logtype==LOGTYPE_NONE) return; 
	time_t tNow=time(NULL);
	struct tm * ltime=localtime(&tNow);
	TCHAR buf[64];
	size_t len=strprintf(buf,TEXT("[%04d-%02d-%02d %02d:%02d:%02d] - \r\n"),
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, 
		ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
	buf[len]=0;
	_dump(buf,len); return;
}

void DebugLogger::dump(LOGLEVEL ll,size_t len,LPCTSTR buf)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//此级别的日志不输出
	if(m_hout==0 || buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime();//打印时间
	if(len>0) _dump(buf,len);
}

void DebugLogger::dump(LOGLEVEL ll,LPCTSTR fmt,...)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//此级别的日志不输出
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	//int len=_vsnprintf_s(buffer,1024,_TRUNCATE,fmt,args);
	va_end(args);
	if(len==-1){//写入数据超过了给定的缓冲区空间大小
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime();//打印时间
	if(len>0) _dump(buffer,len);
	return;
}

void DebugLogger::debug(size_t len,LPCTSTR buf)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(LOGLEVEL_DEBUG<m_loglevel) return;//此级别的日志不输出
	if(m_hout==0 || buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime();//打印时间
	if(len>0) _dump(buf,len);
	return;
}
void DebugLogger::debug(LPCTSTR fmt,...)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(LOGLEVEL_DEBUG<m_loglevel) return;//此级别的日志不输出
	if(m_hout==0) return;
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	//int len=_vsnprintf_s(buffer,1024,_TRUNCATE,fmt,args);
	va_end(args);
	if(len==-1){//写入数据超过了给定的缓冲区空间大小
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime();//打印时间
	if(len>0) _dump(buffer,len);
	return;
}

inline void DebugLogger::_dump(LPCTSTR buf,size_t len)
{
	if(m_logtype==LOGTYPE_NONE) return;
	WaitForSingleObject(m_hMutex, INFINITE);
	switch(m_logtype)
	{
		case LOGTYPE_STDOUT:
#ifdef _DEBUG
			printf("%s",buf);
#else
			fwrite((const void *)buf,sizeof(TCHAR),len,stdout);
#endif
			break;
		case LOGTYPE_FILE:
			fwrite((const void *)buf,sizeof(TCHAR),len,(FILE *)m_hout);
			break;
		case LOGTYPE_HWND:
#ifdef WIN32
		{
			int end = ::GetWindowTextLength((HWND)m_hout);
			::SendMessage((HWND)m_hout, EM_SETSEL, (WPARAM)end, (LPARAM)end);
			TCHAR c=*(buf+len); 
			if(c!=0) *(LPTSTR)(buf+len)=0;
			::SendMessage((HWND)m_hout, EM_REPLACESEL, 0, (LPARAM)buf);
			if(c!=0) *(LPTSTR)(buf+len)=c;
		}
#endif
			break;
		case LOGTYPE_DVIEW:
			OutputDebugString(buf);
			break;
	}//?switch
	ReleaseMutex(m_hMutex);
}

void DebugLogger::dumpBinary(LOGLEVEL ll,const char *buf,size_t len)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//此级别的日志不输出
	if(buf==NULL || len==0) return;

	if(m_bOutputTime) printTime();//打印时间
	
	int i,j,lines=(len+15)/16; //计算共多少行
	size_t count=0;//打印字符计数
	printf("Output Binary data,size=%d, lines=%d\r\n",len,lines);
	for(i=0;i<lines;i++)
	{
		count=i*16;
		for(j=0;j<16;j++)
		{
			if((count+j)<len)
				printf("%02X ",(unsigned char)buf[count+j]);
			else
				printf("   ");
		}
		printf("; ");
		for(j=0;j<16 && (count+j)<len;j++)
		{
			char c=(char)buf[count+j];
			if(c<32 || c>=127) c='.';
			printf("%c",c);
		}
		printf("\r\n");
	}//?for(...
	return;
}

int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps)
{
	if(str==NULL) return 0;
	while(*str==' ') str++;//删除前导空格
	const char *ptr,*ptrStart,*ptrEnd;
	while( (ptr=strchr(str,'=')) )
	{
		char dm=delm; ptrStart=ptr+1;
		if(*ptrStart=='"') {dm='"'; ptrStart++; }
		ptrEnd=ptrStart;
		while(*ptrEnd && *ptrEnd!=dm) ptrEnd++;

		*(char *)ptr=0;
		strlowcase((char *)str);
		maps[str]=std::string(ptrStart,ptrEnd-ptrStart);
		*(char *)ptr='=';

		if(*ptrEnd==0) break;
		str=ptrEnd+1;
		while(*str==' ') str++;//删除前导空格
	}//?while(ptr)
	
	return maps.size();
}
#endif