// Isolated research executable. Never linked into VOID.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

struct Bytes {
  std::vector<char> value;
  ~Bytes(){if(!value.empty())SecureZeroMemory(value.data(),value.size());}
  void load(const char* path){
    if(std::string(path)=="-")return;
    std::ifstream file(path,std::ios::binary|std::ios::ate);auto n=file.tellg();
    if(n<=0||n>1024*1024)throw std::runtime_error("input bound");
    value.resize(static_cast<size_t>(n));file.seekg(0);if(!file.read(value.data(),n))throw std::runtime_error("input read");
  }
};
static size_t discard(char*,size_t n,size_t m,void*){return n*m;}
static CURLcode observe_context(CURL*,void* context,void*){
  auto ctx=static_cast<SSL_CTX*>(context);auto objects=X509_STORE_get0_objects(SSL_CTX_get_cert_store(ctx));
  printf("{\"ssl_context_trust_objects\":%zu}\n",sk_X509_OBJECT_num(objects));return CURLE_OK;
}
static bool parse(const Bytes& bytes,const std::string& password){
  BIO* bio=BIO_new_mem_buf(bytes.value.data(),static_cast<int>(bytes.value.size()));
  PKCS12* p12=bio?d2i_PKCS12_bio(bio,nullptr):nullptr;BIO_free(bio);
  EVP_PKEY* key=nullptr;X509* cert=nullptr;STACK_OF(X509)* chain=nullptr;
  bool ok=p12&&PKCS12_parse(p12,password.c_str(),&key,&cert,&chain)==1;
  if(ok)ok=X509_check_private_key(cert,key)==1;
  printf("{\"p12_parse\":%s,\"input_bytes\":%zu}\n",ok?"true":"false",bytes.value.size());
  if(!ok)for(unsigned long code;(code=ERR_get_error())!=0;){
    const char* reason=ERR_reason_error_string(code);
    printf("{\"p12_error_code\":%lu,\"p12_error_reason\":\"%s\"}\n",code,reason?reason:"unknown");
  }
  EVP_PKEY_free(key);X509_free(cert);sk_X509_pop_free(chain,X509_free);PKCS12_free(p12);ERR_clear_error();return ok;
}
template<class T>static void option(CURL* easy,CURLoption name,T value){
  if(curl_easy_setopt(easy,name,value)!=CURLE_OK)throw std::runtime_error("curl option rejected");
}
int main(int argc,char** argv){
  if(argc==3&&std::string(argv[1])=="--select"){
    const curl_ssl_backend** available=nullptr;curl_global_sslset(CURLSSLBACKEND_NONE,nullptr,&available);
    for(auto p=available;p&&*p;p++)printf("{\"available_backend\":\"%s\"}\n",(*p)->name);
    auto first=curl_global_sslset(CURLSSLBACKEND_NONE,argv[2],nullptr);
    auto init=curl_global_init(CURL_GLOBAL_DEFAULT);
    auto other=curl_global_sslset(CURLSSLBACKEND_NONE,std::string(argv[2])=="openssl"?"schannel":"openssl",nullptr);
    printf("{\"first_selection\":%d,\"global_init\":%d,\"other_selection_after_init\":%d}\n",first,init,other);
    puts(curl_version());curl_global_cleanup();return first||init?1:0;
  }
  if(argc!=10)return 2;
  Bytes certificate,ca;std::string password;CURL* easy=nullptr;bool global=false;int exit_code=1;
  try{
    certificate.load(argv[3]);ca.load(argv[2]);
    if(certificate.value.size()+ca.value.size()>1024*1024)throw std::runtime_error("combined input bound");
    for(char c;std::cin.get(c)&&c!='\n';){if(password.size()==4096)throw std::runtime_error("password bound");password.push_back(c);}
    if(!password.empty()&&password.back()=='\r')password.pop_back();
    if(!certificate.value.empty()&&!parse(certificate,password))throw std::runtime_error("P12 rejected before TLS");
    if(curl_global_init(CURL_GLOBAL_DEFAULT))throw std::runtime_error("global init");global=true;
    const auto* version=curl_version_info(CURLVERSION_NOW);
    printf("{\"curl\":\"%s\",\"tls_provider\":\"%s\",\"http2\":%s}\n",version->version,version->ssl_version,version->features&CURL_VERSION_HTTP2?"true":"false");
    easy=curl_easy_init();if(!easy)throw std::runtime_error("easy allocation");char error[CURL_ERROR_SIZE]{};
    option(easy,CURLOPT_ERRORBUFFER,error);option(easy,CURLOPT_URL,argv[1]);
    option(easy,CURLOPT_SSL_VERIFYPEER,1L);option(easy,CURLOPT_SSL_VERIFYHOST,2L);
    option(easy,CURLOPT_PROXY_SSL_VERIFYPEER,1L);option(easy,CURLOPT_PROXY_SSL_VERIFYHOST,2L);
    option(easy,CURLOPT_SSL_CTX_FUNCTION,observe_context);
    const bool native=std::string(argv[6])=="native";
    option(easy,CURLOPT_SSL_OPTIONS,native?CURLSSLOPT_NATIVE_CA:0L);
    option(easy,CURLOPT_PROXY_SSL_OPTIONS,native?CURLSSLOPT_NATIVE_CA:0L);
    option(easy,CURLOPT_CAINFO,static_cast<const char*>(nullptr));option(easy,CURLOPT_CAPATH,static_cast<const char*>(nullptr));
    option(easy,CURLOPT_PROXY_CAINFO,static_cast<const char*>(nullptr));option(easy,CURLOPT_PROXY_CAPATH,static_cast<const char*>(nullptr));
    curl_blob roots{ca.value.data(),ca.value.size(),CURL_BLOB_NOCOPY};
    if(!ca.value.empty()){option(easy,CURLOPT_CAINFO_BLOB,&roots);option(easy,CURLOPT_PROXY_CAINFO_BLOB,&roots);}
    curl_blob identity{certificate.value.data(),certificate.value.size(),CURL_BLOB_NOCOPY};
    if(!certificate.value.empty()){
      option(easy,CURLOPT_SSLCERT_BLOB,&identity);option(easy,CURLOPT_SSLCERTTYPE,"P12");option(easy,CURLOPT_KEYPASSWD,password.c_str());
      if(std::string(argv[9])=="proxy-identity"){
        option(easy,CURLOPT_PROXY_SSLCERT_BLOB,&identity);option(easy,CURLOPT_PROXY_SSLCERTTYPE,"P12");option(easy,CURLOPT_PROXY_KEYPASSWD,password.c_str());
      }
    }
    const long tls=std::string(argv[4])=="1.3"?CURL_SSLVERSION_TLSv1_3|CURL_SSLVERSION_MAX_TLSv1_3:CURL_SSLVERSION_TLSv1_2|CURL_SSLVERSION_MAX_TLSv1_2;
    option(easy,CURLOPT_SSLVERSION,tls);option(easy,CURLOPT_PROXY_SSLVERSION,tls);
    option(easy,CURLOPT_HTTP_VERSION,std::string(argv[5])=="h2"?CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE:CURL_HTTP_VERSION_1_1);
    option(easy,CURLOPT_PROXY,std::string(argv[8])=="-"?"":argv[8]);option(easy,CURLOPT_NOPROXY,"");
    option(easy,CURLOPT_CONNECTTIMEOUT_MS,5000L);option(easy,CURLOPT_TIMEOUT_MS,10000L);option(easy,CURLOPT_WRITEFUNCTION,discard);
    if(std::string(argv[7])=="ocsp")option(easy,CURLOPT_SSL_VERIFYSTATUS,1L);
    else if(std::string(argv[7])!="-")option(easy,CURLOPT_CRLFILE,argv[7]);
    exit_code=0;
    for(int request=0;request<2;request++){
      auto status=curl_easy_perform(easy);long connections=0,http=0;
      curl_easy_getinfo(easy,CURLINFO_NUM_CONNECTS,&connections);curl_easy_getinfo(easy,CURLINFO_HTTP_VERSION,&http);
      printf("{\"request\":%d,\"curl_status\":%d,\"new_connections\":%ld,\"http_version\":%ld}\n",request,status,connections,http);
      if(status){fprintf(stderr,"%s\n",error);exit_code=1;break;}
    }
  }catch(const std::exception& error){printf("{\"failure\":\"%s\"}\n",error.what());}
  if(easy)curl_easy_cleanup(easy);if(global)curl_global_cleanup();
  if(!password.empty())SecureZeroMemory(password.data(),password.size());
  printf("{\"provider_handles_released\":true,\"private_transport_wiped_on_scope_exit\":true}\n");return exit_code;
}
