// Test-owned credentials only. Production VOID never provisions or deletes keys.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<BYTE>;
void status(const char* operation, ULONG code) {
    std::printf("{\"operation\":\"%s\",\"status\":\"0x%08lX\"}\n", operation, code);
    std::fflush(stdout);
}
void require(const char* operation, SECURITY_STATUS code) {
    status(operation, code);
    if (code) throw std::runtime_error(operation);
}
void check(const char* operation, BOOL ok) { require(operation, ok ? 0 : GetLastError()); }
std::string hex(const Bytes& data) {
    std::string result; const char* digits = "0123456789ABCDEF";
    for (BYTE value : data) { result += digits[value >> 4]; result += digits[value & 15]; }
    return result;
}
std::string hash(const BYTE* data, std::size_t size) {
    BYTE digest[32]; DWORD count = sizeof digest;
    if (!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM, 0, nullptr, data,
                              static_cast<DWORD>(size), digest, &count)) throw std::runtime_error("public hash");
    return hex(Bytes(digest, digest + count));
}
std::string hash(const Bytes& data) { return hash(data.data(), data.size()); }
std::string hash(const std::string& data) { return hash(reinterpret_cast<const BYTE*>(data.data()), data.size()); }
Bytes load(const char* path) {
    if (std::strcmp(path, "-") == 0) return {};
    std::ifstream input(path, std::ios::binary | std::ios::ate); auto size = input.tellg();
    if (size <= 0 || size > 65536) throw std::runtime_error("public input bound");
    Bytes result(static_cast<std::size_t>(size)); input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(result.data()), size)) throw std::runtime_error("public input read");
    return result;
}
void write(const char* path, const Bytes& data) {
    std::ofstream output(path, std::ios::binary);
    if (!output.write(reinterpret_cast<const char*>(data.data()), data.size())) throw std::runtime_error("public output write");
}
Bytes property(NCRYPT_KEY_HANDLE key, const wchar_t* name, DWORD flags = 0) {
    DWORD count = 0; require("property_size", NCryptGetProperty(key, name, nullptr, 0, &count, flags));
    if (count > 65536) throw std::runtime_error("property bound");
    Bytes result(count); require("property_read", NCryptGetProperty(key, name, result.data(), count, &count, flags));
    result.resize(count); return result;
}
Bytes public_key(NCRYPT_KEY_HANDLE key) {
    DWORD count = 0; require("public_size", NCryptExportKey(key, 0, BCRYPT_PUBLIC_KEY_BLOB, nullptr, nullptr, 0, &count, 0));
    if (count > 65536) throw std::runtime_error("public key bound");
    Bytes result(count); require("public_key", NCryptExportKey(key, 0, BCRYPT_PUBLIC_KEY_BLOB, nullptr, result.data(), count, &count, 0));
    result.resize(count); return result;
}
Bytes spki(NCRYPT_KEY_HANDLE key) {
    DWORD count = 0; check("spki_size", CryptExportPublicKeyInfo(key, CERT_NCRYPT_KEY_SPEC, X509_ASN_ENCODING, nullptr, &count));
    if (count > 65536) throw std::runtime_error("SPKI bound"); Bytes result(count);
    check("spki", CryptExportPublicKeyInfo(key, CERT_NCRYPT_KEY_SPEC, X509_ASN_ENCODING,
                                         reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(result.data()), &count));
    return result;
}
Bytes acl(bool registry = false) {
    HANDLE token = nullptr; check("token", OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));
    DWORD count = 0; GetTokenInformation(token, TokenUser, nullptr, 0, &count); Bytes user(count);
    BOOL ok = GetTokenInformation(token, TokenUser, user.data(), count, &count); CloseHandle(token); check("user", ok);
    LPWSTR sid = nullptr; check("sid", ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid));
    const auto ace = registry ? L";CI;GA" : L";;GA";
    std::wstring sddl = L"D:P(A" + std::wstring(ace) + L";;;SY)(A" + ace + L";;;" + sid + L")"; LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    check("acl", ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, &count));
    Bytes result(reinterpret_cast<BYTE*>(descriptor), reinterpret_cast<BYTE*>(descriptor) + count); LocalFree(descriptor); return result;
}
struct Store { HCERTSTORE value = nullptr; ~Store() { if (value) CertCloseStore(value, 0); } };
struct Certificate { PCCERT_CONTEXT value = nullptr; ~Certificate() { if (value) CertFreeCertificateContext(value); } };
struct Owner {
    NCRYPT_PROV_HANDLE provider = 0; NCRYPT_KEY_HANDLE key = 0;
    std::wstring name, store; Bytes marker; bool finalized = false, store_created = false;
    explicit Owner(const std::string& nonce) {
        if (nonce.size() != 64 || nonce.find_first_not_of("0123456789ABCDEF") != std::string::npos)
            throw std::runtime_error("nonce syntax");
        for (std::size_t i = 0; i < nonce.size(); i += 2) marker.push_back(static_cast<BYTE>(std::stoul(nonce.substr(i, 2), nullptr, 16)));
        std::wstring wide(nonce.begin(), nonce.end()); name = L"VOID-HTTP-TEST-" + wide; store = L"VOID-HTTP-TEST-STORE-" + wide;
        require("provider", NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0));
    }
    ~Owner() { if (key) NCryptFreeObject(key); if (provider) NCryptFreeObject(provider); }
    void existing(const char* path) {
        require("owned_key_open", NCryptOpenKey(provider, &key, name.c_str(), 0, NCRYPT_SILENT_FLAG));
        if (public_key(key) != load(path) || property(key, L"VOID-HTTP-TEST-OWNER", NCRYPT_PERSIST_ONLY_FLAG) != marker)
            throw std::runtime_error("credential ownership mismatch");
    }
};
BOOL WINAPI enumerate_store(const void* name, DWORD, PCERT_SYSTEM_STORE_INFO, void*, void* opaque) {
    try {
        auto& rows = *static_cast<std::vector<std::string>*>(opaque);
        auto text = static_cast<const wchar_t*>(name); std::wstring wide(text); std::string encoded;
        for (wchar_t c : wide) { encoded += static_cast<char>(c & 255); encoded += static_cast<char>(c >> 8); }
        Store store; store.value = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG, text);
        if (!store.value) return FALSE;
        std::vector<std::string> certificates; PCCERT_CONTEXT certificate = nullptr;
        while ((certificate = CertEnumCertificatesInStore(store.value, certificate)))
            certificates.push_back(hash(certificate->pbCertEncoded, certificate->cbCertEncoded));
        std::sort(certificates.begin(), certificates.end()); std::string row = hash(encoded);
        for (const auto& item : certificates) row += ":" + item;
        rows.push_back(row); return TRUE;
    } catch (...) { return FALSE; }
}
void inventory() {
    NCRYPT_PROV_HANDLE provider = 0; require("inventory_provider", NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0));
    void* state = nullptr; NCryptKeyName* item = nullptr; std::vector<std::string> keys;
    SECURITY_STATUS code;
    while ((code = NCryptEnumKeys(provider, nullptr, &item, &state, 0)) == ERROR_SUCCESS) {
        std::wstring name(item->pszName), algorithm(item->pszAlgid); std::string row;
        for (wchar_t c : name + L":" + algorithm) { row += static_cast<char>(c & 255); row += static_cast<char>(c >> 8); }
        row += std::to_string(item->dwLegacyKeySpec) + ":" + std::to_string(item->dwFlags);
        keys.push_back(hash(row)); NCryptFreeBuffer(item);
    }
    if (state) NCryptFreeBuffer(state); NCryptFreeObject(provider);
    if (code != NTE_NO_MORE_ITEMS) throw std::runtime_error("key inventory failed");
    std::sort(keys.begin(), keys.end()); std::vector<std::string> stores;
    check("inventory_stores", CertEnumSystemStore(CERT_SYSTEM_STORE_CURRENT_USER, nullptr, &stores, enumerate_store));
    std::sort(stores.begin(), stores.end()); std::string all_keys, all_stores;
    for (const auto& row : keys) all_keys += row + "\n";
    for (const auto& row : stores) all_stores += row + "\n";
    std::printf("{\"key_count\":%zu,\"store_count\":%zu,\"keys_sha256\":\"%s\",\"stores_sha256\":\"%s\"}\n",
                keys.size(), stores.size(), hash(all_keys).c_str(), hash(all_stores).c_str());
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "inventory") == 0) { inventory(); return 0; }
        if (argc < 5) throw std::runtime_error("fixture arguments");
        const std::string action(argv[1]); Owner owner(argv[2]);
        if (action == "create") {
            if (argc != 6 || (std::strcmp(argv[3], "RSA") && std::strcmp(argv[3], "ECDSA"))) throw std::runtime_error("algorithm");
            bool rsa = std::strcmp(argv[3], "RSA") == 0;
            require("create_new_key", NCryptCreatePersistedKey(owner.provider, &owner.key,
                rsa ? NCRYPT_RSA_ALGORITHM : NCRYPT_ECDSA_P256_ALGORITHM, owner.name.c_str(), 0, 0));
            DWORD policy = 0, usage = NCRYPT_ALLOW_SIGNING_FLAG, bits = 2048;
            try {
                require("nonexportable", NCryptSetProperty(owner.key, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<BYTE*>(&policy), sizeof policy, 0));
                require("sign_only", NCryptSetProperty(owner.key, NCRYPT_KEY_USAGE_PROPERTY, reinterpret_cast<BYTE*>(&usage), sizeof usage, 0));
                if (rsa) require("rsa_bits", NCryptSetProperty(owner.key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<BYTE*>(&bits), sizeof bits, 0));
                auto descriptor = acl(); require("private_acl", NCryptSetProperty(owner.key, NCRYPT_SECURITY_DESCR_PROPERTY,
                    descriptor.data(), static_cast<DWORD>(descriptor.size()), DACL_SECURITY_INFORMATION));
                require("owner_marker", NCryptSetProperty(owner.key, L"VOID-HTTP-TEST-OWNER", owner.marker.data(), 32, NCRYPT_PERSIST_FLAG | NCRYPT_PERSIST_ONLY_FLAG));
                require("finalize", NCryptFinalizeKey(owner.key, 0)); owner.finalized = true;
                write(argv[4], public_key(owner.key)); auto info = spki(owner.key); DWORD count = 0;
                check("subject_size", CertStrToNameW(X509_ASN_ENCODING, L"CN=VOID disposable HTTP client", CERT_X500_NAME_STR, nullptr, nullptr, &count, nullptr));
                Bytes subject(count); check("subject", CertStrToNameW(X509_ASN_ENCODING, L"CN=VOID disposable HTTP client", CERT_X500_NAME_STR, nullptr, subject.data(), &count, nullptr));
                CERT_REQUEST_INFO request{}; request.Subject = {count, subject.data()};
                request.SubjectPublicKeyInfo = *reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(info.data());
                CRYPT_ALGORITHM_IDENTIFIER algorithm{}; algorithm.pszObjId = const_cast<LPSTR>(rsa ? szOID_RSA_SHA256RSA : szOID_ECDSA_SHA256);
                check("csr_size", CryptSignAndEncodeCertificate(owner.key, CERT_NCRYPT_KEY_SPEC, X509_ASN_ENCODING,
                    X509_CERT_REQUEST_TO_BE_SIGNED, &request, &algorithm, nullptr, nullptr, &count));
                if (count > 65536) throw std::runtime_error("CSR bound"); Bytes csr(count);
                check("csr_sign", CryptSignAndEncodeCertificate(owner.key, CERT_NCRYPT_KEY_SPEC, X509_ASN_ENCODING,
                    X509_CERT_REQUEST_TO_BE_SIGNED, &request, &algorithm, nullptr, csr.data(), &count)); csr.resize(count); write(argv[5], csr);
            } catch (...) {
                if (owner.finalized) { auto code = NCryptDeleteKey(owner.key, 0); status("failed_create_rollback", code); if (!code) owner.key = 0; }
                throw;
            }
            return 0;
        }
        owner.existing(argv[3]); Bytes encoded = load(argv[4]); Certificate certificate;
        if (!encoded.empty()) {
            certificate.value = CertCreateCertificateContext(X509_ASN_ENCODING, encoded.data(), static_cast<DWORD>(encoded.size()));
            check("certificate", certificate.value != nullptr);
        }
        if (action == "install" || action == "install-public-only") {
            if (!certificate.value) throw std::runtime_error("missing certificate"); auto info = spki(owner.key);
            if (!CertComparePublicKeyInfo(X509_ASN_ENCODING, reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(info.data()),
                                          &certificate.value->pCertInfo->SubjectPublicKeyInfo))
                throw std::runtime_error("certificate/key mismatch");
            status("certificate_key_match", 0);
            check("create_new_store", CertRegisterSystemStore(owner.store.c_str(), CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_CREATE_NEW_FLAG, nullptr, nullptr));
            owner.store_created = true;
            try {
                auto path = L"Software\\Microsoft\\SystemCertificates\\" + owner.store; HKEY registry = nullptr;
                require("store_registry", RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_ALL_ACCESS, &registry)); auto descriptor = acl(true);
                auto code = RegSetKeySecurity(registry, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor.data()); RegCloseKey(registry); require("store_acl", code);
                Store store; store.value = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG, owner.store.c_str()); check("store", store.value != nullptr);
                CRYPT_KEY_PROV_INFO binding{}; binding.pwszContainerName = owner.name.data(); binding.pwszProvName = const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);
                if (action == "install") check("binding", CertSetCertificateContextProperty(certificate.value, CERT_KEY_PROV_INFO_PROP_ID, 0, &binding));
                check("add_certificate", CertAddCertificateContextToStore(store.value, certificate.value, CERT_STORE_ADD_NEW, nullptr));
            } catch (...) {
                status("failed_install_store_rollback", CertUnregisterSystemStore(owner.store.c_str(), CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG) ? 0 : GetLastError()); throw;
            }
            return 0;
        }
        bool public_only = argc == 6 && std::strcmp(argv[5], "public-only") == 0;
        Store store; store.value = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG, owner.store.c_str());
        if (store.value) {
            PCCERT_CONTEXT found = nullptr; unsigned count = 0; bool matching = true;
            while ((found = CertEnumCertificatesInStore(store.value, found))) {
                ++count; matching &= certificate.value && found->cbCertEncoded == encoded.size() && !std::memcmp(found->pbCertEncoded, encoded.data(), encoded.size());
                DWORD size = 0;
                if (!CertGetCertificateContextProperty(found, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &size))
                    matching &= public_only && GetLastError() == CRYPT_E_NOT_FOUND;
                else if (public_only || size > 65536) matching = false;
                else {
                    Bytes bytes(size);
                    if (!CertGetCertificateContextProperty(found, CERT_KEY_PROV_INFO_PROP_ID, bytes.data(), &size)) matching = false;
                    else { auto binding = reinterpret_cast<CRYPT_KEY_PROV_INFO*>(bytes.data());
                        matching &= binding->pwszContainerName && binding->pwszProvName && binding->dwFlags == 0 && binding->dwKeySpec == 0 &&
                            owner.name == binding->pwszContainerName && std::wstring(MS_KEY_STORAGE_PROVIDER) == binding->pwszProvName; }
                }
            }
            if (count != 1 || !matching) throw std::runtime_error("owned store content/binding mismatch");
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND) throw std::runtime_error("owned store inaccessible");
        if (action == "inspect") {
            if (!store.value) throw std::runtime_error("owned store absent");
            auto policy = property(owner.key, NCRYPT_EXPORT_POLICY_PROPERTY), usage = property(owner.key, NCRYPT_KEY_USAGE_PROPERTY);
            if (policy.size() != sizeof(DWORD) || *reinterpret_cast<DWORD*>(policy.data()) != 0) throw std::runtime_error("private export policy changed");
            auto group = property(owner.key, NCRYPT_ALGORITHM_GROUP_PROPERTY); bool rsa = !std::wcscmp(reinterpret_cast<wchar_t*>(group.data()), NCRYPT_RSA_ALGORITHM_GROUP);
            DWORD count = 0; auto raw = NCryptExportKey(owner.key, 0, rsa ? BCRYPT_RSAPRIVATE_BLOB : BCRYPT_ECCPRIVATE_BLOB, nullptr, nullptr, 0, &count, 0);
            auto pkcs8 = NCryptExportKey(owner.key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, nullptr, nullptr, 0, &count, 0);
            status("private_raw_export_query", raw); status("private_pkcs8_export_query", pkcs8);
            if (!raw || !pkcs8) throw std::runtime_error("private export unexpectedly allowed");
            std::printf("{\"public_sha256\":\"%s\",\"certificate_sha256\":\"%s\",\"policy\":\"%s\",\"usage\":\"%s\",\"dacl_sha256\":\"%s\"}\n",
                hash(public_key(owner.key)).c_str(), hash(encoded).c_str(), hex(policy).c_str(), hex(usage).c_str(), hash(property(owner.key, NCRYPT_SECURITY_DESCR_PROPERTY, DACL_SECURITY_INFORMATION)).c_str());
            return 0;
        }
        if (action != "cleanup") throw std::runtime_error("fixture action");
        if (store.value) { check("close_owned_store", CertCloseStore(store.value, CERT_CLOSE_STORE_CHECK_FLAG)); store.value = nullptr;
            check("delete_owned_store", CertUnregisterSystemStore(owner.store.c_str(), CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_DELETE_FLAG)); }
        require("delete_owned_key", NCryptDeleteKey(owner.key, 0)); owner.key = 0;
        NCRYPT_KEY_HANDLE absent = 0; auto code = NCryptOpenKey(owner.provider, &absent, owner.name.c_str(), 0, 0);
        status("owned_key_absent", code); if (absent) NCryptFreeObject(absent); if (code != NTE_BAD_KEYSET) throw std::runtime_error("key deletion not confirmed");
        Store missing; missing.value = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG, owner.store.c_str());
        DWORD error = missing.value ? 0 : GetLastError(); status("owned_store_absent", error);
        if (error != ERROR_FILE_NOT_FOUND) throw std::runtime_error("store deletion not confirmed"); return 0;
    } catch (const std::exception& error) { std::printf("{\"failure\":\"%s\"}\n", error.what()); return 1; }
}
