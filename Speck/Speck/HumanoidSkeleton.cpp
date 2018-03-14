
#include "HumanoidSkeleton.h"
#include <World.h>
#include <App.h>

#include "SpeckPrimitivesGenerator.h"
#include "Json.h"

// FBX
#pragma warning( push )
#pragma warning( disable : 4244 )  
#include <fbxsdk.h>
#include <fbxsdk\utils\fbxgeometryconverter.h>
#pragma warning( pop )

using namespace std;
using namespace DirectX;
using namespace Speck;
using json = nlohmann::json;

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

void HumanoidSkeleton::ProcessBone(FbxNode *node, WorldCommands::AddSpecksCommand *outCommand, const string &boneName)
{
	// FBX
	FbxAMatrix world = node->EvaluateGlobalTransform();
	XMFLOAT4X4 worldF;
	Conv(&worldF, world);
	XMMATRIX worldXM = XMLoadFloat4x4(&worldF);
	XMVECTOR s, r, t;
	XMMatrixDecompose(&s, &r, &t, worldXM);
	XMMATRIX worldWithoutScale = XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(t);

	// Calculate the center of mass
	XMVECTOR com = XMVectorZero();
	for (int i = 0; i < outCommand->newSpecks.size(); i++)
	{
		com += XMLoadFloat3(&outCommand->newSpecks[i].position);
	}
	com /= (float)outCommand->newSpecks.size();

	// Calculate local transform
	FbxVector4 sca = world.GetS();
	XMVECTOR invSca = XMVectorSet(1.0f / (float)sca[0], 1.0f / (float)sca[1], 1.0f / (float)sca[2], 1.0f);
	FbxQuaternion rot = world.GetQ();
	rot.Inverse();
	XMFLOAT4 rotF;
	Conv(&rotF, rot);
	XMVECTOR rotV = XMLoadFloat4(&rotF);
	XMMATRIX rotM = XMMatrixRotationQuaternion(rotV);
	XMMATRIX local = XMMatrixTranslationFromVector(com);

	// Transfrom with the node transform matrix
	for (int i = 0; i < outCommand->newSpecks.size(); ++i)
	{
		XMVECTOR posFinal = XMVector3TransformCoord(XMLoadFloat3(&outCommand->newSpecks[i].position), worldWithoutScale);
		XMStoreFloat3(&outCommand->newSpecks[i].position, posFinal);
	}

	// Add the node to the world
	WorldCommands::AddSpecksCommandResult commandResult;
	if (outCommand->newSpecks.size() > 0)
	{
		GetWorld().ExecuteCommand(*outCommand, &commandResult);
		mNodesAnimData[boneName].index = commandResult.rigidBodyIndex;
	}
	mNodesAnimData[boneName].numberOfSpecks = (UINT)outCommand->newSpecks.size();
	XMStoreFloat3(&mNodesAnimData[boneName].centerOfMass, com);
	XMStoreFloat3(&mNodesAnimData[boneName].inverseScale, invSca);
	mNodesAnimData[boneName].inverseRotation = rotF;
	XMStoreFloat4x4(&mNodesAnimData[boneName].bindPoseWorldTransform, worldWithoutScale);
}

