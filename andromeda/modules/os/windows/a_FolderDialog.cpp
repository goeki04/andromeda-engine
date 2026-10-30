#include "a_FolderDialog.hpp"

#include <algorithm>

#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog
#include <wrl/client.h> // Microsoft::WRL::ComPtr

namespace Andromeda::OS {

    namespace {
        // Windows hands out paths as UTF-16. Converting through std::filesystem::path::string()
        // would use the ANSI code page and break folder names with umlauts, so convert explicitly.
        std::string toUtf8(const wchar_t* text) {
            const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (size <= 1) {
                return {};
            }
            std::string out(static_cast<size_t>(size - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
            return out;
        }

        std::wstring toWide(std::string_view text) {
            if (text.empty()) {
                return {};
            }
            const int length = static_cast<int>(text.size());
            const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
            std::wstring out(static_cast<size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), length, out.data(), size);
            return out;
        }
    }

    std::vector<std::string> pickFolders(void* ownerWindow, std::string_view startFolder) {
        using Microsoft::WRL::ComPtr;
        std::vector<std::string> result;

        // COM has to be initialised on the calling thread. S_OK and S_FALSE ("already was") both
        // need a matching CoUninitialize; RPC_E_CHANGED_MODE means another part of the program set
        // it up differently - the dialog still works then, but the uninit is not ours to do.
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        {
            ComPtr<IFileOpenDialog> dialog;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
                DWORD options = 0;
                dialog->GetOptions(&options);
                dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM);

                // The shell only parses backslashes, while engine paths use '/'. A start folder
                // that fails to parse is skipped and the dialog opens at its default location.
                std::wstring start = toWide(startFolder);
                std::replace(start.begin(), start.end(), L'/', L'\\');
                ComPtr<IShellItem> startItem;
                if (!start.empty() &&
                    SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&startItem)))) {
                    dialog->SetFolder(startItem.Get());
                }

                // Show blocks until the dialog is closed; on cancel it fails with ERROR_CANCELLED.
                ComPtr<IShellItemArray> items;
                if (SUCCEEDED(dialog->Show(static_cast<HWND>(ownerWindow))) && SUCCEEDED(dialog->GetResults(&items))) {
                    DWORD count = 0;
                    items->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i) {
                        ComPtr<IShellItem> item;
                        PWSTR path = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) &&
                            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                            result.push_back(toUtf8(path));
                            CoTaskMemFree(path);
                        }
                    }
                }
            }
        } // The ComPtrs release their objects here - that has to happen before COM shuts down.
        if (SUCCEEDED(init)) {
            CoUninitialize();
        }
        return result;
    }
}
