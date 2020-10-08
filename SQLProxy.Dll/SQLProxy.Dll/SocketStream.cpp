
#include "stdafx.h"
#include <string.h>

#include "SocketStream.h"


#define MAXDATA (32 * 1024)

CSocketStream::CSocketStream()
{
	m_pInBuffer = new unsigned char[MAXDATA];
	m_pOutBuffer = new unsigned char[MAXDATA];
	m_nInBufferIndex = m_nInBufferSize = 0;
	m_nOutBufferIndex = m_nOutBufferSize = 0;
}

CSocketStream::~CSocketStream()
{
	delete[] m_pInBuffer;
	delete[] m_pOutBuffer;
}

//获取有效的可用输出/输入空间大小 //yyc add
unsigned int CSocketStream:: GetOutBufferSize(void)
{ 
	return MAXDATA-m_nOutBufferSize;
}
unsigned int CSocketStream:: GetInBufferSize(void)
{ 
	return MAXDATA-m_nInBufferSize; 
}

void CSocketStream::SetInputSize(unsigned int nSize)
{
	m_nInBufferSize+=nSize; //追加数据
	if(m_nInBufferSize>MAXDATA)
		m_nInBufferSize=MAXDATA;
	
	if(nSize==0 && m_nInBufferIndex!=0)
	{//移动数据
		void *dst=m_pInBuffer;
		void *src=m_pInBuffer+m_nInBufferIndex;
		nSize=m_nInBufferSize - m_nInBufferIndex;
		if(nSize!=0) ::memmove(dst,src,nSize);
		m_nInBufferIndex=0;
		m_nInBufferSize=nSize;
	}
}

unsigned int CSocketStream::Read(void *pData, unsigned int nSize)
{
	if (nSize > m_nInBufferSize - m_nInBufferIndex)  //over the number of bytes of data in the input buffer
		nSize = m_nInBufferSize - m_nInBufferIndex;
	memcpy(pData, m_pInBuffer + m_nInBufferIndex, nSize);
	m_nInBufferIndex += nSize;
	return nSize;
}

unsigned int CSocketStream::Read(unsigned short *pnValue)
{
	unsigned int i, n;
	unsigned char *p, pBuffer[sizeof(unsigned short)];

	p = (unsigned char *)pnValue;
	n = Read(pBuffer, sizeof(unsigned short));
	for (i = 0; i < n; i++)  //reverse byte order
		p[sizeof(unsigned short) - 1 - i] = pBuffer[i];
	return n;
}

unsigned int CSocketStream::Read(unsigned int *pnValue)
{
	unsigned int i, n;
	unsigned char *p, pBuffer[sizeof(unsigned long)];

	p = (unsigned char *)pnValue;
	n = Read(pBuffer, sizeof(unsigned long));
	for (i = 0; i < n; i++)  //reverse byte order
		p[sizeof(unsigned long) - 1 - i] = pBuffer[i];
	return n;
}

unsigned int CSocketStream::Read8(unsigned __int64 *pnValue)
{
	unsigned int i, n;
	unsigned char *p, pBuffer[sizeof(unsigned __int64)];

	p = (unsigned char *)pnValue;
	n = Read(pBuffer, sizeof(unsigned __int64));
	for (i = 0; i < n; i++)  //reverse byte order
		p[sizeof(unsigned __int64) - 1 - i] = pBuffer[i];
	return n;
}

unsigned int CSocketStream::Skip(unsigned int nSize)
{
	if (nSize > m_nInBufferSize - m_nInBufferIndex)  //over the number of bytes of data in the input buffer
		nSize = m_nInBufferSize - m_nInBufferIndex;
	m_nInBufferIndex += nSize;
	return nSize;
}

void CSocketStream::Write(void *pData, unsigned int nSize)
{
	if (m_nOutBufferIndex + nSize > MAXDATA)  //over the size of output buffer
		nSize = MAXDATA - m_nOutBufferIndex;
	memcpy(m_pOutBuffer + m_nOutBufferIndex, pData, nSize);
	m_nOutBufferIndex += nSize;
	if (m_nOutBufferIndex > m_nOutBufferSize)
		m_nOutBufferSize = m_nOutBufferIndex;
}

void CSocketStream::Write(unsigned int nValue)
{
	int i;
	unsigned char *p, pBuffer[sizeof(unsigned long)];

	p = (unsigned char *)&nValue;
	for (i = sizeof(unsigned long) - 1; i >= 0; i--)  //reverse byte order
		pBuffer[i] = p[sizeof(unsigned long) - 1 - i];
	Write(pBuffer, sizeof(unsigned long));
}

void CSocketStream::Write8(unsigned __int64 nValue)
{
	int i;
	unsigned char *p, pBuffer[sizeof(unsigned __int64)];

	p = (unsigned char *)&nValue;
	for (i = sizeof(unsigned __int64) - 1; i >= 0; i--)  //reverse byte order
		pBuffer[i] = p[sizeof(unsigned __int64) - 1 - i];
	Write(pBuffer, sizeof(unsigned __int64));
}

void CSocketStream::Seek(int nOffset, int nFrom)
{
	if (nFrom == SEEK_SET)
		m_nOutBufferIndex = nOffset;
	else if (nFrom == SEEK_CUR)
		m_nOutBufferIndex += nOffset;
	else if (nFrom == SEEK_END)
		m_nOutBufferIndex = m_nOutBufferSize + nOffset;
}

