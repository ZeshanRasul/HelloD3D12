#pragma once
#include "stdafx.h"
#include <chrono>
#include <unordered_map>

const int gNumFrameResources = 3;

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 transform;
};

#define MaxLights 16

struct Light
{
	DirectX::XMFLOAT3 Strength; // Light colour
	float FalloffStart; // point/spot light only
	DirectX::XMFLOAT3 Direction; // Directional/spot light only
	float FalloffEnd; // point/spot light only
	DirectX::XMFLOAT3 Position; // point/spot light only
	float SpotPower; // spot light only
};

struct PassConstants
{
	DirectX::XMFLOAT3 eyePosW;
	float padding;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 proj;
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Indices[0, NUM_DIR_LIGHTS] are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS] are
	// point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, 
	// NUM_DIR_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS]
	// are spot lights for a maximum of MaxLights per object.
	Light Lights [MaxLights];

};

struct Material
{
	std::string Name;

	//Index into CBuffer for this material
	int MaterialCBIndex = -1;

	// Index into shader resource view heap for diffuse texture. Used in texturing.
	int DiffuseSrvHeapIndex = -1;

	int NumFramesDirety = gNumFrameResources;

	// Material constant buffer data used for shading
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };

	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };

	float Roughness = 0.25f;

	DirectX::XMFLOAT4X4 MaterialTransform =
	{ 1.0f, 0.0f, 0.0f, 0.0f,
	  0.0f, 1.0f, 0.0f, 0.0f,
	  0.0f, 0.0f, 1.0f, 0.0f,
	  0.0f, 0.0f, 0.0f, 1.0 };
};

struct MaterialConstants
{
	// Material constant buffer data used for shading
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };

	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };

	float Roughness = 0.25f;

	DirectX::XMFLOAT4X4 MaterialTransform =
	{ 1.0f, 0.0f, 0.0f, 0.0f,
	  0.0f, 1.0f, 0.0f, 0.0f,
	  0.0f, 0.0f, 1.0f, 0.0f,
	  0.0f, 0.0f, 0.0f, 1.0 };
};

class Graphics
{
public:
	Graphics();
	~Graphics();

	void Init(HWND hWnd);
	void Shutdown();
	void Update();
	void Render();

	void GetHardwareAdapter(Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> ppAdapter);

	void CreateCommandQueue();

	void CreateSwapChain(HWND hWnd);
	
	void CreateRTVDescriptorHeap();

	void CreateFrameResources();

	void CreateRootSignature();

	void CompileShaders();

	void CreatePipelineState();

	void CreateCommandList();

	void CloseCommandList();

	void CreateVertexBuffer();

	void CreateFence();

	void WaitForPreviousFrame();

	void PopulateCommandList();

	void CreateDepthStencilView();

	void CreateConstantBuffer();

	UINT CalcConstantBufferByteSize(UINT byteSize);

	void OnMouseMove(WPARAM buttonState, int x, int y);

	void OnMouseDown(WPARAM buttonState, int x, int y);

	void OnMouseUp();

	void BuildMaterials();


public:
	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

		std::wstring ToString()const;

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};
	
private:
	static const int SwapChainBufferCount = 2;
	UINT pRTVDescriptorSize;
	UINT indicesSize;

	Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pWarpAdapter;
	Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory;
	Microsoft::WRL::ComPtr<ID3D12Debug> pDebugController;
	Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> pSwapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDSVDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pConstantBufferDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pMatCBufDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pLightsCBDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> pRenderTargets[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> pDepthStencilView;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> pVertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pPixelShaderBlob;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList;
	Microsoft::WRL::ComPtr<ID3D12Resource> pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> pIndexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> pConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> pMaterialConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> pLightsConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Fence> pFence;

	D3D12_VIEWPORT pVP;

	D3D12_RECT pScissorRect;

	D3D12_VERTEX_BUFFER_VIEW pVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW pIndexBufferView;
	UINT64 pFenceValue;
	HANDLE pFenceEvent;
	UINT pFrameIndex;

	float pTheta = 1.5f * DirectX::XM_PI;
	float pPhi = DirectX::XM_PIDIV4;
	float pRadius = 1.0f;

	POINT pLastMousePos;

	std::chrono::steady_clock::time_point last;
	float dt;
	float pDx;
	float pDy;

	std::unordered_map<std::string, std::unique_ptr<Material>> pMaterials;
};




