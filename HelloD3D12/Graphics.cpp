#include "Graphics.h"
#include <algorithm>
#include <array>
#include "DDSTextureLoader.h"
#include <fstream>

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
		D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &pDevice);
	}
	else
	{
		D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &pDevice);
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

	// Create command list
	CreateCommandList();
	CloseCommandList();
	pCommandList->Reset(pCommandAllocator.Get(), nullptr);
	auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->Filename = L"Textures/WoodCrate01.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		pDevice.Get(), pCommandList.Get(), woodCrateTex->Filename.c_str(),
		woodCrateTex->Resource, woodCrateTex->UploadHeap));
	// Create empty root signature
	CreateRootSignature();

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&pSRVDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(pSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	woodTexResource = woodCrateTex->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodTexResource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = woodTexResource->GetDesc().MipLevels;

	pDevice->CreateShaderResourceView(woodTexResource.Get(), &srvDesc, pSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Compile shaders
	CompileShaders();

	// Create Depth Stencil View
	CreateDepthStencilView();


	// Create and load vertex buffers 
	// and Copy vertices data to vertex buffer
	// and Create vertex buffer views
	CreateVertexBuffer();

	// Build materials for use in material constant buffer 
	BuildMaterials();
	CreateConstantBuffer();
	// Create input element description to define vertex input layout and
	// Create pipeline state object description and object
	CreatePipelineState();

	// Create fence 	
	// and Create event handle
	CreateFence();

	// Close command list
	CloseCommandList();
	ID3D12CommandList* ppCommandLists[] = { pCommandList.Get() };
	pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

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

void Graphics::Update()
{
	ConstantBuffer cb2;
	/*
	DirectX::XMVECTOR pos = DirectX::XMVectorSet(0, -10, -10, 1.0f);
	DirectX::XMVECTOR target = DirectX::XMVectorSet(0, 0, 0, 1);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	*/
	DirectX::XMVECTOR rotationAxis = DirectX::XMVectorSet(0, 1, 1, 0);
	DirectX::XMVECTOR verticalRotationAxis = DirectX::XMVectorSet(1, 0, 1, 0);
		
	float angle = (pDx * 90.0f);
	float verticalAngle = (pDy * 90.0f);

	//DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
	DirectX::XMMATRIX world = DirectX::XMMatrixRotationAxis(rotationAxis, DirectX::XMConvertToRadians(angle)) * DirectX::XMMatrixRotationAxis(verticalRotationAxis, DirectX::XMConvertToRadians(verticalAngle));
	//DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.25f * DirectX::XM_PI, 1280/960, 0.1f, 100.0f);
	//DirectX::XMMATRIX worldViewProj = world * view * proj;
	
	DirectX::XMStoreFloat4x4(&cb2.transform, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));
	DirectX::XMStoreFloat4x4(&cb2.texTransform, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));

	UINT8* pConstantDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(pConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pConstantDataBegin)));
	memcpy(pConstantDataBegin, &cb2, CalcConstantBufferByteSize(sizeof(ConstantBuffer)));
	pConstantBuffer->Unmap(0, nullptr);

	MaterialConstants matCB;
	matCB.DiffuseAlbedo = pMaterials["skull"]->DiffuseAlbedo;
	matCB.FresnelR0 = pMaterials["skull"]->FresnelR0;
	matCB.MaterialTransform = pMaterials["skull"]->MaterialTransform;
	matCB.Roughness = pMaterials["skull"]->Roughness;

	UINT matConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants));

	UINT8* pMatConstantDataBegin;
	CD3DX12_RANGE readRange2(0, 0);
	ThrowIfFailed(pMaterialConstantBuffer->Map(0, &readRange2, reinterpret_cast<void**>(&pMatConstantDataBegin)));
	memcpy(pMatConstantDataBegin, &matCB, matConstantBufferByteSize);
	pMaterialConstantBuffer->Unmap(0, nullptr);

	PassConstants lightsCB;
	lightsCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	lightsCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	lightsCB.Lights[0].Strength = { 0.6f, -0.6f, 0.6f };
	lightsCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	lightsCB.Lights[1].Strength = { 0.3f, -0.3f, 0.3f };
	lightsCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	lightsCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	pEyePos.x = pRadius * sinf(pPhi) * cosf(pTheta);
	pEyePos.z = pRadius * sinf(pPhi) * sinf(pTheta);
	pEyePos.y = pRadius * cosf(pPhi);

	DirectX::XMVECTOR viewPos = DirectX::XMVectorSet(0, 0, 0, 1.0f);
	DirectX::XMVECTOR viewTarget = DirectX::XMVectorZero();
	DirectX::XMVECTOR viewUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//DirectX::XMMATRIX gWorld = DirectX::XMMatrixRotationAxis(rotationAxis, DirectX::XMConvertToRadians(angle)) * DirectX::XMMatrixRotationAxis(verticalRotationAxis, DirectX::XMConvertToRadians(verticalAngle));

	DirectX::XMMATRIX gProj = DirectX::XMMatrixPerspectiveFovLH(0.25 * DirectX::XM_PI, 1280 / 960, 1.0f, 100.0f);

