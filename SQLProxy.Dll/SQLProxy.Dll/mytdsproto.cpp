
#include "stdafx.h"
#pragma warning(disable:4996)

#include "mytds.h"
#include "mytdsproto.h"
#include "DebugLog.h"

typedef struct _TDS_SERVER_TYPE_NAME
{
	unsigned char type_code;
	wchar_t type_name[16];
}TDS_SERVER_TYPE_NAME;
static TDS_SERVER_TYPE_NAME tds_server_type_name[]=
{
	{XSYBNVARCHAR,L"nvarchar"},{SYBNVARCHAR,L"nvarchar"},{XSYBNCHAR,L"nchar"},
	{XSYBVARCHAR,L"varchar"},{XSYBCHAR,L"char"},
	{SYBNTEXT,L"ntext"},{SYBTEXT,L"text"},
	{SYBMSXML,L"xml"},{SYBBITN,L"bit"},
	{XSYBVARBINARY,L"varbinary"},{XSYBBINARY,L"binary"},{SYBVARBINARY,L"varbinary"},{SYBBINARY,L"binary"},
	{SYBINTN,L"int"},{SYBINT8,L"tinyint"},{SYBINT1,L"tinyint"},{SYBINT2,L"smallint"},{SYBINT4,L"int"},{SYBINT8,L"bigint"},
	{SYBREAL,L"real"},{SYBFLT8,L""},{SYBMONEY,L"money"},{SYBMONEY4,L"smallmoney"},{SYBFLTN,L"float"},
	{SYBDATETIMN,L"datetime"},{SYBDATETIME,L"datetime"},
	{0,L"unknowed"}
};

wchar_t *get_typenamefromcode(unsigned char type_code)
{
	TDS_SERVER_TYPE_NAME *ptr_namecode=tds_server_type_name;
	while(ptr_namecode->type_code!=0)
	{
		if(ptr_namecode->type_code==type_code)
			return ptr_namecode->type_name;
		ptr_namecode++;
	}
	return NULL;
}
unsigned char get_typecodefromname(const wchar_t * type_name)
{
	if(type_name==NULL || type_name[0]==0)
		return 0;
	TDS_SERVER_TYPE_NAME *ptr_namecode=tds_server_type_name;
	while(ptr_namecode->type_code!=0)
	{
		if(wcsicmp(ptr_namecode->type_name,type_name)==0)
			return ptr_namecode->type_code;
		ptr_namecode++;
	}
	return NULL;
}


/**
 * Convert from db date format to a structured date format
 * @param datetype source date type. SYBDATETIME or SYBDATETIME4
 * @param di       source date
 * @param dr       destination date
 * @return TDS_FAIL or TDS_SUCCESS
 */
static TDSRET
tds_datecrack(int datetype, const void *di, TDSDATEREC * dr)
{
	int dt_days;
	unsigned int dt_time;

	int years, months, days, ydays, wday, hours, mins, secs, dms;
	int l, n, i, j;

	memset(dr, 0, sizeof(*dr));

	if (datetype == SYBMSDATE || datetype == SYBMSTIME 
	    || datetype == SYBMSDATETIME2 || datetype == SYBMSDATETIMEOFFSET) {
		const TDS_DATETIMEALL *dta = (const TDS_DATETIMEALL *) di;
		dt_days = (datetype == SYBMSTIME) ? 0 : dta->date;
		if (datetype == SYBMSDATE) {
			dms = 0;
			secs = 0;
			dt_time = 0;
		} else {
			dms = dta->time % 10000000u;
			dt_time = dta->time / 10000000u;
			secs = dt_time % 60;
			dt_time = dt_time / 60;
		}
		if (datetype == SYBMSDATETIMEOFFSET) {
			--dt_days;
			dt_time = dt_time + 86400 + dta->offset;
			dt_days += dt_time / 86400;
			dt_time %= 86400;
		}
	} else if (datetype == SYBDATETIME) {
		const TDS_DATETIME *dt = (const TDS_DATETIME *) di;
		dt_time = dt->dttime;
		dms = ((dt_time % 300) * 1000 + 150) / 300 * 10000u;
		dt_time = dt_time / 300;
		secs = dt_time % 60;
		dt_time = dt_time / 60;
		dt_days = dt->dtdays;
	} else if (datetype == SYBDATETIME4) {
		const TDS_DATETIME4 *dt4 = (const TDS_DATETIME4 *) di;
		secs = 0;
		dms = 0;
		dt_days = dt4->days;
		dt_time = dt4->minutes;
	} else
		return TDS_FAIL;

	/*
	 * -53690 is minimun  (1753-1-1) (Gregorian calendar start in 1732) 
	 * 2958463 is maximun (9999-12-31)
	 */
	l = dt_days + (146038 + 146097*4);
	wday = (l + 4) % 7;
	n = (4 * l) / 146097;	/* n century */
	l = l - (146097 * n + 3) / 4;	/* days from xx00-02-28 (y-m-d) */
	i = (4000 * (l + 1)) / 1461001;	/* years from xx00-02-28 */
	l = l - (1461 * i) / 4;	/* year days from xx00-02-28 */
	ydays = l >= 306 ? l - 305 : l + 60;
	l += 31;
	j = (80 * l) / 2447;
	days = l - (2447 * j) / 80;
	l = j / 11;
	months = j + 1 - 12 * l;
	years = 100 * (n - 1) + i + l;
	if (l == 0 && (years & 3) == 0 && (years % 100 != 0 || years % 400 == 0))
		++ydays;

	hours = dt_time / 60;
	mins = dt_time % 60;

	dr->year = years;
	dr->month = months;
	dr->quarter = months / 3;
	dr->day = days;
	dr->dayofyear = ydays;
	dr->week = -1;
	dr->weekday = wday;
	dr->hour = hours;
	dr->minute = mins;
	dr->second = secs;
	dr->decimicrosecond = dms;
	return TDS_SUCCESS;
}


