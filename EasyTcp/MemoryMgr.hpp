#ifndef __MEMORYMGR_HPP_
#define __MEMORYMGR_HPP_

#include <assert.h>
#include <stdlib.h>
#include <mutex>


#ifndef  MAX_MEMORY_SIZE
#define  MAX_MEMORY_SIZE 128
#endif // ! MAX_MEMORY_SIZE

#ifdef _DEBUG
	#ifndef xPrintf
		#include <stdio.h>
		#define xPrintf(...) printf(__VA_ARGS__)
	#endif // xPrintf
#else
	#ifndef xPrintf
		#define xPrintf(...)
	#endif
#endif // _DEBUG

class MemoryBlock;
class MemoryAlloc;
class MemoryMgr;

//描述内存块的信息（内存分配的最小单元）
class MemoryBlock
{
	//32字节
public:
	MemoryAlloc* pAlloc; //所属的内存池
	MemoryBlock* pNext;  //下一块位置
	int nID;             //内存块编号
	int nRef;            //引用次数
	bool bPoll;          //释放在内存池中
private:
	//下面三个是为了内存对齐的
	char c1;
	char c2;
	char c3;
};

//内存池
class MemoryAlloc
{
public:
	MemoryAlloc() {
		_pBuf = nullptr;
		_pHeader = nullptr;
		_nSize = 0;
		_nBlockSize = 0;
	}

	~MemoryAlloc() {
		//只释放_pBuf就可以了
		if (!_pBuf)
			delete _pBuf;
		//allocMem中向系统申请内存的
	}
public:
	void initMemory()//初始化内存池
	{
		xPrintf("initMemory(): _nSize=%d ,_nBlockSize=%d\n", _nSize, _nBlockSize);
		//如果不为空，说明已经初始化过了，直接return
		if (_pBuf)
			return;

		//内存池中单个内存单元的大小（等于MemoryBlock描述信息+实际存储内容的_nSize大小）
		size_t aSingleSize = _nSize + sizeof(MemoryBlock);

		//内存池的总大小
		size_t bufSize = aSingleSize*_nBlockSize;
		_pBuf = (char*)malloc(bufSize);

		//初始化内存池
		//初始化第一块内容
		_pHeader = (MemoryBlock*)_pBuf;
		_pHeader->bPoll = true;
		_pHeader->nID = 0;
		_pHeader->nRef = 0;
		_pHeader->pAlloc = this;
		_pHeader->pNext = nullptr;
		//初始化剩余块的内容
		MemoryBlock* pTempNext = _pHeader;
		for (size_t n = 1; n < _nBlockSize; ++n)
		{
			MemoryBlock* pTemp = (MemoryBlock*)(_pBuf + (n*aSingleSize));
			pTemp->bPoll = true;
			pTemp->nID = n;
			pTemp->nRef = 0;
			pTemp->pAlloc = this;
			pTemp->pNext = nullptr;
			pTempNext->pNext = pTemp;
			pTempNext = pTemp;
		}
	}
	void *allocMemory(size_t nSize) //申请内存
	{
		//加锁
		std::lock_guard<std::mutex> lg(_mutex);

		//如果内存池未初始化，那么进行初始化
		if (!_pBuf)
			initMemory();

		MemoryBlock* pReturn = nullptr;

		//如果内存池使用完了，那么_pHeader应该指向某个内存块的最后nullptr尾指针
		//此时调用malloc向系统申请内存，不向我们的内存池进行内存申请
		if (_pHeader == nullptr) {
			pReturn = (MemoryBlock*)malloc(nSize + sizeof(MemoryBlock));
			pReturn->bPoll = false;
			pReturn->nID = -1;
			pReturn->nRef = 1;
			pReturn->pAlloc = nullptr; //这个不属于内存处管理，所以设置为nullptr
			pReturn->pNext = nullptr;
			printf("MemoryAlloc::allocMem: %llx,id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
		}
		else {
			//申请的时候将_pHeader向后移动
			pReturn = _pHeader;
			_pHeader = _pHeader->pNext;
			//assert(0 == pReturn->nRef);
			pReturn->nRef = 1;
		}

		xPrintf("MemoryAlloc::allocMem: %llx,id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
		//返回可用内存
		return ((char*)pReturn + sizeof(MemoryBlock));
	}

	void freeMemory(void *pMem)        //释放内存
	{
		//参数传入的是实际可用的内存地址，需要减去sizeof(MemoryBlock)才能得到整个单个内存块的内容
		MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));

		//如果申请的内存属于内存池中的，不是系统申请的，则执行if
		if (pBlock->bPoll)
		{
			//加锁
			std::lock_guard<std::mutex> lg(_mutex);
			
			//先将该内存块的引用计数减1，然后判断引用计数是否大于1，如果大于1那么不释放直接return
			if (--pBlock->nRef != 0)
				return;
			
			//释放内存的时候将_pHeader前移，与申请时相反
			pBlock->pNext = _pHeader;
			_pHeader = pBlock;
		}
		else {
			if (--pBlock->nRef != 0)
				return;
			free(pBlock);
		}
	}

protected:
	char*  _pBuf;         //该内存池的起始地址
	MemoryBlock* _pHeader;//该内存池中第一个内存块
	size_t _nSize;        //该内存池中单个内存块的大小
	size_t _nBlockSize;   //该内存池中内存块的数量

