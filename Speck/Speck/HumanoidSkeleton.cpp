
#include "HumanoidSkeleton.h"
#include <World.h>
#include <App.h>
#pragma warning( push )
#pragma warning( disable : 4244 )  
#include <fbxsdk.h>
#include <fbxsdk\utils\fbxgeometryconverter.h>
#pragma warning( pop )
#include <vector>
#include <string>

using namespace std;
using namespace DirectX;
using namespace Speck;

void Conv(fbxsdk::FbxDouble3 *out, const DirectX::XMFLOAT3 &in)
{
	out->mData[0] = in.x;
	out->mData[1] = in.y;
	out->mData[2] = in.z;
}

void Conv(DirectX::XMFLOAT3 *out, const fbxsdk::FbxDouble3 &in)
{
	out->x = (float)in.mData[0];
	out->y = (float)in.mData[1];
	out->z = (float)in.mData[2];
}

void Conv(DirectX::XMFLOAT4 *out, const fbxsdk::FbxDouble4 &in)
{
	out->x = (float)in.mData[0];
	out->y = (float)in.mData[1];
	out->z = (float)in.mData[2];
	out->w = (float)in.mData[3];
}

void Conv(DirectX::XMFLOAT4 *out, const fbxsdk::FbxQuaternion &in)
{
	out->x = (float)in.mData[0];
	out->y = (float)in.mData[1];
	out->z = (float)in.mData[2];
	out->w = (float)in.mData[3];
}

void Conv(DirectX::XMFLOAT4X4 *out, const fbxsdk::FbxDouble4x4 &in)
{
	for (UINT i = 0; i < 4; i++)
	{
		for (UINT j = 0; j < 4; j++)
		{
			out->m[i][j] = (float)in[i][j];
		}
	}
}

void HumanoidSkeleton::ProcessRoot(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *localOut)
{
	FbxPropertyT<FbxDouble3> *nodeTranslationProp = &node->LclTranslation;
	FbxDouble3 translation = nodeTranslationProp->Get(); // get default (bind pose)
	*localOut = XMMatrixIdentity();
}

