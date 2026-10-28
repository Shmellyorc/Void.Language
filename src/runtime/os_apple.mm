#include "void/os.hpp"
#import <Foundation/Foundation.h>
#include <stdexcept>
#include <new>
#include <utility>
#include <cerrno>

namespace vd::os {
namespace {
Result unavailable() { Result r; r.kind = ErrorKind::unavailable; return r; }
Result text(NSString* value) {
    if (!value) return unavailable();
    NSData* bytes = [value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if (!bytes) { Result r; r.kind = ErrorKind::invalid_utf8; return r; }
    if (![bytes length]) return unavailable();
    return checked_path(std::string(static_cast<const char*>([bytes bytes]), [bytes length]));
}
Result url(NSURL* value) {
    if (!value || ![value isFileURL]) return unavailable();
    // Validate the decoded path before the native C-string representation can
    // conceal an embedded NUL or an unrepresentable NSString scalar sequence.
    NSString* path = [value path];
    if (!path) return unavailable();
    NSData* encoded = [path dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if (!encoded) { Result r; r.kind = ErrorKind::invalid_utf8; return r; }
    if ([encoded length]) {
        std::string_view decoded(static_cast<const char*>([encoded bytes]), [encoded length]);
        if (decoded.find('\0') != std::string_view::npos) { Result r; r.kind = ErrorKind::invalid_path; return r; }
    }
    const char* bytes = [value fileSystemRepresentation];
    if (!bytes) { Result r; r.kind = ErrorKind::invalid_utf8; return r; }
    return checked_path(bytes);
}
Result error(NSError* reason) {
    if (!reason) return unavailable();
    NSString* domain = [reason domain];
    if ([domain isEqualToString:NSPOSIXErrorDomain] && [reason code] == ENOMEM) throw std::bad_alloc();
    NSData* bytes = [domain dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if (!bytes) throw std::logic_error("Unrepresentable NSError domain");
    std::string name;
    if ([bytes length]) name.assign(static_cast<const char*>([bytes bytes]), [bytes length]);
    auto kind = ErrorKind::system_failure;
    if (([domain isEqualToString:NSPOSIXErrorDomain] && ([reason code] == EACCES || [reason code] == EPERM)) ||
        ([domain isEqualToString:NSCocoaErrorDomain] && [reason code] == NSFileReadNoPermissionError)) kind = ErrorKind::permission_denied;
    return native_error(static_cast<std::int64_t>([reason code]), std::move(name), kind);
}
Result classify(NSBundle*& bundle) {
    bundle = [NSBundle mainBundle];
    if (!bundle) return unavailable();
    NSURL* root = [bundle bundleURL];
    if (!root || ![root isFileURL]) return unavailable();
    auto root_path = url(root);
    if (root_path.kind != ErrorKind::success) return root_path;
    // A non-app main bundle is a confirmed plain host, even without bundle metadata.
    id package = [[bundle infoDictionary] objectForKey:@"CFBundlePackageType"];
    Result result;
    if (![package isKindOfClass:[NSString class]] || ![package isEqualToString:@"APPL"] ||
        ![[[root path] pathExtension] isEqualToString:@"app"]) return result;
    NSURL* executable = [bundle executableURL];
    if (!executable || ![executable isFileURL]) return unavailable();
    auto image = url(executable);
    if (image.kind != ErrorKind::success) return image;
    result.flag = true;
    return result;
}
}
Result apple_query(Operation operation) {
    @autoreleasepool {
        @try {
            switch (operation) {
            case Operation::home: return text(NSHomeDirectory());
            case Operation::temp: return text(NSTemporaryDirectory());
            case Operation::data_root: case Operation::config_root: case Operation::state_root:
            case Operation::cache_root: {
                NSError* reason = nil;
                auto directory = operation == Operation::cache_root ? NSCachesDirectory : NSApplicationSupportDirectory;
                NSURL* location = [[NSFileManager defaultManager] URLForDirectory:directory
                    inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:&reason];
                if (!location) return error(reason);
                return url(location);
            }
            case Operation::bundle: case Operation::resources: {
                NSBundle* bundle = nil;
                auto result = classify(bundle);
                if (result.kind != ErrorKind::success || !result.flag || operation == Operation::bundle) return result;
                result = url([bundle resourceURL]);
                result.flag = true;
                return result;
            }
            default: throw std::logic_error("Invalid Apple OS operation");
            }
        } @catch (NSException* exception) {
            if ([[exception name] isEqualToString:NSCharacterConversionException]) {
                Result result; result.kind = ErrorKind::invalid_utf8; return result;
            }
            // Allocation/invariant exceptions use the checked Runtime channel.
            throw std::runtime_error("Foundation OS query exception");
        } @catch (id exception) {
            (void)exception;
            throw std::runtime_error("Foundation OS query exception");
        }
    }
}
}