void HumanoidSkeleton::CreateSpecksBody(Speck::App *pApp)
{
	// Parse the JSON file
	json j;
	try
	{
		j = json::parse(mSpeckBodyStructureJSONFile);
	}
	catch (const exception& ex)
	{
		(void)ex; // Mark the object as "used" by casting it to void. It has no influence on the generated machine code, but it will suppress the compiler warning.
		LOG(TEXT("Exception caught while parsing a json file: ") + StrToWStr(mName) + TEXT(" ."), WARNING);
		LOG(StrToWStr(ex.what()), ERROR);
	}

	// Create a body made out of specks
	int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
	FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(0));
	vector<FbxNode *> nodeStack;
	FbxNode *root = mScene->GetRootNode();
	nodeStack.push_back(root);

	// Body creation logic
	FbxNode *skinNode = nullptr;

	while (!nodeStack.empty())
	{
		// Pop the last item on the stack
		FbxNode *node = *(nodeStack.end() - 1);
		nodeStack.pop_back();

		//FbxNodeAttribute* lNodeAttribute = node->GetNodeAttribute();
		string boneName = node->GetName();
		WorldCommands::AddSpecksCommand command;
		command.speckType = WorldCommands::SpeckType::RigidBody;
		command.speckMass = 1.0f;
		command.frictionCoefficient = 0.2f;

		// find the name in the json
		json::iterator it = j.find("bones");
		if (it != j.end())
		{
			json list = it.value();
			for (json::iterator listIt = list.begin(); listIt != list.end(); ++listIt)
			{
				// first check if there is the name we are looking for in this json bone
				json jsonBone = listIt.value();
				json jsonBoneNamesList = jsonBone.at("bones").get<json>();
				string jsonBoneName = "";
				bool boneFound = false;
				for (json::iterator boneNamesListIt = jsonBoneNamesList.begin(); boneNamesListIt != jsonBoneNamesList.end(); ++boneNamesListIt)
				{
					jsonBoneName = boneNamesListIt.value().get<string>();
					if (boneName.compare(jsonBoneName) == 0)
					{
						boneFound = true;
						break;
					}
				}

				// this is the one from the stack
				if (boneFound)
				{
					WorldCommands::NewSpeck speck;
					json speckPositionsList = jsonBone.at("specks").get<json>();
					for (json::iterator speckPositionsListIt = speckPositionsList.begin(); speckPositionsListIt != speckPositionsList.end(); ++speckPositionsListIt)
					{
						json speckPosition = speckPositionsListIt.value();
						speck.position.x = speckPosition.at("x").get<float>();
						speck.position.y = speckPosition.at("y").get<float>();
						speck.position.z = speckPosition.at("z").get<float>();
						command.newSpecks.push_back(speck);
					}

					ProcessBone(node, &command, jsonBoneName);
					break;
				}
			}
		}

		int numChildren = node->GetChildCount();
		for (int i = 0; i < numChildren; i++)
		{
			FbxNode *childNode = node->GetChild(i);
			nodeStack.push_back(childNode);
		}

		if (boneName.find("Guard02") != string::npos)
		{
			skinNode = node;
		}
	}

	// connect the bones
	WorldCommands::AddSpecksCommand commandForJoint;
	commandForJoint.speckType = WorldCommands::SpeckType::RigidBodyJoint;
	commandForJoint.speckMass = 1.0f;
	commandForJoint.frictionCoefficient = 0.2f;
	json::iterator it = j.find("joints");
	if (it != j.end())
	{
		json list = it.value();
		for (json::iterator listIt = list.begin(); listIt != list.end(); ++listIt)
		{
			json jsonJoint = listIt.value();
			json pairsList = jsonJoint.find("pairs").value();
			for (json::iterator pairListIt = pairsList.begin(); pairListIt != pairsList.end(); ++pairListIt)
			{
				json pair = pairListIt.value();
				string jsonJointParentBoneName = pair.at("parentBoneName").get<string>();
				string jsonJointChildBoneName = pair.at("childBoneName").get<string>();
				auto parentIte = mNodesAnimData.find(jsonJointParentBoneName);
				auto childIte = mNodesAnimData.find(jsonJointChildBoneName);

				if (parentIte != mNodesAnimData.end() && childIte != mNodesAnimData.end())
				{
					int parentRBIndex = parentIte->second.index;
					int childRBIndex = childIte->second.index;
					commandForJoint.rigidBodyJoint.rigidBodyIndex[0] = parentRBIndex;
					commandForJoint.rigidBodyJoint.rigidBodyIndex[1] = childRBIndex;
					XMMATRIX parentWorldBindPose = XMLoadFloat4x4(&parentIte->second.bindPoseWorldTransform);
					XMVECTOR det;
					XMMATRIX parentWorldBindPoseInv = XMMatrixInverse(&det, parentWorldBindPose);
					XMMATRIX childWorldBindPose = XMLoadFloat4x4(&childIte->second.bindPoseWorldTransform);
					XMMATRIX childLocalSpaceToParentLocalSpace = childWorldBindPose * parentWorldBindPoseInv;

					XMVECTOR parentCOM = XMLoadFloat3(&parentIte->second.centerOfMass) * (float)parentIte->second.numberOfSpecks;
					XMVECTOR childCOM = XMLoadFloat3(&childIte->second.centerOfMass) * (float)childIte->second.numberOfSpecks;

					// for every speck
					WorldCommands::NewSpeck speck;
					commandForJoint.newSpecks.clear();
					json speckPositionsList = jsonJoint.at("specks").get<json>();
					for (json::iterator speckPositionsListIt = speckPositionsList.begin(); speckPositionsListIt != speckPositionsList.end(); ++speckPositionsListIt)
					{
						// load it
						json speckPosition = speckPositionsListIt.value();
						speck.position.x = speckPosition.at("x").get<float>();
						speck.position.y = speckPosition.at("y").get<float>();
						speck.position.z = speckPosition.at("z").get<float>();
						XMVECTOR jointPosChildLocalSpace = XMLoadFloat3(&speck.position);

						// update the rigid body bones with the mass distribution information
						parentCOM += XMVector3TransformCoord(jointPosChildLocalSpace, childLocalSpaceToParentLocalSpace);
						childCOM += jointPosChildLocalSpace;
						parentIte->second.numberOfSpecks += 1;
						childIte->second.numberOfSpecks += 1;

						// transform it
						XMVECTOR jointPosW = XMVector3TransformCoord(jointPosChildLocalSpace, childWorldBindPose);
						XMStoreFloat3(&speck.position, jointPosW);

						// save it
						commandForJoint.newSpecks.push_back(speck);
					}

					// calculate the new center of mass and store it
					parentCOM = parentCOM / (float)parentIte->second.numberOfSpecks;
					childCOM = childCOM / (float)childIte->second.numberOfSpecks;
					XMStoreFloat3(&parentIte->second.centerOfMass, parentCOM);
					XMStoreFloat3(&childIte->second.centerOfMass, childCOM);

					GetWorld().ExecuteCommand(commandForJoint);
				}
			}
		}
	}

	if (skinNode)
	{
		ProcessRenderSkin(skinNode, pApp);
	}
}

