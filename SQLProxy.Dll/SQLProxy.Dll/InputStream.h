#ifndef _INPUTSTREAM_H_
#define _INPUTSTREAM_H_

#include <stdio.h>

class IInputStream
{
public:
	//½»»»ÍøÂç×Ö½ÚË³Ðò
	template<class T>
	static unsigned int htonb(T &val)
	{
		T tmp=val;
		unsigned char *p0=(unsigned char *)&val;
		unsigned char *p1=(unsigned char *)&tmp+sizeof(T)-1;
		for(unsigned int i=0;i<sizeof(T);i++) *p0++=*p1--;
		return sizeof(T);
	}
public:
	virtual unsigned int Read(void *pData, unsigned int nSize) = 0;
	virtual unsigned int Read(unsigned short *pnValue) = 0;
	virtual unsigned int Read(unsigned int *pnValue) = 0;
	virtual unsigned int Read8(unsigned __int64 *pnValue) = 0;
	virtual unsigned int Skip(unsigned int nSize) = 0;
	virtual unsigned int GetReadPos(void) = 0;
	virtual unsigned int GetInputSize(void)=0;

};

#endif
