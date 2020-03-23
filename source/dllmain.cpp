#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <stdint.h>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include "injector\injector.hpp"
#include "injector\assembly.hpp"
#include "injector\calling.hpp"
#include "injector\hooking.hpp"
#include "injector\utility.hpp"
#include "Pool.h"
#include "Hooking.Patterns.h"

using namespace injector;

uint32_t(__cdecl *NatHash)(const char* str);
CStreamingTypeManager** streamingTypes;
FILE* g_colCacheHandle = nullptr;
DWORD SetBoundsFromShape_loc;
DWORD GetModelInfo_loc;
uint32_t* dwCurrentEpisode;

std::map<uint32_t, uint32_t> g_streamHashes;
std::unordered_map<uint16_t, uint32_t> staticBoundHashesReverse;
std::unordered_map<uint16_t, uint32_t> physBoundHashesReverse;
std::unordered_map<int, std::string> g_indexToName;
std::unordered_set<int> g_isCachedSet;

uint32_t dw_0xF3F224, dw_0x16D7028, dw_0xF2AAA0, dw_0x96FD00, dw_0x15E3698, dw_0xEBB998, dw_0xF411C1, dw_0xF411C2, dw_0x9704A0, dw_0xC0A170;

void LogCollisionBuilding(uint16_t colID, void* building)
{
    if (g_colCacheHandle != nullptr)
    {
        if (g_isCachedSet.find((**(int**)dw_0xF3F224 << 24) | colID) == g_isCachedSet.end())
        {
            uint16_t zero = 0;
            uint32_t colHash = staticBoundHashesReverse[colID];

            fwrite(&zero, sizeof(zero), 1, g_colCacheHandle);
            fwrite(&colHash, sizeof(colHash), 1, g_colCacheHandle);

            CPool* colPool = **(CPool***)(dw_0x16D7028);
            ColPoolItem* item = colPool->GetAt<ColPoolItem>(colID);

            fwrite(item->floaters, sizeof(item->floaters), 1, g_colCacheHandle);
        }
    }
}

injector::hook_back<void*(__cdecl*)(void*, uint16_t)> hbsub_BA3430;
void* __cdecl sub_BA3430(void* building, uint16_t colID)
{
    LogCollisionBuilding(colID, building);
    return hbsub_BA3430.fun(building, colID);
}

void SetDynamicCollisionDataHook(CollisionShape* shape, uint32_t modelHash, uint32_t* colIndex)
{
    if (g_colCacheHandle != nullptr)
    {
        if (g_isCachedSet.find((**(int**)dw_0xF2AAA0 << 24) | *(uint16_t*)colIndex) == g_isCachedSet.end())
        {
            uint16_t zero = 1;
            uint32_t colHash = physBoundHashesReverse[*(uint16_t*)colIndex];

            fwrite(&zero, sizeof(zero), 1, g_colCacheHandle);
            fwrite(&colHash, sizeof(colHash), 1, g_colCacheHandle);

            fwrite(&modelHash, sizeof(modelHash), 1, g_colCacheHandle);
            fwrite(shape, 172, 1, g_colCacheHandle);
        }
    }

    injector::cstd<void*(CollisionShape*, uint32_t, uint32_t*)>::call(dw_0x96FD00, shape, modelHash, colIndex);
}

__declspec(naked) void CBaseModelInfo::SetBoundsFromShape(CollisionShape* shape)
{
    __asm
    {
        mov eax, SetBoundsFromShape_loc
        jmp eax
    }
}

__declspec(naked) CBaseModelInfo* GetModelInfo(uint32_t hash, bool a2)
{
    __asm
    {
        mov eax, GetModelInfo_loc
        jmp eax
    }
}

std::string GetStreamName(int idx, int typeIdx)
{
    return g_indexToName[(typeIdx << 24) | idx];
}

void RegisterWithColCache(const std::string& extn, int extnIndex, uint32_t hash)
{
    int hashIdx = (extn == "wbn") ? 0 : 1;

    g_streamHashes[(hashIdx << 16) | extnIndex] = hash;
}

