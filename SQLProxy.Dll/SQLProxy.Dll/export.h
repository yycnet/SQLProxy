// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 DCMDIR_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何其他项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// DCMDIR_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef SQLPROXYDLL_EXPORTS
#define SQLPROXY_API __declspec(dllexport)
#else
#define SQLPROXY_API __declspec(dllimport)
#endif
#include "x64_86_def.h"	//INTPTR定义

extern "C"
SQLPROXY_API void _stdcall SetLogLevel(HWND hWnd,int LogLevel);
//数据库连通性测试,返回0 成功 1 无效的IP或域名 2、主机不可达 3 服务不可用 4帐号密码错误 5数据库名错误
extern "C"
SQLPROXY_API int _stdcall DBTest(const char  *szHost,int iPort,const char *szUser,const char *szPswd,const char *dbname);

//设置集群数据库信息、启动运行参数以及SQL打印输出过滤参数
extern "C"
SQLPROXY_API void _stdcall SetParameters(int SetID,const char *szParam);
//获取后台某集群SQL服务状态 idx==-1获取SQL集群服务个数
//proid==0SQL服务是否运行正常 ==1返回SQL服务当前并发客户连接数 ==3返回SQL服务当前负载处理量 ==4写入处理量 
extern "C"
SQLPROXY_API INTPTR _stdcall GetDBStatus(int idx,int proid);

//启动SQLProxy代理服务
extern "C"
SQLPROXY_API bool _stdcall StartProxy(int iport);
extern "C"
SQLPROXY_API void _stdcall StopProxy();

extern "C"
SQLPROXY_API int _stdcall ProxyCall(int callID,INTPTR lParam);
