/*
**	FILENAME			DebugLog.h
**
**	PURPOSE				调试信息输出，可输出到指定的窗体或用DebugView查看
**						
*/

#ifndef __DEBUGLOG_H__
#define __DEBUGLOG_H__
#include <iostream>
#include "x64_86_def.h" //INTPTR定义

#ifndef TCHAR
#define LPCTSTR const char *
#define LPTSTR char *
#define TCHAR char
#endif

typedef enum //输出调试信息级别
{
	LOGLEVEL_DEBUG=0,//调试信息
	LOGLEVEL_WARN,//输出警告信息
	LOGLEVEL_INFO,//输出用户记录信息
	LOGLEVEL_ERROR,//输出错误信息
	LOGLEVEL_FATAL, //输出致命错误信息
	LOGLEVEL_NONE	//禁止输出
}LOGLEVEL;
typedef enum //日志输出类型
{
	LOGTYPE_NONE,
	LOGTYPE_STDOUT,//输出到标准输出设备上
	LOGTYPE_FILE,//输出到文件
	LOGTYPE_HWND,//输出到一个windows的窗体上
	LOGTYPE_DVIEW //输出到DebugView
}LOGTYPE;

//如果在工程里定义了禁止DebugLog输出，则编译时会自动忽略所有的Debug输出
#ifndef __NO_DEBUGLOG__
class DebugLogger
{
	HANDLE m_hMutex;
	LOGTYPE m_logtype;//日志输出类型
	LOGLEVEL m_loglevel;//日志级别
	INTPTR m_hout;//输出句柄，对于LOGTYPE_FILE类型对应FILE *,LOGTYPE_HWND类型对应HWND
	TCHAR m_fileopenType[4]; //默认未覆盖打开方式"w"
	bool m_bOutputTime;//是否打印输出时间
	
	static DebugLogger m_logger;//静态日志类实例
	DebugLogger(); //不允许创建此类实例
		
	void _dump(LPCTSTR buf,size_t len);
public:
	~DebugLogger();
	static DebugLogger & getInstance(){ return m_logger; }
	//设置是否打印时间
	void setPrintTime(bool bPrint){ m_bOutputTime=bPrint; return; }
	LOGLEVEL setLogLevel(LOGLEVEL ll){ 
		LOGLEVEL lOld=m_loglevel; m_loglevel=ll; return lOld; }
	//设置日志文件打开方式
	void setOpenfileType(TCHAR c) { m_fileopenType[0]=c; return; }
	//是否允许输出指定级别的日志
	bool ifOutPutLog(LOGLEVEL ll) { return ( (unsigned int)m_loglevel<=(unsigned int)ll ); }
	LOGTYPE LogType() { return m_logtype; }
	LOGTYPE setLogType(LOGTYPE lt,INTPTR lParam);
	void flush(){ 
		if(m_logtype==LOGTYPE_FILE && m_hout)
			::fflush((FILE *)m_hout); return;}
	void dump(LOGLEVEL ll,LPCTSTR fmt,...);
	void dump(LOGLEVEL ll,size_t len,LPCTSTR buf);
	//输出DEBUG级别的日志
	void debug(LPCTSTR fmt,...);
	void debug(size_t len,LPCTSTR buf);
	void dumpBinary(LOGLEVEL ll,const char *buf,size_t len);
	void printTime(); //打印当前时间
	void printTime(std::ostream &os);
	void dump(std::ostream &os,LPCTSTR fmt,...);
	void dump(std::ostream &os,size_t len,LPCTSTR buf);
	void dumpBinary(std::ostream &os,const char *buf,size_t len);
};

//设置在打印消息前先打印时间
#define RW_LOG_SETPRINTTIME(b) \
{ \
	DebugLogger::getInstance().setPrintTime(b); \
}
//设置日志输出级别
#define RW_LOG_SETLOGLEVEL(ll) DebugLogger::getInstance().setLogLevel(ll);

//设置日志输出到指定的文件
#define RW_LOG_SETFILE(filename) \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_FILE,filename); \
}
//设置文件日志为追加方式，而不是覆盖方式
#define RW_LOG_OPENFILE_APPEND() \
{ \
	DebugLogger::getInstance().setOpenfileType('a'); \
}
//设置日志输出到指定的窗口
#define RW_LOG_SETHWND(hWnd) \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_HWND,hWnd); \
}
//设置日志输出到DebugView
#define RW_LOG_SETDVIEW() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_DVIEW,0); \
}
//设置日志输出到标准输出设备stdout
#define RW_LOG_SETSTDOUT() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_STDOUT,0); \
}
//不输出日志
#define RW_LOG_SETNONE() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_NONE,0); \
}

#define RW_LOG_FFLUSH() \
{ \
	DebugLogger::getInstance().flush(); \
}

#define RW_LOG_LOGTYPE DebugLogger::getInstance().LogType
//检查是否允许输出指定级别的日志
#define RW_LOG_CHECK DebugLogger::getInstance().ifOutPutLog
//日志输出
#define RW_LOG_PRINT DebugLogger::getInstance().dump
#define RW_LOG_PRINTBINARY DebugLogger::getInstance().dumpBinary
//打印当前时间
#define RW_LOG_PRINTTIME DebugLogger::getInstance().printTime

#define RW_LOG_DEBUG if(DebugLogger::getInstance().ifOutPutLog(LOGLEVEL_DEBUG)) DebugLogger::getInstance().debug

//如果在工程里定义了禁止DebugLog输出，则编译时会自动忽略所有的Debug输出
#else //#ifndef __NO_DEBUGLOG__

#define NULL_OP		__noop			//NULL
#define RW_LOG_SETPRINTTIME(b)		NULL_OP
#define RW_LOG_SETLOGLEVEL(ll)		NULL_OP
#define RW_LOG_SETFILE(filename)	NULL_OP
#define RW_LOG_OPENFILE_APPEND()	NULL_OP
#define RW_LOG_SETHWND(hWnd)		NULL_OP
#define RW_LOG_SETDVIEW()			NULL_OP
#define RW_LOG_SETSTDOUT()			NULL_OP
#define RW_LOG_SETNONE()			NULL_OP
#define RW_LOG_FFLUSH()				NULL_OP
#define RW_LOG_LOGTYPE				NULL_OP
#define RW_LOG_PRINT				NULL_OP
#define RW_LOG_CHECK				NULL_OP
#define RW_LOG_PRINTBINARY			NULL_OP
#define RW_LOG_PRINTTIME			NULL_OP
#define RW_LOG_DEBUG				NULL_OP

#endif //#ifndef __NO_DEBUGLOG__ 

#endif

