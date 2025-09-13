#include <iostream>
#include <stdexcept>
#include <iomanip> // For std::boolalpha

#include "DynamicBuffer.h"
#include "ShaderReflection.h"
#include "ShaderDebug.h"

// Simple test utility
void check(bool condition, const std::string& testName) {
    std::cout << "  [Test] " << std::left << std::setw(40) << testName << ": "
        << (condition ? "PASSED" : "FAILED") << std::endl;
    if (!condition) {
        throw std::runtime_error("Test failed: " + testName);
    }
}

// =================================================================================
// TEST CASES
// =================================================================================

/**
 * @brief Tests basic layout creation, buffer initialization, setting, and getting values.
 */
void test_simple_layout() {
    std::cout << "\n## Testing Simple Layout ##" << std::endl;

    UBufferElementLayout layout;
    layout.Append<HLSL::EType::Float3>("Position");
    layout.Append<HLSL::EType::Int>("EntityID");
    layout.Append<HLSL::EType::Bool>("bIsVisible");

    UDynamicBuffer buffer(std::move(layout));

    // Check stride calculation
    size_t expectedStride = sizeof(FVector) + sizeof(int32_t) + sizeof(HLSL::bool32);
    check(buffer.GetLayout().GetStride() == expectedStride, "Correct Stride Calculation");

    // Set values
    FVector pos = { 10.0f, 20.0f, 30.0f };
    int32_t id = 42;
    HLSL::bool32 visibility = true;

    buffer[0]["Position"] = pos;
    buffer[0]["EntityID"] = id;
    buffer[0]["bIsVisible"] = visibility;

    // Get and verify values
    FVector outPos = buffer[0]["Position"];
    int32_t outId = buffer[0]["EntityID"];
    bool outVisibility = buffer[0]["bIsVisible"]; // Reading into a C++ bool

    check(outPos.x == pos.x && outPos.y == pos.y && outPos.z == pos.z, "FVector Get/Set");
    check(outId == id, "int32_t Get/Set");
    check(outVisibility == true, "bool32 Get/Set");
}

/**
 * @brief Tests a buffer that contains an array of elements.
 */
void test_array_of_elements() {
    std::cout << "\n## Testing Array of Elements ##" << std::endl;

    UBufferElementLayout layout;
    layout.Append<HLSL::EType::Float>("Value");
    layout.Append<HLSL::EType::Int>("Index");

    const size_t count = 5;
    UDynamicBuffer buffer(std::move(layout), count);

    check(buffer.GetCount() == count, "Buffer element count");

    // Set values for all elements
    for (size_t i = 0; i < count; ++i) {
        buffer[i]["Value"] = static_cast<float>(i) * 1.5f;
        buffer[i]["Index"] = static_cast<int32_t>(i);
    }

    // Verify values
    bool all_match = true;
    for (size_t i = 0; i < count; ++i) {
        float val = buffer[i]["Value"];
        int32_t idx = buffer[i]["Index"];
        if (val != static_cast<float>(i) * 1.5f || idx != static_cast<int32_t>(i)) {
            all_match = false;
            break;
        }
    }
    check(all_match, "Array Get/Set consistency");
}

/**
 * @brief Tests a layout with a nested struct, a key feature.
 * @note This test requires the suggested correction in DynamicBuffer.h to work.
 */
