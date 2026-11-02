#include "a_RevealInFileBrowser.hpp"
#include <Windows.h>
#include <ShlObj.h>
namespace Andromeda::OS {

    bool revealInFileBrowser(const std::filesystem::path& path) {
        const std::filesystem::path native = std::filesystem::path(path).make_preferred();

        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        PIDLIST_ABSOLUTE item = ILCreateFromPathW(native.c_str());
        const bool shown = item != nullptr && SUCCEEDED(SHOpenFolderAndSelectItems(item, 0, nullptr, 0));
        if (item != nullptr) {
            ILFree(item);
        }

        if (SUCCEEDED(init)) {
            CoUninitialize();
        }
        return shown;
    }

}