
#pragma once 

typedef struct tdsnumeric
{
	unsigned char precision;
	unsigned char scale;
	unsigned char array[33];
} TDS_NUMERIC;

typedef struct tdsoldmoney
{
	TDS_INT mnyhigh;
	TDS_UINT mnylow;
} TDS_OLD_MONEY;

typedef union tdsmoney
{
	TDS_OLD_MONEY tdsoldmoney;
	TDS_INT8 mny;
} TDS_MONEY;

typedef struct tdsmoney4
{
	TDS_INT mny4;
} TDS_MONEY4;

typedef struct tdsdatetime
{
	TDS_INT dtdays;
	TDS_INT dttime;
} TDS_DATETIME;

typedef struct tdsdatetime4
{
	TDS_USMALLINT days;
	TDS_USMALLINT minutes;
} TDS_DATETIME4;

/** Used by tds_datecrack */
typedef struct tdsdaterec
{
	TDS_INT year;	       /**< year */
	TDS_INT quarter;       /**< quarter (0-3) */
	TDS_INT month;	       /**< month number (0-11) */
	TDS_INT day;	       /**< day of month (1-31) */
	TDS_INT dayofyear;     /**< day of year  (1-366) */
	TDS_INT week;          /**< 1 - 54 (can be 54 in leap year) */
	TDS_INT weekday;       /**< day of week  (0-6, 0 = sunday) */
	TDS_INT hour;	       /**< 0-23 */
	TDS_INT minute;	       /**< 0-59 */
	TDS_INT second;	       /**< 0-59 */
	TDS_INT decimicrosecond;   /**< 0-9999999 */
	TDS_INT tzone;
} TDSDATEREC;

/* this structure is not directed connected to a TDS protocol but
 * keeps any DATE/TIME information
 */
typedef struct
{
	TDS_UINT8   time;	/* time, 7 digit precision */
	TDS_INT      date;	/* date, 0 = 1900-01-01 */
	TDS_SMALLINT offset;	/* time offset */
	unsigned     time_prec:4;
	unsigned     has_time:1;
	unsigned     has_date:1;
	unsigned     has_offset:1;
} TDS_DATETIMEALL;

typedef int TDSRET;
#define TDS_NO_MORE_RESULTS  ((TDSRET)1)
#define TDS_SUCCESS          ((TDSRET)0)
#define TDS_FAIL             ((TDSRET)-1)
#define TDS_CANCELLED        ((TDSRET)-2)
#define TDS_FAILED(rc) ((rc)<0)
#define TDS_SUCCEED(rc) ((rc)>=0)


/* 
 * <rant> Sybase does an awful job of this stuff, non null ints of size 1 2 
 * and 4 have there own codes but nullable ints are lumped into INTN
 * sheesh! </rant>
 */
