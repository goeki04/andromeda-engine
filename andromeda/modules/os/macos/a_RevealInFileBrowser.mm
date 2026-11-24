#include "a_RevealInFileBrowser.hpp"

#include <system_error>

#import <AppKit/AppKit.h>

namespace Andromeda::OS {

    bool revealInFileBrowser(const std::filesystem::path& path) {
        // activateFileViewerSelectingURLs reports nothing, so check the path ourselves.
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return false;
        }

        @autoreleasepool {
            NSString* nativePath = [NSString stringWithUTF8String:path.c_str()];
            if (nativePath == nil) {
                return false;
            }
            // Opens a Finder window with the item selected.
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ [NSURL fileURLWithPath:nativePath] ]];
            return true;
        }
    }

}
