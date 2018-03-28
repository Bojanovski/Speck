
#include "FBXSceneManager.h"
#include <SpeckEngineDefinitions.h>

// FBX
#pragma warning( push )
#pragma warning( disable : 4244 )  
#include <fbxsdk.h>
#include <fbxsdk\utils\fbxgeometryconverter.h>
#pragma warning( pop )

using namespace std;

FBXSceneManager::FBXSceneManager()
	: mSDKManager(nullptr)
	, mIOs(nullptr)
{
	// Create the FBX SDK manager
	mSDKManager = FbxManager::Create();

	// Create an IOSettings object.
	mIOs = FbxIOSettings::Create(mSDKManager, IOSROOT);
	mSDKManager->SetIOSettings(mIOs);

	// ... Configure the FbxIOSettings object ...

}

FBXSceneManager::~FBXSceneManager()
{
	// First destroy all the scenes
	for (auto it = mScenes.begin(); it != mScenes.end(); it++)
	{
		it->second->Destroy();
	}
	mScenes.clear();

	// Destroy the IOSettings object.
	if (mIOs)
	{
		mIOs->Destroy();
		mIOs = 0;
	}

	// Finnaly destroy the manager
	if (mSDKManager)
	{
		mSDKManager->Destroy();
		mSDKManager = 0;
	}
}

FbxScene * FBXSceneManager::GetScene(const wchar_t * filePath)
{
	map<wstring, FbxScene *>::iterator it = mScenes.find(filePath);
	if (it == mScenes.end())
	{
		// Create an importer.
		FbxImporter* lImporter = FbxImporter::Create(mSDKManager, "");

		// Initialize the importer.
		wstring ws(filePath);
		string filename(ws.begin(), ws.end());
		bool lImportStatus = lImporter->Initialize(filename.c_str(), -1, mSDKManager->GetIOSettings());

		// If any errors occur in the call to FbxImporter::Initialize(), the method returns false.
		// To retrieve the error, you must call GetStatus().GetErrorString() from the FbxImporter object. 
		if (!lImportStatus)
		{
			LOG(L"Call to FbxImporter::Initialize() failed.", ERROR);
			//printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
			return nullptr;
		}

		// Create a new scene so it can be populated by the imported file.
		FbxScene *scene = FbxScene::Create(mSDKManager, "myScene");

		// Import the contents of the file into the scene.
		lImporter->Import(scene);

		// File format version numbers to be populated.
		int lFileMajor, lFileMinor, lFileRevision;

		// Populate the FBX file format version numbers with the import file.
		lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

		// The file has been imported; we can get rid of the unnecessary objects.
		lImporter->Destroy();

		if (scene->GetGlobalSettings().GetSystemUnit() == FbxSystemUnit::cm)
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
			FbxSystemUnit::dm.ConvertScene(scene, lConversionOptions);
		}

		// save it to the map
		mScenes[filePath] = scene;
		return scene;
	}
	else
	{
		return it->second;
	}
}

FbxScene * FBXSceneManager::FixSceneSaveScene(const wchar_t * filePath)
{
	map<wstring, FbxScene *>::iterator it = mScenes.find(filePath);
	if (it != mScenes.end())
	{
		FbxScene *scene = it->second;

		// Fix the geometry
		FbxGeometryConverter gc(mSDKManager);
		gc.Triangulate(scene, true);

		// Bake animation layers
		FbxAnimEvaluator* sceneEvaluator = scene->GetAnimationEvaluator();
		int numStacks = scene->GetSrcObjectCount<FbxAnimStack>();
		for (int i = 0; i < numStacks; i++)
		{
			FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(scene->GetSrcObject<FbxAnimStack>(i));
			int numAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
			FbxTime lFramePeriod;
			lFramePeriod.SetSecondDouble(1.0 / 24); // 24 is the frame rate
			FbxTimeSpan lTimeSpan = pAnimStack->GetLocalTimeSpan();
			pAnimStack->BakeLayers(sceneEvaluator, lTimeSpan.GetStart(), lTimeSpan.GetStop(), lFramePeriod);
		}

		// Create an exporter.
		FbxExporter* exporter = FbxExporter::Create(mSDKManager, "");
		// Initialize the exporter.
		bool lExportStatus = exporter->Initialize(WStrToStr(filePath).c_str(), -1, mSDKManager->GetIOSettings());
		// If any errors occur in the call to FbxExporter::Initialize(), the method returns false.
		// To retrieve the error, you must call GetStatus().GetErrorString() from the FbxImporter object. 
		if (!lExportStatus)
		{
			LOG(L"Call to FbxExporter::Initialize() failed.", ERROR);
			return nullptr;
		}

		// Export the scene.
		exporter->Export(scene);

		// Destroy the exporter.
		exporter->Destroy();

		return scene;
	}
	else
	{
		LOG("Scene not found.", WARNING);
		return nullptr;
	}
}