	std::mutex _mutex;
};

template<size_t nSize, size_t nBlockSize>
class MemoryAlloctor :public MemoryAlloc
{
public:
	MemoryAlloctor()
	{
		xPrintf("MemoryAlloctor\n");
		//为了防止传入的nSize不是整数倍，这样的话会造成内存碎片，因此我们使用运算，如果传入的数值不是整数倍，那么就转换为整数倍
		const size_t n = sizeof(void*);
		_nSize = (nSize / n)*n + (nSize % n ? n : 0);
		_nBlockSize = nBlockSize;
	}
};

//内存池管理工具(单例模式)
class MemoryMgr
{
private:
	MemoryMgr() { 
		init_szAlloc(0, 64, &_mem64);
		init_szAlloc(65, 128, &_mem128);
		//init_szAlloc(129, 256, &_mem256);
		//init_szAlloc(257, 512, &_mem512);
		//init_szAlloc(513, 1024, &_mem1024);
		xPrintf("MemoryMgr\n");
	}
	MemoryMgr(const MemoryMgr& rhs) {}
public:
	static MemoryMgr& getInstance()
	{
		static MemoryMgr mgr;
		return mgr;
	}
	void *allocMem(size_t nSize) //申请内存
	{
		if (nSize <= MAX_MEMORY_SIZE)
			return _szAlloc[nSize]->allocMemory(nSize);
		else {
			MemoryBlock* pReturn = (MemoryBlock*)malloc(nSize + sizeof(MemoryBlock));
			pReturn->bPoll = false;
			pReturn->nID = -1;
			pReturn->nRef = 1;
			pReturn->pAlloc = nullptr;
			pReturn->pNext = nullptr;
			xPrintf("MemoryMgr::allocMem: %llx,id=%d, size=%d\n", pReturn, pReturn->nID, nSize);
			return ((char*)pReturn + sizeof(MemoryBlock));
		}
	}
	void freeMem(void *pMem)     //释放内存
	{
		MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));

		xPrintf("MemoryMgr::freeMem: %llx,id=%d\n", pBlock, pBlock->nID);

		//如果申请的内存属于内存池中的，不是系统申请的，则执行if
		if (pBlock->bPoll)
			pBlock->pAlloc->freeMemory(pMem);
		else
		{
			//先将引用计数减1，然后判断引用计数是否等于0，如果等于0释放内存
			if (--pBlock->nRef == 0)
				free(pBlock);
		}
	}
	void addRef(void* pMem)      //增加内存块的引用计数
	{
		MemoryBlock* pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
		++pBlock->nRef;
	}
private:
	void init_szAlloc(int nBegin, int nEnd, MemoryAlloc* pMemA) //初始化内存池映射数组
	{
		for (int n = nBegin; n <= nEnd; n++)
			_szAlloc[n] = pMemA;
	}
private:
	MemoryAlloctor<64, 4000000> _mem64;   //内存池，该内存池有100块单个内存块，每个内存块64KB(下同)
	MemoryAlloctor<128, 1000000> _mem128;
	/*MemoryAlloctor<256, 100> _mem256;
	MemoryAlloctor<512, 100> _mem512;
	MemoryAlloctor<1024, 100> _mem1024;*/
	MemoryAlloc* _szAlloc[MAX_MEMORY_SIZE + 1]; //用来存储上面的那些内存池
};

#endif