#include "Graphics.h"
#include <algorithm>
#include <array>

Graphics::Graphics()
	:
	pFrameIndex(0)
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

	// Create render target view descriptor heap and depth stencil view descriptor heap
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

	// Create Depth Stencil View
	CreateDepthStencilView();

	// Create input element description to define vertex input layout and
	// Create pipeline state object description and object
	CreatePipelineState();

	// Create command list
	CreateCommandList();

	// Close command list
	CloseCommandList();

	// Create and load vertex buffers 
	// and Copy vertices data to vertex buffer
	// and Create vertex buffer views
	CreateVertexBuffer();

	CreateConstantBuffer();

	// Create fence 	
	// and Create event handle
	CreateFence();

	// Wait for GPU to complete (check on fence)
	WaitForPreviousFrame();
}

void Graphics::Shutdown()
{
	// Wait for GPU to finish (final check on fence)
	WaitForPreviousFrame();
	// Close event handle
	CloseHandle(pFenceEvent);
}

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 transform;
};

void Graphics::Update()
{
	

	ConstantBuffer cb2;
	
	float x = pRadius * sinf(pPhi) * cosf(pTheta);
	float z = (pRadius * sinf(pPhi) * sinf(pTheta));
	float y = pRadius * cosf(pPhi);

	DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorZero();
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	DirectX::XMMATRIX world = DirectX::XMMatrixScaling(0.5f, 0.5f, 0.5f) * DirectX::XMMatrixRotationY(0.45f);
	DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();

	DirectX::XMMATRIX worldViewProj = world * view * proj;
	
	DirectX::XMStoreFloat4x4(&cb2.transform, DirectX::XMMatrixTranspose(worldViewProj));
//	DirectX::XMStoreFloat4x4(&cb2.transform, DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationY(0.45f)));

	UINT8* pConstantDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pConstantDataBegin)));
	memcpy(pConstantDataBegin, &cb2, CalcConstantBufferByteSize(sizeof(ConstantBuffer)));
	pConstantBuffer->Unmap(0, nullptr);

}

void Graphics::Render()
{
	//////////////////////////////
	// POPULATE COMMAND LIST /////
	//////////////////////////////
	PopulateCommandList();
	

	//////////////////////////////
	// EXECUTE COMMAND LIST //////
	//////////////////////////////
	ID3D12CommandList* ppCommandLists[] = { pCommandList.Get() };
	pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//////////////////////////////
	// PRESENT COMMAND LIST //////
	//////////////////////////////
	ThrowIfFailed(pSwapChain->Present(1, 0));

	//////////////////////////////
	// WAIT FOR GPU TO FINISH ////
	//////////////////////////////
	// Check on fence
	WaitForPreviousFrame();

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

	Microsoft::WRL::ComPtr<IDXGISwapChain1> pTempSwapChain;


	ThrowIfFailed(pFactory2->CreateSwapChainForHwnd(pCommandQueue.Get(), hWnd, &scDesc, nullptr, nullptr, &pTempSwapChain));

	ThrowIfFailed(pTempSwapChain.As(&pSwapChain));
	pFrameIndex = pSwapChain->GetCurrentBackBufferIndex();
}

void Graphics::CreateRTVDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescHeapDesc = {};
	rtvDescHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&rtvDescHeapDesc, __uuidof(ID3D12DescriptorHeap), &pRTVDescriptorHeap));

	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC dsvDescHeapDesc = {};
	dsvDescHeapDesc.NumDescriptors = 1;
	dsvDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescHeapDesc.NodeMask = 0;
	

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&dsvDescHeapDesc, __uuidof(ID3D12DescriptorHeap), &pDSVDescriptorHeap));
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
	D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorTableRanges[0].NumDescriptors = 1;
	descriptorTableRanges[0].BaseShaderRegister = 0;
	descriptorTableRanges[0].RegisterSpace = 0;
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges);
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0];

	D3D12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable = descriptorTable;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

