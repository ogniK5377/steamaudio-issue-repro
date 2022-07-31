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
		if (crashAddress >= me32.modBaseAddr && crashAddress <= (me32.modBaseAddr+me32.modBaseSize))
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
	} \
} while (0);

constexpr std::array<IPLVector3, 4> UnitSquareVerts{ {
	{0.0f, 0.0f, 0.0f},
	{1.0f, 0.0f, 0.0f},
	{1.0f, 1.0f, 0.0f},
	{0.0f, 1.0f, 0.0f}
} };

constexpr std::array<IPLTriangle, 2> UnitSquareTriangles{ {
	{0, 1, 2},
	{0, 2, 3}
} };

constexpr std::array<IPLMaterial, 1> UnitSquareMaterials{ {
	{ {0.1f, 0.1f, 0.1f}, 0.5f, {0.2f, 0.2f, 0.2f} }
} };

constexpr std::array<IPLint32, 2> UnitSquareMaterialIndices{ 0, 0 };

IPLStaticMesh BuildRandomSceneGeometry(IPLScene pScene, int nSquareCount)
{
	ASSERT(pScene != nullptr);
	std::vector<IPLVector3> verticies;
	std::vector<IPLTriangle> triangles;
	std::vector<IPLint32> materialIndicies;
	std::vector<IPLMaterial> materials;

	materials.push_back(UnitSquareMaterials[0]);

	// Fixed seed to ensure our output is the same
	std::mt19937 rng{ 1337 };
	constexpr auto nMaxValue = static_cast<float>(std::mt19937::max());

	auto GetRandomFloat = [&](float flMin, float flMax) {
		const float flDiff = flMax - flMin;
		const float flValue = (static_cast<float>(rng()) / nMaxValue);
		// (min + (x - 0) * (max - min)) / (1 - 0)
		return flMin + flValue * flDiff;
	};

	for (int i = 0; i < nSquareCount; i++)
	{
		const int nVertexOffset = i * 4;
		const float flScale = GetRandomFloat(1.0f, 3.0f);
		const IPLVector3 vRandomPosition{
			GetRandomFloat(-5.0f, 5.0f), GetRandomFloat(-5.0f, 5.0f), GetRandomFloat(-5.0f, 5.0f)
		};

		for (const auto vert : UnitSquareVerts)
		{
			verticies.push_back({
				(vert.x * flScale) + vRandomPosition.x,
				(vert.y * flScale) + vRandomPosition.y,
				(vert.z * flScale) + vRandomPosition.z,
				});
		}

		for (const auto triangle : UnitSquareTriangles)
		{
			triangles.push_back({
					triangle.indices[0] + nVertexOffset,
					triangle.indices[1] + nVertexOffset,
					triangle.indices[2] + nVertexOffset
				});
			materialIndicies.push_back(0);
		}
	}

	IPLStaticMeshSettings staticMeshSettings{};
	staticMeshSettings.numVertices = static_cast<IPLint32>(verticies.size());
	staticMeshSettings.numTriangles = static_cast<IPLint32>(triangles.size());
	staticMeshSettings.numMaterials = static_cast<IPLint32>(materials.size());

	staticMeshSettings.vertices = verticies.data();
	staticMeshSettings.triangles = triangles.data();
	staticMeshSettings.materialIndices = materialIndicies.data();
	staticMeshSettings.materials = materials.data();

	IPLStaticMesh pStaticMesh = nullptr;
	ASSERT(iplStaticMeshCreate(pScene, &staticMeshSettings, &pStaticMesh) == IPL_STATUS_SUCCESS);
	iplStaticMeshAdd(pStaticMesh, pScene);
	iplSceneCommit(pScene);

	return pStaticMesh;
}

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

int main(int argc, char** argv)
{
	// Just so we can log our exceptions
	SetupExceptionCatcher();

	bool bDontUseEmbree = false;

	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "--no-embree") == 0)
		{
			bDontUseEmbree = true;
			break;
		}
	}

	IPLContextSettings contextSettings{};
	contextSettings.version = STEAMAUDIO_VERSION;

	IPLContext pContext = nullptr;
	ASSERT(iplContextCreate(&contextSettings, &pContext) == IPL_STATUS_SUCCESS);

	IPLEmbreeDevice pDevice = nullptr;
	IPLSceneSettings sceneSettings{};

	if (!bDontUseEmbree)
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
		IPLStaticMesh pStaticMesh = BuildRandomSceneGeometry(pScene, 50);

		iplSceneSaveOBJ(pScene, "Scene1.obj");

		iplStaticMeshRemove(pStaticMesh, pScene);
		iplSceneCommit(pScene);
		iplStaticMeshRelease(&pStaticMesh);
	}

	{
		IPLStaticMesh pStaticMesh = BuildRandomSceneGeometry(pScene, 50000);

		iplSceneSaveOBJ(pScene, "Scene2.obj");

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
