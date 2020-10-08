
// msjexhnd.cpp

//==========================================
// Matt Pietrek
// Microsoft Systems Journal, April 1997
// FILE: MSJEXHND.CPP
//==========================================
#include <windows.h>
#include <tchar.h>
#pragma warning(disable:4996)

#include "msjexhnd.h"

//============================== Global Variables =============================

//
// Declare the static variables of the MSJExceptionHandler class
//
TCHAR MSJExceptionHandler::m_szLogFileName[MAX_PATH];
LPTOP_LEVEL_EXCEPTION_FILTER MSJExceptionHandler::m_previousFilter;
HANDLE MSJExceptionHandler::m_hReportFile;
BOOL MSJExceptionHandler::m_bCrash2Dmp=FALSE;

MSJExceptionHandler g_MSJExceptionHandler; // Declare global instance of class

//============================== Class Methods =============================

//=============
// Constructor 
//=============
MSJExceptionHandler::MSJExceptionHandler( )
{
    // Install the unhandled exception filter function
    m_previousFilter = SetUnhandledExceptionFilter(MSJUnhandledExceptionFilter);

    // Figure out what the report file will be named, and store it away
    GetModuleFileName( 0, m_szLogFileName, MAX_PATH );

    // Look for the '.' before the "EXE" extension. Replace the extension
    // with "RPT"
    PTSTR pszDot = _tcsrchr( m_szLogFileName, _T('.') );
    if ( pszDot )
    {
        pszDot++;   // Advance past the '.'
        if ( _tcslen(pszDot) >= 3 )
            _tcscpy( pszDot, _T("RPT") );   // "RPT" -> "Report"
    }
	LPSTR szCmd=::GetCommandLine();
	m_bCrash2Dmp=(szCmd && _tcsstr(szCmd,_T(" -crash2dmp") )!=NULL)?TRUE:FALSE;
}

//============
// Destructor 
//============
MSJExceptionHandler::~MSJExceptionHandler( )
{
    SetUnhandledExceptionFilter( m_previousFilter );
}

//==============================================================
// Lets user change the name of the report file to be generated 
//==============================================================
void MSJExceptionHandler::SetLogFileName( PTSTR pszLogFileName )
{
    _tcscpy( m_szLogFileName, pszLogFileName );
}

