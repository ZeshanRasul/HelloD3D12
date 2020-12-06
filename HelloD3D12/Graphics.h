#pragma once
#include "stdafx.h"

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
	Microsoft::WRL::ComPtr<ID3D12Fence> pFence;

	D3D12_VIEWPORT pVP;

	D3D12_RECT pScissorRect;

	D3D12_VERTEX_BUFFER_VIEW pVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW pIndexBufferView;
	UINT64 pFenceValue;
	HANDLE pFenceEvent;
	UINT pFrameIndex;
};



