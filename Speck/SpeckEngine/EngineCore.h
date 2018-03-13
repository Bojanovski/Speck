
#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include "SpeckEngineDefinitions.h"

namespace Speck
{
	//-------------------------------------------------------------------------------------
	//	This structure holds all graphic and system related values.
	//	It is ment to be initialzed
	//	only once (in graphic initialization class) and then
	//	passed to game components via pointer.
	//-------------------------------------------------------------------------------------

	class Timer;
	class Camera;
	class CameraController;
	class InputHandler;
	class DirectXCore;

	struct ProcessAndSystemData;
	namespace AppCommands
	{
		struct LimitFrameTimeCommand;
		struct UpdateCameraCommand;
		struct LoadResourceCommand;
		struct CreateMaterialCommand;
		struct CreateStaticGeometryCommand;
		struct CreateSkinnedGeometryCommand;
	}
	namespace WorldCommands
	{
		struct AddEnviromentMapCommand;
		struct CreatePSOGroupCommand;
	}

	class EngineCore
	{
		friend class D3DApp;
		friend class SpeckApp;
		friend class SpecksHandler;
		friend class SpeckWorld;
		friend class CubeRenderTarget;
		friend struct RenderItem;

		friend struct AppCommands::LimitFrameTimeCommand;
		friend struct AppCommands::UpdateCameraCommand;
		friend struct AppCommands::LoadResourceCommand;
		friend struct AppCommands::CreateMaterialCommand;
		friend struct AppCommands::CreateStaticGeometryCommand;
		friend struct AppCommands::CreateSkinnedGeometryCommand;
		friend struct WorldCommands::AddEnviromentMapCommand;
		friend struct WorldCommands::CreatePSOGroupCommand;

	public:
		DLL_EXPORT EngineCore();
		DLL_EXPORT ~EngineCore();
		// Make these inaccessible.
		EngineCore(const EngineCore &engineCore) = delete;
		EngineCore &operator=(const EngineCore &engineCore) = delete;

		//
		// GETTERS
		//
		InputHandler const *GetInputHandler() const { return mInputHandler.get(); }
		Camera const &GetCamera() const { return *mCamera; }
		ProcessAndSystemData const *GetProcessAndSystemData() const { return mProcessAndSystemData.get(); }
		Timer const *GetTimer() const { return mTimer.get(); }
		DLL_EXPORT int GetClientHeight() const;
		DLL_EXPORT int GetClientWidth() const;
		DLL_EXPORT float GetAspectRatio() const;
		DirectXCore const &GetDirectXCore() const { return *mDirectXCore.get(); };

	private:
		//
		// GETTERS
		//
		Camera &GetCamera() { return *mCamera; }
		Timer *GetTimer() { return mTimer.get(); }
		DirectXCore &GetDirectXCore() { return *mDirectXCore.get(); };

		//
		// SETTERS
		//
		void SetCamera(Camera *newCamera) { mCamera = newCamera; }
		void SetDefaultCamera() { mCamera = mDefaultCamera.get(); }

	private:
		// Pointer to the currently used camera component and it's controller.
		Camera *mCamera;
		// Default camera
		std::unique_ptr<Camera> mDefaultCamera;
		// Information about process and system.
		std::unique_ptr<ProcessAndSystemData> mProcessAndSystemData;
		// Used to keep track of the “delta-time” and game time (§4.4).
		std::unique_ptr<Timer> mTimer;
		// Input stuff
		std::unique_ptr<InputHandler> mInputHandler;
		// All the DirectX stuff goes here
		std::unique_ptr<DirectXCore> mDirectXCore;
	};
}

#endif