//===========================================================
// Entry point where control comes on an unhandled exception 
//===========================================================
LONG WINAPI MSJExceptionHandler::MSJUnhandledExceptionFilter(
                                    PEXCEPTION_POINTERS pExceptionInfo )
{
    m_hReportFile = CreateFile( m_szLogFileName,
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                0,
                                OPEN_ALWAYS,
                                FILE_FLAG_WRITE_THROUGH,//FILE_ATTRIBUTE_NORMAL
                                0 );

    if ( m_hReportFile )
    {
        SetFilePointer( m_hReportFile, 0, 0, FILE_END );

        GenerateExceptionReport( pExceptionInfo );
		if(m_bCrash2Dmp)
			MiniDumpReport(pExceptionInfo); 
		else
			_tprintf( _T("Disable Dump information(*.dmp) by MiniDumpWriteDump of DBGHELP.DLL\n") );

        CloseHandle( m_hReportFile );
        m_hReportFile = 0;
    }

    if ( m_previousFilter )
        return m_previousFilter( pExceptionInfo );
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

#define DUMP_BY_DBGHELP	//yyc add 支持采用DBGHELP.dll dump崩溃信息
#ifdef DUMP_BY_DBGHELP
#include <DbgHelp.h>
typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                                         CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                         CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                         CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
void MSJExceptionHandler::MiniDumpReport(
    PEXCEPTION_POINTERS pExceptionInfo )
{
	MINIDUMPWRITEDUMP m_pfnMiniDumpWriteDump=NULL;
	HMODULE m_hDll= LoadLibrary(_T("DBGHELP.DLL"));
	if(m_hDll!=NULL)
		m_pfnMiniDumpWriteDump=(MINIDUMPWRITEDUMP)GetProcAddress(m_hDll, _T("MiniDumpWriteDump"));
	if(m_pfnMiniDumpWriteDump!=NULL)
	{
		_tprintf( _T("Dump information(*.dmp) by MiniDumpWriteDump of DBGHELP.DLL\n") );
		
		TCHAR szDmpFileName[MAX_PATH];
		_tcscpy( szDmpFileName,m_szLogFileName);
		PTSTR pszDot = _tcsrchr( szDmpFileName, _T('.') );
		if ( pszDot ) _tcscpy( ++pszDot, _T("DMP") );
		HANDLE hDmpFile = CreateFile( szDmpFileName,
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                0,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                0 );
		if(hDmpFile!=NULL){
			_MINIDUMP_EXCEPTION_INFORMATION ExInfo = {0};
			ExInfo.ThreadId = GetCurrentThreadId();
			ExInfo.ExceptionPointers = pExceptionInfo;
			ExInfo.ClientPointers = NULL;
			BOOL bOK = (*m_pfnMiniDumpWriteDump)(GetCurrentProcess(), GetCurrentProcessId(), hDmpFile, MiniDumpNormal, &ExInfo, NULL, NULL);
			if(!bOK)
				_tprintf( _T("Failed to to save dump file!\n") );
			else  _tprintf( _T("Success to save dump file!\n") );
			CloseHandle( hDmpFile );
		}
		
	}else
		_tprintf( _T("DBGHELP.DLL not found or too old. Please upgrade to version 5.1 (or later)\n") );

	if(m_hDll!=NULL) ::FreeLibrary(m_hDll);
}
#else
void MSJExceptionHandler::MiniDumpReport(
    PEXCEPTION_POINTERS pExceptionInfo )
{
	return;
}
#endif

//===========================================================================
// Open the report file, and write the desired information to it. Called by 
// MSJUnhandledExceptionFilter                                               
//===========================================================================
#include <time.h>
void MSJExceptionHandler::GenerateExceptionReport(
    PEXCEPTION_POINTERS pExceptionInfo )
{
    // Start out with a banner
    //_tprintf( _T("//=====================================================\n") );
	time_t tNow=time(NULL);
	struct tm * ltime=localtime(&tNow);
	_tprintf( _T("//=====================%04d-%02d-%02d %02d:%02d:%02d========================\n"), 
				(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, 
				ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    // First print information about the type of fault
    _tprintf(   _T("Exception code: %08X %s\n"),
                pExceptionRecord->ExceptionCode,
                GetExceptionString(pExceptionRecord->ExceptionCode) );

    // Now print information about where the fault occured
    TCHAR szFaultingModule[MAX_PATH]= _T("");
    DWORD section, offset;
    GetLogicalAddress( pExceptionRecord->ExceptionAddress,
                        szFaultingModule,
                        sizeof( szFaultingModule ),
                        section, offset );

    _tprintf( _T("Fault address: %08X %02X:%08X %s\n"),
                pExceptionRecord->ExceptionAddress,
                section, offset, szFaultingModule );

    PCONTEXT pCtx = pExceptionInfo->ContextRecord;

    // Show the registers
    #ifdef _M_IX86 // Intel Only!
    _tprintf( _T("\nRegisters:\n") );

    _tprintf(_T("EAX:%08X\nEBX:%08X\nECX:%08X\nEDX:%08X\nESI:%08X\nEDI:%08X\n"),
            pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx, pCtx->Esi, pCtx->Edi );

    _tprintf( _T("CS:EIP:%04X:%08X\n"), pCtx->SegCs, pCtx->Eip );
    _tprintf( _T("SS:ESP:%04X:%08X EBP:%08X\n"),
                pCtx->SegSs, pCtx->Esp, pCtx->Ebp );
    _tprintf( _T("DS:%04X ES:%04X FS:%04X GS:%04X\n"),
                pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
    _tprintf( _T("Flags:%08X\n"), pCtx->EFlags );

    // Walk the stack using x86 specific code
    IntelStackWalk( pCtx );

    #endif

    _tprintf( _T("\n") );
}

//======================================================================
// Given an exception code, returns a pointer to a static string with a 
// description of the exception                                         
//======================================================================
LPTSTR MSJExceptionHandler::GetExceptionString( DWORD dwCode )
{
    #define EXCEPTION( x ) case EXCEPTION_##x: return _T(#x);

    switch ( dwCode )
    {
        EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )
        EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )
        EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )
        EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )
        EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )
        EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )
        EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )
        EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )
        EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )
        EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
    }

    // If not one of the "known" exceptions, try to get the string
    // from NTDLL.DLL's message table.

    static TCHAR szBuffer[512] = { 0 };

    FormatMessage( FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                    GetModuleHandle( _T("NTDLL.DLL") ),
                    dwCode, 0, szBuffer, sizeof( szBuffer ), 0 );

    return szBuffer;
}

