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

	void GetHardwareAdapter(IDXGIFactory1* pFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> ppAdapter);

private:
	Microsoft::WRL::ComPtr<IDXGIAdapter1> ppAdapter;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pWarpAdapter;
	IDXGIFactory1* pFactory;

};