#pragma once
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <string>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Forward declaration
void PrintType(ID3D11ShaderReflectionType* type, int indentLevel);

std::string GetVariableTypeName(D3D_SHADER_VARIABLE_TYPE type, D3D_SHADER_VARIABLE_CLASS Class, UINT rows, UINT columns) {
    if (Class == D3D_SVC_MATRIX_ROWS || Class == D3D_SVC_MATRIX_COLUMNS) {
        return "matrix";
    }
    if (Class == D3D_SVC_VECTOR) {
        return "float" + std::to_string(columns);
    }
    switch (type) {
    case D3D_SVT_BOOL: return "bool";
    case D3D_SVT_INT: return "int";
    case D3D_SVT_FLOAT: return "float";
    case D3D_SVT_STRING: return "string";
    default: return "unknown";
    }
}

void PrintType(ID3D11ShaderReflectionType* type, int indentLevel) {
    D3D11_SHADER_TYPE_DESC typeDesc;
    type->GetDesc(&typeDesc);

    auto printIndent = [&](int level) {
        for (int i = 0; i < level; ++i) {
            std::cout << "  ";
        }
    };

    if (typeDesc.Class == D3D_SVC_STRUCT) {
        for (UINT i = 0; i < typeDesc.Members; ++i) {
            ID3D11ShaderReflectionType* memberType = type->GetMemberTypeByIndex(i);
            D3D11_SHADER_TYPE_DESC memberDesc;
            memberType->GetDesc(&memberDesc);

            printIndent(indentLevel);
            std::cout << type->GetMemberTypeName(i) << " (Offset: " << memberDesc.Offset << ", Size: " << memberDesc.Offset << ", Type: " << GetVariableTypeName(memberDesc.Type, memberDesc.Class, memberDesc.Rows, memberDesc.Columns) << ")" << std::endl;

            if (memberDesc.Class == D3D_SVC_STRUCT) {
                PrintType(memberType, indentLevel + 1);
            }
        }
    }
}

void PrintCBuffers(ID3D11ShaderReflection* reflector) {
    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

    for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
        ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
        D3D11_SHADER_BUFFER_DESC cbDesc;
        cb->GetDesc(&cbDesc);

        std::cout << "\n--- " << cbDesc.Name << " --- \n";

        for (UINT j = 0; j < cbDesc.Variables; ++j) {
            ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(j);
            D3D11_SHADER_VARIABLE_DESC varDesc;
            var->GetDesc(&varDesc);

            ID3D11ShaderReflectionType* type = var->GetType();
            D3D11_SHADER_TYPE_DESC typeDesc;
            type->GetDesc(&typeDesc);

            std::cout << varDesc.Name << " (Offset: " << varDesc.StartOffset << ", Size: " << varDesc.Size << ", Type: " << GetVariableTypeName(typeDesc.Type, typeDesc.Class, typeDesc.Rows, typeDesc.Columns) << ")" << std::endl;

            if (typeDesc.Class == D3D_SVC_STRUCT) {
                PrintType(type, 1);
            }
        }
        std::cout << "---------------------\n";
    }
}