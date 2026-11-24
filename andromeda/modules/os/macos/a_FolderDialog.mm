#include "a_FolderDialog.hpp"

#import <AppKit/AppKit.h>

namespace Andromeda::OS {

    std::vector<std::string> pickFolders(void* ownerWindow, std::string_view startFolder) {
        std::vector<std::string> folders;

        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.canChooseDirectories = YES;
            panel.canChooseFiles = NO;
            panel.allowsMultipleSelection = YES;
            panel.canCreateDirectories = YES;

            if (!startFolder.empty()) {
                // string_view is not null-terminated, so pass the length explicitly.
                NSString* start = [[NSString alloc] initWithBytes:startFolder.data()
                                                           length:startFolder.size()
                                                         encoding:NSUTF8StringEncoding];
                if (start != nil) {
                    panel.directoryURL = [NSURL fileURLWithPath:start isDirectory:YES];
                }
            }

            // runModal blocks until the panel is closed - the same contract as the Windows
            // dialog. (beginSheetModalForWindow would attach it to the window, but returns
            // immediately and reports the result later in a block.)
            if ([panel runModal] == NSModalResponseOK) {
                for (NSURL* url in panel.URLs) {
                    if (const char* path = url.path.UTF8String) {
                        folders.emplace_back(path);
                    }
                }
            }

            // Hand keyboard focus back to the SDL window, otherwise the editor needs an
            // extra click before it reacts to keys again.
            if (ownerWindow != nullptr) {
                [(__bridge NSWindow*)ownerWindow makeKeyAndOrderFront:nil];
            }
        }

        return folders;
    }

}
