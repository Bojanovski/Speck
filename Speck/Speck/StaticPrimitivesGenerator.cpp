
#include "StaticPrimitivesGenerator.h"
#include <World.h>
#include <WorldCommands.h>

using namespace std;
using namespace DirectX;
using namespace Speck;

StaticPrimitivesGenerator::StaticPrimitivesGenerator(World &world)
	: WorldUser(world)
{

}

StaticPrimitivesGenerator::~StaticPrimitivesGenerator()
{

}

void StaticPrimitivesGenerator::GenerateBox(const XMFLOAT3 & scale, const char * materialName, const XMFLOAT3 & position, const XMFLOAT3 & rotation)
{
	// add the skin
	WorldCommands::AddRenderItemCommand aric;
	XMStoreFloat4x4(&aric.texTransform, XMMatrixScaling(2000.0f, 1.0f, 1.0f)); // needs to be addressed
	aric.geometryName = "shapeGeo";
	aric.materialName = materialName;
	aric.meshName = "box";
	aric.type = WorldCommands::RenderItemType::Static;
	aric.staticRenderItem.worldTransform.mS = scale;
	aric.staticRenderItem.worldTransform.mT = position;
	XMStoreFloat4(&aric.staticRenderItem.worldTransform.mR, XMQuaternionRotationRollPitchYaw(rotation.x, rotation.y, rotation.z));
	GetWorld().ExecuteCommand(aric);

	// add the collision
	WorldCommands::AddStaticColliderCommand ascc;
	ascc.transform.mR = aric.staticRenderItem.worldTransform.mR;
	ascc.transform.mT = aric.staticRenderItem.worldTransform.mT;
	ascc.transform.mS = aric.staticRenderItem.worldTransform.mS;
	GetWorld().ExecuteCommand(ascc);
}
