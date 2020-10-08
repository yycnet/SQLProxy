#ifndef _SOCKETSTREAM_H_
#define _SOCKETSTREAM_H_

#include "InputStream.h"
#include "OutputStream.h"

class CSocketStream : public IInputStream, public IOutputStream
{
public:
	CSocketStream();
	virtual ~CSocketStream();
	//获取有效的可用输出/输入空间大小 //yyc add
	unsigned int GetOutBufferSize(void);
	unsigned int GetInBufferSize(void);
	//获取输出/输入缓冲的数据首地址
	unsigned char *GetOutput(void){return m_pOutBuffer; }
	unsigned char *GetInput(void){return m_pInBuffer; }
	//获取当前待输出的总字节数
	unsigned int GetOutputSize(void){ return m_nOutBufferSize; }
	//获取当前未处理的输入字节数 number of bytes of rest data in the input buffer
	unsigned int GetInputSize(void){ return m_nInBufferSize - m_nInBufferIndex; }
	//清空输入输出缓冲
	void OutputReset(void){m_nOutBufferIndex = m_nOutBufferSize = 0;} 
	void InputReset(void){m_nInBufferIndex = m_nInBufferSize = 0;} 
	
	void SetInputSize(unsigned int nSize);
	unsigned int Read(void *pData, unsigned int nSize);
	unsigned int Read(unsigned int *pnValue);
	unsigned int Read(unsigned short *pnValue);
	unsigned int Read8(unsigned __int64 *pnValue);
	unsigned int Skip(unsigned int nSize);
	unsigned int GetReadPos(void){ return m_nInBufferIndex; }
	//yyc add 2017-11-05 恢复已处理数据指针
	void UnSkip(unsigned int nSize){ m_nInBufferIndex=(nSize>=m_nInBufferIndex)?0:(m_nInBufferIndex-nSize); }

	void Write(void *pData, unsigned int nSize);
	void Write(unsigned int nValue);
	void Write8(unsigned __int64 nValue);
	void Seek(int nOffset, int nFrom);
	int GetWritePos(void){ return m_nOutBufferIndex; }

private:
	unsigned char *m_pInBuffer, *m_pOutBuffer;
	//m_nInBufferIndex输入缓冲未处理数据位置，m_nInBufferSize输入缓冲总数据大小
	//m_nOutBufferIndex输出缓冲写入位置索引，m_nOutBufferSize当前总的写入数据大小
	unsigned int m_nInBufferIndex, m_nInBufferSize, m_nOutBufferIndex, m_nOutBufferSize;
};

#endif
