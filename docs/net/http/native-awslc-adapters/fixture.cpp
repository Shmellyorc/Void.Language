// Operator-controlled provisioning only. The adapter never creates/deletes keys.
#include "common.hpp"
struct Fixture{
  NCRYPT_PROV_HANDLE provider=0;NCRYPT_KEY_HANDLE key=0;std::wstring name,store;std::vector<BYTE> marker;
  explicit Fixture(const std::string& nonce){marker=unhex(nonce);if(marker.size()!=32)throw std::runtime_error("nonce size");name=L"VOID-ADAPTER-TEST-"+wide(nonce);store=L"VOID-ADAPTER-TEST-STORE-"+wide(nonce);require("fixture_provider",NCryptOpenStorageProvider(&provider,MS_KEY_STORAGE_PROVIDER,0));}
  ~Fixture(){if(key)NCryptFreeObject(key);if(provider)NCryptFreeObject(provider);}
  void existing(const char* public_path){require("fixture_key_open",NCryptOpenKey(provider,&key,name.c_str(),0,NCRYPT_SILENT_FLAG));Bytes expected;expected.load(public_path);
    if(public_blob(key)!=expected.value||property(key,L"VOID-ADAPTER-TEST-OWNER",NCRYPT_PERSIST_ONLY_FLAG)!=marker)throw std::runtime_error("fixture ownership mismatch");}
};
static void write(const char* path,const std::vector<BYTE>& data){std::ofstream out(path,std::ios::binary);if(!out.write(reinterpret_cast<const char*>(data.data()),data.size()))throw std::runtime_error("public file write");}
int main(int argc,char** argv){
  if(argc==4&&std::string(argv[1])=="purge-aia"){
    try{
      Bytes encoded;encoded.load(argv[2]);Certificate expected;expected.value=CertCreateCertificateContext(X509_ASN_ENCODING,encoded.value.data(),static_cast<DWORD>(encoded.value.size()));check("aia_public_certificate",expected.value!=nullptr);
      if(cert_hash(expected.value)!=argv[3])throw std::runtime_error("AIA cleanup digest mismatch");
      Store ca;ca.value=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,L"CA");check("aia_current_user_ca_open",ca.value!=nullptr);
      PCCERT_CONTEXT found=CertFindCertificateInStore(ca.value,X509_ASN_ENCODING,0,CERT_FIND_EXISTING,expected.value,nullptr);
      if(!found){status("aia_test_certificate_already_absent",GetLastError());return 0;}
      if(found->cbCertEncoded!=expected.value->cbCertEncoded||memcmp(found->pbCertEncoded,expected.value->pbCertEncoded,found->cbCertEncoded)){CertFreeCertificateContext(found);throw std::runtime_error("AIA exact DER mismatch");}
      DWORD size=0;bool has_key=CertGetCertificateContextProperty(found,CERT_KEY_PROV_INFO_PROP_ID,nullptr,&size)||CertGetCertificateContextProperty(found,CERT_KEY_CONTEXT_PROP_ID,nullptr,&size);
      if(has_key){CertFreeCertificateContext(found);throw std::runtime_error("AIA entry has private-key association");}
      check("aia_generated_public_certificate_removed",CertDeleteCertificateFromStore(found));return 0;
    }catch(const std::exception& error){printf("{\"failure\":\"%s\"}\n",error.what());return 1;}
  }
  if(argc<5)return 2;
  try{
    std::string action=argv[1];Fixture fixture(argv[2]);
    if(action=="create"){
      if(argc!=6)return 2;bool rsa=std::string(argv[3])=="RSA";
      require("fixture_create",NCryptCreatePersistedKey(fixture.provider,&fixture.key,rsa?NCRYPT_RSA_ALGORITHM:NCRYPT_ECDSA_P256_ALGORITHM,fixture.name.c_str(),0,0));
      DWORD zero=0,usage=NCRYPT_ALLOW_SIGNING_FLAG,bits=2048;
      require("fixture_nonexportable",NCryptSetProperty(fixture.key,NCRYPT_EXPORT_POLICY_PROPERTY,reinterpret_cast<BYTE*>(&zero),sizeof zero,0));
      require("fixture_sign_only",NCryptSetProperty(fixture.key,NCRYPT_KEY_USAGE_PROPERTY,reinterpret_cast<BYTE*>(&usage),sizeof usage,0));
      if(rsa)require("fixture_rsa_bits",NCryptSetProperty(fixture.key,NCRYPT_LENGTH_PROPERTY,reinterpret_cast<BYTE*>(&bits),sizeof bits,0));
      auto acl=private_acl();require("fixture_private_dacl",NCryptSetProperty(fixture.key,NCRYPT_SECURITY_DESCR_PROPERTY,acl.data(),static_cast<DWORD>(acl.size()),DACL_SECURITY_INFORMATION));
      require("fixture_marker",NCryptSetProperty(fixture.key,L"VOID-ADAPTER-TEST-OWNER",fixture.marker.data(),32,NCRYPT_PERSIST_FLAG|NCRYPT_PERSIST_ONLY_FLAG));
      require("fixture_finalize",NCryptFinalizeKey(fixture.key,0));auto public_data=public_blob(fixture.key);write(argv[4],public_data);
      auto info=spki(fixture.key);DWORD size=0;check("subject_size",CertStrToNameW(X509_ASN_ENCODING,L"CN=VOID adapter disposable client",CERT_X500_NAME_STR,nullptr,nullptr,&size,nullptr));std::vector<BYTE> subject(size);
      check("subject",CertStrToNameW(X509_ASN_ENCODING,L"CN=VOID adapter disposable client",CERT_X500_NAME_STR,nullptr,subject.data(),&size,nullptr));
      CERT_REQUEST_INFO request{};request.dwVersion=0;request.Subject={size,subject.data()};request.SubjectPublicKeyInfo=*reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(info.data());
      CRYPT_ALGORITHM_IDENTIFIER algorithm{};algorithm.pszObjId=const_cast<LPSTR>(rsa?szOID_RSA_SHA256RSA:szOID_ECDSA_SHA256);
      check("csr_size",CryptSignAndEncodeCertificate(fixture.key,CERT_NCRYPT_KEY_SPEC,X509_ASN_ENCODING,X509_CERT_REQUEST_TO_BE_SIGNED,&request,&algorithm,nullptr,nullptr,&size));
      if(size>65536)throw std::runtime_error("CSR bound");std::vector<BYTE> csr(size);
      check("csr_sign_nonexportable",CryptSignAndEncodeCertificate(fixture.key,CERT_NCRYPT_KEY_SPEC,X509_ASN_ENCODING,X509_CERT_REQUEST_TO_BE_SIGNED,&request,&algorithm,nullptr,csr.data(),&size));csr.resize(size);write(argv[5],csr);
      printf("{\"fixture_created\":true,\"algorithm\":\"%s\"}\n",rsa?"RSA":"ECDSA");return 0;
    }
    fixture.existing(argv[3]);Bytes encoded;encoded.load(argv[4]);Certificate certificate;
    if(!encoded.value.empty()){certificate.value=CertCreateCertificateContext(X509_ASN_ENCODING,encoded.value.data(),static_cast<DWORD>(encoded.value.size()));check("fixture_certificate",certificate.value!=nullptr);}
    if(action=="install"||action=="install-bad-association"){
      if(!certificate.value)throw std::runtime_error("fixture certificate missing");auto info=spki(fixture.key);
      bool matching=CertComparePublicKeyInfo(X509_ASN_ENCODING,reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(info.data()),&certificate.value->pCertInfo->SubjectPublicKeyInfo)!=FALSE;
      if(!matching&&action!="install-bad-association")throw std::runtime_error("fixture certificate/key mismatch");
      check("fixture_store_create_new",CertRegisterSystemStore(fixture.store.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_CREATE_NEW_FLAG,nullptr,nullptr));
      HKEY registry=nullptr;auto path=L"Software\\Microsoft\\SystemCertificates\\"+fixture.store;require("fixture_store_registry",RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_ALL_ACCESS,&registry));auto acl=private_acl(true);
      auto s=RegSetKeySecurity(registry,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,acl.data());RegCloseKey(registry);require("fixture_store_acl",s);
      Store store;store.value=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,fixture.store.c_str());check("fixture_store_open",store.value!=nullptr);
      CRYPT_KEY_PROV_INFO binding{};binding.pwszContainerName=fixture.name.data();binding.pwszProvName=const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);binding.dwKeySpec=0;
      check("fixture_bind_provider",CertSetCertificateContextProperty(certificate.value,CERT_KEY_PROV_INFO_PROP_ID,0,&binding));check("fixture_add_der",CertAddCertificateContextToStore(store.value,certificate.value,CERT_STORE_ADD_NEW,nullptr));
      status("fixture_install_complete",0);return 0;
    }
    Store store;store.value=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG|CERT_STORE_READONLY_FLAG,fixture.store.c_str());
    if(store.value){PCCERT_CONTEXT item=nullptr;unsigned count=0;bool valid=true;while((item=CertEnumCertificatesInStore(store.value,item))){++count;valid&=certificate.value&&item->cbCertEncoded==certificate.value->cbCertEncoded&&!memcmp(item->pbCertEncoded,certificate.value->pbCertEncoded,item->cbCertEncoded);}
      if(count!=1||!valid)throw std::runtime_error("fixture store identity mismatch");
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("fixture store inaccessible");
    if(action=="inspect"){
      auto pub=public_blob(fixture.key),policy=property(fixture.key,NCRYPT_EXPORT_POLICY_PROPERTY),usage=property(fixture.key,NCRYPT_KEY_USAGE_PROPERTY),acl=property(fixture.key,NCRYPT_SECURITY_DESCR_PROPERTY,DACL_SECURITY_INFORMATION);
      auto acl_hash=fingerprint(acl);auto certificate_hash=certificate.value?cert_hash(certificate.value):"";
      printf("{\"public_blob\":\"%s\",\"policy\":\"%s\",\"usage\":\"%s\",\"dacl_sha256\":\"%s\",\"certificate_sha256\":\"%s\"}\n",hex(pub.data(),pub.size()).c_str(),hex(policy.data(),policy.size()).c_str(),hex(usage.data(),usage.size()).c_str(),acl_hash.c_str(),certificate_hash.c_str());return 0;
    }
    if(action!="cleanup")return 2;
    if(store.value){check("fixture_store_close",CertCloseStore(store.value,CERT_CLOSE_STORE_CHECK_FLAG));store.value=nullptr;check("fixture_store_delete",CertUnregisterSystemStore(fixture.store.c_str(),CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_DELETE_FLAG));}
    require("fixture_owned_key_delete",NCryptDeleteKey(fixture.key,0));fixture.key=0;
    NCRYPT_KEY_HANDLE check_key=0;auto s=NCryptOpenKey(fixture.provider,&check_key,fixture.name.c_str(),0,0);status("fixture_key_absent",s);if(check_key)NCryptFreeObject(check_key);if(s!=NTE_BAD_KEYSET)throw std::runtime_error("fixture key residue");
    HCERTSTORE check_store=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG,fixture.store.c_str());DWORD error=check_store?0:GetLastError();status("fixture_store_absent",error);if(check_store)CertCloseStore(check_store,0);if(error!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("fixture store residue");return 0;
  }catch(const std::exception& error){printf("{\"failure\":\"%s\"}\n",error.what());return 1;}
}
