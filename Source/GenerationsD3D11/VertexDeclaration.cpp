﻿#include "VertexDeclaration.h"
#include "TypeConverter.h"
#include "VertexShader.h"

void VertexDeclaration::addIfNotExist(LPCSTR semanticName, UINT semanticIndex, DXGI_FORMAT format)
{
    for (auto& element : inputElements)
    {
        if (strcmp(element.SemanticName, semanticName) == 0 && element.SemanticIndex == semanticIndex)
            return;
    }

    auto inputElement = inputElements[0];
    inputElement.SemanticName = semanticName;
    inputElement.SemanticIndex = semanticIndex;
    inputElement.Format = format;

    inputElements.push_back(inputElement);
}

VertexDeclaration::VertexDeclaration(ID3D11Device* device, const D3DVERTEXELEMENT9* pVertexElements)
{
    for (int i = 0; ; i++)
    {
        if (pVertexElements[i].Stream == 0xFF)
            break;

        // Sloppily validate declaration, some declarations passed by game don't have D3DDECL_END()
        bool stop = false;

        for (int j = 0; j < i; j++)
            stop |= pVertexElements[i].Usage == pVertexElements[j].Usage && pVertexElements[i].UsageIndex == pVertexElements[j].UsageIndex;

        if (stop)
            break;

        D3D11_INPUT_ELEMENT_DESC desc;
        desc.SemanticName = TypeConverter::getDeclUsage((D3DDECLUSAGE)pVertexElements[i].Usage);
        desc.SemanticIndex = pVertexElements[i].UsageIndex;
        desc.Format = TypeConverter::getDeclType((D3DDECLTYPE)pVertexElements[i].Type);
        desc.InputSlot = pVertexElements[i].Stream;
        desc.AlignedByteOffset = pVertexElements[i].Offset;
        desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = 0;

        inputElements.push_back(desc);
    }

    vertexElements = std::make_unique<D3DVERTEXELEMENT9[]>(inputElements.size() + 1);
    memcpy(vertexElements.get(), pVertexElements, sizeof(D3DVERTEXELEMENT9) * inputElements.size());
    vertexElements[inputElements.size() - 1] = D3DDECL_END();
    vertexElementCount = inputElements.size();

    // TODO: Do this depending on the shader reflection
    addIfNotExist("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT);
    addIfNotExist("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT);
    addIfNotExist("TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT);
    addIfNotExist("BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT);
    addIfNotExist("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT);
    addIfNotExist("TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT);
    addIfNotExist("TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT);
    addIfNotExist("TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT);
    addIfNotExist("COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    addIfNotExist("BLENDWEIGHT", 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    addIfNotExist("BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT);
}

ID3D11InputLayout* VertexDeclaration::getInputLayout(ID3D11Device* device, const VertexShader* vertexShader)
{
    const auto pair = inputLayouts.find(vertexShader);
    if (pair != inputLayouts.end())
        return pair->second.Get();

    ID3D11InputLayout* inputLayout = vertexShader->createInputLayout(device, inputElements.data(), inputElements.size());
    inputLayouts.insert(std::make_pair(vertexShader, inputLayout));

    return inputLayout;
}

FUNCTION_STUB(HRESULT, VertexDeclaration::GetDevice, Device** ppDevice)

HRESULT VertexDeclaration::GetDeclaration(D3DVERTEXELEMENT9* pElement, UINT* pNumElements)
{
    memcpy(pElement, vertexElements.get(), vertexElementCount * sizeof(D3DVERTEXELEMENT9));
    *pNumElements = vertexElementCount;

    return S_OK;
}