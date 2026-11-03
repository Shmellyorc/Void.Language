#pragma once
struct Identity{
  Store store;Certificate cert;HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key=0;DWORD spec=0;BOOL release=FALSE;
  bool rsa=false,retry=false,pkcs1=false;uint16_t forced=0;unsigned signs=0,completes=0;
  ~Identity(){if(key&&release){if(spec==CERT_NCRYPT_KEY_SPEC)NCryptFreeObject(key);else CryptReleaseContext(key,0);}}
  void open(const std::string& reference){
    auto first=reference.find('\\'),last=reference.rfind('\\');
    if(first==std::string::npos||first==last||reference.substr(0,first)!="CurrentUser"||last-first>128)throw std::runtime_error("unsupported store reference");
    auto thumbprint=unhex(reference.substr(last+1));if(thumbprint.size()!=20)throw std::runtime_error("thumbprint syntax");
    auto name=wide(reference.substr(first+1,last-first-1));store.value=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG|CERT_STORE_READONLY_FLAG,name.c_str());check("identity_store_open",store.value!=nullptr);
    CRYPT_HASH_BLOB hash{20,thumbprint.data()};cert.value=CertFindCertificateInStore(store.value,X509_ASN_ENCODING,0,CERT_FIND_SHA1_HASH,&hash,nullptr);check("identity_exact_find",cert.value!=nullptr);
    check("identity_acquire_silent_compare",CryptAcquireCertificatePrivateKey(cert.value,CRYPT_ACQUIRE_ONLY_NCRYPT_KEY_FLAG|CRYPT_ACQUIRE_COMPARE_KEY_FLAG|CRYPT_ACQUIRE_SILENT_FLAG,nullptr,&key,&spec,&release));
    printf("{\"identity_key_spec\":%lu,\"caller_free\":%s}\n",spec,release?"true":"false");
    if(spec!=CERT_NCRYPT_KEY_SPEC)throw std::runtime_error("legacy provider unsupported in prototype");
    auto info=spki(key);if(!CertComparePublicKeyInfo(X509_ASN_ENCODING,reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(info.data()),&cert.value->pCertInfo->SubjectPublicKeyInfo))throw std::runtime_error("certificate/key mismatch");
    status("identity_public_key_matches",0);
    auto group=property(key,NCRYPT_ALGORITHM_GROUP_PROPERTY);rsa=wcscmp(reinterpret_cast<wchar_t*>(group.data()),NCRYPT_RSA_ALGORITHM_GROUP)==0;
    auto policy=property(key,NCRYPT_EXPORT_POLICY_PROPERTY);DWORD value=0;memcpy(&value,policy.data(),sizeof value);printf("{\"identity_export_policy\":%lu}\n",value);if(value)throw std::runtime_error("fixture is exportable");
    DWORD size=0;auto raw=NCryptExportKey(key,0,rsa?BCRYPT_RSAPRIVATE_BLOB:BCRYPT_ECCPRIVATE_BLOB,nullptr,nullptr,0,&size,0);
    auto p8=NCryptExportKey(key,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,nullptr,0,&size,0);status("identity_private_raw_export_query",raw);status("identity_private_pkcs8_export_query",p8);
    if(!raw||!p8)throw std::runtime_error("private export query unexpectedly allowed");
  }
  ssl_private_key_result_t sign(SSL*,uint8_t* out,size_t* out_len,size_t maximum,uint16_t algorithm,const uint8_t* input,size_t input_len){
    ++signs;printf("{\"sign_call\":%u,\"signature_algorithm\":%u,\"async_retry\":%s}\n",signs,algorithm,retry?"true":"false");
    if(retry)return ssl_private_key_retry;
    const EVP_MD* md=SSL_get_signature_algorithm_digest(algorithm);LPCWSTR hash=nullptr;
    if(md==EVP_sha256())hash=BCRYPT_SHA256_ALGORITHM;else if(md==EVP_sha384())hash=BCRYPT_SHA384_ALGORITHM;else if(md==EVP_sha512())hash=BCRYPT_SHA512_ALGORITHM;
    if(!hash||input_len>1024*1024)return ssl_private_key_failure;
    BYTE digest[EVP_MAX_MD_SIZE];unsigned digest_len=0;if(!EVP_Digest(input,input_len,digest,&digest_len,md,nullptr))return ssl_private_key_failure;
    BCRYPT_PSS_PADDING_INFO pss{hash,digest_len};BCRYPT_PKCS1_PADDING_INFO padding{hash};DWORD flags=NCRYPT_SILENT_FLAG;void* details=nullptr;
    if(rsa){
      bool use_pss=algorithm==SSL_SIGN_RSA_PSS_RSAE_SHA256||algorithm==SSL_SIGN_RSA_PSS_RSAE_SHA384||algorithm==SSL_SIGN_RSA_PSS_RSAE_SHA512;
      if(!use_pss&&algorithm!=SSL_SIGN_RSA_PKCS1_SHA256&&algorithm!=SSL_SIGN_RSA_PKCS1_SHA384&&algorithm!=SSL_SIGN_RSA_PKCS1_SHA512)return ssl_private_key_failure;
      flags|=use_pss?NCRYPT_PAD_PSS_FLAG:NCRYPT_PAD_PKCS1_FLAG;details=use_pss?static_cast<void*>(&pss):static_cast<void*>(&padding);
    }else if(SSL_get_signature_algorithm_key_type(algorithm)!=EVP_PKEY_EC)return ssl_private_key_failure;
    DWORD size=0;auto s=NCryptSignHash(key,details,digest,digest_len,nullptr,0,&size,flags);status("ncrypt_signature_size",s);if(s||size>16384)return ssl_private_key_failure;
    std::vector<BYTE> signature(size);s=NCryptSignHash(key,details,digest,digest_len,signature.data(),size,&size,flags);status("ncrypt_sign",s);if(s)return ssl_private_key_failure;signature.resize(size);
    if(!rsa){
      if(size%2||!size)return ssl_private_key_failure;
      ECDSA_SIG* pair=ECDSA_SIG_new();if(!pair)return ssl_private_key_failure;
      BIGNUM* r=BN_bin2bn(signature.data(),size/2,nullptr);BIGNUM* t=BN_bin2bn(signature.data()+size/2,size/2,nullptr);
      if(!r||!t||!ECDSA_SIG_set0(pair,r,t)){BN_free(r);BN_free(t);ECDSA_SIG_free(pair);return ssl_private_key_failure;}
      int encoded=i2d_ECDSA_SIG(pair,nullptr);std::vector<BYTE> der(encoded>0?encoded:0);auto cursor=der.data();if(encoded>0)i2d_ECDSA_SIG(pair,&cursor);ECDSA_SIG_free(pair);if(encoded<=0)return ssl_private_key_failure;signature.swap(der);
    }
    if(signature.size()>maximum)return ssl_private_key_failure;memcpy(out,signature.data(),signature.size());*out_len=signature.size();return ssl_private_key_success;
  }
};