bool operator==(const AppCommands::StaticMeshVertex &left, const AppCommands::StaticMeshVertex &right)
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

	AppCommands::CreateSkinnedGeometryCommand csgc;
	csgc.geometryName = node->GetName();
	csgc.meshName = meshPt->GetName();

	// for each polygon
	for (int i = 0; i < polyCount; i++)
	{
		// for each triangle
		int polyVertCount = meshPt->GetPolygonSize(i);
		for (int j = 0; j < polyVertCount; j++)
		{
			AppCommands::SkinnedMeshVertex vert;
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
			size_t size = csgc.vertices.size();
			size_t i;
			for (i = 0; i < size; i++)
			{
				if (vert == csgc.vertices[i])
				{
					break;
				}
			}

			if (i == size)
			{
				csgc.vertices.push_back(vert);
			}
			csgc.indices.push_back((uint32_t)i);
		}
	}

	//
	// Skinning
	//
	struct VertexSkinningData
	{
		UINT count = 0;
		int bones[10];
		float weights[10];
	};
	vector<VertexSkinningData> verticesSkinningData;
	verticesSkinningData.resize(csgc.vertices.size());
	int nDeformers = meshPt->GetDeformerCount();
	FbxSkin *pSkin = (FbxSkin*)meshPt->GetDeformer(0, FbxDeformer::eSkin);

	// iterate bones
	int ncBones = pSkin->GetClusterCount();
	for (int boneIndex = 0; boneIndex < ncBones; ++boneIndex)
	{
		// cluster
		FbxCluster* cluster = pSkin->GetCluster(boneIndex);

		// bone ref
		FbxNode* pBone = cluster->GetLink();
		string boneName = pBone->GetName();
		int rigidBodyIndex = mNodesAnimData[boneName].index;
		if (rigidBodyIndex == -1) rigidBodyIndex = 0;

		// Get the bind pose
		FbxAMatrix bindPoseMatrix, transformMatrix;
		cluster->GetTransformMatrix(transformMatrix);
		cluster->GetTransformLinkMatrix(bindPoseMatrix);
		XMFLOAT4X4 bindPoseMatrixF;
		Conv(&bindPoseMatrixF, bindPoseMatrix);
		XMMATRIX bindPose = XMLoadFloat4x4(&bindPoseMatrixF);
		XMVECTOR det;
		XMMATRIX invBindPose = XMMatrixInverse(&det, bindPose);

		// decomposed transform components
		FbxVector4 vS = bindPoseMatrix.GetS();
		FbxVector4 vR = bindPoseMatrix.GetR();
		FbxVector4 vT = bindPoseMatrix.GetT();

		int *pVertexIndices = cluster->GetControlPointIndices();
		double *pVertexWeights = cluster->GetControlPointWeights();

		// Iterate through all the vertices, which are affected by the bone
		int ncVertexIndices = cluster->GetControlPointIndicesCount();
		for (int iBoneVertexIndex = 0; iBoneVertexIndex < ncVertexIndices; iBoneVertexIndex++)
		{
			int niVertex = pVertexIndices[iBoneVertexIndex];
			float fWeight = (float)pVertexWeights[iBoneVertexIndex];
			UINT count = verticesSkinningData[niVertex].count;
			// instead of the bone index, put rigidBodyIndex
			verticesSkinningData[niVertex].bones[count] = rigidBodyIndex;
			verticesSkinningData[niVertex].weights[count] = fWeight;
			++verticesSkinningData[niVertex].count;
		}
	}

	// for each vertex migrate the weights
	for (int i = 0; i < (int)csgc.vertices.size(); ++i)
	{
		AppCommands::SkinnedMeshVertex &vert = csgc.vertices[i];
		vert.BoneWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
		int newArray[] = { 0, 0, 0, 0 };
		memcpy(vert.BoneIndices, newArray, sizeof(int) * 4);
		VertexSkinningData &vertSkinningData = verticesSkinningData[i];
		float weightSum = 0.0f;

		// populate with weights
		for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
		{
			// find the greatest weight
			float greatestWeight = 0.0f;
			int greatestWeightIndex = -1;
			for (int k = 0; k < (int)vertSkinningData.count; k++)
			{
				if (greatestWeight < vertSkinningData.weights[k])
				{
					greatestWeight = vertSkinningData.weights[k];
					greatestWeightIndex = vertSkinningData.bones[k];
					vertSkinningData.weights[k] = 0.0f; // make it 'used'
				}
			}

			// add it to the list
			if (greatestWeightIndex != -1)
			{
				vert.BoneWeights[j] = greatestWeight;
				vert.BoneIndices[j] = greatestWeightIndex;
				weightSum += greatestWeight;
			}
		}

		// fix the sum
		for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j)
		{
			vert.BoneWeights[j] /= weightSum;
		}
	}

	pApp->ExecuteCommand(csgc);

	// Add an object
	WorldCommands::AddRenderItemCommand cmd3;
	XMStoreFloat4x4(&cmd3.texTransform, XMMatrixScaling(20.0f, 20.0f, 0.4f));
	cmd3.geometryName = csgc.geometryName;
	cmd3.materialName = "pbrMatTest";
	cmd3.meshName = csgc.meshName;
	cmd3.type = WorldCommands::RenderItemType::SpeckSkeletalBody;
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

