#pragma once
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

#include <cassert>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "DynamicBuffer.h"

class UShaderReflection
{
public:
	enum class EType
	{
		VertexShader,
	//	GeometryShader
		PixelShader
	};

private:
	struct FConstantBufferInfo
	{
		UINT Size;
		UINT BindPoint;
	};

public:
	~UShaderReflection() = default;

	/** NOTE: Need ComPtr? */
	/** NOTE: ChatGPT says that only ShaderReflection requires release. */
	/** FIX: Should add shader type as parameter. */
	UShaderReflection(ID3D11Device* Device, ID3DBlob* ShaderBlob)
	{
		/** NOTE: ID3D11ShaderReflection doesn't require release? */
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> ShaderReflection;
		D3DReflect(
			ShaderBlob->GetBufferPointer(),
			ShaderBlob->GetBufferSize(),
			IID_PPV_ARGS(ShaderReflection.ReleaseAndGetAddressOf())
			//IID_ID3D11ShaderReflection,
			//reinterpret_cast<void**>(ShaderReflection.ReleaseAndGetAddressOf())
		);

		/** FIX: if-else to distinguish shader type */
		ReflectVertexShader(Device, ShaderBlob, ShaderReflection.Get());
		/** FIX: if-else to distinguish shader type */

		D3D11_SHADER_DESC ShaderDesc;
		ShaderReflection->GetDesc(&ShaderDesc);

		for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
		{
			ID3D11ShaderReflectionConstantBuffer* 
			ShaderReflectionConstantBuffer = ShaderReflection->GetConstantBufferByIndex(i);

			D3D11_SHADER_BUFFER_DESC ShaderBufferDesc;
			ShaderReflectionConstantBuffer->GetDesc(&ShaderBufferDesc);

			/** Constant Buffer */
			D3D11_BUFFER_DESC ConstantBufferDesc	= {};
			ConstantBufferDesc.ByteWidth			= ShaderBufferDesc.Size;
			ConstantBufferDesc.Usage				= D3D11_USAGE_DEFAULT;
			ConstantBufferDesc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
			ConstantBufferDesc.CPUAccessFlags		= 0;
			ConstantBufferDesc.MiscFlags			= 0;
			ConstantBufferDesc.StructureByteStride	= 0;

			// Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;

			/** FIX: Should un-comment this after debugging */
			 //Device->CreateBuffer(&ConstantBufferDesc, nullptr, ConstantBuffer.ReleaseAndGetAddressOf());

			// ConstantBufferMap.try_emplace(ShaderBufferDesc.Name, ConstantBuffer);

			/** Constant Buffer Info */
			D3D11_SHADER_INPUT_BIND_DESC ShaderInputBindDesc;
			ShaderReflection->GetResourceBindingDescByName(ShaderBufferDesc.Name, &ShaderInputBindDesc);

			FConstantBufferInfo ConstantBufferInfo	= {};
			ConstantBufferInfo.BindPoint			= ShaderInputBindDesc.BindPoint;
			ConstantBufferInfo.Size					= ShaderBufferDesc.Size;
			ConstantBufferInfoMap.try_emplace(ShaderBufferDesc.Name, ConstantBufferInfo);

			/** Constant Dynamic Buffer */
			UBufferElementLayout Layout;
			ReflectConstantBuffer(ShaderReflectionConstantBuffer, &Layout);
			ConstantDynamicBufferMap.try_emplace(ShaderBufferDesc.Name, std::move(Layout));
		}
	}

	/** Deleted copy, move constructors */
	UShaderReflection(const UShaderReflection&) = delete;
	UShaderReflection(UShaderReflection&&) = delete;

	/** Deleted copy, move assignment operators */
	UShaderReflection& operator=(const UShaderReflection&) = delete;
	UShaderReflection& operator=(UShaderReflection&&) = delete;

public:
	/** FIX: Delete this after debugging */
	TMap<FString, UDynamicBuffer>& GetConstantDynamicBufferMap()
	{
		return ConstantDynamicBufferMap;
	}

	UDynamicBuffer& GetConstantDynamicBuffer(const FString& Name)
	{
		/** TODO: Assertion? */
		return ConstantDynamicBufferMap.at(Name);
	}

