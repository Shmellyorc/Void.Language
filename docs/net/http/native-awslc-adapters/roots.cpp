// Read-only public-anchor control. No identity/key lookup or trust-store write.
#include "common.hpp"
int main(int argc,char** argv){
  if(argc!=2)return 2;
  try{
    Store system,exclusive;system.value=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_CURRENT_USER|CERT_STORE_OPEN_EXISTING_FLAG|CERT_STORE_READONLY_FLAG,L"ROOT");check("system_root_readonly",system.value!=nullptr);
    exclusive.value=CertOpenStore(CERT_STORE_PROV_MEMORY,0,0,CERT_STORE_CREATE_NEW_FLAG,nullptr);check("audit_snapshot_memory_store",exclusive.value!=nullptr);Bytes material;material.load(argv[1]);memory_roots(exclusive.value,material);
    CERT_CHAIN_ENGINE_CONFIG config{};config.cbSize=sizeof config;config.hExclusiveRoot=exclusive.value;config.dwExclusiveFlags=CERT_CHAIN_EXCLUSIVE_ENABLE_CA_FLAG;HCERTCHAINENGINE engine=nullptr;check("audit_exclusive_engine",CertCreateCertificateChainEngine(&config,&engine));
    bool found=false;PCCERT_CONTEXT cert=nullptr;unsigned count=0;
    while((cert=CertEnumCertificatesInStore(system.value,cert))){
      if(++count>256)break;if(CertVerifyTimeValidity(nullptr,cert->pCertInfo))continue;
      CERT_CHAIN_PARA parameters{};parameters.cbSize=sizeof parameters;parameters.dwUrlRetrievalTimeout=1000;LPSTR usage=const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH);parameters.RequestedUsage.dwType=USAGE_MATCH_TYPE_AND;parameters.RequestedUsage.Usage={1,&usage};
      PCCERT_CHAIN_CONTEXT ambient=nullptr;DWORD flags=CERT_CHAIN_REVOCATION_CHECK_CHAIN|CERT_CHAIN_REVOCATION_ACCUMULATIVE_TIMEOUT;
      if(!CertGetCertificateChain(nullptr,cert,nullptr,nullptr,&parameters,flags,nullptr,&ambient))continue;
      DWORD initial=ambient->TrustStatus.dwErrorStatus;CertFreeCertificateChain(ambient);if(initial)continue;
      PCCERT_CHAIN_CONTEXT replaced=nullptr;BOOL ok=CertGetCertificateChain(engine,cert,nullptr,nullptr,&parameters,flags,nullptr,&replaced);DWORD changed=replaced?replaced->TrustStatus.dwErrorStatus:0;
      if(replaced)CertFreeCertificateChain(replaced);status("system_anchor_default_chain_trust",initial);status("system_anchor_explicit_replacement_trust",changed);
      auto fingerprint=cert_hash(cert);printf("{\"public_system_anchor_sha256\":\"%s\",\"private_identity_accessed\":false}\n",fingerprint.c_str());
      found=ok&&(changed&CERT_TRUST_IS_UNTRUSTED_ROOT);break;
    }
    if(cert)CertFreeCertificateContext(cert);CertFreeCertificateChainEngine(engine);if(!found)throw std::runtime_error("system anchor replacement control not established");return 0;
  }catch(const std::exception& error){printf("{\"failure\":\"%s\"}\n",error.what());return 1;}
}
