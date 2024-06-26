#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <BlueBlur.h>

#if _DEBUG
#define PRINT(x, ...) printf(x, __VA_ARGS__);
#else
#define PRINT(x, ...)
#endif

#define FUNCTION_STUB(returnType, name, ...) \
    returnType name(__VA_ARGS__) \
    { \
        PRINT("GenerationsD3D11: %s() stubbed\n", #name); \
        return (returnType)E_FAIL; \
    }

#include <Windows.h>
#include <detours.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3d12.h>
#include <d3d9.h>
#include <dxgi1_4.h>

#include <DDSTextureLoader11.h>
#include <lz4.h>

#define inline __forceinline
#include <xxhash.h>
#undef inline

#include <oneapi/tbb.h>

#include <bitset>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <Helpers.h>
#include <INIReader.h>
#include <LostCodeLoader.h>

using Microsoft::WRL::ComPtr;

struct XXHash
{
    std::size_t operator()(XXH32_hash_t value) const
    {
        return value;
    }
};

template<typename T>
using XXHashMap = std::unordered_map<XXH32_hash_t, T, XXHash>;