//	DirectX::XMMATRIX gView = DirectX::XMMatrixLookAtLH(viewPos, viewTarget, viewUp);

//	DirectX::XMMATRIX gViewProj = DirectX::XMMatrixMultiply(gView, gProj);
	DirectX::XMStoreFloat3(&lightsCB.eyePosW, viewPos);
	DirectX::XMStoreFloat4x4(&lightsCB.view, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));

	UINT lightConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	UINT8* pLightsConstantDataBegin;
	CD3DX12_RANGE readRange3(0, 0);
	ThrowIfFailed(pLightsConstantBuffer->Map(0, &readRange3, reinterpret_cast<void**>(&pLightsConstantDataBegin)));
	memcpy(pLightsConstantDataBegin, &lightsCB, lightConstantBufferByteSize);
	pLightsConstantBuffer->Unmap(0, nullptr);
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
	/*
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	*/

	D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorTableRanges[0].NumDescriptors = 1;
	descriptorTableRanges[0].BaseShaderRegister = 0;
	descriptorTableRanges[0].RegisterSpace = 0;
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges);
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0];

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

//  slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	
	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[0].DescriptorTable = descriptorTable;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(slotRootParameter), slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> signature;
	Microsoft::WRL::ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf()));
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

}

void Graphics::CreatePipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
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
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			TexC(uv) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TexC(u, v) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz)
			:
			Position(px, py, pz),
			Normal(nx, ny, nz)
			{}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexC;

	};
	/*
	std::array<Vertex, 24> vertices;
	float w2 = 1.0f;
	float h2 = 1.0f;
	float d2 = 1.0f;
	
	// Fill in the front face vertex data.
	vertices[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f,  0.0f, 1.0f);
	vertices[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f,  0.0f, 0.0f);
	vertices[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f,  1.0f, 0.0f);
	vertices[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f,  1.0f, 1.0f);
	
	// Fill in the back face vertex data.
	vertices[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	vertices[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	vertices[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	vertices[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	
	// Fill in the top face vertex data.
	vertices[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	vertices[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
	vertices[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
	vertices[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
	
	// Fill in the bottom face vertex data.
	vertices[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f);
	vertices[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);
	vertices[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
	vertices[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
	
	// Fill in the left face vertex data.
	vertices[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	vertices[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	vertices[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	vertices[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	
	// Fill in the right face vertex data.
	vertices[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	vertices[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	vertices[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	vertices[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	std::array<UINT32, 36> indices;

	// Fill in the front face index data
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 0; indices[4] = 2; indices[5] = 3;

	// Fill in the back face index data
	indices[6] = 4; indices[7] = 5; indices[8] = 6;
	indices[9] = 4; indices[10] = 6; indices[11] = 7;

	// Fill in the top face index data
	indices[12] = 8; indices[13] = 9; indices[14] = 10;
	indices[15] = 8; indices[16] = 10; indices[17] = 11;

	// Fill in the bottom face index data
	indices[18] = 12; indices[19] = 13; indices[20] = 14;
	indices[21] = 12; indices[22] = 14; indices[23] = 15;

	// Fill in the left face index data
	indices[24] = 16; indices[25] = 17; indices[26] = 18;
	indices[27] = 16; indices[28] = 18; indices[29] = 19;

	// Fill in the right face index data
	indices[30] = 20; indices[31] = 21; indices[32] = 22;
	indices[33] = 20; indices[34] = 22; indices[35] = 23;
	*/

	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, "Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);

	for (UINT i = 0; i < vcount; i++)
	{
		fin >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; i++)
	{ 
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	indicesSize = indices.size();

	const UINT vertexBufferByteSize = vertices.size() * sizeof(Vertex);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pVertexBuffer));

	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, vertices.data(), vertexBufferByteSize);
	pVertexBuffer->Unmap(0, nullptr);

	pVertexBufferView.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
	pVertexBufferView.SizeInBytes = vertexBufferByteSize;
	pVertexBufferView.StrideInBytes = sizeof(Vertex);

	const UINT indexBufferByteSize = (UINT)indices.size() * sizeof(int32_t);

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(indexBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pIndexBuffer));

	UINT8* pIndexDataBegin;
	CD3DX12_RANGE readRange1(0, 0);
	ThrowIfFailed(pIndexBuffer->Map(0, &readRange1, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, indices.data(), indexBufferByteSize);
	pIndexBuffer->Unmap(0, nullptr);

	pIndexBufferView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
	pIndexBufferView.SizeInBytes = indexBufferByteSize;
	pIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

void Graphics::BuildMaterials()
{
	auto cubeMaterial = std::make_unique<Material>();
	cubeMaterial->Name = "skull";
	cubeMaterial->MaterialCBIndex = 0;
	cubeMaterial->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	cubeMaterial->FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
	cubeMaterial->Roughness = 0.3f;

	pMaterials["skull"] = std::move(cubeMaterial);
}


void Graphics::CreateConstantBuffer()
{
	// CUBE ROTATION CBUFFER////////////

	D3D12_DESCRIPTOR_HEAP_DESC constantBufferHeapDesc = {};
	constantBufferHeapDesc.NumDescriptors = 1;
	constantBufferHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	constantBufferHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&constantBufferHeapDesc, __uuidof(ID3D12DescriptorHeap), &pConstantBufferDescriptorHeap));

	ConstantBuffer cb;

	DirectX::XMVECTOR rotationAxis = DirectX::XMVectorSet(0, 1, 1, 0);
	DirectX::XMVECTOR verticalRotationAxis = DirectX::XMVectorSet(1, 0, 1, 0);

	float angle = (pDx * 90.0f);
	float verticalAngle = (pDy * 90.0f);

	DirectX::XMMATRIX world = DirectX::XMMatrixRotationAxis(rotationAxis, DirectX::XMConvertToRadians(angle)) * DirectX::XMMatrixRotationAxis(verticalRotationAxis, DirectX::XMConvertToRadians(verticalAngle));

	DirectX::XMStoreFloat4x4(&cb.transform, DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f)));
	
	DirectX::XMStoreFloat4x4(&cb.texTransform, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));

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


	////// MATERIAL CONSTANT BUFFER ///////

	D3D12_DESCRIPTOR_HEAP_DESC matCBHeapDesc = {};
	matCBHeapDesc.NumDescriptors = 1;
	matCBHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	matCBHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&matCBHeapDesc, __uuidof(ID3D12DescriptorHeap), &pMatCBufDescriptorHeap));
	
	MaterialConstants matCB;
	matCB.DiffuseAlbedo = pMaterials["skull"]->DiffuseAlbedo;
	matCB.FresnelR0 = pMaterials["skull"]->FresnelR0;
	matCB.MaterialTransform = pMaterials["skull"]->MaterialTransform;
	matCB.Roughness = pMaterials["skull"]->Roughness;

	UINT matConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants));

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(matConstantBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		_uuidof(ID3D12Resource), &pMaterialConstantBuffer
	));

	D3D12_CONSTANT_BUFFER_VIEW_DESC matConstantBufferViewDesc = {};
	matConstantBufferViewDesc.BufferLocation = pMaterialConstantBuffer->GetGPUVirtualAddress();
	matConstantBufferViewDesc.SizeInBytes = matConstantBufferByteSize;

	pDevice->CreateConstantBufferView(&matConstantBufferViewDesc, pMatCBufDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	ZeroMemory(&matCB, sizeof(MaterialConstants));

	UINT8* pMatConstantDataBegin;
	CD3DX12_RANGE readRange2(0, 0);
	ThrowIfFailed(pMaterialConstantBuffer->Map(0, &readRange2, reinterpret_cast<void**>(&pMatConstantDataBegin)));
	memcpy(pMatConstantDataBegin, &matCB, matConstantBufferByteSize);
	pMaterialConstantBuffer->Unmap(0, nullptr);

	/// Lights cbPass Constant Buffer /////////

	D3D12_DESCRIPTOR_HEAP_DESC lightsCBHeapDesc = {};
	lightsCBHeapDesc.NumDescriptors = 1;
	lightsCBHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	lightsCBHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&lightsCBHeapDesc, __uuidof(ID3D12DescriptorHeap), &pLightsCBDescriptorHeap));

	PassConstants lightsCB;
	lightsCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	lightsCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	lightsCB.Lights[0].Strength = { 0.6f, -0.6f, 0.6f };
	lightsCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	lightsCB.Lights[1].Strength = { 0.3f, -0.3f, 0.3f };
	lightsCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	lightsCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	pEyePos.x = pRadius * sinf(pPhi) * cosf(pTheta);
	pEyePos.z = pRadius * sinf(pPhi) * sinf(pTheta);
	pEyePos.y = pRadius * cosf(pPhi);

	DirectX::XMVECTOR viewPos = DirectX::XMVectorSet(pEyePos.x, pEyePos.y, pEyePos.z, 1.0f);
	DirectX::XMVECTOR viewTarget = DirectX::XMVectorZero();
	DirectX::XMVECTOR viewUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//DirectX::XMMATRIX gWorld = DirectX::XMMatrixRotationAxis(rotationAxis, DirectX::XMConvertToRadians(angle)) * DirectX::XMMatrixRotationAxis(verticalRotationAxis, DirectX::XMConvertToRadians(verticalAngle));

	DirectX::XMMATRIX gProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), 1280 / 960, 1.0f, 100.0f);

	DirectX::XMMATRIX gView = DirectX::XMMatrixLookAtLH(viewPos, viewTarget, viewUp);

	DirectX::XMMATRIX gViewProj = DirectX::XMMatrixMultiply(gView, gProj);
	DirectX::XMStoreFloat3(&lightsCB.eyePosW, viewPos);
	DirectX::XMStoreFloat4x4(&lightsCB.view, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));


	UINT lightConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	ThrowIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(lightConstantBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		__uuidof(ID3D12Resource), &pLightsConstantBuffer
	));

	D3D12_CONSTANT_BUFFER_VIEW_DESC lightsConstantBufferViewDesc = {};
	lightsConstantBufferViewDesc.BufferLocation = pLightsConstantBuffer->GetGPUVirtualAddress();
	lightsConstantBufferViewDesc.SizeInBytes = lightConstantBufferByteSize;

	pDevice->CreateConstantBufferView(&lightsConstantBufferViewDesc, pLightsCBDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	ZeroMemory(&lightsCB, sizeof(PassConstants));

	UINT8* pLightsConstantDataBegin;
	CD3DX12_RANGE readRange3(0, 0);
	ThrowIfFailed(pLightsConstantBuffer->Map(0, &readRange3, reinterpret_cast<void**>(&pLightsConstantDataBegin)));
	memcpy(pLightsConstantDataBegin, &lightsCB, lightConstantBufferByteSize);
	pLightsConstantBuffer->Unmap(0, nullptr);

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
	// TODO: Fix this
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
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { pSRVDescriptorHeap.Get() };
	pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	// Set graphics root signature
	pCommandList->SetGraphicsRootSignature(pRootSignature.Get());
	
	UINT objConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(ConstantBuffer));
	UINT matConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT lightConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = pConstantBuffer->GetGPUVirtualAddress();

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(pSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	pCommandList->SetGraphicsRootDescriptorTable(0, tex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pFrameIndex, pRTVDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE pDSVHandle(pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	pCommandList->ClearDepthStencilView(pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	pCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

	D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = pMaterialConstantBuffer->GetGPUVirtualAddress() + pMaterials["skull"]->MaterialCBIndex * matConstantBufferByteSize;
	pCommandList->SetGraphicsRootConstantBufferView(2, matCBAddress);

	D3D12_GPU_VIRTUAL_ADDRESS lightsCBAddress = pLightsConstantBuffer->GetGPUVirtualAddress();
	pCommandList->SetGraphicsRootConstantBufferView(3, lightsCBAddress);

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

	// Record commands into command list
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 1, &pScissorRect);
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &pVertexBufferView);
	pCommandList->IASetIndexBuffer(&pIndexBufferView);
	pCommandList->DrawIndexedInstanced(indicesSize, 1, 0, 0, 0);

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

		const auto old = last;
		last = std::chrono::steady_clock::now();
		std::chrono::duration<float> frameTime = last - old;
		dt += frameTime.count();

		pDx += DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - pLastMousePos.x));
		pDy += DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - pLastMousePos.y));
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

void Graphics::OnMouseDown(WPARAM buttonState, int x, int y)
{
	pLastMousePos.x = x;
	pLastMousePos.y = y;
	last = std::chrono::steady_clock::now();
}

void Graphics::OnMouseUp()
{
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Graphics::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}