	UBufferElementLayout GetVertexBufferElementLayout()
	{
		return VertexBufferElementLayout.Clone();
	}

private:
	/** NOTE: Need ComPtr? */
	static void ReflectConstantBuffer(ID3D11ShaderReflectionConstantBuffer* ShaderReflectionConstantBuffer, UBufferElementLayout* OutLayout)
	{
		D3D11_SHADER_BUFFER_DESC ShaderBufferDesc;
		ShaderReflectionConstantBuffer->GetDesc(&ShaderBufferDesc);

		for (UINT i = 0; i < ShaderBufferDesc.Variables; ++i)
		{
			ID3D11ShaderReflectionVariable* ShaderReflectionVariable = ShaderReflectionConstantBuffer->GetVariableByIndex(i);
			D3D11_SHADER_VARIABLE_DESC ShaderVariableDesc;
			ShaderReflectionVariable->GetDesc(&ShaderVariableDesc);

			ID3D11ShaderReflectionType* ShaderReflectionType = ShaderReflectionVariable->GetType();

			/** TODO: StartOffset or use Offset in SHADER_TYPE_DESC? */
			ReflectConstantBufferVariable(ShaderReflectionType, ShaderVariableDesc.Name, ShaderVariableDesc.StartOffset, OutLayout);
		}
	}

	/** NOTE: Recursive helper function for constant buffer reflection. */
	static void ReflectConstantBufferVariable(ID3D11ShaderReflectionType* ShaderReflectionType, const FString& Name, size_t StartOffset, UBufferElementLayout* OutLayout)
	{
		D3D11_SHADER_TYPE_DESC ShaderTypeDesc;
		ShaderReflectionType->GetDesc(&ShaderTypeDesc);

		/** TODO: Change Type size. It should be signed type. */
		size_t CurrentStride = OutLayout->GetCurrentStride();
		assert(StartOffset >= CurrentStride && "StartOffset should not be smaller than current stride.");
		int PaddingSize = static_cast<int>(StartOffset - CurrentStride);
		if (PaddingSize > 0)
		{
			OutLayout->AppendPadding(PaddingSize);
		}

		if (ShaderTypeDesc.Class == D3D_SVC_SCALAR)
		{
			switch (ShaderTypeDesc.Type)
			{
			case D3D_SVT_BOOL:
				OutLayout->Append<HLSL::EType::Bool>(Name);
				break;
			case D3D_SVT_INT:
				OutLayout->Append<HLSL::EType::Int>(Name);
				break;
			case D3D_SVT_FLOAT:
				OutLayout->Append<HLSL::EType::Float>(Name);
				break;
			default:
				assert(false && "Unsupported Scalar type");
				break;
			}
		}
		else if (ShaderTypeDesc.Class == D3D_SVC_VECTOR)
		{
			assert(ShaderTypeDesc.Rows == 1 && "HLSL Vectors should have 1 row.");
			if (ShaderTypeDesc.Columns == 3)
			{
				OutLayout->Append<HLSL::EType::Float3>(Name);
			}
			else if (ShaderTypeDesc.Columns == 4)
			{
				OutLayout->Append<HLSL::EType::Float4>(Name);
			}
			else
			{
				assert(false && "Unsupported Vector size");
			}
		}
		else if (ShaderTypeDesc.Class == D3D_SVC_MATRIX_ROWS || ShaderTypeDesc.Class == D3D_SVC_MATRIX_COLUMNS)
		{
			assert(ShaderTypeDesc.Rows == 4 && ShaderTypeDesc.Columns == 4 && "Unsupported Matrix Size");
			OutLayout->Append<HLSL::EType::Matrix>(Name);
		}
		else if (ShaderTypeDesc.Class == D3D_SVC_STRUCT)
		{
			UBufferElementLayout Layout;
			for (UINT i = 0; i < ShaderTypeDesc.Members; ++i)
			{
				const char* MemberName = ShaderReflectionType->GetMemberTypeName(i);

				ID3D11ShaderReflectionType* MemberShaderReflectionType = ShaderReflectionType->GetMemberTypeByIndex(i);

				D3D11_SHADER_TYPE_DESC MemberShaderTypeDesc;
				MemberShaderReflectionType->GetDesc(&MemberShaderTypeDesc);

				// Recursive call for struct members
				ReflectConstantBufferVariable(MemberShaderReflectionType, MemberName, MemberShaderTypeDesc.Offset, &Layout);
			}
			OutLayout->AppendStruct(Name, std::move(Layout));
		}
		else
		{
			assert(false && "Unsupported Data Class");
		}
	}