//==============================================================================
// Given a linear address, locates the module, section, and offset containing 
// that address.                                                               
//                                                                             
// Note: the szModule paramater buffer is an output buffer of length specified 
// by the len parameter (in characters!)                                       
//==============================================================================
BOOL MSJExceptionHandler::GetLogicalAddress(
        PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset )
{
    MEMORY_BASIC_INFORMATION mbi;

    if (addr==NULL || !VirtualQuery( addr, &mbi, sizeof(mbi) ) )
        return FALSE;

    DWORD hMod = (DWORD)mbi.AllocationBase;

    if (hMod==0 || !GetModuleFileName( (HMODULE)hMod, szModule, len ) )
        return FALSE;

    // Point to the DOS header in memory
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;

    // From the DOS header, find the NT (PE) header
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + pDosHdr->e_lfanew);

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );

    DWORD rva = (DWORD)addr - hMod; // RVA is offset from module load address

    // Iterate through the section table, looking for the one that encompasses
    // the linear address.
    for (   unsigned i = 0;
            i < pNtHdr->FileHeader.NumberOfSections;
            i++, pSection++ )
    {
        DWORD sectionStart = pSection->VirtualAddress;
        DWORD sectionEnd = sectionStart
                    + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);

        // Is the address in this section???
        if ( (rva >= sectionStart) && (rva <= sectionEnd) )
        {
            // Yes, address is in the section. Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.
            section = i+1;
            offset = rva - sectionStart;
            return TRUE;
        }
    }

    return FALSE;   // Should never get here!
}

//============================================================
// Walks the stack, and writes the results to the report file 
//============================================================
void MSJExceptionHandler::IntelStackWalk( PCONTEXT pContext )
{
    _tprintf( _T("\nCall stack:\n") );
	
	//Frame - 当前地址所处函数的EBP值(应该保存的是栈底指针)
	//Address - 当前地址
	//Args - 当前地址所处函数可能的参数
    _tprintf( _T("Frame   Address     Logical addr Module  Args\n") );
	
	PDWORD pPrevFrame=0x0;
    DWORD pc = pContext->Eip;
    PDWORD pFrame = (PDWORD)pContext->Ebp;
	do{
		TCHAR szModule[MAX_PATH] = _T("");
        DWORD section = 0, offset = 0;
        if(!GetLogicalAddress((PVOID)pc, szModule,sizeof(szModule),section,offset ))
			break;

        _tprintf( _T("%08X %08X %04X:%08X %s"),
                     pFrame,pc, section, offset, szModule );
		if ( (DWORD)pFrame & 3 )    // Frame pointer must be aligned on a
            break;
		if ( pFrame <= pPrevFrame )
            break;
		// Can two DWORDs be read from the supposed frame address?          
        if ( IsBadWritePtr(pFrame, sizeof(PVOID)*2) )
            break;
		//打印可能的参数
		_tprintf( _T("  %08X %08X %08X\n"),pFrame[2],pFrame[3],pFrame[4]);

		pc = pFrame[1];
        pPrevFrame = pFrame;
        pFrame = (PDWORD)pFrame[0]; // precede to next higher frame on stack
	}while(1);
	_tprintf( _T("\n") );
	//说明：一般函数调用都是 压栈参数，压栈返回地址， 压栈EBP，将当前ESP赋值给EBP，进行局部变量分配(ESP+局部变量大小)
	//因此一般当前函数EBP保存的是函数入口的栈指针
	//[EBP]: 保存的EBP [EBP+4] 返回地址 [EBP+8] 第一个参数...... [EBP-4] 第一个局部变量
	//但编译器有时可能会优化不采用在函数入口处保存栈底EBP然后通过栈底指针EBP访问参数和局部变量可能会直接通过ESP访问参数和变量，避免了针对EBP的操作提高效率
	//譬如MS的编译器采用O2优化选项编译
}

//============================================================================
// Helper function that writes to the report file, and allows the user to use 
// printf style formating                                                     
//============================================================================
int __cdecl MSJExceptionHandler::_tprintf(const TCHAR * format, ...)
{
    TCHAR szBuff[1024];
    int retValue;
    DWORD cbWritten;
    va_list argptr;
          
    va_start( argptr, format );
    retValue = wvsprintf( szBuff, format, argptr );
    va_end( argptr );

    WriteFile( m_hReportFile, szBuff, retValue * sizeof(TCHAR), &cbWritten, 0 );

    return retValue;
}