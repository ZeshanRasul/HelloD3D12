#pragma once
#include "stdafx.h"

class Graphics
{
public:
	Graphics();
	~Graphics();

	void Init();
	void Shutdown();
	void Render();

	void GetHardwareAdapter(Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> ppAdapter);

	void CreateCommandQueue();

private:
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pWarpAdapter;
	Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory;
	Microsoft::WRL::ComPtr<ID3D12Debug> pDebugController;
	Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;

};