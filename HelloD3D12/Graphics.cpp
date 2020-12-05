#include "Graphics.h"

Graphics::Graphics()
{
}

Graphics::~Graphics()
{
	Shutdown();
}

void Graphics::Init()
{
	//////////////////////////////
	// 1) INITIALIZE PIPELINE ////
	//////////////////////////////

	// Create Factory 
	CreateDXGIFactory1(_uuidof(pFactory), (void**)&pFactory);
	
	// Query hardware adapters
	GetHardwareAdapter(pFactory, pAdapter);

	// Enable debug layer

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(pDebugController.GetAddressOf(), &pDebugController))))
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
	// Create swap chain
	// Create render target view descriptor heap
	// Create frame resources (RTV for each frame)
	// Create a command allocator

	//////////////////////////////
	// 2) INITIALIZE ASSETS///////
	//////////////////////////////

	// Create empty root signature
	// Compile shaders
	// Create vertex input layout
	// Create pipeline state object description
	// Create pipeline state object
	// Create command list
	// Close command list
	// Create and load vertex buffers
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