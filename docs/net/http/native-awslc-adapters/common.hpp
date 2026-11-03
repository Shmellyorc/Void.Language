// Isolated research helpers. No production or automatic credential provisioning.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/ecdsa.h>
#include <openssl/pool.h>
#include <openssl/err.h>
#define CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <memory>
#include <algorithm>

static void status(const char* operation,ULONG value){printf("{\"operation\":\"%s\",\"status\":\"0x%08lX\"}\n",operation,value);fflush(stdout);}
static void require(const char* operation,SECURITY_STATUS value){status(operation,value);if(value)throw std::runtime_error(operation);}
static void check(const char* operation,BOOL value){require(operation,value?0:GetLastError());}
static std::wstring wide(const std::string& text){return std::wstring(text.begin(),text.end());}
static std::string hex(const BYTE* data,size_t size){const char* digits="0123456789ABCDEF";std::string value;for(size_t i=0;i<size;i++){value+=digits[data[i]>>4];value+=digits[data[i]&15];}return value;}
static std::vector<BYTE> unhex(const std::string& text){
  if(text.empty()||text.size()%2||text.size()>4096)throw std::runtime_error("hex bound");std::vector<BYTE> bytes;
  auto digit=[](char c)->BYTE{if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;throw std::runtime_error("hex syntax");};
  for(size_t i=0;i<text.size();i+=2)bytes.push_back((digit(text[i])<<4)|digit(text[i+1]));return bytes;
}
struct Bytes{
  std::vector<BYTE> value;
  ~Bytes(){if(!value.empty())SecureZeroMemory(value.data(),value.size());}
  void load(const char* path){if(std::string(path)=="-")return;std::ifstream in(path,std::ios::binary|std::ios::ate);auto n=in.tellg();
    if(n<=0||n>1024*1024)throw std::runtime_error("input bound");value.resize(static_cast<size_t>(n));in.seekg(0);if(!in.read(reinterpret_cast<char*>(value.data()),n))throw std::runtime_error("input read");}
};
static std::vector<BYTE> property(NCRYPT_KEY_HANDLE key,const wchar_t* name,DWORD flags=0){
  DWORD size=0;require("key_property_size",NCryptGetProperty(key,name,nullptr,0,&size,flags));if(size>65536)throw std::runtime_error("property bound");
  std::vector<BYTE> value(size);require("key_property_read",NCryptGetProperty(key,name,value.data(),size,&size,flags));value.resize(size);return value;
}
static std::vector<BYTE> public_blob(NCRYPT_KEY_HANDLE key){DWORD size=0;require("public_size",NCryptExportKey(key,0,BCRYPT_PUBLIC_KEY_BLOB,nullptr,nullptr,0,&size,0));
  if(size>65536)throw std::runtime_error("public bound");std::vector<BYTE> value(size);require("public_export",NCryptExportKey(key,0,BCRYPT_PUBLIC_KEY_BLOB,nullptr,value.data(),size,&size,0));value.resize(size);return value;}
static std::vector<BYTE> spki(NCRYPT_KEY_HANDLE key){DWORD size=0;check("public_info_size",CryptExportPublicKeyInfo(key,CERT_NCRYPT_KEY_SPEC,X509_ASN_ENCODING,nullptr,&size));
  if(size>65536)throw std::runtime_error("SPKI bound");std::vector<BYTE> value(size);check("public_info",CryptExportPublicKeyInfo(key,CERT_NCRYPT_KEY_SPEC,X509_ASN_ENCODING,reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(value.data()),&size));return value;}
static std::string cert_hash(PCCERT_CONTEXT cert){BYTE hash[32];DWORD size=32;check("certificate_sha256",CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM,0,nullptr,cert->pbCertEncoded,cert->cbCertEncoded,hash,&size));return hex(hash,size);}
static std::string fingerprint(const std::vector<BYTE>& data){BYTE hash[32];DWORD size=32;check("public_metadata_sha256",CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM,0,nullptr,data.data(),static_cast<DWORD>(data.size()),hash,&size));return hex(hash,size);}
static std::vector<BYTE> private_acl(bool registry=false){HANDLE token=nullptr;check("token_open",OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token));DWORD n=0;
  GetTokenInformation(token,TokenUser,nullptr,0,&n);std::vector<BYTE> data(n);BOOL ok=GetTokenInformation(token,TokenUser,data.data(),n,&n);CloseHandle(token);check("token_sid",ok);
  LPWSTR sid=nullptr;check("sid_text",ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid));std::wstring ace=registry?L";CI;GA":L";;GA";std::wstring sddl=L"D:P(A"+ace+L";;;SY)(A"+ace+L";;;"+std::wstring(sid)+L")";LocalFree(sid);
  PSECURITY_DESCRIPTOR sd=nullptr;ULONG size=0;check("key_acl",ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,&size));
  std::vector<BYTE> result(reinterpret_cast<BYTE*>(sd),reinterpret_cast<BYTE*>(sd)+size);LocalFree(sd);return result;}
struct Store{HCERTSTORE value=nullptr;~Store(){if(value)CertCloseStore(value,0);}};
struct Certificate{PCCERT_CONTEXT value=nullptr;~Certificate(){if(value)CertFreeCertificateContext(value);}};
static void memory_roots(HCERTSTORE store,const Bytes& bytes){
  BIO* input=BIO_new_mem_buf(bytes.value.data(),static_cast<int>(bytes.value.size()));if(!input)throw std::runtime_error("root BIO");
  unsigned count=0;while(X509* cert=PEM_read_bio_X509(input,nullptr,nullptr,nullptr)){
    int size=i2d_X509(cert,nullptr);std::vector<BYTE> der(size>0?size:0);auto pointer=der.data();if(size<=0||size>65536||++count>128){X509_free(cert);BIO_free(input);throw std::runtime_error("root bound");}
    i2d_X509(cert,&pointer);X509_free(cert);BOOL ok=CertAddEncodedCertificateToStore(store,X509_ASN_ENCODING,der.data(),size,CERT_STORE_ADD_NEW,nullptr);if(!ok){BIO_free(input);throw std::runtime_error("root add");}
  }BIO_free(input);ERR_clear_error();if(!count)throw std::runtime_error("empty roots");
}
