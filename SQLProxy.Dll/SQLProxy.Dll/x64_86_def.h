
#pragma once

//为了方便编译64位/32位dll，最好是将极可能用作保存指针又可能作为数据的变量用宏定义代替
//32位编译时定义为int； 64位编译时定义为__int64 
//重点注意回调函数的用户自定义参数类型定义，dll输出api的参数或返回有可能指向数值或指针就要用宏替换原本的定义
//vc不管64位还是32位编译环境int和long都是4字节的，long long为64位的。

//#ifndef INTPTR
#ifdef WIN32
#define INTPTR  int
#else
#define INTPTR  __int64
#endif
//#endif