void test_nested_struct() {
    std::cout << "\n## Testing Nested Struct ##" << std::endl;
    std::cout << "  NOTE: This requires the corrected UBufferElementLayout::operator[]" << std::endl;

    UBufferElementLayout sceneLayout;
    sceneLayout.Append<HLSL::EType::Matrix>("ViewProjection");
    sceneLayout.Append<HLSL::EType::Struct>("Light");

    // Define the nested struct's layout using the (corrected) operator[]
    auto& lightLayout = sceneLayout["Light"];
    lightLayout.Append<HLSL::EType::Float3>("Position");
    lightLayout.Append<HLSL::EType::Float4>("Color");
    lightLayout.Append<HLSL::EType::Float>("Intensity");

    UDynamicBuffer buffer(std::move(sceneLayout));

    // Check stride
    size_t expectedLightStride = sizeof(FVector) + sizeof(FVector4) + sizeof(float);
    size_t expectedTotalStride = sizeof(FMatrix) + expectedLightStride;
    check(buffer.GetLayout().GetStride() == expectedTotalStride, "Nested stride calculation");

    // Set values at both levels
    FMatrix viewProj = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };
    FVector lightPos = { 100.f, 200.f, 50.f };
    FVector4 lightColor = { 1.0f, 0.8f, 0.5f, 1.0f };
    float intensity = 550.0f;

    buffer[0]["ViewProjection"] = viewProj;
    buffer[0]["Light"]["Position"] = lightPos;
    buffer[0]["Light"]["Color"] = lightColor;
    buffer[0]["Light"]["Intensity"] = intensity;

    // Verify values
    FVector outLightPos = buffer[0]["Light"]["Position"];
    FVector4 outLightColor = buffer[0]["Light"]["Color"];
    float outIntensity = buffer[0]["Light"]["Intensity"];

    check(outLightPos.x == lightPos.x && outLightPos.y == lightPos.y, "Nested FVector Get/Set");
    check(outLightColor.x == lightColor.x && outLightColor.y == lightColor.y, "Nested FVector4 Get/Set");
    check(outIntensity == intensity, "Nested float Get/Set");
}

/**
 * @brief Tests a layout with a very deeply nested struct.
 */
void test_deep_nested_struct() {
    std::cout << "\n## Testing Deeply Nested Struct ##" << std::endl;

    // Create the top-level layout
    UBufferElementLayout rootLayout;
    rootLayout.Append<HLSL::EType::Float>("RootValue");
    rootLayout.Append<HLSL::EType::Struct>("Level1");

    // Nesting structs four levels deep
    auto& level1Layout = rootLayout["Level1"];
    level1Layout.Append<HLSL::EType::Int>("Level1_ID");
    level1Layout.Append<HLSL::EType::Struct>("Level2");

    auto& level2Layout = level1Layout["Level2"];
    level2Layout.Append<HLSL::EType::Matrix>("Level2_Matrix");
    level2Layout.Append<HLSL::EType::Struct>("Level3");

    auto& level3Layout = level2Layout["Level3"];
    level3Layout.Append<HLSL::EType::Float3>("Level3_Position");
    level3Layout.Append<HLSL::EType::Struct>("Level4");

    auto& level4Layout = level3Layout["Level4"];
    level4Layout.Append<HLSL::EType::Bool>("Level4_Visibility");
    level4Layout.Append<HLSL::EType::Float4>("Level4_Color");

    UDynamicBuffer buffer(std::move(rootLayout));

    // Print the layout for debugging
    std::cout << "\n--- Deeply Nested Layout --- \n";
    buffer.GetLayout().Print(std::cout);
    std::cout << "---------------------------\n";

    // Calculate the expected stride
    size_t expectedLevel4Stride = sizeof(HLSL::bool32) + sizeof(FVector4);
    size_t expectedLevel3Stride = sizeof(FVector) + expectedLevel4Stride;
    size_t expectedLevel2Stride = sizeof(FMatrix) + expectedLevel3Stride;
    size_t expectedLevel1Stride = sizeof(int32_t) + expectedLevel2Stride;
    size_t expectedTotalStride = sizeof(float) + expectedLevel1Stride;

    check(buffer.GetLayout().GetStride() == expectedTotalStride, "Deeply nested stride calculation");

    // Set values at each level
    float rootValue = 123.45f;
    int32_t level1Id = 101;
    FMatrix level2Matrix;
    // Note: Use direct array initialization for FMatrix
    level2Matrix.mat[0][0] = 1.0f;
    level2Matrix.mat[3][3] = 16.0f;

    FVector level3Pos = { 11.1f, 22.2f, 33.3f };
    bool level4Vis = true;
    FVector4 level4Color = { 0.1f, 0.2f, 0.3f, 0.4f };

    buffer[0]["RootValue"] = rootValue;
    buffer[0]["Level1"]["Level1_ID"] = level1Id;
    buffer[0]["Level1"]["Level2"]["Level2_Matrix"] = level2Matrix;
    buffer[0]["Level1"]["Level2"]["Level3"]["Level3_Position"] = level3Pos;
    buffer[0]["Level1"]["Level2"]["Level3"]["Level4"]["Level4_Visibility"] = level4Vis;
    buffer[0]["Level1"]["Level2"]["Level3"]["Level4"]["Level4_Color"] = level4Color;

    // Verify values at each level using the implicit cast operator
    float outRootValue = buffer[0]["RootValue"];
    check(outRootValue == rootValue, "Root value Get/Set");

    int32_t outLevel1Id = buffer[0]["Level1"]["Level1_ID"];
    check(outLevel1Id == level1Id, "Level 1 ID Get/Set");

    // For matrices, just check a couple of elements for simplicity
    FMatrix outLevel2Matrix = buffer[0]["Level1"]["Level2"]["Level2_Matrix"];
    check(outLevel2Matrix.mat[0][0] == level2Matrix.mat[0][0] && outLevel2Matrix.mat[3][3] == level2Matrix.mat[3][3], "Level 2 Matrix Get/Set");

    FVector outLevel3Pos = buffer[0]["Level1"]["Level2"]["Level3"]["Level3_Position"];
    check(outLevel3Pos.x == level3Pos.x, "Level 3 position Get/Set");

    bool outLevel4Vis = buffer[0]["Level1"]["Level2"]["Level3"]["Level4"]["Level4_Visibility"];
    check(outLevel4Vis == level4Vis, "Level 4 visibility Get/Set");

    FVector4 outLevel4Color = buffer[0]["Level1"]["Level2"]["Level3"]["Level4"]["Level4_Color"];
    check(outLevel4Color.w == level4Color.w, "Level 4 color Get/Set");
}