void HumanoidSkeleton::Initialize(const wchar_t * fbxFilePath, const wchar_t *speckStructureJSON, App *pApp, bool saveFixed)
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
	bool lImportStatus = lImporter->Initialize(filename.c_str(), -1, mSDKManager->GetIOSettings());

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
		bool lExportStatus = exporter->Initialize(filename.c_str(), -1, mSDKManager->GetIOSettings());
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

	// Destroy the IOSettings object.
	ios->Destroy();

	// Open the JSON file, read from it and close it
	fstream fileStream;
	fileStream.open(speckStructureJSON, fstream::in | fstream::out | fstream::app);
	fileStream.seekg(0, ios::end);
	mSpeckBodyStructureJSONFile.reserve(fileStream.tellg());
	fileStream.seekg(0, ios::beg);
	mSpeckBodyStructureJSONFile.assign((istreambuf_iterator<char>(fileStream)), istreambuf_iterator<char>());
	fileStream.close();

	CreateSpecksBody(pApp);

	// Set the state
	mState = BindPose;
}

void HumanoidSkeleton::UpdateAnimation(float time)
{
	int numStacks = mScene->GetSrcObjectCount<FbxAnimStack>();
	FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(mScene->GetSrcObject<FbxAnimStack>(numStacks - 1));
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

		// Get a reference to node�s local transform.
		FbxMatrix nodeTransform = sceneEvaluator->GetNodeGlobalTransform(node, myTime);
		string name = node->GetName();

		WorldCommands::UpdateSpeckRigidBodyCommand command;
		command.movementMode = WorldCommands::RigidBodyMovementMode::CPU;

		command.rigidBodyIndex = mNodesAnimData[name].index;
		if (command.rigidBodyIndex != -1)
		{
			XMFLOAT4X4 worldMatrix;
			Conv(&worldMatrix, nodeTransform);
			XMMATRIX w = XMLoadFloat4x4(&worldMatrix);
			XMVECTOR inverseLocalTranslation = XMLoadFloat3(&mNodesAnimData[name].centerOfMass);
			XMVECTOR inverseLocalRotation = XMLoadFloat4(&mNodesAnimData[name].inverseRotation);
			XMVECTOR inverseLocalScale = XMLoadFloat3(&mNodesAnimData[name].inverseScale);
			XMMATRIX inverseLocalTransform = XMMatrixTranslationFromVector(inverseLocalTranslation) * XMMatrixRotationQuaternion(inverseLocalRotation) * XMMatrixScalingFromVector(inverseLocalScale);
			w = XMMatrixMultiply(inverseLocalTransform, w);
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
