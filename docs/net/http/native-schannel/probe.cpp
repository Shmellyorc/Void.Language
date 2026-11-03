// Standalone feasibility experiment. Never linked into VOID.
#define SECURITY_WIN32
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>
#include <sddl.h>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <thread>

static void result(const char* op, ULONG status) {
  printf("{\"operation\":\"%s\",\"status\":\"0x%08lX\"}\n", op, status); fflush(stdout);
}
static void require(const char* op, SECURITY_STATUS s) {
  result(op, s); if(s) throw std::runtime_error(op);
}
static void wincheck(const char* op, BOOL ok) { require(op, ok ? 0 : GetLastError()); }
static std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::string hex(const BYTE* p, size_t n) {
  static const char h[]="0123456789ABCDEF"; std::string s;
  for(size_t i=0;i<n;i++){s+=h[p[i]>>4];s+=h[p[i]&15];} return s;
}
static std::vector<BYTE> unhex(const std::string& s){
  if(s.empty()||s.size()%2||s.size()>4096)throw std::runtime_error("hex bound");std::vector<BYTE> v;
  auto digit=[](char c)->BYTE{if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;throw std::runtime_error("hex input");};
  for(size_t i=0;i<s.size();i+=2)v.push_back((digit(s[i])<<4)|digit(s[i+1]));return v;
}
static std::string digest(const BYTE* bytes,DWORD n){
  BYTE hash[32];DWORD size=sizeof hash;wincheck("sha256",CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM,0,nullptr,bytes,n,hash,&size));return hex(hash,size);
}
static std::wstring mode;
static void checkpoint(const wchar_t* stage){
  if(mode!=std::wstring(L"stop-")+stage)return;
  printf("{\"checkpoint\":true}\n");fflush(stdout);std::string wait;std::getline(std::cin,wait);
  throw std::runtime_error("checkpoint released without termination");
}
static std::vector<BYTE> property(NCRYPT_HANDLE h, LPCWSTR name, DWORD flags=0) {
  DWORD n=0; require("property_size", NCryptGetProperty(h,name,nullptr,0,&n,flags));
  if(n>1024*1024) throw std::runtime_error("property bound");
  std::vector<BYTE> v(n); require("property_read",NCryptGetProperty(h,name,v.data(),n,&n,flags)); return v;
}
static std::vector<BYTE> public_blob(NCRYPT_KEY_HANDLE key) {
  DWORD n=0; require("public_size",NCryptExportKey(key,0,BCRYPT_PUBLIC_KEY_BLOB,nullptr,nullptr,0,&n,0));
  if(n>65536) throw std::runtime_error("public bound"); std::vector<BYTE> v(n);
  require("public_export",NCryptExportKey(key,0,BCRYPT_PUBLIC_KEY_BLOB,nullptr,v.data(),n,&n,0));v.resize(n);return v;
}
static std::wstring user_sid(){
  HANDLE token=nullptr;wincheck("token_open",OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token));
  DWORD n=0;GetTokenInformation(token,TokenUser,nullptr,0,&n);std::vector<BYTE> bytes(n);
  BOOL ok=GetTokenInformation(token,TokenUser,bytes.data(),n,&n);CloseHandle(token);wincheck("token_user",ok);
  LPWSTR sid=nullptr;wincheck("sid_convert",ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid));
  std::wstring value(sid);LocalFree(sid);return value;
}
static std::vector<BYTE> private_dacl(const std::wstring& sid,bool deny_delete=false,bool registry_tree=false){
  auto sddl=registry_tree?L"D:P(A;CI;KA;;;SY)(A;CI;KA;;;"+sid+L")":L"D:P"+std::wstring(deny_delete?L"(D;;SD;;;"+sid+L")":L"")+L"(A;;GA;;;SY)(A;;GA;;;"+sid+L")";
  PSECURITY_DESCRIPTOR sd=nullptr;ULONG n=0;
  wincheck("dacl_construct",ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,&n));
  std::vector<BYTE> v(reinterpret_cast<BYTE*>(sd),reinterpret_cast<BYTE*>(sd)+n);LocalFree(sd);return v;
}
static void verify_dacl(PSECURITY_DESCRIPTOR sd,bool protected_required=true){
  SECURITY_DESCRIPTOR_CONTROL control=0;DWORD revision=0;BOOL present=FALSE,def=FALSE;PACL acl=nullptr;
  wincheck("dacl_read",GetSecurityDescriptorDacl(sd,&present,&acl,&def));
  wincheck("dacl_control",GetSecurityDescriptorControl(sd,&control,&revision));
  if(!present||!acl||acl->AceCount!=2||(protected_required&&!(control&SE_DACL_PROTECTED)))throw std::runtime_error("DACL shape");
  const auto sid=user_sid();PSID current=nullptr,system=nullptr;ConvertStringSidToSidW(sid.c_str(),&current);ConvertStringSidToSidW(L"S-1-5-18",&system);
  bool valid=true;
  for(DWORD i=0;i<acl->AceCount;i++){void* p=nullptr;if(!GetAce(acl,i,&p)){valid=false;break;}
    auto ace=static_cast<ACCESS_ALLOWED_ACE*>(p);
    if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE||
       (!EqualSid(&ace->SidStart,current)&&!EqualSid(&ace->SidStart,system)))valid=false;
  }
  LocalFree(current);LocalFree(system);if(!valid)throw std::runtime_error("DACL principal");result("dacl_isolated",0);
}
static void registry_dacl(HKEY key,bool protected_required){
  DWORD n=0;RegGetKeySecurity(key,DACL_SECURITY_INFORMATION,nullptr,&n);if(n>65536)throw std::runtime_error("registry ACL bound");
  std::vector<BYTE> v(n);require("store_dacl_readback",RegGetKeySecurity(key,DACL_SECURITY_INFORMATION,v.data(),&n));verify_dacl(v.data(),protected_required);
}
static void sign_control(NCRYPT_KEY_HANDLE key){
  auto group=property(key,NCRYPT_ALGORITHM_GROUP_PROPERTY);bool rsa=wcscmp(reinterpret_cast<wchar_t*>(group.data()),NCRYPT_RSA_ALGORITHM_GROUP)==0;
  BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};DWORD flags=rsa?NCRYPT_PAD_PKCS1_FLAG:0;
  BYTE challenge[32]{};DWORD n=0;require("sign_size",NCryptSignHash(key,rsa?&padding:nullptr,challenge,sizeof challenge,nullptr,0,&n,flags));
  if(n>65536)throw std::runtime_error("signature bound");std::vector<BYTE> signature(n);
  require("sign",NCryptSignHash(key,rsa?&padding:nullptr,challenge,sizeof challenge,signature.data(),n,&n,flags));
  require("verify_signature",NCryptVerifySignature(key,rsa?&padding:nullptr,challenge,sizeof challenge,signature.data(),n,flags));
}
struct Secret { std::vector<BYTE> v; void wipe(){if(!v.empty())SecureZeroMemory(v.data(),v.size());v.clear();} ~Secret(){wipe();} };
struct Owner {
  NCRYPT_PROV_HANDLE provider=0; NCRYPT_KEY_HANDLE key=0; HCERTSTORE store=0, staging=0;
  PCCERT_CONTEXT cert=nullptr; NCRYPT_KEY_HANDLE temporary=0; BOOL free_temp=FALSE;
  std::wstring name, store_name; bool owned_key=false, owned_store=false;
  HANDLE live_lock=INVALID_HANDLE_VALUE;
  ~Owner(){cleanup();}
  void cleanup(){
    if(cert){CertFreeCertificateContext(cert);cert=nullptr;}
    if(temporary && free_temp) NCryptFreeObject(temporary); temporary=0;
    if(staging){result("staging_close",CertCloseStore(staging,CERT_CLOSE_STORE_CHECK_FLAG)?0:GetLastError());staging=0;}
    if(store){result("store_close",CertCloseStore(store,CERT_CLOSE_STORE_CHECK_FLAG)?0:GetLastError());store=0;}
    if(owned_store){BOOL b=CertUnregisterSystemStore(store_name.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_DELETE_FLAG);
      result("store_delete",b?0:GetLastError());if(b)owned_store=false;
      HCERTSTORE check=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,store_name.c_str());
      result("store_open_after_delete",check?0:GetLastError());if(check)CertCloseStore(check,0);}
    if(owned_key){if(!key)NCryptOpenKey(provider,&key,name.c_str(),0,0);
      SECURITY_STATUS s=key?NCryptDeleteKey(key,0):NTE_BAD_KEYSET;result("key_delete",s);if(!s){key=0;owned_key=false;}
      NCRYPT_KEY_HANDLE check=0;SECURITY_STATUS t=NCryptOpenKey(provider,&check,name.c_str(),0,0);
      result("key_open_after_delete",t);if(check)NCryptFreeObject(check);}
    if(key){NCryptFreeObject(key);key=0;} if(provider){NCryptFreeObject(provider);provider=0;}
    if(owned_key||owned_store){printf("{\"residual\":true}\n");fflush(stdout);}
    if(live_lock!=INVALID_HANDLE_VALUE){CloseHandle(live_lock);live_lock=INVALID_HANDLE_VALUE;}
  }
};
#include "reserved-import.hpp"
// This is an experiment-specific recovery probe, not a production journal parser.
// It accepts only a bounded nonce/public-key/certificate receipt generated here.
static int recover(const wchar_t* path,bool operator_cleanup){
  Owner o;
  std::wstring lock_path=std::wstring(path)+L".lock";
  o.live_lock=CreateFileW(lock_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  result("recovery_lock",o.live_lock==INVALID_HANDLE_VALUE?GetLastError():0);
  if(o.live_lock==INVALID_HANDLE_VALUE)return 3;
  std::ifstream f(path,std::ios::binary);f.seekg(0,std::ios::end);auto length=f.tellg();
  if(length<0||length>4096)throw std::runtime_error("journal bound");f.seekg(0);
  std::string nonce,public_hash,cert_hash;std::getline(f,nonce);std::getline(f,public_hash);std::getline(f,cert_hash);
  auto marker=unhex(nonce);if(marker.size()!=32||unhex(public_hash).size()!=32||unhex(cert_hash).size()!=32)throw std::runtime_error("journal invalid");
  o.name=L"VOID-SCHANNEL-TEST-"+widen(nonce);o.store_name=L"VOID-SCHANNEL-TEST-STORE-"+widen(nonce);
  require("recovery_provider",NCryptOpenStorageProvider(&o.provider,MS_KEY_STORAGE_PROVIDER,0));
  SECURITY_STATUS s=NCryptOpenKey(o.provider,&o.key,o.name.c_str(),0,0);result("recovery_key_open",s);
  bool key_present=!s,key_owned=false,store_present=false,store_owned=false;
  if(key_present){
    auto pub=public_blob(o.key);if(digest(pub.data(),static_cast<DWORD>(pub.size()))!=public_hash)throw std::runtime_error("recovery public mismatch");
    DWORD n=32;BYTE actual[32]{};s=NCryptGetProperty(o.key,L"VOID-SCHANNEL-TEST-OWNER",actual,sizeof actual,&n,NCRYPT_PERSIST_ONLY_FLAG);
    result("recovery_key_marker",s);key_owned=!s&&n==32&&!memcmp(actual,marker.data(),32);
    if(operator_cleanup)key_owned=true;
  }else if(s!=NTE_BAD_KEYSET)throw std::runtime_error("recovery key inaccessible");
  o.store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,o.store_name.c_str());
  DWORD store_status=o.store?0:GetLastError();result("recovery_store_open",store_status);store_present=o.store!=nullptr;
  if(!store_present&&store_status!=ERROR_FILE_NOT_FOUND&&store_status!=CRYPT_E_NOT_FOUND)throw std::runtime_error("recovery store inaccessible");
  if(store_present){
    HKEY registry=nullptr;const auto rp=L"Software\\Microsoft\\SystemCertificates\\"+o.store_name;
    require("recovery_store_registry",RegOpenKeyExW(HKEY_CURRENT_USER,rp.c_str(),0,KEY_READ,&registry));
    BYTE actual[32]{};DWORD n=32,type=0;s=RegQueryValueExW(registry,L"VOID-SCHANNEL-TEST-OWNER",nullptr,&type,actual,&n);RegCloseKey(registry);
    result("recovery_store_marker",s);store_owned=!s&&type==REG_BINARY&&n==32&&!memcmp(actual,marker.data(),32);
    unsigned count=0;PCCERT_CONTEXT c=nullptr;bool certificates_match=true;
    while((c=CertEnumCertificatesInStore(o.store,c))){++count;certificates_match&=digest(c->pbCertEncoded,c->cbCertEncoded)==cert_hash;}
    if(count>1||!certificates_match)throw std::runtime_error("recovery store identity");
    if(operator_cleanup)store_owned=true;
  }
  if((key_present&&!key_owned)||(store_present&&!store_owned)){
    printf("{\"quarantined\":true}\n");return 4;
  }
  o.owned_key=key_present;o.owned_store=store_present;o.cleanup();return o.owned_key||o.owned_store?5:0;
}
int wmain(int argc,wchar_t** argv){
  if(argc==2&&wcscmp(argv[1],L"inventory")==0){
    try{
      Owner inventory;require("inventory_provider",NCryptOpenStorageProvider(&inventory.provider,MS_KEY_STORAGE_PROVIDER,0));
      void* state=nullptr;NCryptKeyName* item=nullptr;SECURITY_STATUS s=0;std::vector<std::string> rows;
      while(!(s=NCryptEnumKeys(inventory.provider,nullptr,&item,&state,0))){
        std::wstring value=item->pszName;value+=L"|";value+=item->pszAlgid;value+=L"|"+std::to_wstring(item->dwLegacyKeySpec)+L"|"+std::to_wstring(item->dwFlags);
        rows.push_back(digest(reinterpret_cast<const BYTE*>(value.data()),static_cast<DWORD>(value.size()*sizeof(wchar_t))));NCryptFreeBuffer(item);item=nullptr;
      }
      if(state)NCryptFreeBuffer(state);require("inventory_enum_end",s==NTE_NO_MORE_ITEMS?0:s);std::sort(rows.begin(),rows.end());
      for(auto& row:rows)printf("{\"key_inventory_sha256\":\"%s\"}\n",row.c_str());
      printf("{\"key_count\":%zu}\n",rows.size());return 0;
    }catch(const std::exception& e){printf("{\"failure\":\"%s\"}\n",e.what());return 1;}
  }
  if(argc<3||argc>4)return 2; Owner o;
  if(wcscmp(argv[1],L"recover")==0||wcscmp(argv[1],L"operator-cleanup")==0){
    try{return recover(argv[2],wcscmp(argv[1],L"operator-cleanup")==0);}catch(const std::exception& e){printf("{\"failure\":\"%s\"}\n",e.what());return 1;}}
  mode=argc==4?argv[3]:L"normal";
  bool reserved=mode.rfind(L"reserved:",0)==0;if(reserved)mode.erase(0,9);
  bool deferred=reserved||mode!=L"immediate";
  bool deferred_collision=mode==L"deferred-collision";
  bool test_collision=deferred_collision||mode==L"collision";
  try{
    // Password arrives over a pipe, never argv, environment, receipt or file.
    std::string password; std::getline(std::cin,password);
    if(!password.empty() && password.back()=='\r')password.pop_back();
    std::wstring pw=widen(password);
    SecureZeroMemory(password.data(),password.size()); password.clear();
    Secret input; std::ifstream f(argv[1],std::ios::binary);input.v.assign(std::istreambuf_iterator<char>(f),{});
    if(input.v.empty()||input.v.size()>1024*1024)throw std::runtime_error("input bound");
    CRYPT_DATA_BLOB blob{static_cast<DWORD>(input.v.size()),input.v.data()};
    auto bad=pw+L"wrong";HCERTSTORE wrong=PFXImportCertStore(&blob,bad.c_str(),PKCS12_NO_PERSIST_KEY|PKCS12_ALWAYS_CNG_KSP|CRYPT_USER_KEYSET|CRYPT_EXPORTABLE);
    result("bad_password",wrong?0:GetLastError());SecureZeroMemory(bad.data(),bad.size()*sizeof(wchar_t));
    if(wrong){CertCloseStore(wrong,0);throw std::runtime_error("bad password accepted");}
    o.staging=PFXImportCertStore(&blob,pw.c_str(),PKCS12_NO_PERSIST_KEY|PKCS12_ALWAYS_CNG_KSP|CRYPT_USER_KEYSET|CRYPT_EXPORTABLE);
    SecureZeroMemory(pw.data(),pw.size()*sizeof(wchar_t));pw.clear();wincheck("pfx_decode",o.staging!=nullptr);
    unsigned keys=0,certs=0;PCCERT_CONTEXT c=nullptr;
    while((c=CertEnumCertificatesInStore(o.staging,c))){
      if(++certs>16)throw std::runtime_error("certificate bound");DWORD n=0;
      if(CertGetCertificateContextProperty(c,CERT_KEY_CONTEXT_PROP_ID,nullptr,&n)){
        ++keys;if(!o.cert)o.cert=CertDuplicateCertificateContext(c);
      }
    }
    if(keys!=1)throw std::runtime_error("expected exactly one private key");
    DWORD spec=0;HCRYPTPROV_OR_NCRYPT_KEY_HANDLE acquired=0;
    wincheck("acquire_cng",CryptAcquireCertificatePrivateKey(o.cert,CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG|CRYPT_ACQUIRE_CACHE_FLAG|CRYPT_ACQUIRE_SILENT_FLAG,nullptr,&acquired,&spec,&o.free_temp));
    o.temporary=acquired;if(spec!=CERT_NCRYPT_KEY_SPEC)throw std::runtime_error("not CNG");
    DWORD public_size=0;wincheck("spki_size",CryptExportPublicKeyInfo(acquired,spec,X509_ASN_ENCODING,nullptr,&public_size));
    if(public_size>65536)throw std::runtime_error("SPKI bound");std::vector<BYTE> spki(public_size);
    auto public_info=reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(spki.data());
    wincheck("spki_export",CryptExportPublicKeyInfo(acquired,spec,X509_ASN_ENCODING,public_info,&public_size));
    wincheck("certificate_public_key_match",CertComparePublicKeyInfo(X509_ASN_ENCODING,public_info,&o.cert->pCertInfo->SubjectPublicKeyInfo));
    auto ph=property(o.temporary,NCRYPT_PROVIDER_HANDLE_PROPERTY);NCRYPT_PROV_HANDLE actual=*reinterpret_cast<NCRYPT_PROV_HANDLE*>(ph.data());
    auto pn=property(actual,NCRYPT_NAME_PROPERTY);if(wcscmp(reinterpret_cast<wchar_t*>(pn.data()),MS_KEY_STORAGE_PROVIDER))throw std::runtime_error("wrong provider");
    auto policy=property(o.temporary,NCRYPT_EXPORT_POLICY_PROPERTY);
    printf("{\"temporary_export_policy\":%lu,\"key_spec\":%lu,\"caller_free\":%s,\"certificates\":%u}\n",*reinterpret_cast<DWORD*>(policy.data()),spec,o.free_temp?"true":"false",certs);
    auto original=public_blob(o.temporary);
    DWORD n=0;result("plaintext_pkcs8_control",NCryptExportKey(o.temporary,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,nullptr,0,&n,0));
    // Policy 1 permits encrypted export, not plaintext. Do not elevate policy.
    BYTE bridge_random[32];require("bridge_random",BCryptGenRandom(nullptr,bridge_random,sizeof bridge_random,BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    auto bridge_hex=hex(bridge_random,sizeof bridge_random);Secret bridge;bridge.v.resize((bridge_hex.size()+1)*sizeof(wchar_t));
    auto bridge_text=reinterpret_cast<wchar_t*>(bridge.v.data());
    for(size_t i=0;i<bridge_hex.size();++i)bridge_text[i]=bridge_hex[i];
    SecureZeroMemory(bridge_random,sizeof bridge_random);SecureZeroMemory(bridge_hex.data(),bridge_hex.size());
    NCryptBuffer secret{static_cast<ULONG>(bridge.v.size()),NCRYPTBUFFER_PKCS_SECRET,bridge.v.data()};
    const char oid[]=szOID_PKCS_12_pbeWithSHA1And3KeyTripleDES;
    NCryptBuffer algorithm{sizeof oid,NCRYPTBUFFER_PKCS_ALG_OID,const_cast<char*>(oid)};
    struct { CRYPT_PKCS12_PBE_PARAMS parameters; BYTE salt[8]; } pbe{{10000,8},{}};
    require("pbe_salt",BCryptGenRandom(nullptr,pbe.salt,sizeof pbe.salt,BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    NCryptBuffer alg_params{sizeof pbe,NCRYPTBUFFER_PKCS_ALG_PARAM,&pbe};
    NCryptBuffer export_params[]{secret,algorithm,alg_params};
    NCryptBufferDesc export_desc{NCRYPTBUFFER_VERSION,3,export_params};
    require("encrypted_pkcs8_size",NCryptExportKey(o.temporary,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&export_desc,nullptr,0,&n,0));
    if(n>1024*1024)throw std::runtime_error("PKCS8 bound");Secret exported;exported.v.resize(n);
    require("encrypted_pkcs8_export",NCryptExportKey(o.temporary,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&export_desc,exported.v.data(),n,&n,0));exported.v.resize(n);
    BYTE random[32];require("random",BCryptGenRandom(nullptr,random,sizeof random,BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    auto nonce=hex(random,sizeof random);o.name=L"VOID-SCHANNEL-TEST-"+widen(nonce);o.store_name=L"VOID-SCHANNEL-TEST-STORE-"+widen(nonce);
    // Public metadata is written before creation. No secret is serialized.
    const auto metadata=nonce+"\n"+digest(original.data(),static_cast<DWORD>(original.size()))+"\n"+digest(o.cert->pbCertEncoded,o.cert->cbCertEncoded)+"\n";
    auto lock_path=std::wstring(argv[2])+L".lock";
    o.live_lock=CreateFileW(lock_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);wincheck("liveness_lock",o.live_lock!=INVALID_HANDLE_VALUE);
    HANDLE journal=CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);wincheck("intent_create_new",journal!=INVALID_HANDLE_VALUE);
    DWORD written=0;BOOL journal_ok=WriteFile(journal,metadata.data(),static_cast<DWORD>(metadata.size()),&written,nullptr)&&written==metadata.size()&&FlushFileBuffers(journal);CloseHandle(journal);wincheck("intent_flush",journal_ok);
    checkpoint(L"intent");
    require("provider_open",NCryptOpenStorageProvider(&o.provider,MS_KEY_STORAGE_PROVIDER,0));
    auto provider_version=property(o.provider,NCRYPT_VERSION_PROPERTY);
    printf("{\"software_ksp_version\":%lu}\n",*reinterpret_cast<DWORD*>(provider_version.data()));
    Secret raw;
    if(reserved)prepare_raw_import(o.provider,exported,secret,o.temporary,raw);
    NCryptBuffer named{static_cast<ULONG>((o.name.size()+1)*sizeof(wchar_t)),NCRYPTBUFFER_PKCS_KEY_NAME,o.name.data()};
    NCryptBuffer params[]{named,secret};NCryptBufferDesc desc{NCRYPTBUFFER_VERSION,2,params};
    SECURITY_STATUS s=reserved?create_import_handle(o.provider,o.temporary,o.name,raw,&o.key):
      NCryptImportKey(o.provider,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&desc,&o.key,exported.v.data(),n,deferred?NCRYPT_DO_NOT_FINALIZE_FLAG:0);
    if(!s&&!reserved)o.owned_key=true;require(reserved?"candidate_import_prepared":deferred?"named_import_deferred":"named_import_immediate",s);
    checkpoint(L"import");
    if(!deferred){auto initial=property(o.key,NCRYPT_EXPORT_POLICY_PROPERTY);
      printf("{\"immediate_import_export_policy\":%lu}\n",*reinterpret_cast<DWORD*>(initial.data()));
      if(*reinterpret_cast<DWORD*>(initial.data()))throw std::runtime_error("immediate default export policy");}
    NCRYPT_KEY_HANDLE reopened=0;s=NCryptOpenKey(o.provider,&reopened,o.name.c_str(),0,0);result("open_before_finalize",s);if(reopened)NCryptFreeObject(reopened);
    DWORD zero=0,usage=NCRYPT_ALLOW_SIGNING_FLAG;
    if(deferred){
      require("export_policy_zero",NCryptSetProperty(o.key,NCRYPT_EXPORT_POLICY_PROPERTY,reinterpret_cast<PBYTE>(&zero),sizeof zero,0));
      require("signing_usage",NCryptSetProperty(o.key,NCRYPT_KEY_USAGE_PROPERTY,reinterpret_cast<PBYTE>(&usage),sizeof usage,0));
    }
    auto acl=private_dacl(user_sid());
    require("key_set_dacl_before_finalize",NCryptSetProperty(o.key,NCRYPT_SECURITY_DESCR_PROPERTY,acl.data(),static_cast<DWORD>(acl.size()),DACL_SECURITY_INFORMATION));
    if(mode!=L"stop-key-unmarked"){
      if(mode==L"stop-wrong-marker")random[0]^=1;
      result("marker_before_finalize",NCryptSetProperty(o.key,L"VOID-SCHANNEL-TEST-OWNER",random,sizeof random,NCRYPT_PERSIST_FLAG|NCRYPT_PERSIST_ONLY_FLAG));
      if(mode==L"stop-wrong-marker")random[0]^=1;
    }
    reopened=0;s=NCryptOpenKey(o.provider,&reopened,o.name.c_str(),0,0);result("open_after_marker_before_finalize",s);if(reopened)NCryptFreeObject(reopened);
    if(deferred){s=NCryptFinalizeKey(o.key,0);if(!s&&reserved)o.owned_key=true;require("finalize",s);}
    checkpoint(L"finalize");
    checkpoint(L"key-unmarked");checkpoint(L"wrong-marker");
    require("key_reopen",NCryptOpenKey(o.provider,&reopened,o.name.c_str(),0,0));
    auto pub=public_blob(reopened);if(pub!=original)throw std::runtime_error("public mismatch");
    auto final_policy=property(reopened,NCRYPT_EXPORT_POLICY_PROPERTY);if(*reinterpret_cast<DWORD*>(final_policy.data()))throw std::runtime_error("export policy nonzero");
    auto key_type=property(reopened,NCRYPT_KEY_TYPE_PROPERTY);if(*reinterpret_cast<DWORD*>(key_type.data())&NCRYPT_MACHINE_KEY_FLAG)throw std::runtime_error("machine key");
    auto persisted_marker=property(reopened,L"VOID-SCHANNEL-TEST-OWNER",NCRYPT_PERSIST_ONLY_FLAG);
    if(persisted_marker.size()!=sizeof random||memcmp(persisted_marker.data(),random,sizeof random))throw std::runtime_error("marker mismatch");
    result("marker_survived_finalize",0);
    auto key_sd=property(reopened,NCRYPT_SECURITY_DESCR_PROPERTY,DACL_SECURITY_INFORMATION);verify_dacl(key_sd.data());
    sign_control(reopened);
    auto plain_status=NCryptExportKey(reopened,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,nullptr,0,&n,0);result("final_plaintext_private_export",plain_status);
    auto encrypted_status=NCryptExportKey(reopened,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&export_desc,nullptr,0,&n,0);result("final_encrypted_private_export",encrypted_status);
    if(!plain_status||!encrypted_status)throw std::runtime_error("final private export allowed");
    NCryptFreeObject(reopened);
    require("marker_after_finalize",NCryptSetProperty(o.key,L"VOID-SCHANNEL-TEST-OWNER",random,sizeof random,NCRYPT_PERSIST_FLAG|NCRYPT_PERSIST_ONLY_FLAG));
    if(test_collision&&reserved)reserved_collision_gate(o,o.temporary,raw,random,exported,secret);
    if(test_collision&&!reserved){
    // Different synthetic key material prevents idempotent same-key import from
    // being confused with protection of an unrelated credential.
    Owner sentinel;sentinel.name=o.name+L"-sentinel";
    require("sentinel_provider",NCryptOpenStorageProvider(&sentinel.provider,MS_KEY_STORAGE_PROVIDER,0));
    s=NCryptCreatePersistedKey(sentinel.provider,&sentinel.key,NCRYPT_RSA_ALGORITHM,sentinel.name.c_str(),0,0);
    if(!s)sentinel.owned_key=true;require("sentinel_create",s);
    require("sentinel_finalize",NCryptFinalizeKey(sentinel.key,0));
    require("sentinel_marker",NCryptSetProperty(sentinel.key,L"VOID-SCHANNEL-TEST-OWNER",random,sizeof random,NCRYPT_PERSIST_FLAG|NCRYPT_PERSIST_ONLY_FLAG));
    auto sentinel_public=public_blob(sentinel.key);
    NCryptBuffer collision_name{static_cast<ULONG>((sentinel.name.size()+1)*sizeof(wchar_t)),NCRYPTBUFFER_PKCS_KEY_NAME,sentinel.name.data()};
    NCryptBuffer collision_params[]{collision_name,secret};NCryptBufferDesc collision_desc{NCRYPTBUFFER_VERSION,2,collision_params};
    NCRYPT_KEY_HANDLE collision=0;
    s=NCryptImportKey(o.provider,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&collision_desc,&collision,exported.v.data(),static_cast<DWORD>(exported.v.size()),deferred_collision?NCRYPT_DO_NOT_FINALIZE_FLAG:0);
    result(deferred_collision?"key_collision_deferred_import":"key_collision_immediate_import",s);
    if(deferred_collision&&!s){s=NCryptFinalizeKey(collision,0);result("key_collision_finalize",s);}
    if(collision){auto imported_name=property(collision,NCRYPT_NAME_PROPERTY);
      printf("{\"collision_import_name_matches\":%s}\n",wcscmp(reinterpret_cast<wchar_t*>(imported_name.data()),sentinel.name.c_str())==0?"true":"false");NCryptFreeObject(collision);}
    NCRYPT_KEY_HANDLE sentinel_check=0;require("sentinel_reopen",NCryptOpenKey(sentinel.provider,&sentinel_check,sentinel.name.c_str(),0,0));
    DWORD marker_size=32;BYTE collision_marker[32]{};
    auto marker_status=NCryptGetProperty(sentinel_check,L"VOID-SCHANNEL-TEST-OWNER",collision_marker,sizeof collision_marker,&marker_size,NCRYPT_PERSIST_ONLY_FLAG);
    result("collision_sentinel_marker_read",marker_status);
    auto sentinel_after=public_blob(sentinel_check);NCryptFreeObject(sentinel_check);
    printf("{\"sentinel_public_unchanged\":%s,\"sentinel_marker_unchanged\":%s}\n",sentinel_after==sentinel_public?"true":"false",!marker_status&&marker_size==sizeof random&&!memcmp(collision_marker,random,sizeof random)?"true":"false");
    if(s!=NTE_EXISTS)throw std::runtime_error("collision policy");
    if(marker_status||memcmp(collision_marker,random,sizeof random)||sentinel_after!=sentinel_public)throw std::runtime_error("collision altered owned sentinel");
    result("collision_sentinel_unchanged",0);
    sentinel.cleanup();
    }
    wincheck("store_create_new",CertRegisterSystemStore(o.store_name.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_CREATE_NEW_FLAG,nullptr,nullptr));o.owned_store=true;
    checkpoint(L"store-unmarked");
    o.store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,o.store_name.c_str());wincheck("store_open",o.store!=nullptr);
    result("store_collision",CertRegisterSystemStore(o.store_name.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_CREATE_NEW_FLAG,nullptr,nullptr)?0:GetLastError());
    const auto registry_path=L"Software\\Microsoft\\SystemCertificates\\"+o.store_name;
    HKEY store_registry=nullptr;require("store_registry_open",RegOpenKeyExW(HKEY_CURRENT_USER,registry_path.c_str(),0,KEY_ALL_ACCESS,&store_registry));
    auto registry_acl=private_dacl(user_sid(),false,true);
    s=RegSetKeySecurity(store_registry,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,registry_acl.data());
    result("store_set_dacl",s);if(s){RegCloseKey(store_registry);throw std::runtime_error("store ACL");}
    require("store_marker_write",RegSetValueExW(store_registry,L"VOID-SCHANNEL-TEST-OWNER",0,REG_BINARY,random,sizeof random));
    registry_dacl(store_registry,true);
    require("store_marker_flush",RegFlushKey(store_registry));RegCloseKey(store_registry);
    checkpoint(L"store-marked");
    PCCERT_CONTEXT fresh=CertCreateCertificateContext(X509_ASN_ENCODING,o.cert->pbCertEncoded,o.cert->cbCertEncoded);wincheck("fresh_der",fresh!=nullptr);
    CRYPT_KEY_PROV_INFO info{};info.pwszContainerName=o.name.data();info.pwszProvName=const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);
    BOOL b=CertSetCertificateContextProperty(fresh,CERT_KEY_PROV_INFO_PROP_ID,0,&info);
    if(b)b=CertAddCertificateContextToStore(o.store,fresh,CERT_STORE_ADD_NEW,nullptr);CertFreeCertificateContext(fresh);wincheck("store_add_bound_der",b);
    BYTE sha1[20];n=sizeof sha1;wincheck("thumbprint",CertGetCertificateContextProperty(o.cert,CERT_SHA1_HASH_PROP_ID,sha1,&n));
    HKEY leaf_registry=nullptr;const auto leaf_path=registry_path+L"\\Certificates\\"+widen(hex(sha1,20));
    require("store_leaf_registry",RegOpenKeyExW(HKEY_CURRENT_USER,leaf_path.c_str(),0,KEY_READ,&leaf_registry));registry_dacl(leaf_registry,false);RegCloseKey(leaf_registry);
    // Handle closure leaves durable objects; exact reopen proves the difference.
    wincheck("store_handle_close",CertCloseStore(o.store,CERT_CLOSE_STORE_CHECK_FLAG));o.store=nullptr;
    o.store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,o.store_name.c_str());wincheck("store_reopen_after_close",o.store!=nullptr);
    unsigned stored=0;PCCERT_CONTEXT item=nullptr;while((item=CertEnumCertificatesInStore(o.store,item)))++stored;
    if(stored!=1)throw std::runtime_error("private store contents");result("store_exactly_one_certificate",0);
    require("key_handle_close",NCryptFreeObject(o.key));o.key=0;
    require("key_reopen_after_close",NCryptOpenKey(o.provider,&o.key,o.name.c_str(),0,0));
    if(o.temporary && o.free_temp)NCryptFreeObject(o.temporary);o.temporary=0;
    CertFreeCertificateContext(o.cert);o.cert=nullptr;wincheck("staging_release",CertCloseStore(o.staging,CERT_CLOSE_STORE_CHECK_FLAG));o.staging=0;
    exported.wipe();bridge.wipe();input.wipe();raw.wipe();
    printf("{\"ready\":true,\"selector\":\"CurrentUser\\\\%s\\\\%s\"}\n",("VOID-SCHANNEL-TEST-STORE-"+nonce).c_str(),hex(sha1,20).c_str());fflush(stdout);
    std::string command;std::getline(std::cin,command);if(!command.empty()&&command.back()=='\r')command.pop_back();
    if(command!="cleanup")throw std::runtime_error("control closed");
    if(mode==L"deny-delete"){
      auto denied=private_dacl(user_sid(),true);
      require("deny_delete_dacl",NCryptSetProperty(o.key,NCRYPT_SECURITY_DESCR_PROPERTY,denied.data(),static_cast<DWORD>(denied.size()),DACL_SECURITY_INFORMATION|NCRYPT_PERSIST_FLAG));
      require("denial_key_handle_close",NCryptFreeObject(o.key));o.key=0;
      require("denial_key_reopen",NCryptOpenKey(o.provider,&o.key,o.name.c_str(),0,0));
      s=NCryptDeleteKey(o.key,0);result("denied_key_delete",s);
      if(!s){o.key=0;o.owned_key=false;throw std::runtime_error("delete denial ineffective");}
      require("restore_test_dacl",NCryptSetProperty(o.key,NCRYPT_SECURITY_DESCR_PROPERTY,acl.data(),static_cast<DWORD>(acl.size()),DACL_SECURITY_INFORMATION|NCRYPT_PERSIST_FLAG));
    }
    if(mode==L"deny-store-delete"){
      HKEY registry=nullptr;require("denial_store_registry",RegOpenKeyExW(HKEY_CURRENT_USER,registry_path.c_str(),0,KEY_ALL_ACCESS,&registry));
      auto denied=private_dacl(user_sid(),true);
      s=RegSetKeySecurity(registry,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,denied.data());
      result("deny_store_delete_dacl",s);
      BOOL deleted=FALSE;if(!s)deleted=CertUnregisterSystemStore(o.store_name.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_DELETE_FLAG);
      DWORD denial=deleted?0:GetLastError();result("denied_store_delete",denial);
      // Only restore the ACL on this exact test-created registry key.
      auto restore=RegSetKeySecurity(registry,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,registry_acl.data());RegCloseKey(registry);result("restore_test_store_dacl",restore);
      if(deleted){o.owned_store=false;throw std::runtime_error("store delete denial ineffective");}
      if(s||restore)throw std::runtime_error("store denial setup or restore");
      wincheck("denial_journal_retained",GetFileAttributesW(argv[2])!=INVALID_FILE_ATTRIBUTES);
    }
    o.cleanup();return 0;
  }catch(const std::exception& e){printf("{\"failure\":\"%s\"}\n",e.what());fflush(stdout);return 1;}
}
