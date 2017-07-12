
#include "World.h"
#include "PSOGroup.h"
#include "RenderItem.h"

using namespace Speck;

World::World()
{
}

World::~World()
{
}

void World::Initialize(App *app)
{
	mApp = app;
}

int World::ExecuteCommand(const WorldCommand & command, CommandResult *result)
{
	return command.Execute(mApp, result);
}

void World::Update()
{
}

void World::PreDrawUpdate()
{
}

void World::Draw(UINT stage)
{
}