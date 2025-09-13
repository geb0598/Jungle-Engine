#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <stdexcept> // For std::runtime_error

/** Temporary Using Keywords (Following UE Conventions) */
template<typename T>
using TArray = std::vector<T>;

using FString = std::string;

template<typename T, typename U>
using TMap = std::unordered_map<T, U>;

template<typename T>
using TUniquePtr = std::unique_ptr<T>;

/** Temporary Definitions */
struct FVector { float x, y, z; };
struct FVector4 { float x, y, z, w; };
struct FMatrix { float mat[4][4]; };

namespace HLSL
{
    /**
     * @brief Defines the data types that can be represented in the buffer.
     */
    enum class EType
    {
        Bool,
        Int,
        Float,
        Float3,
        Float4,
        Matrix,
        Struct,
        Padding /** NOTE: Special data type introduced to implement padding */
    };

    // Use a 32-bit integer for HLSL bools, as they are typically 4 bytes.
    using bool32 = uint32_t;

    /**
     * @brief Maps HLSL::EType enum to a C++ type and its size.
     */
    template<EType Type>
    struct TypeInfo;

    template<> 
    struct TypeInfo<EType::Bool> 
    { 
        using InternalType = bool32; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Bool";
    };
    template<> 
    struct TypeInfo<EType::Int> 
    { 
        using InternalType = int32_t; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Int";
    };
    /** TODO: Is float always 32-bit? */
    template<> 
    struct TypeInfo<EType::Float> 
    { 
        using InternalType = float; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Float";
    };
    template<> 
    struct TypeInfo<EType::Float3> 
    { 
        using InternalType = FVector; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Float3";
    };
    template<> 
    struct TypeInfo<EType::Float4> 
    { 
        using InternalType = FVector4; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Float4";
    };
    template<> 
    struct TypeInfo<EType::Matrix> 
    { 
        using InternalType = FMatrix; 
        static constexpr size_t Size = sizeof(InternalType); 
        static constexpr const char* Name = "Matrix";
    };
    /** NOTE: Struct has no predefined InternalType or Size. */
    // template<> struct TypeInfo<EType::Struct> {}

} // namespace HLSL

/** TODO: What if duplicated names? */
/** TODO: Unanonymous paddings? */
/** NOTE: Never access elements with index unless you fully know about padding. */
class UBufferElementLayout
{
public:
    struct FField
    {
        HLSL::EType Type;
        FString Name;
        size_t Stride;
        size_t Offset;
        TUniquePtr<UBufferElementLayout> Layout;
    };

public:
    ~UBufferElementLayout() = default;

    UBufferElementLayout() : Stride(0), bIsFinalized(false) {}

    /** Allow move construction and assignment. */
    UBufferElementLayout(const UBufferElementLayout&) = delete;
    UBufferElementLayout(UBufferElementLayout&&) noexcept = default;

    UBufferElementLayout& operator=(const UBufferElementLayout&) = delete;
    UBufferElementLayout& operator=(UBufferElementLayout&&) noexcept = default;

    UBufferElementLayout& operator[](const char* Name)
    {
        return (*this)[FString(Name)];
    }

    UBufferElementLayout& operator[](const FString& Name)
    {
        assert(!bIsFinalized && "Finalized layout is not subscriptable.");

        assert(FieldIndexMap.count(Name) && "Struct should be appended before using.");

        auto& Field = Fields[FieldIndexMap[Name]];
        assert(Field.Type == HLSL::EType::Struct && "Only struct is subscriptable");

        return *(Field.Layout.get());
    }

    template<HLSL::EType Type>
    void Append(FString InName)
    {
        assert(!bIsFinalized && "Cannot append to finalized layout.");

        FField Field    = {};
        Field.Type      = Type;
        Field.Name      = InName;
        /** NOTE: Should use if constexpr to ignore else branch in compilation time. */
        if constexpr (Type == HLSL::EType::Struct)
        {
            Field.Stride = 0; /** Deferred */
            Field.Layout = std::make_unique<UBufferElementLayout>();
        }
        else
        {
            Field.Stride = HLSL::TypeInfo<Type>::Size;
            Field.Layout = nullptr;
        }
        Field.Offset = 0; /** Deferred */

        Fields.push_back(std::move(Field));

        FieldIndexMap[InName] = Fields.size() - 1;
    }

    /** NOTE: Append Struct */
    void AppendStruct(FString InName, UBufferElementLayout Layout)
    {
        assert(!bIsFinalized && "Cannot append to finalized layout.");

        FField Field    = {};
        Field.Type      = HLSL::EType::Struct;
        Field.Name      = InName;
        Field.Stride    = 0;
        Field.Layout    = std::make_unique<UBufferElementLayout>(std::move(Layout));
        Field.Offset    = 0;

        Fields.push_back(std::move(Field));

        FieldIndexMap[InName] = Fields.size() - 1;
    }