void Graphics::CreateDepthStencilView()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
	depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	/*
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = 1280;
	depthStencilDesc.Height = 960;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	*/

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 1280, 960, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		__uuidof(ID3D12Resource),
		&pDepthStencilView
	));
	pDSVDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

	pDevice->CreateDepthStencilView(pDepthStencilView.Get(), &depthStencilViewDesc, pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	/*
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	pDevice->CreateShaderResourceView(&pShaderResourceView, &srvDesc, )
	*/
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
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
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
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 colour;
	};
	/*
	std::array<Vertex, sizeof(Vertex)> vertices =
	{
		Vertex{{DirectX::XMFLOAT3(-0.5f, +0.5f, +0.5f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+0.5f, -0.5f, +0.5f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-0.5f, -0.5f, +0.5f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+0.5f, +0.5f, +0.5f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-0.75f, +0.75f, +0.7f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(0.0f, 0.0f, +0.7f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-0.75f, +0.0f, +0.7f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+0.0f, +0.75f, +0.7f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}}
	};

	std::array<std::uint16_t, 36> indices
	{
		0, 1, 2,
		0, 3, 1,
		4, 5, 6,
		4, 7, 5
		
	};
	*/
	
	std::array<Vertex, sizeof(Vertex)> vertices =
	{
		Vertex{{DirectX::XMFLOAT3(-1.00f, -1.00f, -1.00f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-1.00f, +1.00f, -1.00f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+1.00f, +1.00f, -1.00f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+1.00f, -1.00f, -1.00f)}, {DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-1.00f, -1.00f, +1.00f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(-1.00f, +1.00f, +1.00f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+1.00f, +1.00f, +1.00f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}},
		Vertex{{DirectX::XMFLOAT3(+1.00f, -1.00f, +1.00f)}, {DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}}
	};

	std::array<std::uint16_t, 36> indices
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7


	};
	
	indicesSize = (UINT)indices.size();

	const UINT vertexBufferSize = sizeof(vertices);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pVertexBuffer));

	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, vertices.data(), sizeof(vertices));
	pVertexBuffer->Unmap(0, nullptr);

	pVertexBufferView.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
	pVertexBufferView.SizeInBytes = vertexBufferSize;
	pVertexBufferView.StrideInBytes = sizeof(Vertex);

	const UINT indexBufferSize = sizeof(indices);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pIndexBuffer));

	UINT8* pIndexDataBegin;
	CD3DX12_RANGE readRange1(0, 0);
	ThrowIfFailed(pIndexBuffer->Map(0, &readRange1, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, indices.data(), sizeof(indices));
	pIndexBuffer->Unmap(0, nullptr);

	pIndexBufferView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
	pIndexBufferView.SizeInBytes = indexBufferSize;
	pIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
}

void Graphics::CreateConstantBuffer()
{
	D3D12_DESCRIPTOR_HEAP_DESC constantBufferHeapDesc = {};
	constantBufferHeapDesc.NumDescriptors = 1;
	constantBufferHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	constantBufferHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&constantBufferHeapDesc, __uuidof(ID3D12DescriptorHeap), &pConstantBufferDescriptorHeap));



	ConstantBuffer cb;

	float x = pRadius * sinf(pPhi) * cosf(pTheta);
	float z = pRadius * sinf(pPhi) * sinf(pTheta);
	float y = pRadius * cosf(pPhi);

	DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorZero();
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();

	DirectX::XMMATRIX worldViewProj = world * view * proj;
	DirectX::XMStoreFloat4x4(&cb.transform, DirectX::XMMatrixTranspose(worldViewProj));
//	DirectX::XMStoreFloat4x4(&cb.transform, DirectX::XMMatrixTranspose(DirectX::XMMatrixRotationZ(45.0f)));

	

	UINT constantBufferByteSize = CalcConstantBufferByteSize(sizeof(ConstantBuffer));

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(constantBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pConstantBuffer
	));

	D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {};
	constantBufferViewDesc.BufferLocation = pConstantBuffer->GetGPUVirtualAddress();
	constantBufferViewDesc.SizeInBytes = constantBufferByteSize;

	pDevice->CreateConstantBufferView(&constantBufferViewDesc, pConstantBufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	ZeroMemory(&cb, sizeof(ConstantBuffer));

	UINT8* pConstantDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pConstantDataBegin)));
	memcpy(pConstantDataBegin, &cb, constantBufferByteSize);
	pConstantBuffer->Unmap(0, nullptr);

}

