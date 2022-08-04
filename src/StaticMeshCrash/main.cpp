#include <array>
#include <iostream>
#include <random>
#include <vector>
#include "phonon.h"

// Exception handler windows only
#ifdef WIN32
#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tlhelp32.h>

static void* pExceptionHandlerHandle;
LONG WINAPI ExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	std::cerr << "Exception (0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << ") @ ";
	const auto PID = GetCurrentProcessId();
	const auto crashAddress = ExceptionInfo->ExceptionRecord->ExceptionAddress;

	// Take a snapshot of our current process
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
	hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, PID);
	if (hModuleSnap == INVALID_HANDLE_VALUE)
	{
		std::cerr << crashAddress << std::endl;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Try to fetch a module
	MODULEENTRY32 me32{};
	me32.dwSize = sizeof(MODULEENTRY32);
	if (!Module32First(hModuleSnap, &me32))
	{
		CloseHandle(hModuleSnap);
		std::cerr << crashAddress << std::endl;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Iterate through our modules
	do
	{
		// If it lands into our module, take a relative offset
		if (crashAddress >= me32.modBaseAddr && crashAddress <= (me32.modBaseAddr + me32.modBaseSize))
		{
			std::cerr << me32.szModule << "+" << (void*)((std::uintptr_t)crashAddress - (std::uintptr_t)me32.modBaseAddr) << std::endl;
			return EXCEPTION_CONTINUE_SEARCH;
		}
	} while (Module32Next(hModuleSnap, &me32));
	CloseHandle(hModuleSnap);

	// Couldn't find it, just spit out the actual address
	std::cerr << crashAddress << std::endl;

	return EXCEPTION_CONTINUE_SEARCH;
}

#endif

#define ASSERT(condition) do {\
	if (!(condition)) [[unlikely]] { \
		std::cerr << "Condition " << (#condition) << " failed!" << std::endl; \
		std::exit(0); \
	} \
} while (0);

void SetupExceptionCatcher()
{
#ifdef WIN32
	pExceptionHandlerHandle = AddVectoredExceptionHandler(1, ExceptionHandler);
#endif
}

void CleanupExceptionHandler()
{
#ifdef WIN32
	if (pExceptionHandlerHandle != nullptr)
	{
		RemoveVectoredExceptionHandler(pExceptionHandlerHandle);
		pExceptionHandlerHandle = nullptr;
	}
#endif
}

IPLStaticMesh LoadSceneFromFile(IPLScene pScene, bool bLimitTriangles)
{
	struct SceneHeader {
		IPLint32 numVerticies{};
		IPLint32 numTriangles{};
		IPLint32 numMaterials{};
	};

	SceneHeader fileHeader{};
	std::vector<IPLVector3> verticies;
	std::vector<IPLTriangle> triangles;
	std::vector<IPLint32> materialIndicies;
	std::vector<IPLMaterial> materials;

	FILE* fp = fopen("Scene.raw", "rb");
	ASSERT(fp != nullptr);
	
	// Not going to bother checking this, only a test case
	fread(&fileHeader, sizeof(SceneHeader), 1, fp);
	verticies.resize(fileHeader.numVerticies);
	triangles.resize(fileHeader.numTriangles);
	materialIndicies.resize(fileHeader.numTriangles);
	materials.resize(fileHeader.numMaterials);
	fread(verticies.data(), sizeof(IPLVector3), verticies.size(), fp);
	fread(triangles.data(), sizeof(IPLTriangle), triangles.size(), fp);
	fread(materialIndicies.data(), sizeof(IPLint32), materialIndicies.size(), fp);
	fread(materials.data(), sizeof(IPLMaterial), materials.size(), fp);
	fclose(fp);

	IPLStaticMeshSettings staticMeshSettings{};
	staticMeshSettings.numVertices = (IPLint32)verticies.size();
	staticMeshSettings.numTriangles = (IPLint32)bLimitTriangles ? 646 : triangles.size();
	staticMeshSettings.numMaterials = (IPLint32)materials.size();

	staticMeshSettings.vertices = verticies.data();
	staticMeshSettings.triangles = triangles.data();
	staticMeshSettings.materialIndices = materialIndicies.data();

	staticMeshSettings.materials = materials.data();

	IPLStaticMesh pMesh = nullptr;
	if (iplStaticMeshCreate(pScene, &staticMeshSettings, &pMesh) != IPL_STATUS_SUCCESS)
	{
		return nullptr;
	}
	return pMesh;
}

int main(int argc, char** argv)
{
	// Just so we can log our exceptions
	SetupExceptionCatcher();

	bool bUseEmbree = false;
	bool bLimitTriangles = false;

	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "--embree") == 0)
		{
			bUseEmbree = true;
			break;
		}

		if (strcmp(argv[i], "--limit-triangle") == 0)
		{
			bLimitTriangles = true;
			break;
		}
	}

	IPLContextSettings contextSettings{};
	contextSettings.version = STEAMAUDIO_VERSION;

	IPLContext pContext = nullptr;
	ASSERT(iplContextCreate(&contextSettings, &pContext) == IPL_STATUS_SUCCESS);

	IPLEmbreeDevice pDevice = nullptr;
	IPLSceneSettings sceneSettings{};

	if (bUseEmbree)
	{
		IPLEmbreeDeviceSettings deviceSettings{};
		ASSERT(iplEmbreeDeviceCreate(pContext, &deviceSettings, &pDevice) == IPL_STATUS_SUCCESS);
		sceneSettings.type = IPL_SCENETYPE_EMBREE;
		sceneSettings.embreeDevice = pDevice;
	}
	else
	{
		sceneSettings.type = IPL_SCENETYPE_DEFAULT;
	}

	IPLScene pScene = nullptr;
	ASSERT(iplSceneCreate(pContext, &sceneSettings, &pScene) == IPL_STATUS_SUCCESS);

	{
		IPLStaticMesh pStaticMesh = LoadSceneFromFile(pScene, bLimitTriangles);
		iplStaticMeshRemove(pStaticMesh, pScene);
		iplSceneCommit(pScene);
		iplStaticMeshRelease(&pStaticMesh);
	}

	iplSceneRelease(&pScene);
	iplEmbreeDeviceRelease(&pDevice);
	iplContextRelease(&pContext);

	// Remove our exception handler
	CleanupExceptionHandler();
	return 0;
}