//打印输出unicode字符串,iLen: UCS字符串长度 (含有中文，不能直接S%输出，双字节和unicode不是一回事???)
void printUnicodeStr(const wchar_t *szText,int iLen,std::ostream &os ,bool bCRLF)
{
	int iBufLen=iLen*sizeof(short); 
	char *szBuf=new char[iBufLen+3]; //"\r\n\0"
	if(szBuf==NULL) return;

	int lret=WideCharToMultiByte(CP_ACP,WC_COMPOSITECHECK|WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR,
					(wchar_t *)szText,iLen,szBuf,iBufLen,NULL,NULL);
	if(bCRLF){
		szBuf[lret]='\r'; szBuf[++lret]='\n'; szBuf[++lret]=0;}
	else szBuf[lret]=0;
	RW_LOG_PRINT(os,lret,szBuf);
	delete[] szBuf;
}
void printUnicodeStr(const wchar_t *szText,int iLen,int ll ,bool bCRLF)
{
	int iBufLen=iLen*sizeof(short); 
	char *szBuf=new char[iBufLen+3]; //"\r\n\0"
	if(szBuf==NULL) return;

	int lret=WideCharToMultiByte(CP_ACP,WC_COMPOSITECHECK|WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR,
					(wchar_t *)szText,iLen,szBuf,iBufLen,NULL,NULL);
	if(bCRLF){
		szBuf[lret]='\r'; szBuf[++lret]='\n'; szBuf[++lret]=0;}
	else szBuf[lret]=0;
	RW_LOG_PRINT((LOGLEVEL)ll,lret,szBuf);
	delete[] szBuf;
}

