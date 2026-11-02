#include "a_Trash.hpp"
#include <Windows.h>
#include <shellapi.h>
#include <ShObjIdl.h>
#include <wrl/client.h>
namespace Andromeda::OS {
    bool moveToTrash(const std::filesystem::path& path) {
        using Microsoft::WRL::ComPtr;

        const std::filesystem::path native = std::filesystem::path(path).make_preferred();

        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        bool moved = false;
        {
            ComPtr<IFileOperation> operation;
            ComPtr<IShellItem> item;
            const DWORD flags = FOFX_RECYCLEONDELETE | FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&operation))) &&
                SUCCEEDED(operation->SetOperationFlags(flags)) &&
                SUCCEEDED(SHCreateItemFromParsingName(native.c_str(), nullptr, IID_PPV_ARGS(&item))) &&
                SUCCEEDED(operation->DeleteItem(item.Get(), nullptr)) && SUCCEEDED(operation->PerformOperations())) {
                BOOL aborted = FALSE;
                moved = SUCCEEDED(operation->GetAnyOperationsAborted(&aborted)) && !aborted;
            }
        }
        if (SUCCEEDED(init)) {
            CoUninitialize();
        }
        return moved;
    }
}