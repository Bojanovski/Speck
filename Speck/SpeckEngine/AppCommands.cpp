
#include "AppCommands.h"
#include "DirectXHeaders.h"
#include "SpeckApp.h"
#include "DirectXCore.h"
#include "FrameResource.h"
#include "GenericShaderStructures.h"
#include "GeometryGenerator.h"
#include "PSOGroup.h"
#include "RenderItem.h"
#include "SpeckWorld.h"
#include "MaterialShaderStructure.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace Speck::AppCommands;

int SetWindowTitleCommand::Execute(void *ptIn, void *ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	sApp->ChangeMainWindowCaption(text);
	return 0;
}

int LoadResourceCommand::Execute(void *ptIn, void *ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();
	// DirectX CommandList is needed to execute this command.
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Reset(dxCore.GetCommandAllocator(), nullptr));

	int retVal = 1;
	switch (resType)
	{
		case ResourceType::Texture:
		{
			auto tex = make_unique<Texture>();
			tex->Name = name;
			ThrowIfFailed(CreateDDSTextureFromFile12(dxCore.GetDevice(), dxCore.GetCommandList(), path.c_str(), tex->Resource, tex->UploadHeap));
			sApp->mTextures[tex->Name] = move(tex);
			retVal = 0;
		}
		break;
		case ResourceType::Shader:
		{
			sApp->mShaders[name] = LoadBinary(path);
			retVal = 0;
		}
		break;
		case ResourceType::Geometry:
		{
			retVal = 0; // not implemented
		}
		break;
		default:
		{
			LOG(L"No type specified while loading resource.", ERROR);
			retVal = 1; // not good
		}
		break;
	}

	// Execute the initialization commands.
	ThrowIfFailed(dxCore.GetCommandList()->Close());
	ID3D12CommandList* cmdsListsInitialization[] = { dxCore.GetCommandList() };
	dxCore.GetCommandQueue()->ExecuteCommandLists(_countof(cmdsListsInitialization), cmdsListsInitialization);

	// Wait until initialization is complete.
	dxCore.FlushCommandQueue();
	return retVal;
}

int CreateMaterialCommand::Execute(void *ptIn, void *ptOut) const
{
	SpeckApp *sApp = static_cast<SpeckApp*>(ptIn);
	auto &dxCore = sApp->GetEngineCore().GetDirectXCore();

	// Create the material.
	unique_ptr<Material> material = make_unique<PBRMaterial>();
	PBRMaterial *texMatPt = static_cast<PBRMaterial *>(material.get());
	texMatPt->DiffuseAlbedo = DiffuseAlbedo;
	texMatPt->FresnelR0 = FresnelR0;
	texMatPt->Roughness = Roughness;

	// Get textures
	const char *tNames[] = { &albedoTexName[0], &normalTexName[0], &heightTexName[0], &metalnessTexName[0], &roughnessTexName[0], &aoTexName[0] };
	ID3D12Resource *t[MATERIAL_TEXTURES_COUNT];
	for (int i = 0; i < MATERIAL_TEXTURES_COUNT; ++i)
	{
		if (strlen(tNames[i]) == 0)
		{// Use of default
			t[i] = sApp->GetDefaultTexture(i);
		}
		else if (sApp->mTextures.find(tNames[i]) != sApp->mTextures.end())
		{// Use the specified
			t[i] = sApp->mTextures[tNames[i]]->Resource.Get();
		}
		else
		{// Error, specified not found, use default.
			LOG(L"Specified texture not found, using default.", ERROR);
			t[i] = sApp->GetDefaultTexture(i);
		}
	}

	//
	// Create the SRV heaps for item.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = MATERIAL_TEXTURES_COUNT;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(dxCore.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texMatPt->mSrvDescriptorHeap)));

	//
	// Fill out the heap with SRV descriptors for scene.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(texMatPt->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < MATERIAL_TEXTURES_COUNT; ++i)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = t[i]->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = t[i]->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		dxCore.GetDevice()->CreateShaderResourceView(t[i], &srvDesc, hDescriptor);
		// next descriptor
		hDescriptor.Offset(1, dxCore.GetCbvSrvUavDescriptorSize());
	}

	if (sApp->mMaterials.find(materialName) != sApp->mMaterials.end())
	{
		// Material with this key already exists, overwriting.
		material->MatCBIndex = sApp->mMaterials[materialName]->MatCBIndex;
	}
	else if (sApp->mLatestMatCBIndex >= sApp->mMaxNumberOfMaterials)
	{
		LOG(L"Trying to create a new material while all the material space is used up.", ERROR);
		return 1;
	}
	else // everything is ok, add a brand new material
	{
		material->MatCBIndex = sApp->mLatestMatCBIndex++;
	}
	sApp->mMaterials[materialName] = move(material);
	return 0;
}