void test_mixed_types_and_alignment() {
    std::cout << "\n## Testing Mixed Types and Alignment ##" << std::endl;

    UBufferElementLayout layout;
    layout.Append<HLSL::EType::Float4>("Color");
    layout.Append<HLSL::EType::Bool>("bIsEnabled");
    layout.Append<HLSL::EType::Float>("Alpha");
    layout.Append<HLSL::EType::Int>("Index");

    UDynamicBuffer buffer(std::move(layout));

    // Expected stride calculation
    size_t expectedStride = sizeof(FVector4) + sizeof(HLSL::bool32) + sizeof(float) + sizeof(int32_t);
    check(buffer.GetLayout().GetStride() == expectedStride, "Mixed types stride calculation");

    // Set and verify values
    FVector4 color = { 0.5f, 0.5f, 0.5f, 1.0f };
    bool isEnabled = true;
    float alpha = 0.75f;
    int32_t index = 123;

    buffer[0]["Color"] = color;
    buffer[0]["bIsEnabled"] = isEnabled;
    buffer[0]["Alpha"] = alpha;
    buffer[0]["Index"] = index;

    FVector4 outColor = buffer[0]["Color"];
    bool outIsEnabled = buffer[0]["bIsEnabled"];
    float outAlpha = buffer[0]["Alpha"];
    int32_t outIndex = buffer[0]["Index"];

    check(outColor.x == color.x && outColor.w == color.w, "FVector4 Get/Set");
    check(outIsEnabled == isEnabled, "bool Get/Set");
    check(outAlpha == alpha, "float Get/Set");
    check(outIndex == index, "int32_t Get/Set");
}

