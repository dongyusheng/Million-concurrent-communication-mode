#ifndef __CELLOBJECTPOLL_HPP__
#define __CELLOBJECTPOLL_HPP__

#ifdef _DEBUG
#include <stdio.h>
#define xPrintf(...) printf(__VA_ARGS__)
#else
#define xPrintf(...)
#endif // _DEBUG

#include <stdio.h>
#include <mutex>

template<typename Type,size_t nPoolSize>
class CELLObjectPoll
{
public:
	CELLObjectPoll() {

	}
	~CELLObjectPoll(){
		
	}
public:
	//释放对象
	//申请对象
	void *allocObjMemory(size_t nSize)
	{
		//加锁
		std::lock_guard<std::mutex> lg(_mutex);

		//如果内存池未初始化，那么进行初始化
		if (!_pBuf)
			initMemory();

		NodeHeader* pReturn = nullptr;

		//如果内存池使用完了，那么_pHeader应该指向某个内存块的最后nullptr尾指针
		//此时调用malloc向系统申请内存，不向我们的内存池进行内存申请
		if (_pHeader == nullptr) {
			pReturn = (NodeHeader*)new char[sizeof(Type) + sizeof(NodeHeader)];
			pReturn->bPoll = false;
			pReturn->nID = -1;
			pReturn->nRef = 1;
			pReturn->pNext = nullptr;
		}
		else {
			//申请的时候将_pHeader向后移动
			pReturn = _pHeader;
			_pHeader = _pHeader->pNext;
			pReturn->nRef = 1;
		}

		xPrintf("allocObjMemory: %llx,id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
		//返回可用内存
		return ((char*)pReturn + sizeof(NodeHeader));
	}

	//初始化对象池
	void initPool()
	{
		if (_pBuf)
			return;
		
		//对象池中单个内存单元的大小
		size_t aSingleSize = sizeof(Type) + sizeof(NodeHeader);

		//对象池总的大小
		size_t n = nPoolSize*aSingleSize;
		_pBuf = new char[n];

		
		//初始化对象池
		//先初始化第一个对象单元
		_pHeader = (NodeHeader*)_pBuf;
		_pHeader->nID = 0;
		_pHeader->nRef = 0;
		_pHeader->bPool = true;
		_pHeader->pNext = nullptr;
		//初始化剩余的对象单元
		NodeHeader* pTemp = _pHeader;
		for (size_t n = 1; n < nPoolSize; ++n)
		{
			NodeHeader* pTempNext = (NodeHeader*)_pBuf + (n*aSingleSize);
			pTempNext->nID = n;
			pTempNext->nRef = 0;
			pTempNext->bPool = true;
			pTempNext->pNext = nullptr;
			pTemp->pNext = pTempNext;
			pTemp = pTempNext;
		}
	}
private:
	class NodeHeader
	{
	public:
		NodeHeader* pNext; //下一块位置
		int nID;           //内存块编号
		char nRef;         //引用次数
		bool bPool;        //释放在对象池中
	private:
		//这两个无用，是为了内存对齐补齐，x64平台下NodeHeader刚好为16KB
		char c1;
		char c2;
	};
private:
	NodeHeader* _pHeader; //
	char* _pBuf;          //对象池起始地址
	std::mutex _mutex;
};



template<typename Type>
class ObjectPollBase
{
public:
	void* operator new(size_t nSize)
	{
		return malloc(nSize);
	}
	void operator delete(void* p)
	{
		free(p);
	}

	template<typename ...Args>
	static Type* createObject(Args ... args)
	{
		Type* obj = new Type(args...);
		return obj;
	}
	static void destroyObject(Type* obj)
	{
		delete obj;
	}
};


#endif