typedef enum
{
	SYBCHAR = 47,		/* 0x2F */
#define SYBCHAR	SYBCHAR
	SYBVARCHAR = 39,	/* 0x27 */
#define SYBVARCHAR	SYBVARCHAR
	SYBINTN = 38,		/* 0x26 */
#define SYBINTN	SYBINTN
	SYBINT1 = 48,		/* 0x30 */
#define SYBINT1	SYBINT1
	SYBINT2 = 52,		/* 0x34 */
#define SYBINT2	SYBINT2
	SYBINT4 = 56,		/* 0x38 */
#define SYBINT4	SYBINT4
	SYBFLT8 = 62,		/* 0x3E */
#define SYBFLT8	SYBFLT8
	SYBDATETIME = 61,	/* 0x3D */
#define SYBDATETIME	SYBDATETIME
	SYBBIT = 50,		/* 0x32 */
#define SYBBIT	SYBBIT
	SYBTEXT = 35,		/* 0x23 */
#define SYBTEXT	SYBTEXT
	SYBNTEXT = 99,		/* 0x63 */
#define SYBNTEXT	SYBNTEXT
	SYBIMAGE = 34,		/* 0x22 */
#define SYBIMAGE	SYBIMAGE
	SYBMONEY4 = 122,	/* 0x7A */
#define SYBMONEY4	SYBMONEY4
	SYBMONEY = 60,		/* 0x3C */
#define SYBMONEY	SYBMONEY
	SYBDATETIME4 = 58,	/* 0x3A */
#define SYBDATETIME4	SYBDATETIME4
	SYBREAL = 59,		/* 0x3B */
#define SYBREAL	SYBREAL
	SYBBINARY = 45,		/* 0x2D */
#define SYBBINARY	SYBBINARY
	SYBVOID = 31,		/* 0x1F */
#define SYBVOID	SYBVOID
	SYBVARBINARY = 37,	/* 0x25 */
#define SYBVARBINARY	SYBVARBINARY
	SYBBITN = 104,		/* 0x68 */
#define SYBBITN	SYBBITN
	SYBNUMERIC = 108,	/* 0x6C */
#define SYBNUMERIC	SYBNUMERIC
	SYBDECIMAL = 106,	/* 0x6A */
#define SYBDECIMAL	SYBDECIMAL
	SYBFLTN = 109,		/* 0x6D */
#define SYBFLTN	SYBFLTN
	SYBMONEYN = 110,	/* 0x6E */
#define SYBMONEYN	SYBMONEYN
	SYBDATETIMN = 111,	/* 0x6F */
#define SYBDATETIMN	SYBDATETIMN

/*
 * MS only types
 */
	SYBNVARCHAR = 103,	/* 0x67 */
#define SYBNVARCHAR	SYBNVARCHAR
	SYBINT8 = 127,		/* 0x7F */
#define SYBINT8	SYBINT8
	XSYBCHAR = 175,		/* 0xAF */
#define XSYBCHAR	XSYBCHAR
	XSYBVARCHAR = 167,	/* 0xA7 */
#define XSYBVARCHAR	XSYBVARCHAR
	XSYBNVARCHAR = 231,	/* 0xE7 */
#define XSYBNVARCHAR	XSYBNVARCHAR
	XSYBNCHAR = 239,	/* 0xEF */
#define XSYBNCHAR	XSYBNCHAR
	XSYBVARBINARY = 165,	/* 0xA5 */
#define XSYBVARBINARY	XSYBVARBINARY
	XSYBBINARY = 173,	/* 0xAD */
#define XSYBBINARY	XSYBBINARY
	SYBUNIQUE = 36,		/* 0x24 */
#define SYBUNIQUE	SYBUNIQUE
	SYBVARIANT = 98, 	/* 0x62 */
#define SYBVARIANT	SYBVARIANT
	SYBMSUDT = 240,		/* 0xF0 */
#define SYBMSUDT SYBMSUDT
	SYBMSXML = 241,		/* 0xF1 */
#define SYBMSXML SYBMSXML
	SYBMSDATE = 40,  	/* 0x28 */
#define SYBMSDATE SYBMSDATE
	SYBMSTIME = 41,  	/* 0x29 */
#define SYBMSTIME SYBMSTIME
	SYBMSDATETIME2 = 42,  	/* 0x2a */
#define SYBMSDATETIME2 SYBMSDATETIME2
	SYBMSDATETIMEOFFSET = 43,/* 0x2b */
#define SYBMSDATETIMEOFFSET SYBMSDATETIMEOFFSET
/*
 * Sybase only types
 */
	SYBLONGBINARY = 225,	/* 0xE1 */
#define SYBLONGBINARY	SYBLONGBINARY
	SYBUINT1 = 64,		/* 0x40 */
#define SYBUINT1	SYBUINT1
	SYBUINT2 = 65,		/* 0x41 */
#define SYBUINT2	SYBUINT2
	SYBUINT4 = 66,		/* 0x42 */
#define SYBUINT4	SYBUINT4
	SYBUINT8 = 67,		/* 0x43 */
#define SYBUINT8	SYBUINT8
	SYBBLOB = 36,		/* 0x24 */
#define SYBBLOB		SYBBLOB
	SYBBOUNDARY = 104,	/* 0x68 */
#define SYBBOUNDARY	SYBBOUNDARY
	SYBDATE = 49,		/* 0x31 */
#define SYBDATE		SYBDATE
	SYBDATEN = 123,		/* 0x7B */
#define SYBDATEN	SYBDATEN
	SYB5INT8 = 191,		/* 0xBF */
#define SYB5INT8		SYB5INT8
	SYBINTERVAL = 46,	/* 0x2E */
#define SYBINTERVAL	SYBINTERVAL
	SYBLONGCHAR = 175,	/* 0xAF */
#define SYBLONGCHAR	SYBLONGCHAR
	SYBSENSITIVITY = 103,	/* 0x67 */
#define SYBSENSITIVITY	SYBSENSITIVITY
	SYBSINT1 = 176,		/* 0xB0 */
#define SYBSINT1	SYBSINT1
	SYBTIME = 51,		/* 0x33 */
#define SYBTIME		SYBTIME
	SYBTIMEN = 147,		/* 0x93 */
#define SYBTIMEN	SYBTIMEN
	SYBUINTN = 68,		/* 0x44 */
#define SYBUINTN	SYBUINTN
	SYBUNITEXT = 174,	/* 0xAE */
#define SYBUNITEXT	SYBUNITEXT
	SYBXML = 163,		/* 0xA3 */
#define SYBXML		SYBXML
} TDS_SERVER_TYPE;

//Query RCP; 保存SQL语句参数
typedef struct _tds_sql_param
{
	wstring name;		//参数名称 UCS
	wstring type_name;	//参数类型名称 譬如nvarchar char int...
	unsigned char type_code;	//参数类型
	unsigned short def_len;	//参数定义长度(非字节长度)
	unsigned short real_len;//实际大小(ptr_val有效字节长度)
	void * ptr_val;		//指向参数值的指针

	_tds_sql_param(){
		def_len=real_len=0;
		type_code=0;
		ptr_val=NULL;
	}
	~_tds_sql_param(){
		if(ptr_val!=NULL) delete[] ptr_val;
	}
}tds_sql_param;
//输出打印SQL Param信息; @参数名 参数类型(长度) 值\r\n
void print_sqlparam(tds_sql_param &param,std::ostream &os);
//解析sql参数定义信息,==0发生错误，否则返回解析处理字节数
unsigned int parse_sqlparam_def(void * pinstream,unsigned int nSize,tds_sql_param &param);
//解析sql参数值信息 ==0发生错误，否则返回解析处理字节数
unsigned int parse_sqlparam_val(void * pinstream,tds_sql_param &param);

//打印输出unicode字符串,iLen: UCS字符串长度; bCRLF 是否增加回车换行
void printUnicodeStr(const wchar_t *szText,int iLen,int ll ,bool bCRLF);
void printUnicodeStr(const wchar_t *szText,int iLen,std::ostream &os,bool bCRLF);