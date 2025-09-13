struct test
{
    float xy;
};

struct nested_test
{
    float3 f;
    test trs;
};

struct nn_test
{
    int x;
    nested_test t;
};

cbuffer constants : register(b0)
{
    row_major float4x4 MVP;
    float x;
    float y;
    float3 vec3;
    float4 vec;
    bool helloo;
    bool world;
    bool bIs;
    bool bIsIs;
    nn_test hello;
}

struct VS_INPUT
{
    float4 Position : POSITION; // Input position from vertex buffer
    float4 Color : COLOR; // Input color from vertex buffer
    float3 Hello : TEXCOORD1;
    int y : POSITION2;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION; // Transformed position to pass the pixel shader
    float4 Color : COLOR; // Color to pass to the pixel shader
};

PS_INPUT main(VS_INPUT Input)
{
    PS_INPUT Output;

    // 위치를 MVP 행렬로 변환
    Output.Position = mul(Input.Position, MVP);

    // 색상 그대로 전달
    Output.Color = Input.Color;

    return Output;
}