	/** TODO: Variable Name, Type support (unsigned int) */
	/** TODO: What about matrix or struct? */
	void ReflectVertexShader(ID3D11Device* Device, ID3DBlob* ShaderBlob, ID3D11ShaderReflection* ShaderReflection)
	{
		D3D11_SHADER_DESC ShaderDesc;
		ShaderReflection->GetDesc(&ShaderDesc);

		TArray<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;

		for (UINT i = 0; i < ShaderDesc.InputParameters; ++i)
		{
			D3D11_SIGNATURE_PARAMETER_DESC SignatureParameterDesc;
			ShaderReflection->GetInputParameterDesc(i, &SignatureParameterDesc);

			D3D11_INPUT_ELEMENT_DESC InputElementDesc	= {};
			InputElementDesc.SemanticName				= SignatureParameterDesc.SemanticName;
			InputElementDesc.SemanticIndex				= SignatureParameterDesc.SemanticIndex;
			InputElementDesc.InputSlot					= 0;
			InputElementDesc.AlignedByteOffset			= D3D11_APPEND_ALIGNED_ELEMENT;
			InputElementDesc.InputSlotClass				= D3D11_INPUT_PER_VERTEX_DATA;
			InputElementDesc.InstanceDataStepRate		= 0;

			FString ParameterName = SignatureParameterDesc.SemanticName + std::to_string(SignatureParameterDesc.SemanticIndex);
			if (SignatureParameterDesc.Mask == 1) /** 0b0001 One component */
			{
				if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
				{
					/** TODO: Support unsigned integer type? */
					/** NOTE: Cannot fetch name of variables directly. */
					InputElementDesc.Format = DXGI_FORMAT_R32_UINT;
					VertexBufferElementLayout.Append<HLSL::EType::Int>(ParameterName);
				}
				else if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
				{
					InputElementDesc.Format = DXGI_FORMAT_R32_SINT;
					VertexBufferElementLayout.Append<HLSL::EType::Int>(ParameterName);
				}
				else if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				{
					InputElementDesc.Format = DXGI_FORMAT_R32_FLOAT;
					VertexBufferElementLayout.Append<HLSL::EType::Float>(ParameterName);
				}
				else
				{
					assert(false && "Unsupported Type.");
				}
			}
			else if (SignatureParameterDesc.Mask <= 3) /** 0b0011 Two component */
			{
				if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				{
					InputElementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
					assert(false && "TODO: Float2 Support.");
				}
				else
				{
					assert(false && "Unsupported Type.");
				}
			}
			else if (SignatureParameterDesc.Mask <= 7) /** 0b0111 Three component */
			{
				if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				{
					InputElementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
					VertexBufferElementLayout.Append<HLSL::EType::Float3>(ParameterName);
				}
				else
				{
					assert(false && "Unsupported Type.");
				}
			}
			else if (SignatureParameterDesc.Mask <= 15) /** 0b1111 Four component */
			{
				if (SignatureParameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
				{
					InputElementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
					VertexBufferElementLayout.Append<HLSL::EType::Float4>(ParameterName);
				}
				else
				{
					assert(false && "Unsupported Type.");
				}
			}
			else
			{
				assert(false && "Unsupported component size.");
			}

			InputElementDescs.push_back(InputElementDesc);
		}

		VertexBufferElementLayout.Finalize();

		/** TODO: Check HResult */
		/*
		Device->CreateInputLayout(
			InputElementDescs.data(),
			static_cast<UINT>(InputElementDescs.size()),
			ShaderBlob->GetBufferPointer(),
			ShaderBlob->GetBufferSize(),
			InputLayout.ReleaseAndGetAddressOf()
		);
		*/
	}

	private:
	/** For Vertex Shader */
	//Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
	UBufferElementLayout VertexBufferElementLayout;
	//UDynamicBuffer VertexDynamicBuffer;

	/** Constant Buffer Maps */
	TMap<FString, Microsoft::WRL::ComPtr<ID3D11Buffer>> ConstantBufferMap;
	TMap<FString, FConstantBufferInfo> ConstantBufferInfoMap;
	TMap<FString, UDynamicBuffer> ConstantDynamicBufferMap;
};
