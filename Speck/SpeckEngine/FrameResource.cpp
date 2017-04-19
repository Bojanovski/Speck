
#include "FrameResource.h"

using namespace std;
using namespace DirectX;
using namespace Speck;

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT maxInstanceCount, UINT maxSingleCount, UINT materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB			= make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    MaterialBuffer	= make_unique<UploadBuffer<MaterialBufferData>>(device, materialCount, false);
	InstanceBuffer	= make_unique<UploadBuffer<SpeckInstanceData>>(device, maxInstanceCount, false);
	ObjectCB		= make_unique<UploadBuffer<ObjectConstants>>(device, maxSingleCount, true);
}

FrameResource::~FrameResource()
{

}