// alter the hash in case of a resource-streamed file
static uint32_t AlterHash(int hashIdx, int typeIdx, uint32_t oldHash)
{
    auto it = g_streamHashes.find((hashIdx << 16) | typeIdx);

    if (it != g_streamHashes.end())
    {
        oldHash ^= it->second;
        oldHash |= 0x80000000;
    }

    return oldHash;
}

static int IsStreamingModuleItemCached(int itemIdx, int moduleIdx)
{
    return g_isCachedSet.find((moduleIdx << 24) | itemIdx) == g_isCachedSet.end();
}

void PreloadCollisions()
{
    if (g_colCacheHandle != nullptr)
    {
        fclose(g_colCacheHandle);
        g_colCacheHandle = nullptr;
    }

    // load existing collision data
    const char* cacheFileName = nullptr;
    const char* cacheFileName_dlc0 = ".\\colCache.dat";
    const char* cacheFileName_dlc1 = ".\\colCache_tlad.dat";
    const char* cacheFileName_dlc2 = ".\\colCache_tbogt.dat";

    if (*dwCurrentEpisode == 2)
        cacheFileName = cacheFileName_dlc2;
    else
        if (*dwCurrentEpisode == 1)
            cacheFileName = cacheFileName_dlc1;
        else
            cacheFileName = cacheFileName_dlc0;

    auto devHandle = fopen(cacheFileName, "rb");

    staticBoundHashesReverse.clear();
    physBoundHashesReverse.clear();

    // generate a table of collision IDs to hashes

    // static
    std::unordered_map<uint32_t, uint16_t> staticBoundHashes;

    CPool* colPool = **(CPool***)(dw_0x16D7028);
    for (int i = 0; i < colPool->GetCount(); i++)
    {
        uint32_t hash = NatHash(GetStreamName(i, **(int**)dw_0xF3F224).c_str()) & 0x7FFFFFFF; // StaticBounds module

        hash = AlterHash(0, i, hash);

        staticBoundHashes[hash] = i;
        staticBoundHashesReverse[i] = hash;
    }

    // physical
    std::unordered_map<uint32_t, uint16_t> physBoundHashes;

    CPool* physPool = **(CPool***)(dw_0x15E3698);
    for (int i = 0; i < physPool->GetCount(); i++)
    {
        uint32_t hash = NatHash(GetStreamName(i, **(int**)dw_0xF2AAA0).c_str()) & 0x7FFFFFFF; // Physics module

        hash = AlterHash(1, i, hash);

        physBoundHashes[hash] = i;
        physBoundHashesReverse[i] = hash;
    }

    if (devHandle != nullptr)
    {
        // load the collisions from the file
        uint16_t type;

        while (fread(&type, sizeof(type), 1, devHandle))
        {
            uint32_t colHash;

            if (type == 0)
            {
                if (fread(&colHash, sizeof(colHash), 1, devHandle))
                {
                    auto it = staticBoundHashes.find(colHash);

                    if (it != staticBoundHashes.end())
                    {
                        uint16_t colId = it->second;

                        ColPoolItem* item = colPool->GetAt<ColPoolItem>(colId);
                        if (item != nullptr)
                        {
                            fread(item->floaters, sizeof(item->floaters), 1, devHandle);
                            g_isCachedSet.insert((**(int**)dw_0xF3F224 << 24) | colId);
                        }
                    }
                    else
                    {
                        fseek(devHandle, sizeof(((ColPoolItem*)0)->floaters), SEEK_CUR);
                    }
                }
            }
            else if (type == 1)
            {
                CollisionShape shape;
                uint32_t modelHash;

                if (fread(&colHash, sizeof(colHash), 1, devHandle))
                {
                    if (fread(&modelHash, sizeof(modelHash), 1, devHandle))
                    {
                        if (fread(&shape, sizeof(shape), 1, devHandle))
                        {
                            shape.vtable = *(uint32_t*)dw_0xEBB998;

                            CBaseModelInfo* modelInfo = GetModelInfo(modelHash, false);

                            if (modelInfo)
                            {
                                auto it = physBoundHashes.find(colHash);

                                if (it != physBoundHashes.end())
                                {
                                    modelInfo->m_colIndex = it->second;
                                    modelInfo->SetBoundsFromShape(&shape);

                                    g_isCachedSet.insert((**(int**)dw_0xF2AAA0 << 24) | it->second);
                                }
                            }
                        }
                    }
                }
            }
        }

        fclose(devHandle);

        // reopen the file with a pointer to the end of the file
        devHandle = fopen(cacheFileName, "rb");
        fseek(devHandle, 0, SEEK_END);
    }
    else
    {
        devHandle = fopen(cacheFileName, "wb");
    }

    // mark cache as in-use (so we'll check cache flags)
    injector::WriteMemory<uint8_t>(dw_0xF411C1, true, true);
    injector::WriteMemory<uint8_t>(dw_0xF411C2, true, true);

    // mark the colcache file in a global variable
    g_colCacheHandle = devHandle;

    // load the remaining collisions from disk (and we'll store them to cache if need be)
    injector::cstd<void()>::call(dw_0x9704A0); // physical
    injector::cstd<void()>::call(dw_0xC0A170); // static

    // close the handle to flush data
    fclose(g_colCacheHandle);

    g_colCacheHandle = nullptr;

    g_streamHashes.clear();
    staticBoundHashesReverse.clear();
    physBoundHashesReverse.clear();
    g_indexToName.clear();
    g_isCachedSet.clear();
}

