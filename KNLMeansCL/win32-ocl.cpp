#ifdef _MSC_VER
#include <windows.h>
#include <delayimp.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <stdlib.h>

#define DLL_DIR L"intelocl"

#define ENV_VAR "OCL_ICD_FILENAMES"
#define ICD_DLL "intelocl64.dll"


#include <iostream>

namespace {
namespace fs = std::filesystem;
static fs::path dllDir() {
	static const std::wstring res = []() -> std::wstring {
		HMODULE mod = 0;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char *)dllDir, &mod)) {
			std::vector<wchar_t> buf;
			size_t n = 0;
			do {
				buf.resize(buf.size() + MAX_PATH);
				n = GetModuleFileNameW(mod, buf.data(), buf.size());
			} while (n >= buf.size());
			buf.resize(n);
			std::wstring path(buf.begin(), buf.end());
			return path;
		}
		throw std::runtime_error("unable to locate myself");
	}();
	return fs::path(res).parent_path();
}

extern "C" FARPROC WINAPI delayload_hook(unsigned reason, DelayLoadInfo* info) {
	switch (reason) {
	case dliNoteStartProcessing:
	case dliNoteEndProcessing:
		// Nothing to do here.
		break;
	case dliNotePreLoadLibrary: {
		fs::path dir = dllDir() / DLL_DIR;
		fs::path icd_dll = dir / std::string(ICD_DLL);
		std::string env = std::string(ENV_VAR "=") + icd_dll.string();
		std::cerr << "setting " << env << std::endl;
		_putenv(env.c_str());

		//std::cerr << "loading " << info->szDll << std::endl;
		fs::path loader_dll = dir / "OpenCL.dll";
		std::string path = loader_dll.string();
		HMODULE h = LoadLibraryA(path.c_str());
		std::cerr << "loading " << path << ": " << h << std::endl;
		return (FARPROC)h;
	}
	case dliNotePreGetProcAddress:
		// Nothing to do here.
		break;
	case dliFailLoadLib:
	case dliFailGetProc:
		// Returning NULL from error notifications will cause the delay load
		// runtime to raise a VcppException structured exception, that some code
		// might want to handle.
		return NULL;
		break;
	default:
		abort(); // unreachable.
		break;
	}
	// Returning NULL causes the delay load machinery to perform default
	// processing for this notification.
	return NULL;
}
} // namespace

extern "C" {
	const PfnDliHook __pfnDliNotifyHook2 = delayload_hook;
	const PfnDliHook __pfnDliFailureHook2 = delayload_hook;
};
#endif // _MSC_VER