void Graphics::CreateFence()
{
	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
	pFenceValue = 1;

	pFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (pFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Graphics::WaitForPreviousFrame()
{
	// Signal and increment the fence value
	const UINT64 fence = pFenceValue;
	ThrowIfFailed(pCommandQueue->Signal(pFence.Get(), fence));
	pFenceValue++;

	// Wait until the previous frame is finished
	if (pFence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(pFence->SetEventOnCompletion(fence, pFenceEvent));
		WaitForSingleObject(pFenceEvent, INFINITE);
	}

	pFrameIndex = pSwapChain->GetCurrentBackBufferIndex();
}

void Graphics::PopulateCommandList()
{
	// Reset command list allocator
	ThrowIfFailed(pCommandAllocator->Reset());

	// Reset command list
	ThrowIfFailed(pCommandList->Reset(pCommandAllocator.Get(), pPipelineState.Get()));
	
	// Set graphics root signature
	pCommandList->SetGraphicsRootSignature(pRootSignature.Get());
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { pConstantBufferDescriptorHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	pCommandList->SetGraphicsRootDescriptorTable(0, pConstantBufferDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Set viewport and scissor rectangles
	pVP.Width = 1280;
	pVP.Height = 960;
	pVP.TopLeftX = 0;
	pVP.TopLeftY = 0;
	pVP.MaxDepth = 1.0f;
	pVP.MinDepth = 0.0f;
	pCommandList->RSSetViewports(1, &pVP);

	pScissorRect.top = 0;
	pScissorRect.left = 0;
	pScissorRect.right = 1280;
	pScissorRect.bottom = 960;

	pCommandList->RSSetScissorRects(1, &pScissorRect);

	// Set resource barrier indicating back buffer is to be used as render target
	// and Set render target to OM
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pRenderTargets[pFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pFrameIndex, pRTVDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE pDSVHandle(pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	pCommandList->ClearDepthStencilView(pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Record commands into command list
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 1, &pScissorRect);
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &pVertexBufferView);
	pCommandList->IASetIndexBuffer(&pIndexBufferView);
	pCommandList->DrawIndexedInstanced(36, 1, 0, 0, 0);

	// Indicate back buffer will be used to present after command list has executed
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pRenderTargets[pFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Close command list
	CloseCommandList();
}



UINT Graphics::CalcConstantBufferByteSize(UINT byteSize)
{
	// Constant buffers must be a multiple of the minimum hardware
	// allocation size (usually 256 bytes).  So round up to nearest
	// multiple of 256.  We do this by adding 255 and then masking off
	// the lower 2 bytes which store all bits < 256.
	// Example: Suppose byteSize = 300.
	// (300 + 255) & ~255
	// 555 & ~255
	// 0x022B & ~0x00ff
	// 0x022B & 0xff00
	// 0x0200
	// 512
	return (byteSize + 255) & ~255;
}

void Graphics::OnMouseMove(WPARAM buttonState, int x, int y)
{
	if ((buttonState & MK_LBUTTON) != 0)
	{
		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - pLastMousePos.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - pLastMousePos.y));

		pTheta += dx;
		pPhi += dy;

		pPhi = std::clamp(pPhi, 0.1f, DirectX::XM_PI - 0.1f);
	}
	else if ((buttonState & MK_RBUTTON) != 0)
	{
		float dx = 0.005f * static_cast<float>(x - pLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - pLastMousePos.y);

		pRadius += dx - dy;

		pRadius = std::clamp(pRadius, 1.0f, 15.0f);
	}

	pLastMousePos.x = x;
	pLastMousePos.y = y;
}