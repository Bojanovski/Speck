
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

	class GameTimer;
	struct ProcessAndSystemData;
	class Camera;
	class CameraController;
	class InputHandler;
	class DirectXCore;

	class EngineCore
	{
		friend class D3DApp;

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
		Camera &GetCamera() { return *mCamera; }
		CameraController &GetCameraController() { return *mCameraController; }
		ProcessAndSystemData const *GetProcessAndSystemData() const { return mProcessAndSystemData.get(); }
		GameTimer const *GetTimer() const { return mTimer.get(); }
		DLL_EXPORT int GetClientHeight() const;
		DLL_EXPORT int GetClientWidth() const;
		DLL_EXPORT float GetAspectRatio() const;
		DirectXCore &GetDirectXCore() { return *mDirectXCore.get(); };

		//
		// SETTERS
		//
		void SetCamera(Camera *newCamera) { mCamera = newCamera; }
		void SetCameraController(CameraController *newCameraController) { mCameraController = newCameraController; }
		void SetDefaultCamera() { mCamera = mDefaultCamera.get(); }

	private:
		// Pointer to the currently used camera component and it's controller.
		Camera *mCamera;
		CameraController *mCameraController = nullptr; // can be null
		// Default camera
		std::unique_ptr<Camera> mDefaultCamera;
		// Information about process and system.
		std::unique_ptr<ProcessAndSystemData> mProcessAndSystemData;
		// Used to keep track of the “delta-time” and game time (§4.4).
		std::unique_ptr<GameTimer> mTimer;
		// Input stuff
		std::unique_ptr<InputHandler> mInputHandler;
		// All the DirectX stuff goes here
		std::unique_ptr<DirectXCore> mDirectXCore;
	};
}

#endif