void HumanoidSkeleton::ProcessHips(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	int nx = 3;
	int ny = 2;
	int nz = 2;
	float width = (nx - 1) * 20.0f * mSpeckRadius;
	float height = (ny - 1) * 20.0f * mSpeckRadius;
	float depth = (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = width / (nx - 1);
	float dy = height / (ny - 1);
	float dz = depth / (nz - 1);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, y, 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessLegPart(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *worldOut, XMMATRIX *localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	int nx = 2;
	int ny = 7;
	int nz = 2;
	float width		= (nx - 1) * 20.0f * mSpeckRadius;
	float height	= (ny - 1) * 20.0f * mSpeckRadius;
	float depth		= (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = width / (nx - 1);
	float dy = height / (ny - 1);
	float dz = depth / (nz - 1);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, y - /*room for joint*/ 20.0f * mSpeckRadius, 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessSpinePart(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *worldOut, XMMATRIX *localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	int nx = 4;
	int ny = 1;
	int nz = 4;
	float width = (nx - 1) * 20.0f * mSpeckRadius;
	float height = (ny - 1) * 20.0f * mSpeckRadius;
	float depth = (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = (nx > 1) ? width / (nx - 1) : 0.0f;
	float dy = (ny > 1) ? height / (ny - 1) : 0.0f;
	float dz = (nz > 1) ? depth / (nz - 1) : 0.0f;

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, y , 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				//if (j == 0 && (i == 1 || i == 2)) continue;
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, /*fabsf(i*dx + x) * 0.1f*/ + j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessChest(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *worldOut, XMMATRIX *localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	int nx = 4;
	int ny = 1;
	int nz = 4;
	float width = (nx - 1) * 20.0f * mSpeckRadius;
	float height = (ny - 1) * 20.0f * mSpeckRadius;
	float depth = (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = (nx > 1) ? width / (nx - 1) : 0.0f;
	float dy = (ny > 1) ? height / (ny - 1) : 0.0f;
	float dz = (nz > 1) ? depth / (nz - 1) : 0.0f;

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, y - /*just a little correction*/ 8.0f * mSpeckRadius, 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				//if (j == 0 && (i == 1 || i == 2)) continue;
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, /*fabsf(i*dx + x) * 0.1f +*/ j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// second layer (smaller)
	nx = 2;
	ny = 1;
	nz = 4;
	width = (nx - 1) * 20.0f * mSpeckRadius;
	height = (ny - 1) * 20.0f * mSpeckRadius;
	depth = (nz - 1) * 20.0f * mSpeckRadius;
	x = -0.5f*width;
	y = -0.5f*height + 20.0f * mSpeckRadius;
	z = -0.5f*depth;
	dx = (nx > 1) ? width / (nx - 1) : 0.0f;
	dy = (ny > 1) ? height / (ny - 1) : 0.0f;
	dz = (nz > 1) ? depth / (nz - 1) : 0.0f;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				//if (j == 0 && (i == 1 || i == 2)) continue;
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, /*fabsf(i*dx + x) * 0.1f +*/ j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessShoulder(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *worldOut, XMMATRIX *localOut, bool right)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	float shoulderHalfWidth = 10.0f * mSpeckRadius;
	float sign = (right) ? -1.0f : 1.0f;
	*localOut = XMMatrixTranslation(sign * shoulderHalfWidth, 0.0f, 0.0f) * rotM;


	WorldCommands::NewSpeck speck;
	XMVECTOR pos;

	pos = XMVector3TransformCoord(XMVectorSet(10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius * 0.70716f, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	pos = XMVector3TransformCoord(XMVectorSet(-10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius * 0.70716f, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	//pos = XMVector3TransformCoord(XMVectorSet(0.0f, 10.0f * mSpeckRadius, 10.0f * mSpeckRadius * 0.70716f, 1.0f), *localOut);
	//XMStoreFloat3(&speck.position, pos);
	//outCommand->newSpecks.push_back(speck);

	pos = XMVector3TransformCoord(XMVectorSet(0.0f, -10.0f * mSpeckRadius, 10.0f * mSpeckRadius * 0.70716f, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessArmPart(FbxNode *node, FbxAnimLayer *animLayer, WorldCommands::AddSpecksCommand *outCommand, XMMATRIX *worldOut, XMMATRIX *localOut, bool right)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	int nx = 5;
	int ny = 2;
	int nz = 2;
	float width = (nx - 1) * 20.0f * mSpeckRadius;
	float height = (ny - 1) * 20.0f * mSpeckRadius;
	float depth = (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = width / (nx - 1);
	float dy = height / (ny - 1);
	float dz = depth / (nz - 1);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	float sign = (right) ? -1.0f : 1.0f;
	*localOut = XMMatrixTranslation(sign * (-x + /*room for joint*/ 0.707f * 20.0f * mSpeckRadius), 0.0f, 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessNeck(FbxNode * node, FbxAnimLayer * animLayer, WorldCommands::AddSpecksCommand * outCommand, XMMATRIX * worldOut, XMMATRIX * localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, 0.0f, 0.0f) * rotM;


	WorldCommands::NewSpeck speck;
	XMVECTOR pos;

	pos = XMVector3TransformCoord(XMVectorSet(-10.0f * mSpeckRadius, 0.0f, 10.0f * mSpeckRadius, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	pos = XMVector3TransformCoord(XMVectorSet(10.0f * mSpeckRadius, 0.0f, 10.0f * mSpeckRadius, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	pos = XMVector3TransformCoord(XMVectorSet(-10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	pos = XMVector3TransformCoord(XMVectorSet(10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius, 1.0f), *localOut);
	XMStoreFloat3(&speck.position, pos);
	outCommand->newSpecks.push_back(speck);

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::ProcessHead(fbxsdk::FbxNode * node, fbxsdk::FbxAnimLayer * animLayer, Speck::WorldCommands::AddSpecksCommand * outCommand, DirectX::XMMATRIX * worldOut, DirectX::XMMATRIX * localOut)
{
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	*worldOut = XMLoadFloat4x4(&worldF);

	int nx = 3;
	int ny = 3;
	int nz = 3;
	float width = (nx - 1) * 20.0f * mSpeckRadius;
	float height = (ny - 1) * 20.0f * mSpeckRadius;
	float depth = (nz - 1) * 20.0f * mSpeckRadius;
	float x = -0.5f*width;
	float y = -0.5f*height;
	float z = -0.5f*depth;
	float dx = width / (nx - 1);
	float dy = height / (ny - 1);
	float dz = depth / (nz - 1);

	// Calculate local transform
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	*localOut = XMMatrixTranslation(0.0f, -y, 0.0f) * rotM;

	for (int i = 0; i < nx; ++i)
	{
		for (int j = 0; j < ny; ++j)
		{
			for (int k = 0; k < nz; ++k)
			{
				WorldCommands::NewSpeck speck;
				// Position instanced along a 3D grid.
				XMVECTOR pos = XMVectorSet(i*dx + x, j*dy + y, k*dz + z, 1.0f);
				pos = XMVector3TransformCoord(pos, *localOut);
				XMStoreFloat3(&speck.position, pos);
				outCommand->newSpecks.push_back(speck);
			}
		}
	}

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		FbxDouble3 pos;
		Conv(&pos, outCommand->newSpecks[i].position);
		pos = world.MultT(pos);
		Conv(&outCommand->newSpecks[i].position, pos);
	}
}

void HumanoidSkeleton::CreateSpecksBody(Speck::App *pApp)
{
	// Create the body made out of specks
	int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
	FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(0));
	int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
	FbxAnimLayer* lAnimLayer = pAnimStack->GetMember<FbxAnimLayer>(0);
	int numAnimCurveNodes = lAnimLayer->GetMemberCount<FbxNode>();
	vector<FbxNode *> nodeStack;
	FbxNode *root = mScene->GetRootNode();
	nodeStack.push_back(root);

	// Body creation logic
	bool addKnee = false;
	bool addElbow = false;
	bool addShoulder = false;
	bool addHip = false;
	bool addSpine = false;
	bool addSpine1 = false;
	bool addChest = false;
	bool addShoulderChest = false;
	bool addNeck = false;
	bool addHead = false;
	int lastAddedRigidBodyIndex = -1;
	FbxNode *skinNode = nullptr;

	while (!nodeStack.empty())
	{
		// Pop the last item on the stack
		FbxNode *node = *(nodeStack.end() - 1);
		nodeStack.pop_back();

		//FbxNodeAttribute* lNodeAttribute = node->GetNodeAttribute();
		string name = node->GetName();
		string originalName = name;
		WorldCommands::AddSpecksCommand command;
		WorldCommands::AddSpecksCommandResult commandResult;
		command.speckType = WorldCommands::SpeckType::RigidBody;
		command.frictionCoefficient = 0.2f;
		XMMATRIX worldTransform = XMMatrixIdentity();
		XMMATRIX localTransform = XMMatrixIdentity();
		if (name.find("RootNode") != string::npos)
		{
			ProcessRoot(node, lAnimLayer, &command, &localTransform);
			name = "RootNode";
		}
		else if (name.find("Hips") != string::npos)
		{
			ProcessHips(node, lAnimLayer, &command, &localTransform);
			name = "Hips";
		}
		else if (name.find("LeftUpLeg") != string::npos)
		{
			ProcessLegPart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addHip = true;
			name = "LeftUpLeg";
		}
		else if (name.find("RightUpLeg") != string::npos)
		{
			ProcessLegPart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addHip = true;
			name = "RightUpLeg";
		}
		else if (name.find("LeftLeg") != string::npos)
		{
			ProcessLegPart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addKnee = true;
			name = "LeftLeg";
		}
		else if (name.find("RightLeg") != string::npos)
		{
			ProcessLegPart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addKnee = true;
			name = "RightLeg";
		}
		else if (name.find("Spine") != string::npos && name.find("Spine1") == string::npos && name.find("Spine2") == string::npos)
		{
			ProcessSpinePart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addSpine = true;
			name = "Spine";
		}
		else if (name.find("Spine1") != string::npos)
		{
			ProcessSpinePart(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addSpine1 = true;
			name = "Spine1";
		}
		else if (name.find("Spine2") != string::npos)
		{
			ProcessChest(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addChest = true;
			name = "Spine2";
		}
		else if (name.find("RightShoulder") != string::npos)
		{
			ProcessShoulder(node, lAnimLayer, &command, &worldTransform, &localTransform, true);
			addShoulderChest = true;
			name = "RightShoulder";
		}
		else if (name.find("LeftShoulder") != string::npos)
		{
			ProcessShoulder(node, lAnimLayer, &command, &worldTransform, &localTransform, false);
			addShoulderChest = true;
			name = "LeftShoulder";
		}
		else if (name.find("RightArm") != string::npos)
		{
			ProcessArmPart(node, lAnimLayer, &command, &worldTransform, &localTransform, true);
			addShoulder = true;
			name = "RightArm";
		}
		else if (name.find("LeftArm") != string::npos)
		{
			ProcessArmPart(node, lAnimLayer, &command, &worldTransform, &localTransform, false);
			addShoulder = true;
			name = "LeftArm";
		}
		else if (name.find("RightForeArm") != string::npos)
		{
			ProcessArmPart(node, lAnimLayer, &command, &worldTransform, &localTransform, true);
			addElbow = true;
			name = "RightForeArm";
		}
		else if (name.find("LeftForeArm") != string::npos)
		{
			ProcessArmPart(node, lAnimLayer, &command, &worldTransform, &localTransform, false);
			addElbow = true;
			name = "LeftForeArm";
		}
		else if (name.find("Neck") != string::npos)
		{
			ProcessNeck(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addNeck = true;
			name = "Neck";
		}
		else if (name.find("Head") != string::npos && name.find("End") == string::npos)
		{
			ProcessHead(node, lAnimLayer, &command, &worldTransform, &localTransform);
			addHead = true;
			name = "Head";
		}

		// Save the name in the dictionary
		mNamesDictionary[originalName] = name;

		if (command.newSpecks.size() > 0)
		{
			GetWorld().ExecuteCommand(command, &commandResult);
			mNodesAnimData[name].index = commandResult.rigidBodyIndex;
			XMStoreFloat4x4(&mNodesAnimData[name].localTransform, localTransform);

			if (addHip)
			{
				addHip = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["Hips"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				commandForJoint.newSpecks.resize(1);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addKnee)
			{
				addKnee = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(-10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				XMVECTOR jointPosL2 = XMVectorSet(10.0f * mSpeckRadius, 0.0f, -10.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW2 = XMVector3TransformCoord(jointPosL2, worldTransform);
				commandForJoint.newSpecks.resize(2);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				XMStoreFloat3(&commandForJoint.newSpecks[1].position, jointPosW2);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addSpine)
			{
				addSpine = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["Hips"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				commandForJoint.newSpecks.clear();
				int nx = 4;
				int ny = 4;
				float width = (nx - 1) * 20.0f * mSpeckRadius;
				float height = (ny - 1) * 20.0f * mSpeckRadius;
				float x = -0.5f*width;
				float y = -0.5f*height;
				float dx = width / (nx - 1);
				float dy = height / (ny - 1);

				for (int i = 0; i < nx; ++i)
				{
					for (int j = 0; j < ny; ++j)
					{
						XMVECTOR jointPosL = XMVectorSet(i*dx + x, -20.0f * mSpeckRadius, j*dy + y, 1.0f);
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosL, worldTransform);
						WorldCommands::NewSpeck ns;
						XMStoreFloat3(&ns.position, jointPosW);
						commandForJoint.newSpecks.push_back(ns);
					}
				}
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addSpine1)
			{
				addSpine1 = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				commandForJoint.newSpecks.clear();
				int nx = 4;
				int ny = 4;
				float width = (nx - 1) * 20.0f * mSpeckRadius;
				float height = (ny - 1) * 20.0f * mSpeckRadius;
				float x = -0.5f*width;
				float y = -0.5f*height;
				float dx = width / (nx - 1);
				float dy = height / (ny - 1);

				for (int i = 0; i < nx; ++i)
				{
					for (int j = 0; j < ny; ++j)
					{
						XMVECTOR jointPosL = XMVectorSet(i*dx + x, -20.0f * mSpeckRadius, j*dy + y, 1.0f);
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosL, worldTransform);
						WorldCommands::NewSpeck ns;
						XMStoreFloat3(&ns.position, jointPosW);
						commandForJoint.newSpecks.push_back(ns);
					}
				}
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addChest)
			{
				addChest = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				commandForJoint.newSpecks.clear();
				int nx = 4;
				int ny = 4;
				float width = (nx - 1) * 20.0f * mSpeckRadius;
				float height = (ny - 1) * 20.0f * mSpeckRadius;
				float x = -0.5f*width;
				float y = -0.5f*height;
				float dx = width / (nx - 1);
				float dy = height / (ny - 1);

				for (int i = 0; i < nx; ++i)
				{
					for (int j = 0; j < ny; ++j)
					{
						XMVECTOR jointPosL = XMVectorSet(i*dx + x, -35.0f * mSpeckRadius, j*dy + y, 1.0f);
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosL, worldTransform);
						WorldCommands::NewSpeck ns;
						XMStoreFloat3(&ns.position, jointPosW);
						commandForJoint.newSpecks.push_back(ns);
					}
				}
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addShoulderChest)
			{
				addShoulderChest = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["Spine2"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(10.0f * mSpeckRadius, 12.0f * mSpeckRadius, 20.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				XMVECTOR jointPosL2 = XMVectorSet(-10.0f * mSpeckRadius, 12.0f * mSpeckRadius, 20.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW2 = XMVector3TransformCoord(jointPosL2, worldTransform);
				XMVECTOR jointPosL3 = XMVectorSet(0.0f * mSpeckRadius, 12.0f * mSpeckRadius, 20.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW3 = XMVector3TransformCoord(jointPosL3, worldTransform);

				commandForJoint.newSpecks.resize(3);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				XMStoreFloat3(&commandForJoint.newSpecks[1].position, jointPosW2);
				XMStoreFloat3(&commandForJoint.newSpecks[2].position, jointPosW3);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addElbow)
			{
				addElbow = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(0.0f, 0.0f, 10.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				XMVECTOR jointPosL2 = XMVectorSet(0.0f, 0.0f, -10.0f * mSpeckRadius, 1.0f);
				XMVECTOR jointPosW2 = XMVector3TransformCoord(jointPosL2, worldTransform);
				commandForJoint.newSpecks.resize(2);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				XMStoreFloat3(&commandForJoint.newSpecks[1].position, jointPosW2);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addShoulder)
			{
				addShoulder = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				commandForJoint.newSpecks.resize(1);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addNeck)
			{
				addNeck = false;
				// connect to chest
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["Spine2"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				commandForJoint.newSpecks.clear();
				int nx = 2;
				int ny = 3;
				float width = (nx - 1) * 20.0f * mSpeckRadius;
				float height = (ny - 1) * 20.0f * mSpeckRadius;
				float x = -0.5f*width;
				float y = -0.5f*height;
				float dx = width / (nx - 1);
				float dy = height / (ny - 1);

				for (int i = 0; i < nx; ++i)
				{
					for (int j = 0; j < ny; ++j)
					{
						XMVECTOR jointPosL = XMVectorSet(i*dx + x, -20.0f * mSpeckRadius, j*dy + y, 1.0f);
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosL, worldTransform);
						WorldCommands::NewSpeck ns;
						XMStoreFloat3(&ns.position, jointPosW);
						commandForJoint.newSpecks.push_back(ns);
					}
				}
				GetWorld().ExecuteCommand(commandForJoint);

				// connect to right shoulder
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["RightShoulder"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				XMVECTOR jointPosL1 = XMVectorSet(-40.0f * mSpeckRadius, -10.0f * mSpeckRadius, 0.0f, 1.0f);
				XMVECTOR jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				commandForJoint.newSpecks.resize(1);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				GetWorld().ExecuteCommand(commandForJoint);

				// connect to left shoulder
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = mNodesAnimData["LeftShoulder"].index;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				jointPosL1 = XMVectorSet(40.0f * mSpeckRadius, -10.0f * mSpeckRadius, 0.0f, 1.0f);
				jointPosW1 = XMVector3TransformCoord(jointPosL1, worldTransform);
				commandForJoint.newSpecks.resize(1);
				XMStoreFloat3(&commandForJoint.newSpecks[0].position, jointPosW1);
				GetWorld().ExecuteCommand(commandForJoint);
			}

			if (addHead)
			{
				addHead = false;
				WorldCommands::AddSpecksCommand commandForJoint;
				commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
				commandForJoint.frictionCoefficient = command.frictionCoefficient;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = lastAddedRigidBodyIndex;
				commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = commandResult.rigidBodyIndex;

				commandForJoint.newSpecks.clear();
				int nx = 2;
				int ny = 2;
				float width = (nx - 1) * 20.0f * mSpeckRadius;
				float height = (ny - 1) * 20.0f * mSpeckRadius;
				float x = -0.5f*width;
				float y = -0.5f*height;
				float dx = width / (nx - 1);
				float dy = height / (ny - 1);

				for (int i = 0; i < nx; ++i)
				{
					for (int j = 0; j < ny; ++j)
					{
						XMVECTOR jointPosL = XMVectorSet(i*dx + x, -20.0f * mSpeckRadius, j*dy + y, 1.0f);
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosL, worldTransform);
						WorldCommands::NewSpeck ns;
						XMStoreFloat3(&ns.position, jointPosW);
						commandForJoint.newSpecks.push_back(ns);
					}
				}
				GetWorld().ExecuteCommand(commandForJoint);
			}

			lastAddedRigidBodyIndex = commandResult.rigidBodyIndex;
		}
		else
		{
			mNodesAnimData[name].index = -1;
		}

		int numChildren = node->GetChildCount();
		for (int i = 0; i < numChildren; i++)
		{
			FbxNode *childNode = node->GetChild(i);
			nodeStack.push_back(childNode);
		}

		if (name.find("Alpha_Surface") != string::npos)
		{
			skinNode = node;
		}
	}

	// Once the whole 'mNamesDictionary' is constructed.
	//if (skinNode) ProcessRenderSkin(skinNode, pApp);
}

bool operator==(const AppCommands::MeshVertex &left, const AppCommands::MeshVertex &right)
{
	if (left.Normal == right.Normal &&
		left.Position == right.Position &&
		left.TangentU == right.TangentU &&
		left.TexC == right.TexC)
		return true;
	else
		return false;
}

void HumanoidSkeleton::ProcessRenderSkin(fbxsdk::FbxNode * node, App *pApp)
{
	FbxMesh *meshPt = node->GetMesh();
	bool triMesh = meshPt->IsTriangleMesh();
	FbxAMatrix world = node->EvaluateGlobalTransform();

	int polyCount = meshPt->mPolygons.GetCount();
	FbxVector4* controlPoints = meshPt->GetControlPoints();
	int controlPointCount = meshPt->GetControlPointsCount();

	FbxStringList pUVSetNameList;
	meshPt->GetUVSetNames(pUVSetNameList);

	AppCommands::CreateGeometryCommand cgc;
	cgc.geometryName = node->GetName();
	cgc.meshName = meshPt->GetName();

	// for each polygon
	for (int i = 0; i < polyCount; i++)
	{
		// for each triangle
		int polyVertCount = meshPt->GetPolygonSize(i);
		for (int j = 0; j < polyVertCount; j++)
		{

			AppCommands::MeshVertex vert;
			int cpIndex = meshPt->GetPolygonVertex(i, j);

			controlPoints[cpIndex].mData[3] = 1.0f;
			FbxVector4 pos = world.MultT(controlPoints[cpIndex]);

			vert.Position = XMFLOAT3((float)pos.mData[0], (float)pos.mData[1], (float)pos.mData[2]);

			FbxVector4 pNormal;
			meshPt->GetPolygonVertexNormal(i, j, pNormal);
			vert.Normal = XMFLOAT3((float)pNormal.mData[0], (float)pNormal.mData[1], (float)pNormal.mData[2]);

			FbxVector2 pUV;
			bool hasUV = false, retVal = false;
			for (int k = 0; k < pUVSetNameList.GetCount(); k++)
			{
				retVal = meshPt->GetPolygonVertexUV(i, j, pUVSetNameList[k], pUV, hasUV);
				if (retVal) break;
			}
			if (retVal)
				vert.TexC = XMFLOAT2((float)pUV.mData[0], (float)pUV.mData[1]);

			// Add to command lists
			size_t size = cgc.vertices.size();
			size_t i;
			for (i = 0; i < size; i++)
			{
				if (vert == cgc.vertices[i])
				{
					break;
				}
			}

			if (i == size)
			{
				cgc.vertices.push_back(vert);
			}
			cgc.indices.push_back((uint32_t)i);
		}
	}

	////
	//// Skinning
	////
	//struct VertexSkinningData
	//{
	//	UINT count = 0;
	//	UINT bones[10];
	//	float weights[10];
	//};
	//vector<VertexSkinningData> verticesSkinningData;
	//verticesSkinningData.resize(cgc.vertices.size());
	//int nDeformers = meshPt->GetDeformerCount();
	//FbxSkin *pSkin = (FbxSkin*)meshPt->GetDeformer(0, FbxDeformer::eSkin);

	//// iterate bones
	//int ncBones = pSkin->GetClusterCount();
	//for (int boneIndex = 0; boneIndex < ncBones; ++boneIndex)
	//{
	//	// cluster
	//	FbxCluster* cluster = pSkin->GetCluster(boneIndex);

	//	// bone ref
	//	FbxNode* pBone = cluster->GetLink();
	//	string originalName = pBone->GetName();
	//	string name = mNamesDictionary[originalName];
	//	UINT rigidBodyIndex = mNodesAnimData[name].index;

	//	// Get the bind pose
	//	FbxAMatrix bindPoseMatrix, transformMatrix;
	//	cluster->GetTransformMatrix(transformMatrix);
	//	cluster->GetTransformLinkMatrix(bindPoseMatrix);
	//	XMFLOAT4X4 bindPoseMatrixF;
	//	Conv(&bindPoseMatrixF, bindPoseMatrix);
	//	XMMATRIX bindPose = XMLoadFloat4x4(&bindPoseMatrixF);
	//	XMVECTOR det;
	//	XMMATRIX invBindPose = XMMatrixInverse(&det, bindPose);

	//	// decomposed transform components
	//	FbxVector4 vS = bindPoseMatrix.GetS();
	//	FbxVector4 vR = bindPoseMatrix.GetR();
	//	FbxVector4 vT = bindPoseMatrix.GetT();

	//	int *pVertexIndices = cluster->GetControlPointIndices();
	//	double *pVertexWeights = cluster->GetControlPointWeights();

	//	// Iterate through all the vertices, which are affected by the bone
	//	int ncVertexIndices = cluster->GetControlPointIndicesCount();
	//	for (int iBoneVertexIndex = 0; iBoneVertexIndex < ncVertexIndices; iBoneVertexIndex++)
	//	{
	//		int niVertex = pVertexIndices[iBoneVertexIndex];
	//		float fWeight = (float)pVertexWeights[iBoneVertexIndex];
	//		UINT count = verticesSkinningData[niVertex].count;
	//		verticesSkinningData[niVertex].bones[count] = boneIndex;
	//		verticesSkinningData[niVertex].weights[count] = fWeight;
	//		++verticesSkinningData[niVertex].count;

	//		if (rigidBodyIndex == 5)
	//		{
	//			XMVECTOR pos;
	//			XMVECTOR nor;
	//			if (verticesSkinningData[niVertex].count > 1) // not first bone
	//			{
	//				pos = XMLoadFloat3(&cgc.vertices[niVertex].Position);
	//				nor = XMLoadFloat3(&cgc.vertices[niVertex].Normal);
	//			}
	//			else // first bone
	//			{
	//				pos = XMVectorZero();
	//				nor = XMVectorZero();
	//			}
	//			//XMStoreFloat3(&cgc.vertices[niVertex].Position, pos);
	//			//XMStoreFloat3(&cgc.vertices[niVertex].Normal, nor);
	//		}
	//	}
	//}

	pApp->ExecuteCommand(cgc);

	// Add an object
	WorldCommands::AddRenderItemCommand cmd3;
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(20.0f, 20.0f, 0.4f));
	cmd3.geometryName = cgc.geometryName;
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = cgc.meshName;
	cmd3.type = WorldCommands::RenderItemType::Static;
	cmd3.staticRenderItem.worldTransform.mS = XMFLOAT3(1.0f, 1.0f, 1.0f);
	cmd3.staticRenderItem.worldTransform.mT = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMStoreFloat4(&cmd3.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 0.0f));
	GetWorld().ExecuteCommand(cmd3);
}

HumanoidSkeleton::HumanoidSkeleton(World &w)
	: WorldUser(w),
	mScene(0),
	mSDKManager(0)
{
	
}

HumanoidSkeleton::~HumanoidSkeleton()
{
	if (mScene)
	{
		mScene->Destroy();
		mScene = 0;
	}
	if (mSDKManager)
	{
		mSDKManager->Destroy();
		mSDKManager = 0;
	}
}

void HumanoidSkeleton::Initialize(const wchar_t * fbxFilePath, App *pApp, bool saveFixed)
{
	// Get world properties
	WorldCommands::GetWorldPropertiesCommand gwp;
	WorldCommands::GetWorldPropertiesCommandResult gwpr;
	GetWorld().ExecuteCommand(gwp, &gwpr);
	mSpeckRadius = gwpr.speckRadius;

	// Create the FBX SDK manager
	mSDKManager = FbxManager::Create();

	// Create an IOSettings object.
	FbxIOSettings * ios = FbxIOSettings::Create(mSDKManager, IOSROOT);
	mSDKManager->SetIOSettings(ios);

	// ... Configure the FbxIOSettings object ...

	// Create an importer.
	FbxImporter* lImporter = FbxImporter::Create(mSDKManager, "");

	// Initialize the importer.
	wstring ws(fbxFilePath);
	string filename(ws.begin(), ws.end());
	bool lImportStatus = lImporter->Initialize(&filename[0], -1, mSDKManager->GetIOSettings());

	// If any errors occur in the call to FbxImporter::Initialize(), the method returns false.
	// To retrieve the error, you must call GetStatus().GetErrorString() from the FbxImporter object. 
	if (!lImportStatus)
	{
		LOG(L"Call to FbxImporter::Initialize() failed.", ERROR);
		//printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
		return;
	}

	// Create a new scene so it can be populated by the imported file.
	mScene = FbxScene::Create(mSDKManager, "myScene");

	// Import the contents of the file into the scene.
	lImporter->Import(mScene);

	// File format version numbers to be populated.
	int lFileMajor, lFileMinor, lFileRevision;

	// Populate the FBX file format version numbers with the import file.
	lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

	// The file has been imported; we can get rid of the unnecessary objects.
	lImporter->Destroy();
	ios->Destroy();

	if (mScene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::cm)
	{
		const FbxSystemUnit::ConversionOptions lConversionOptions = {
			false, /* mConvertRrsNodes */
			true, /* mConvertAllLimits */
			true, /* mConvertClusters */
			true, /* mConvertLightIntensity */
			true, /* mConvertPhotometricLProperties */
			true  /* mConvertCameraClipPlanes */
		};

		// Convert the scene to meters using the defined options.
		FbxSystemUnit::dm.ConvertScene(mScene, lConversionOptions);
	}

	if (saveFixed)
	{
		// Fix the geometry
		FbxGeometryConverter gc(mSDKManager);
		gc.Triangulate(mScene, true);

		// Bake animation layers
		FbxAnimEvaluator* sceneEvaluator = mScene->GetAnimationEvaluator();
		int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
		for (int i = 0; i < numStacks; i++)
		{
			FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(i));
			int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
			FbxTime lFramePeriod;
			lFramePeriod.SetSecondDouble(1.0 / 24); // 24 is the frame rate
			FbxTimeSpan lTimeSpan = pAnimStack->GetLocalTimeSpan();
			pAnimStack->BakeLayers(sceneEvaluator, lTimeSpan.GetStart(), lTimeSpan.GetStop(), lFramePeriod);
		}

		// Create an exporter.
		FbxExporter* exporter = FbxExporter::Create(mSDKManager, "");
		// Initialize the exporter.
		bool lExportStatus = exporter->Initialize(&filename[0], -1, mSDKManager->GetIOSettings());
		// If any errors occur in the call to FbxExporter::Initialize(), the method returns false.
		// To retrieve the error, you must call GetStatus().GetErrorString() from the FbxImporter object. 
		if (!lExportStatus)
		{
			LOG(L"Call to FbxExporter::Initialize() failed.", ERROR);
			return;
		}

		// Export the scene.
		exporter->Export(mScene);
		// Destroy the exporter.
		exporter->Destroy();
	}

	CreateSpecksBody(pApp);

	// Set the state
	mState = BindPose;
}

void HumanoidSkeleton::UpdateAnimation(float time)
{
	// Create the body made out of specks
	int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
	FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(numStacks-1));
	FbxTimeSpan &timeSpan = pAnimStack->GetLocalTimeSpan();
	float animTime = (float)timeSpan.GetDuration().GetSecondDouble();
	int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
	FbxAnimLayer* animLayer = pAnimStack->GetMember<FbxAnimLayer>(0);
	FbxAnimEvaluator* sceneEvaluator = mScene->GetAnimationEvaluator();

	// Create a time object.
	FbxTime myTime;					// The time for each key in the animation curve(s)
	if (time > animTime)
	{
		int a = (int)(time / animTime);
		float newTime = (time - animTime*a);
		myTime.SetSecondDouble(newTime);
	}
	else
		myTime.SetSecondDouble(time);

	vector<FbxNode *> nodeStack;
	FbxNode *root = mScene->GetRootNode();
	nodeStack.push_back(root);

	while (!nodeStack.empty())
	{
		// Pop the last item on the stack
		FbxNode *node = *(nodeStack.end() - 1);
		nodeStack.pop_back();

		// Get a reference to node’s local transform.
		FbxMatrix nodeTransform = sceneEvaluator->GetNodeGlobalTransform(node, myTime);

		string name = node->GetName();
		name = mNamesDictionary[name];

		WorldCommands::UpdateSpeckRigidBodyCommand command;
		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;

		command.rigidBodyIndex = mNodesAnimData[name].index;
		if (command.rigidBodyIndex != -1)
		{
			XMFLOAT4X4 worldMatrix;
			Conv(&worldMatrix, nodeTransform);
			XMMATRIX w = XMLoadFloat4x4(&worldMatrix);
			XMMATRIX localTransform = XMLoadFloat4x4(&mNodesAnimData[name].localTransform);
			w = XMMatrixMultiply(localTransform, w);
			XMVECTOR s, r, t;
			XMMatrixDecompose(&s, &r, &t, w);
			XMStoreFloat3(&command.transform.mT, t);
			XMStoreFloat4(&command.transform.mR, r);
			//XMStoreFloat3(&command.transform.mS, s);
			GetWorld().ExecuteCommand(command);
		}
		int numChildren = node->GetChildCount();
		for (int i = 0; i < numChildren; i++)
		{
			FbxNode *childNode = node->GetChild(i);
			nodeStack.push_back(childNode);
		}
	}
	// Set the state
	mState = Animating;
}

void HumanoidSkeleton::StartSimulation()
{
	if (mState == Simulating) return;

	// Create the body made out of specks
	int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
	FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(0));
	FbxTimeSpan &timeSpan = pAnimStack->GetLocalTimeSpan();
	float animTime = (float)timeSpan.GetDuration().GetSecondDouble();
	int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
	FbxAnimLayer* animLayer = pAnimStack->GetMember<FbxAnimLayer>(0);
	FbxAnimEvaluator* sceneEvaluator = mScene->GetAnimationEvaluator();

	vector<FbxNode *> nodeStack;
	FbxNode *root = mScene->GetRootNode();
	nodeStack.push_back(root);

	while (!nodeStack.empty())
	{
		// Pop the last item on the stack
		FbxNode *node = *(nodeStack.end() - 1);
		nodeStack.pop_back();

		string name = node->GetName();
		name = mNamesDictionary[name];

		WorldCommands::UpdateSpeckRigidBodyCommand command;
		command.movementMode = WorldCommands::RigidBodyMovementMode::GPU;

		command.rigidBodyIndex = mNodesAnimData[name].index;
		if (command.rigidBodyIndex != -1)
		{
			//XMStoreFloat3(&command.transform.mS, s);
			GetWorld().ExecuteCommand(command);
		}
		int numChildren = node->GetChildCount();
		for (int i = 0; i < numChildren; i++)
		{
			FbxNode *childNode = node->GetChild(i);
			nodeStack.push_back(childNode);
		}
	}

	// Set the state
	mState = Simulating;
}
