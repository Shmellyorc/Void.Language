#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vd {
struct ManagedNativeApprovalSubject;

class NativeApprovalRequired : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class NativeApproval {
public:
    enum class Mode { terminal, reuse_only };
private:
    struct Payload;
    const Mode mode_;
    std::unique_ptr<Payload> payload_;
    std::string id_, pending_;
public:
    explicit NativeApproval(Mode mode);
    ~NativeApproval();
    NativeApproval(const NativeApproval&) = delete;
    bool authorized() const { return !id_.empty(); }
    std::filesystem::path authorize(const ManagedNativeApprovalSubject&, bool jit);
    // Publish consent only after the frontend's ordinary recapture has succeeded.
    void remember();
};

std::vector<std::string> native_approvals();
void revoke_native_approval(std::string_view id);
// CLI-owned default output directories follow ordinary project OS access permissions.
std::filesystem::path project_build_directory(const std::filesystem::path& root);
}
