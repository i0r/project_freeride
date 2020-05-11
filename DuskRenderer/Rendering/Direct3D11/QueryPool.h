
#pragma once

#if DUSK_D3D11
#include <vector>

struct ID3D11Query;
enum D3D11_QUERY;

struct QueryPool
{
    u32                     capacity;
    u32                     currentAllocableIndex;
    i32                     frequencySampleCount;
    u32                     currentDisjointAllocableIndex;
    D3D11_QUERY             targetType;
    bool                    isRecordingDisjointQuery;
    ID3D11Query**           queryHandles;
    ID3D11Query**           disjointQueryHandles;
    std::vector<u32>        queryDisjointTable;
    std::vector<uint64_t>   disjointQueriesResult;
};
#endif