    /** NOTE: Append Padding */
    /** TODO: Introduce unique identifier for paddings (e.g., name mangling) or just exclude it from FieldIndexMap. */
    void AppendPadding(size_t Bytes)
    {
        assert(!bIsFinalized && "Cannot append to finalized layout.");
        assert(Bytes >= 0 && "Padding size should be larger or equal to 0.");

        if (Bytes == 0)
        {
            return; /** Do nothing when padding size is 0. */
        }

        FField Field    = {};
        Field.Type      = HLSL::EType::Padding;
        Field.Name      = "Padding";
        Field.Stride    = Bytes;
        Field.Layout    = nullptr;
        Field.Offset    = 0; /** Deferred */

        Fields.push_back(std::move(Field));
        
        /** No entry in FieldIndexMap to keep it inaccessible from user */
    }

    UBufferElementLayout Clone() const
    {
        assert(bIsFinalized && "Cannot clone before being finalized.");

        UBufferElementLayout Layout;

        for (const auto& Field : Fields)
        {
            FField NewField = {};
            NewField.Type   = Field.Type;
            NewField.Name   = Field.Name;
            NewField.Stride = Field.Stride;
            NewField.Offset = Field.Offset;
            
            if (Field.Type == HLSL::EType::Struct && Field.Layout)
            {
                NewField.Layout = std::make_unique<UBufferElementLayout>(Field.Layout->Clone());
            }
            else
            {
                NewField.Layout = nullptr;
            }
			Layout.Fields.push_back(std::move(NewField));
            if (Field.Type != HLSL::EType::Padding)
            {
                Layout.FieldIndexMap[Field.Name] = Layout.Fields.size() - 1;
            }
        }
        Layout.bIsFinalized = true;

        return Layout;
    }

    size_t GetStride() const
    {
        assert(bIsFinalized && "Cannot get stride before being finalized.");

        return Stride;
    }

    /** NOTE: It doesn't affect bIsFinalized and state of fields. */
    /** NOTE: This is introduced for padding. */
    size_t GetCurrentStride() const
    {
        size_t Stride = 0;
        for (const auto& Field : Fields)
        {
            if (Field.Type == HLSL::EType::Struct)
            {
                if (!Field.Layout)
                {
                    continue;
                }
                Stride += Field.Layout->GetCurrentStride();
            }
            else
            {
                Stride += Field.Stride;
            }
        }
        return Stride;
    }

    const TArray<FField>& GetFields() const
    {
        assert(bIsFinalized && "Cannot get fields before being finalized.");

        return Fields;
    }

    const FField& GetField(size_t Index) const
    {
        assert(bIsFinalized && "Cannot get field before being finalized.");

        /** TODO: Throw exception when OOB? */
        return Fields[Index];
    }

    const FField& GetField(const FString& Name) const
    {
        assert(bIsFinalized && "Cannot get field before being finalized.");

        /** TODO: Throw exception when key not found? */
        auto it = FieldIndexMap.find(Name);
        return GetField(it->second);
    }

    /** Recursive Function */
    void Print(std::ostream& Stream, int IndentLevel = 0) const
    {
		assert(bIsFinalized && "Cannot print layout before being finalized.");

		auto PrintIndent = [&](int Level) {
			for (int i = 0; i < Level; ++i)
			{
				Stream << "  ";
			}
		};

		for (const auto& Field : Fields)
		{
			PrintIndent(IndentLevel);
			Stream << Field.Name << " (Offset: " << Field.Offset << ", Size: " << Field.Stride << ", Type: ";

			switch (Field.Type)
			{
			case HLSL::EType::Bool:
				Stream << "Bool";
				break;
			case HLSL::EType::Int:
				Stream << "Int";
				break;
			case HLSL::EType::Float:
				Stream << "Float";
				break;
			case HLSL::EType::Float3:
				Stream << "Float3";
				break;
			case HLSL::EType::Float4:
				Stream << "Float4";
				break;
			case HLSL::EType::Matrix:
				Stream << "Matrix";
				break;
			case HLSL::EType::Struct:
				Stream << "Struct";
				break;
			case HLSL::EType::Padding:
				Stream << "Padding";
				break;
			}
			Stream << ")" << std::endl;

			if (Field.Type == HLSL::EType::Struct)
			{
				Field.Layout->Print(Stream, IndentLevel + 1);
			}
		}
    }

    void Finalize()
    {
        assert(!bIsFinalized && "Cannot finalize a layout for multiple times");

        size_t Offset = 0;
        for (size_t i = 0; i < Fields.size(); ++i)
        {
            auto& Field = Fields[i];
            Field.Offset = Offset;

            if (Field.Type == HLSL::EType::Struct)
            {
                /** TODO: Is it safe to allow empty struct? */
                if (!Field.Layout)
                {
                    continue;
                }
                Field.Layout->Finalize();
                Field.Stride = Field.Layout->GetStride();
            }

            Offset += Field.Stride;
        }

        Stride = Offset;
        bIsFinalized = true;
    }

private:
    TArray<FField> Fields;
    /** Retrieve index of Field in Fields through its name. */
    TMap<FString, size_t> FieldIndexMap;