void test_multiple_elements_in_struct() {
    std::cout << "\n## Testing Multiple Elements in Struct ##" << std::endl;

    UBufferElementLayout layout;
    layout.Append<HLSL::EType::Float>("X");
    layout.Append<HLSL::EType::Float>("Y");

    const size_t elementCount = 3;
    UDynamicBuffer buffer(std::move(layout), elementCount);

    check(buffer.GetCount() == elementCount, "Correct element count");

    // Set values for two distinct elements
    buffer[0]["X"] = 1.0f;
    buffer[0]["Y"] = 2.0f;

    buffer[1]["X"] = 3.0f;
    buffer[1]["Y"] = 4.0f;

    buffer[2]["X"] = 5.0f;
    buffer[2]["Y"] = 6.0f;

    // Verify values, ensuring element boundaries are respected
    float x0 = buffer[0]["X"];
    float y0 = buffer[0]["Y"];
    check(x0 == 1.0f, "Element 0 - X");
    check(y0 == 2.0f, "Element 0 - Y");

    float x1 = buffer[1]["X"];
    float y1 = buffer[1]["Y"];
    check(x1 == 3.0f, "Element 1 - X");
    check(y1 == 4.0f, "Element 1 - Y");

    float x2 = buffer[2]["X"];
    float y2 = buffer[2]["Y"];
    check(x2 == 5.0f, "Element 2 - X");
    check(y2 == 6.0f, "Element 2 - Y");
}

void test_edge_cases() {
    std::cout << "\n## Testing Edge Cases ##" << std::endl;

    // Test 1: Empty Layout
    {
        UBufferElementLayout layout;
        UDynamicBuffer buffer(std::move(layout));
        check(buffer.GetLayout().GetStride() == 0, "Empty layout stride is 0");
        check(buffer.GetCount() == 0, "Empty layout count is 0");
    }

    // Test 2: Layout with an Empty Struct
    {
        UBufferElementLayout layout;
        layout.Append<HLSL::EType::Int>("ValueBefore");
        layout.Append<HLSL::EType::Struct>("EmptyStruct"); // No members added
        layout.Append<HLSL::EType::Int>("ValueAfter");

        UDynamicBuffer buffer(std::move(layout));
        size_t expectedStride = sizeof(int32_t) + 0 + sizeof(int32_t);
        check(buffer.GetLayout().GetStride() == expectedStride, "Empty struct has 0 stride");

        buffer[0]["ValueBefore"] = 111;
        buffer[0]["ValueAfter"] = 999;

        int32_t before = buffer[0]["ValueBefore"];
        int32_t after = buffer[0]["ValueAfter"];
        check(before == 111 && after == 999, "Data around empty struct");
    }
}

// =================================================================================
// MAIN
// =================================================================================

int main() {
    // Use std::boolalpha for prettier boolean output
    std::cout << std::boolalpha;

    try {
        test_simple_layout();
        test_array_of_elements();
        test_nested_struct();
        test_deep_nested_struct();
        test_mixed_types_and_alignment();
        test_multiple_elements_in_struct();
        test_edge_cases();
    }
    catch (const std::exception& e) {
        std::cerr << "\n*** A test failed with an exception: " << e.what() << " ***" << std::endl;
        return 1;
    }

    std::cout << "\n==================================="
              << "\nAll tests completed successfully!"
              << "\n===================================" << std::endl;

    std::cout << "\n===================================" << std::endl;
    std::cout << "Shader tests Start!" << std::endl;
    std::cout << "===================================" << std::endl;

    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return -1;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
    hr = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
        IID_PPV_ARGS(&reflector));
    if (FAILED(hr)) {
        std::cerr << "Shader reflection failed." << std::endl;
        return -1;
    }

    PrintCBuffers(reflector.Get());

    UShaderReflection ShaderReflection(nullptr, shaderBlob.Get());
    auto VertexBufferElementLayout = ShaderReflection.GetVertexBufferElementLayout();
    VertexBufferElementLayout.Print(std::cout);
    std::cout << "------------------------------------------------\n";
    for (const auto& [name, buffer] : ShaderReflection.GetConstantDynamicBufferMap())
    {
        std::cout << "Name: " << name << "\n";
        buffer.GetLayout().Print(std::cout);
    }
    return 0;
}
