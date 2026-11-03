#pragma once
struct Trust{
  Store roots;HCERTCHAINENGINE engine=nullptr;bool enabled=false,offline=false,retry=false;unsigned calls=0;DWORD last_error=0;
  ~Trust(){if(engine)CertFreeCertificateChainEngine(engine);}
  void prepare(const Bytes& ca,const std::string& mode){
    enabled=mode!="aws";offline=mode=="offline";retry=mode=="verify-retry";
    if(!enabled||ca.value.empty())return;
    roots.value=CertOpenStore(CERT_STORE_PROV_MEMORY,0,0,CERT_STORE_CREATE_NEW_FLAG,nullptr);check("exclusive_root_store",roots.value!=nullptr);memory_roots(roots.value,ca);
    CERT_CHAIN_ENGINE_CONFIG config{};config.cbSize=sizeof config;config.hExclusiveRoot=roots.value;config.dwExclusiveFlags=CERT_CHAIN_EXCLUSIVE_ENABLE_CA_FLAG;
    check("exclusive_chain_engine",CertCreateCertificateChainEngine(&config,&engine));
  }
  bool verify(SSL* ssl){
    auto start=std::chrono::steady_clock::now();++calls;
    const char* hostname=SSL_get_servername(ssl,TLSEXT_NAMETYPE_host_name);
    // No expected peer-name/proxy metadata is provided by curl's public ctx
    // callback. DNS SNI is locally configured by curl. Fail closed without it.
    if(!hostname||!*hostname){last_error=ERROR_INVALID_NAME;status("native_verifier_missing_peer_name",last_error);return false;}
    std::string host(hostname);if(host.size()>253||host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-")!=std::string::npos){last_error=ERROR_INVALID_NAME;return false;}
    auto peer=SSL_get0_peer_certificates(ssl);size_t count=sk_CRYPTO_BUFFER_num(peer),total=0;
    if(!count||count>16){last_error=ERROR_INVALID_DATA;return false;}
    Store supplied;supplied.value=CertOpenStore(CERT_STORE_PROV_MEMORY,0,0,CERT_STORE_CREATE_NEW_FLAG,nullptr);if(!supplied.value){last_error=GetLastError();return false;}
    Certificate leaf;
    for(size_t i=0;i<count;i++){
      auto buffer=sk_CRYPTO_BUFFER_value(peer,i);auto n=CRYPTO_BUFFER_len(buffer);if(n>1024*1024-total){last_error=ERROR_INVALID_DATA;return false;}total+=n;
      PCCERT_CONTEXT added=nullptr;if(!CertAddEncodedCertificateToStore(supplied.value,X509_ASN_ENCODING,CRYPTO_BUFFER_data(buffer),static_cast<DWORD>(n),CERT_STORE_ADD_ALWAYS,&added)){last_error=GetLastError();return false;}
      if(i==0)leaf.value=added;else CertFreeCertificateContext(added);
    }
    CERT_CHAIN_PARA parameters{};parameters.cbSize=sizeof parameters;LPSTR usage=const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);
    parameters.RequestedUsage.dwType=USAGE_MATCH_TYPE_AND;parameters.RequestedUsage.Usage={1,&usage};parameters.dwUrlRetrievalTimeout=1000;
    DWORD flags=CERT_CHAIN_REVOCATION_CHECK_CHAIN|CERT_CHAIN_REVOCATION_ACCUMULATIVE_TIMEOUT;
    if(offline)flags|=CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY|CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL;
    PCCERT_CHAIN_CONTEXT chain=nullptr;BOOL built=CertGetCertificateChain(engine,leaf.value,nullptr,supplied.value,&parameters,flags,nullptr,&chain);
    DWORD build_error=built?0:GetLastError();status("native_chain_build",build_error);
    DWORD trust_error=chain?chain->TrustStatus.dwErrorStatus:0;status("native_chain_trust",trust_error);
    DWORD policy_error=0;
    if(built&&chain){
      auto expected=wide(host);SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra{};extra.cbSize=sizeof extra;extra.dwAuthType=AUTHTYPE_SERVER;extra.pwszServerName=expected.data();
      CERT_CHAIN_POLICY_PARA policy{};policy.cbSize=sizeof policy;policy.pvExtraPolicyPara=&extra;
      CERT_CHAIN_POLICY_STATUS outcome{};outcome.cbSize=sizeof outcome;
      if(!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL,chain,&policy,&outcome))policy_error=GetLastError();else policy_error=outcome.dwError;
    }status("native_ssl_policy",policy_error);if(chain)CertFreeCertificateChain(chain);
    last_error=build_error?build_error:policy_error?policy_error:trust_error&~CERT_TRUST_IS_NOT_TIME_NESTED;
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    printf("{\"windows_verification\":%s,\"hostname\":\"%s\",\"peer_certificates\":%zu,\"callback_ms\":%.3f,\"offline\":%s}\n",!last_error?"true":"false",host.c_str(),count,ms,offline?"true":"false");return !last_error;
  }
};