//解析sql参数定义信息,==0发生错误，否则返回解析处理字节数
//sql参数定义为UCS字符串类型,格式: @参数名 类型(长度) 或 @参数名 类型
//nSize - pinstream指向的UCS字符串长度，==0则直到空结束符'\0\0'
unsigned int parse_sqlparam_def(void * pinstream,unsigned int nSize,tds_sql_param &param)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	
	unsigned int nCount=0;
	wchar_t *ptr_b=(wchar_t *)pstream;
	wchar_t *ptr_e=(wchar_t *)pstream;
	while(nCount<nSize && *ptr_e!=0)
	{
		if(*ptr_e==L' ')
		{
			int n=(ptr_e-ptr_b);
			param.name.assign(ptr_b,n);
			ptr_b=ptr_e+1;
		}else if(*ptr_e==L'(')
		{
			int n=(ptr_e-ptr_b);
			param.type_name.assign(ptr_b,n);
			ptr_b=ptr_e+1;
		}else if(*ptr_e==L')')
		{//括号中内容，可能为max，譬如 @a nvarchar(max)
			int w_len=0;
			swscanf(ptr_b,L"%d",&w_len);
			param.def_len=(w_len>0)?w_len:0xffff;
			break; //处理完毕
		}
		ptr_e++; nCount++;
	}//?while
	
	if(param.name[0]==0) return 0;
	if(param.type_name[0]==0) //可能的格式 @b1 int
		param.type_name.assign(ptr_b,(ptr_e-ptr_b) );
	param.type_code =get_typecodefromname(param.type_name.c_str());

	return (*ptr_e!=0)?nSize*sizeof(wchar_t):
			((unsigned char *)ptr_e-(unsigned char *)pinstream);
}
//解析sql参数值信息 ==0发生错误，否则返回解析处理字节数
/*格式： INT8(参数名长度) + 参数名(UCS) + INT8(Flags) + INT8(参数类型Type) + INT8(定义字节长度) + INT8(option)+
//		 INT8[5](collate info TDS7.1+) + INT8( TDS 5.0 ??? )
//	Flags: 0x1 TDS_RPC_OUTPUT output parameter
           0x2 TDS_RPC_NODEF  output parameter has no default value. 
                                 Valid only with TDS_RPC_OUTPUT.
    Type:	0xE7 XSYBNVARCHAR
			0x26 SYBINTN
			0x6f SYBDATETIMN
	collate info: 头两字节为windows codepage
*/
unsigned int parse_sqlparam_val(void * pinstream,tds_sql_param &param)
{
	unsigned char *pstream=(unsigned char *)pinstream;
	
	unsigned char nameLen=*pstream++;
	param.name.assign((wchar_t *)pstream,(unsigned int)nameLen);
	pstream+=nameLen*sizeof(wchar_t);
	
	unsigned char flags=*pstream++;	//一般为00，含义未知
	unsigned char type=*pstream++;
	unsigned int realLen,defLen; //realLen参数值的字节长度，defLen参数定义的字符长度
	
	if( 
		type==XSYBNVARCHAR ||	//0xE7
		type==XSYBNCHAR || 		//0xEF
		type==SYBNVARCHAR || 	//0x67
		type==XSYBVARCHAR ||	//0xA7
		type==XSYBCHAR )		//0xAF
	{//INT8(Flags) + INT8(参数类型Type) + INT16(定义字节长度) +INT8[5](collate info TDS7.1+) + INT16(实际字节长度)
		defLen=*(unsigned short *)pstream; pstream+=sizeof(unsigned short);
		//如果defLen==0xFFFF,则定义长度为max；对于nvarchar或nchar 应表述为字符长度，因此defLen=字节长度/2
		if(defLen!=0xFFFF && (type==XSYBNVARCHAR ||type==XSYBNCHAR ||type==SYBNVARCHAR) ) 
			defLen=defLen>>1;
		
		pstream+=5; //skip collate info
		realLen=*(unsigned short *)pstream; pstream+=sizeof(unsigned short);

		//????? 测试发现某参数定义长度为max，真实长度为0，但后面确跟着10个0
		if(defLen==0xFFFF && realLen==0) 
			pstream+=0x0A; //跳过10个字节
	}else if(
		type==SYBTEXT || 
		type==SYBNTEXT			//0x23
		)
	{//INT8(Flags) + INT8(参数类型Type) + INT32(定义字节长度) +INT8[5](collate info TDS7.1+) + INT32(实际字节长度)
		defLen=*(unsigned int *)pstream; pstream+=sizeof(unsigned int);
		//如果defLen==0xFFFFFFFF,则定义长度为max；对于NTEXT 应表述为字符长度，因此defLen=字节长度/2
		if(defLen!=0xFFFFFFFF && type==SYBNTEXT) defLen=defLen>>1;

		pstream+=5; //skip collate info
		realLen=*(unsigned int *)pstream; pstream+=sizeof(unsigned int);
	}
	else if( 
		type==SYBINTN ||		//0x26
		type==SYBINT1 || type==SYBINT2 || type==SYBINT4 || type==SYBINT8 || 
		type==SYBDATETIMN || SYBDATETIME4 || SYBDATETIME || 	//6F
		type==SYBFLTN || type==SYBREAL || 	//6D (8字节) 3B(4字节)
		type==SYBMONEY || type==SYBMONEY4 
		)
	{// INT8(Flags) + INT8(参数类型Type) + INT8(定义字节长度) + INT8(option)
		defLen=*pstream++;	//1字节类型长度
		pstream++;			//skip 1byte (一般此字节和上一字节值一致)
		realLen=defLen;		//实际长度为类型长度
	}else if(type==SYBBITN) //one bit
	{// INT8(Flags) + INT8(参数类型Type) + INT8(定义字节长度) + INT8(option)
		defLen=*pstream++;	//1字节类型长度,此值始终应为1
		pstream++;			//skip 1byte (一般此字节和上一字节值一致)
		realLen=defLen;		//实际长度为类型长度(固定1字节，但实际只有最低1位有效)
	}
	else
	{
		wchar_t *ptr_typename=get_typenamefromcode(type);
		printUnicodeStr(param.name.c_str(),param.name.length(),LOGLEVEL_ERROR,false);
		RW_LOG_PRINT(LOGLEVEL_ERROR," 未处理参数类型 0x%02X %S\r\n",type,((ptr_typename!=NULL)?ptr_typename:L"") );
		RW_LOG_PRINTBINARY(LOGLEVEL_ERROR,(const char *)pstream-2,32);
		return 0;
	}
	 
	char *ptr_tmp=new char[realLen+2]; //nchar的空结束符为2字节0000
	if(ptr_tmp!=NULL){
		if(realLen!=0) ::memcpy(ptr_tmp,pstream,realLen);
		ptr_tmp[realLen]=ptr_tmp[realLen+1]=0;
	}
	pstream+=realLen; //skip 参数值长度的字节

	param.ptr_val=ptr_tmp;
	param.real_len=realLen;
	param.def_len =defLen;	
	param.type_code=type;

	return pstream-(unsigned char *)pinstream;
}
//输出打印SQL Param信息; @参数名 参数类型(长度) 值\r\n
void print_sqlparam(tds_sql_param &param,std::ostream &os)
{
	printUnicodeStr(param.name.c_str(),param.name.length(),os,false);
	RW_LOG_PRINT(os,1," ");
	const wchar_t *ptr_typename=NULL; int len_typename=0;
	if(param.type_name[0]!=0){
		ptr_typename=param.type_name.c_str();
		len_typename=param.type_name.length();
	}else{
		ptr_typename=get_typenamefromcode(param.type_code);
		if(ptr_typename!=NULL) len_typename=wcslen(ptr_typename);
	}

	if(ptr_typename!=NULL)
		printUnicodeStr(ptr_typename,len_typename,os,false);
	else
		RW_LOG_PRINT(os,"[0x%02X]",param.type_code);
	
	unsigned char type=param.type_code;
	if( type==XSYBNVARCHAR ||	//E7
		type==XSYBNCHAR || 		//EF
		type==SYBNVARCHAR || 	//67
		type==SYBNTEXT
		)
	{
		RW_LOG_PRINT(os,"(%d) ",param.def_len);
		if(param.ptr_val!=NULL)
			printUnicodeStr((const wchar_t *)param.ptr_val,(param.real_len>>1),os,false);
	}else if( 
		type==XSYBVARCHAR ||	//0xA7
		type==XSYBCHAR || 
		type==SYBTEXT
		)
	{
		RW_LOG_PRINT(os,"(%d) ",param.def_len);
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os,param.real_len,(const char *)param.ptr_val);
	}else if(type==SYBINTN || type==SYBINT4)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %d",*(int *)(param.ptr_val));
	}else if(type==SYBINT1)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %d",*(unsigned char *)(param.ptr_val));
	}else if(type==SYBINT2)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %d",*(short *)(param.ptr_val));
	}else if(type==SYBINT8)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %d",*(__int64 *)(param.ptr_val));
	}else if(type==SYBBITN)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %d",((*(unsigned char *)(param.ptr_val)) & 0x01) );
	}else if(type==SYBREAL)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %g",*(float *)(param.ptr_val));
	}else if(type==SYBFLTN)
	{
		if(param.ptr_val!=NULL)
			RW_LOG_PRINT(os," %g",*(double *)(param.ptr_val));
	}
	else if(type==SYBMONEY4)
	{
		if(param.ptr_val!=NULL){
			TDS_MONEY4 mny; mny.mny4=*(TDS_INT *)(param.ptr_val);
			RW_LOG_PRINT(os," %g",(float)(mny.mny4/10000.0));
		}
	}else if(type==SYBMONEY)
	{
		if(param.ptr_val!=NULL){
			TDS_MONEY mny; mny.mny=*(TDS_INT8 *)(param.ptr_val);
			RW_LOG_PRINT(os," %g",(double)(mny.mny/10000.0));
		}
	}else if(type==SYBDATETIMN || type==SYBDATETIME)
	{
		TDS_DATETIME dt; TDSDATEREC when;
		if(param.ptr_val!=NULL){
		dt.dtdays=*(TDS_INT *)(param.ptr_val);
		dt.dttime=*((TDS_INT *)param.ptr_val+1);
		tds_datecrack(type, &dt, &when);
		RW_LOG_PRINT(os," %04d-%02d-%02d %02d:%02d:%02d",
			when.year, when.month+1, when.day, when.hour, when.minute, when.second);
		}
	}else if(type==SYBDATETIME4)
	{
		TDS_DATETIME4 dt; TDSDATEREC when;
		if(param.ptr_val!=NULL){
		dt.days=*(TDS_USMALLINT *)(param.ptr_val);
		dt.minutes=*((TDS_USMALLINT *)param.ptr_val+1);
		tds_datecrack(type, &dt, &when);
		RW_LOG_PRINT(os," %04d-%02d-%02d %02d:%02d:%02d",
			when.year, when.month+1, when.day, when.hour, when.minute, when.second);
		}
	}
	else
	{
		RW_LOG_PRINT(os,6,"......");
	}
	RW_LOG_PRINT(os,2,"\r\n");
	
}

