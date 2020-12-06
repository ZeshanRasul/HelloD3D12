#include "Graphics.h"

Graphics::Graphics()
{
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Init(HWND hWnd)
{
	//////////////////////////////
	// 1) INITIALIZE PIPELINE ////
	//////////////////////////////

	// Create Factory 
	CreateDXGIFactory1(_uuidof(pFactory), (void**)&pFactory);

	// Query hardware adapters
	GetHardwareAdapter(pFactory, pAdapter);

	// Enable debug layer

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
	{
		pDebugController->EnableDebugLayer();
	}
	// Create the device
	if (pAdapter == nullptr)
	{
		D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), &pDevice);
	}
	else
	{
		D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), &pDevice);
	}

	// Create command queue
	CreateCommandQueue();

	// Create swap chain
	CreateSwapChain(hWnd);

	// Create render target view descriptor heap
	CreateRTVDescriptorHeap();

	// Create frame resources (RTV for each frame)
	CreateFrameResources();

	// Create a command allocator
	ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &pCommandAllocator));

	//////////////////////////////
	// 2) INITIALIZE ASSETS///////
	//////////////////////////////

	// Create empty root signature
	CreateRootSignature();

	// Compile shaders
	CompileShaders();

	
	
	// Create input element description to define vertex input layout and
	// Create pipeline state object description and object
	CreatePipelineState();

	// Create command list
	CreateCommandList();

	// Close command list
	CloseCommandList();

	// Create and load vertex buffers
	CreateVertexBuffer();

	// Create vertex buffer views
	// Create fence
	// Create event handle
	// Wait for GPU to complete (check on fence)
}

void Graphics::Shutdown()
{
	// Wait for GPU to finish (final check on fence)
	// Close event handle
}

void Graphics::Render()
{
	//////////////////////////////
	// POPULATE COMMAND LIST /////
	//////////////////////////////
	// Reset command list allocatory
	// Reset command list
	// Set graphics root signature
	// Set viewport and scissor rectangles
	// Set resource barrier indicating back buffer is to be used as render target
	// Record commands into command list
	// Indicate back buffer will be used to present after command list has executed
	// Close command list

	//////////////////////////////
	// EXECUTE COMMAND LIST //////
	//////////////////////////////

	//////////////////////////////
	// PRESENT COMMAND LIST //////
	//////////////////////////////

	//////////////////////////////
	// WAIT FOR GPU TO FINISH ////
	//////////////////////////////
	// Check on fence

}

void Graphics::GetHardwareAdapter(Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> ppAdapter)
{
	ppAdapter = nullptr;
	
	Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
	{
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter)); adapterIndex++)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_10_0, _uuidof(ID3D12Device), nullptr)))
			{

				break;
			}
		}
	}

	else
	{
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_10_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	ppAdapter = adapter.Detach();
	if (ppAdapter == nullptr)
	{
		factory6->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter));	
	}
}

void Graphics::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	pDevice->CreateCommandQueue(&cqDesc, __uuidof(ID3D12CommandQueue), &pCommandQueue);
}

Graphics::DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
	ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{
}

void Graphics::CreateSwapChain(HWND hWnd)
{
	Microsoft::WRL::ComPtr<IDXGIFactory2> pFactory2;
	pFactory->QueryInterface(IID_PPV_ARGS(&pFactory2));
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.Width = 1280;
	scDesc.Height = 960;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferCount = SwapChainBufferCount;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.SampleDesc.Count = 1;
	
	//DXGI_SWAP_CHAIN_FULLSCREEN_DESC scFSDesc = {};



	ThrowIfFailed(pFactory2->CreateSwapChainForHwnd(pCommandQueue.Get(), hWnd, &scDesc, nullptr, nullptr, &pSwapChain));
}

void Graphics::CreateRTVDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescHeapDesc = {};
	rtvDescHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&rtvDescHeapDesc, __uuidof(ID3D12DescriptorHeap), &pRTVDescriptorHeap));

	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

}

void Graphics::CreateFrameResources()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT n = 0; n < SwapChainBufferCount; n++)
	{
		ThrowIfFailed(pSwapChain->GetBuffer(n, IID_PPV_ARGS(&pRenderTargets[n])));
		pDevice->CreateRenderTargetView(pRenderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, pRTVDescriptorSize);
	}
}

void Graphics::CreateRootSignature()
{
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> signature;
	Microsoft::WRL::ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(ID3D12RootSignature), &pRootSignature));
}

void Graphics::CompileShaders()
{
#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	ThrowIfFailed(D3DCompileFromFile(L"Shaders/VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &pVertexShaderBlob, nullptr));
	ThrowIfFailed(D3DCompileFromFile(L"Shaders/PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pPixelShaderBlob, nullptr));
}

void Graphics::CreatePipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = pRootSignature.Get();
	psoDesc.VS = {reinterpret_cast<UINT8*>(pVertexShaderBlob->GetBufferPointer()), pVertexShaderBlob->GetBufferSize()};
	psoDesc.PS = {reinterpret_cast<UINT8*>(pPixelShaderBlob->GetBufferPointer()), pPixelShaderBlob->GetBufferSize()};
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState)));
}

void Graphics::CreateCommandList()
{
	ThrowIfFailed(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator.Get(), pPipelineState.Get(), __uuidof(ID3D12CommandList), &pCommandList));
}

void Graphics::CloseCommandList()
{
	ThrowIfFailed(pCommandList->Close());
}

void Graphics::CreateVertexBuffer()
{
	struct Vertex
	{
		DirectX::XMFLOAT2 position;
		DirectX::XMFLOAT4 colour;
	};

	Vertex vertices[] =
	{
		{{0.0f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
	};

	const UINT vertexBufferSize = sizeof(vertices);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_COMMON, nullptr,
		__uuidof(ID3D12Resource), &pVertexBuffer));
}