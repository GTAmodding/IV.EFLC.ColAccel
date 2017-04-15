#pragma once
#include <stdint.h>

class CPool
{
public:
	void* m_pObjects;
	char* m_pFlags;
	int m_nCount;
	int m_nEntrySize;
	int m_nTop;
	int m_nUsed;
	bool m_bAllocated;

public:
	template <typename T>
	T* GetAt(int index)
	{
		if (index >= m_nCount)
		{
			return nullptr;
		}

		if (m_pFlags[index] < 0)
		{
			return nullptr;
		}

		return (T*)(((char*)m_pObjects) + (index * m_nEntrySize));
	}

	int GetCount()
	{
		return m_nCount;
	}

	int GetIndex(void* pointer)
	{
		int index = (((uintptr_t)pointer) - ((uintptr_t)m_pObjects)) / m_nEntrySize;

		if (index < 0 || index >= m_nCount)
		{
			//FatalError("Invalid pool pointer passed");
		}

		return index;
	}
};

template <class T, class TBase>
class CPoolExtensions
{
private:
	T* m_pObjects;
	CPool* m_basePool;

public:
	CPoolExtensions(CPool* basePool)
		: m_basePool(basePool)
	{
		m_pObjects = (T*)(operator new(sizeof(T) * basePool->GetCount()));
	}

	~CPoolExtensions()
	{
		delete m_pObjects;
	}

	T* GetAt(int index)
	{
		if (index >= m_basePool->GetCount())
		{
			return nullptr;
		}

		if (!m_basePool->IsValidIndex(index))
		{
			return nullptr;
		}

		return &m_pObjects[index];
	}

	T* GetAtPointer(TBase* baseEntry)
	{
		return &m_pObjects[m_basePool->GetIndex(baseEntry)];
	}
};

class CPools
{
private:
	static CPool*& ms_pBuildingPool;

public:
	static inline CPool* GetBuildingPool() { return ms_pBuildingPool; }
};

struct ColPoolItem
{
	char pad[16];
	float floaters[7];
};

struct CStreamingType
{
	uint32_t pad;
	void*(*getAt)(int index);
	void*(*defrag)(void* unknown, void* blockMap);
	uint32_t pad2;
	void(*releaseData)(int index);
	int(*getIndexByName)(const char* name);
	void(*unkFunc)();
	void(*addRef)(int index);
	void(*release)(int index);
	int(*getUsageCount)(int index);
	int(*getParents)(int index, int* parents);
	void(*onLoad)(int index, void* blockMap, int a3);
	void(*setData)(int index, void* physicalData);
	char typeName[32];
	char ext[4];
	int startIndex;
	uint32_t pad3;
	uint32_t fileVersion;
};

struct CStreamingTypeManager
{
	CStreamingType types[25];
};

struct CollisionShape
{
	uint32_t vtable;
	char data[168];
};

class CBaseModelInfo
{
public:
	virtual ~CBaseModelInfo() = 0;

	void SetBoundsFromShape(CollisionShape* shape);

	char m_pad[76];
	uint16_t m_pad2;
	uint16_t m_colIndex;
};