static int RegisterFileName(const char* name, int type)
{
    int typeIdx = type / 100;
    int modelIdx = (*streamingTypes)->types[typeIdx].getIndexByName(name);

    g_indexToName[(typeIdx << 24) | modelIdx] = name;

    return modelIdx;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        dwCurrentEpisode = *hook::pattern("83 3D ? ? ? ? ? 6A 01 8D 44 24 3C 50").get(0).get<uint32_t*>(2);
        dw_0xF3F224 = (uint32_t)hook::pattern("A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C3").get(0).get<uint32_t>(1);
        dw_0x16D7028 = (uint32_t)hook::pattern("8B 0D ? ? ? ? 83 EC 1C 56 33 F6 39 71 08 0F 8E").get(0).get<uint32_t>(2);
        dw_0xF2AAA0 = (uint32_t)hook::pattern("A3 ? ? ? ? E8 ? ? ? ? 6A 30").get(0).get<uint32_t>(1);
        dw_0x96FD00 = (uint32_t)hook::pattern("8B 44 24 0C 53 57 8B 38 6A 00 FF 74 24 14 E8 ? ? ? ? 8B D8 83 C4 08").get(0).get<uint32_t>(0);
        auto CPhysicsStore = hook::pattern("E8 ? ? ? ? FF 35 ? ? ? ? 8B F0 56 E8 ? ? ? ? 83 C4 0C 83 F8 FF").get(0).get<uint32_t>(0);
        auto getEntryByKey = injector::GetBranchDestination(CPhysicsStore, true).as_int();
        dw_0x15E3698 = (uint32_t)(getEntryByKey + 2);
        dw_0xEBB998 = (uint32_t)hook::pattern("56 8B F1 8B 96 ? ? ? ? C7 06 ? ? ? ? 85 D2 74 11 64 A1 ? ? ? ? 52 8B 00 8B 48 08 8B 01 FF 50 0C C7 06 ? ? ? ? 5E C3").get(0).get<uint32_t>(38);
        dw_0xF411C1 = *hook::pattern("80 3D ? ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? 74 4C 68").get(0).get<uint32_t>(2);
        dw_0xF411C2 = *hook::pattern("C6 05 ? ? ? ? 01 75 ? C6 05 ? ? ? ? 00 C3").get(0).get<uint32_t>(2);
        auto pattern = hook::pattern("80 3D ? ? ? ? ? 0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? C3");
        dw_0x9704A0 = (uint32_t)pattern.get(1).get<uint32_t>(0);
        dw_0xC0A170 = (uint32_t)pattern.get(0).get<uint32_t>(0);

        //////////////////////////////////////////////////////
        uint32_t dw_0x7BDBF0 = (uint32_t)hook::pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 83 C4 04 8B 51 08 33 F6 89 44 24 14 85 D2").get(0).get<uint32_t>(0);
        uint32_t dw_0x98E850 = (uint32_t)hook::pattern("55 8B EC 83 E4 F8 8B 45 08 F3 0F 10 41").get(0).get<uint32_t>(0);
        uint32_t dw_0x98AAE0 = (uint32_t)hook::pattern("8B 44 24 04 89 44 24 04 66 A1 ? ? ? ? 66 85 C0").get(1).get<uint32_t>(0);
        NatHash = (uint32_t(__cdecl *)(const char* str))(injector::GetBranchDestination(dw_0x7BDBF0, true).as_int());
        SetBoundsFromShape_loc = dw_0x98E850;
        GetModelInfo_loc = dw_0x98AAE0;

        uint32_t dw_0x8D8772 = (uint32_t)hook::pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 33 F6 0F B7 4A 04 85 C9 7E 1B 8B 3A 8D 87").get(0).get<uint32_t>(0);
        injector::MakeNOP(dw_0x8D8772, 5, true);
        injector::MakeCALL(dw_0x8D8772 + 5, PreloadCollisions, true);

        //uint32_t dw_0xC09721 = (uint32_t)hook::pattern("8B EC 83 E4 F0 83 EC 48 56 57 68 C0 00 00 00").get(0).get<uint32_t>(0);
        //struct CreateStaticCollisionBuildingHook
        //{
        //    void operator()(injector::reg_pack& regs)
        //    {
        //        LogCollisionBuilding(*(uint16_t*)(regs.esp + 8 + 4), *(void**)(regs.esp + 4 + 4));
        //        regs.ebp = regs.esp;
        //        regs.esp &= 0xFFFFFFF0;
        //    }
        //}; injector::MakeInline<CreateStaticCollisionBuildingHook>(dw_0xC09721);
        pattern = hook::pattern("50 E8 ? ? ? ? 8B 4E 30 83 C4 08 89 04 B9");
        hbsub_BA3430.fun = injector::MakeCALL(pattern.get_first(1), sub_BA3430).get();
        pattern = hook::pattern("E8 ? ? ? ? 83 C4 08 89 46 30 5F 5E 83 C4 08 C3");
        hbsub_BA3430.fun = injector::MakeCALL(pattern.get_first(0), sub_BA3430).get();

        uint32_t dw_0x96FF48 = (uint32_t)hook::pattern("68 ? ? ? ? E8 ? ? ? ? 33 C0 39 06 5E 0F 95 C0 C3").get(0).get<uint32_t>(1);
        injector::WriteMemory(dw_0x96FF48, SetDynamicCollisionDataHook, true);

        uint32_t dw_0x832E80 = (uint32_t)hook::pattern("8B 44 24 08 6B C0 64 B9 ? ? ? ? 8B 80 ? ? ? ? 03 44 24 04 50 E8 ? ? ? ? C3").get(1).get<uint32_t>(0);
        injector::MakeJMP(dw_0x832E80, IsStreamingModuleItemCached, true);

        // img index entry add function, call to file handler registration
        uint32_t dw_0x1227F40 = (uint32_t)hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 8B E8 33 FF 85 ED 7E 36 8D 64 24 00").get(0).get<uint32_t>(1);
        uint32_t dw_0xBCC370 = (uint32_t)hook::pattern("50 8B 86 ? ? ? ? FF D0 83 C4 04 83 F8 FF").get(0).get<uint32_t>(0);
        streamingTypes = (CStreamingTypeManager**)dw_0x1227F40;
        struct StoreImgEntryNameStub
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = RegisterFileName((const char*)regs.eax, regs.esi);
            }
        }; injector::MakeInline<StoreImgEntryNameStub>(dw_0xBCC370, dw_0xBCC370 + 12);

    }
    return TRUE;
}