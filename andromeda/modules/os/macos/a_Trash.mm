#include "a_Trash.hpp"

#include "a_logger.hpp"

#import <Foundation/Foundation.h>

namespace Andromeda::OS {

    bool moveToTrash(const std::filesystem::path& path) {
        @autoreleasepool {
            // On macOS std::filesystem::path is UTF-8 already. stringWithUTF8String returns
            // nil for invalid UTF-8, and fileURLWithPath:nil throws - so check first.
            NSString* nativePath = [NSString stringWithUTF8String:path.c_str()];
            if (nativePath == nil) {
                A_ERROR("moveToTrash: '{}' is not valid UTF-8", path.string());
                return false;
            }

            NSURL* url = [NSURL fileURLWithPath:nativePath];
            NSError* error = nil;
            // Same as "Move to Trash" in Finder, so "Put Back" works.
            if (![[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&error]) {
                A_ERROR("moveToTrash: cannot move '{}' to the Trash: {}", path.string(),
                        error.localizedDescription.UTF8String);
                return false;
            }
            return true;
        }
    }

}