    size_t Stride;

    bool bIsFinalized;
};

class UBufferElement
{
public:
    using value_type = char;

    ~UBufferElement() = default;

    UBufferElement(value_type* InBufferData, const UBufferElementLayout* InLayout)
        : BufferData(InBufferData), Layout(InLayout), Field(nullptr)
    {
        /** TODO: What if Layout is nullptr due to empty struct? */
        assert(BufferData && "Buffer Data must not be a null pointer.");
    }

    UBufferElement operator[](const FString& Name)
    {
        const auto& Field = Layout->GetField(Name);

        if (Field.Type == HLSL::EType::Struct)
        {
            return UBufferElement(BufferData + Field.Offset, Field.Layout.get(), &Field);
        }
        else
        {
            return UBufferElement(BufferData + Field.Offset, Layout, &Field);
        }
    }

    /** TODO: What if HLSL type size is mismatched with C++ type size? */
	/** TODO: Should I check is_trivially_copyable before using memcpy? */
    template<typename TValue>
    UBufferElement& operator=(const TValue& Value)
    {
        assert(Field && Field->Type != HLSL::EType::Struct && "Cannot assign a value to a struct.");
        // Special case for C++ bool type to match HLSL::bool32
        if constexpr (std::is_same_v<TValue, bool>)
        {
            assert(Field->Type == HLSL::EType::Bool && "Field is not a bool type.");
            assert(Field->Stride == sizeof(HLSL::bool32) && "Mismatched size for bool type.");

            HLSL::bool32 BoolValue = Value; // Converts true to 1 and false to 0
            std::memcpy(BufferData, &BoolValue, sizeof(HLSL::bool32));
        }
        else // For all other types
        {
            assert(Field->Stride == sizeof(TValue) && "Mismatched size between provided type and layout field.");
            std::memcpy(BufferData, &Value, sizeof(TValue));
        }

        return *this;
    }

    /** NOTE: Be careful to use assign data from buffer into a variable. It would cause an UB. */
    /** TODO: Improve Type stability. */
    template <typename Type>
    operator Type() const
    {
        assert(Field && Field->Type != HLSL::EType::Struct && "Cannot read a value from a struct.");

        /** TODO: Improve bool support. */
        if constexpr (std::is_same_v<Type, bool>)
        {
            assert(Field->Type == HLSL::EType::Bool && "Field is not a bool type.");
            assert(Field->Stride == sizeof(HLSL::bool32) && "MIsmtached size for bool type.");
            return *reinterpret_cast<const HLSL::bool32*>(BufferData) != 0;
        }
        else
        {
            assert(Field->Stride == sizeof(Type) && "MIsmtached size between requested type and layout field");
            return *reinterpret_cast<const Type*>(BufferData);
        }
        /** NOTE: For now, there is only one case of size mismatch between HLSL and C++. */
    }

private:
    UBufferElement(value_type* InBufferData, const UBufferElementLayout* InLayout, const UBufferElementLayout::FField* InField)
        : BufferData(InBufferData), Layout(InLayout), Field(InField) {
    }

    value_type* BufferData;
    const UBufferElementLayout* Layout;
    const UBufferElementLayout::FField* Field;
};

class UDynamicBuffer
{
public:
    using value_type = char;

    UDynamicBuffer(UBufferElementLayout InLayout)
        : Layout(std::move(InLayout))
    {
        Layout.Finalize();

        Buffer.resize(Layout.GetStride());
    }

    UDynamicBuffer(UBufferElementLayout InLayout, size_t Count)
        : Layout(std::move(InLayout))
    {
        Layout.Finalize();

        Buffer.resize(Layout.GetStride() * Count);
    }

    UBufferElement operator[](size_t Index)
    {
        assert(Index < GetCount() && "Buffer access Out-of-Bounds.");
        return UBufferElement(const_cast<value_type*>(Buffer.data()) + Index * Layout.GetStride(), &Layout);
    }

    size_t GetCount() const
    {
        if (Layout.GetStride() > 0)
        {
            return Buffer.size() / Layout.GetStride();
        }
        else
        {
            /** NOTE: Be careful not to divide-by-zero. */
            return 0;
        }
    }

#ifndef NDEBUG
    /** NOTE: Do not use this method except for debug purpose. */
    const UBufferElementLayout& GetLayout() const
    {
        return Layout;
    }
#endif

private:
    TArray<value_type> Buffer;
    UBufferElementLayout Layout;
};
