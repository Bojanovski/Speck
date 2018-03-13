
#include "FrameResource.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT maxRenderItemsCount, UINT materialCount)
{
    THROW_IF_FAILED(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB											= make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    MaterialBuffer									= make_unique<UploadBuffer<MaterialBufferData>>(device, materialCount, false);
	RenderItemConstantsBuffer					= make_unique<UploadBuffer<RenderItemConstants>>(device, maxRenderItemsCount, true);
}

FrameResource::~FrameResource()
{

}