#include "common.hpp"
#include "trust.hpp"
#include "signing.hpp"
#include <iostream>
#include <thread>

struct Client;
static int context_index(){static int index=SSL_CTX_get_ex_new_index(0,nullptr,nullptr,nullptr,nullptr);return index;}
static ssl_verify_result_t native_verify(SSL*,uint8_t*);
static ssl_private_key_result_t native_sign(SSL*,uint8_t*,size_t*,size_t,uint16_t,const uint8_t*,size_t);
static ssl_private_key_result_t native_complete(SSL*,uint8_t*,size_t*,size_t);
static const SSL_PRIVATE_KEY_METHOD key_method{native_sign,nullptr,native_complete};
static CURLcode configure_context(CURL*,void*,void*);
static size_t discard(char*,size_t size,size_t count,void*){return size*count;}
template<class T>static void option(CURL* easy,CURLoption name,T value){if(curl_easy_setopt(easy,name,value))throw std::runtime_error("curl option rejected");}
struct Client{
  Bytes ca,p12;Trust trust;Identity identity;std::string password;CURL* easy=nullptr;curl_slist* resolve=nullptr;char error[CURL_ERROR_SIZE]{};bool use_store=false;
  ~Client(){if(easy)curl_easy_cleanup(easy);if(resolve)curl_slist_free_all(resolve);if(!password.empty())SecureZeroMemory(password.data(),password.size());}
  void prepare(char** argv,const std::string& secret){
    password=secret;ca.load(argv[2]);trust.prepare(ca,argv[5]);use_store=std::string(argv[4])=="store";
    if(use_store){identity.open(argv[3]);identity.retry=std::string(argv[8])=="sign-retry";identity.pkcs1=std::string(argv[8])=="pkcs1";
      if(std::string(argv[8])=="pss384")identity.forced=SSL_SIGN_RSA_PSS_RSAE_SHA384;
      if(std::string(argv[8])=="pss512")identity.forced=SSL_SIGN_RSA_PSS_RSAE_SHA512;}
    else if(std::string(argv[4])=="p12")p12.load(argv[3]);
    easy=curl_easy_init();if(!easy)throw std::runtime_error("easy allocation");
    option(easy,CURLOPT_URL,argv[1]);option(easy,CURLOPT_ERRORBUFFER,error);option(easy,CURLOPT_WRITEFUNCTION,discard);
    option(easy,CURLOPT_SSL_VERIFYPEER,1L);option(easy,CURLOPT_SSL_VERIFYHOST,2L);option(easy,CURLOPT_PROXY_SSL_VERIFYPEER,1L);option(easy,CURLOPT_PROXY_SSL_VERIFYHOST,2L);
    option(easy,CURLOPT_CAINFO,static_cast<const char*>(nullptr));option(easy,CURLOPT_CAPATH,static_cast<const char*>(nullptr));
    option(easy,CURLOPT_PROXY_CAINFO,static_cast<const char*>(nullptr));option(easy,CURLOPT_PROXY_CAPATH,static_cast<const char*>(nullptr));
    option(easy,CURLOPT_SSL_OPTIONS,0L);option(easy,CURLOPT_PROXY_SSL_OPTIONS,0L);
    curl_blob roots{ca.value.data(),ca.value.size(),CURL_BLOB_NOCOPY};if(!ca.value.empty()){option(easy,CURLOPT_CAINFO_BLOB,&roots);option(easy,CURLOPT_PROXY_CAINFO_BLOB,&roots);}
    curl_blob certificate{p12.value.data(),p12.value.size(),CURL_BLOB_NOCOPY};if(!p12.value.empty()){option(easy,CURLOPT_SSLCERT_BLOB,&certificate);option(easy,CURLOPT_SSLCERTTYPE,"P12");option(easy,CURLOPT_KEYPASSWD,password.c_str());}
    option(easy,CURLOPT_SSL_CTX_FUNCTION,configure_context);option(easy,CURLOPT_SSL_CTX_DATA,this);
    long tls=std::string(argv[6])=="1.2"?CURL_SSLVERSION_TLSv1_2|CURL_SSLVERSION_MAX_TLSv1_2:CURL_SSLVERSION_TLSv1_3|CURL_SSLVERSION_MAX_TLSv1_3;
    option(easy,CURLOPT_SSLVERSION,tls);option(easy,CURLOPT_PROXY_SSLVERSION,tls);option(easy,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_1);
    option(easy,CURLOPT_PROXY,std::string(argv[7])=="-"?"":argv[7]);option(easy,CURLOPT_NOPROXY,"");
    const bool deadline=std::string(argv[8])=="deadline";
    option(easy,CURLOPT_CONNECTTIMEOUT_MS,deadline?100L:4000L);option(easy,CURLOPT_TIMEOUT_MS,deadline?100L:8000L);
    std::string routes=argv[9];size_t start=0;
    while(routes!="-"&&start<routes.size()){
      auto end=routes.find('|',start);auto item=routes.substr(start,end==std::string::npos?end:end-start);auto next=curl_slist_append(resolve,item.c_str());if(!next)throw std::runtime_error("resolve allocation");resolve=next;if(end==std::string::npos)break;start=end+1;
    }if(resolve)option(easy,CURLOPT_RESOLVE,resolve);
  }
  bool perform(unsigned owner,unsigned request){
    auto result=curl_easy_perform(easy);long connections=0;curl_easy_getinfo(easy,CURLINFO_NUM_CONNECTS,&connections);
    printf("{\"owner\":%u,\"request\":%u,\"curl_status\":%d,\"new_connections\":%ld,\"native_verify_calls\":%u,\"sign_calls\":%u,\"complete_calls\":%u}\n",owner,request,result,connections,trust.calls,identity.signs,identity.completes);
    if(result)fprintf(stderr,"%s\n",error);return result==CURLE_OK;
  }
};
static Client* client(SSL* ssl){return static_cast<Client*>(SSL_CTX_get_ex_data(SSL_get_SSL_CTX(ssl),context_index()));}
static ssl_verify_result_t native_verify(SSL* ssl,uint8_t* alert){
  try{auto state=client(ssl);if(!state){*alert=SSL_AD_INTERNAL_ERROR;return ssl_verify_invalid;}
    if(state->trust.retry){++state->trust.calls;status("native_verification_retry_requested",0);return ssl_verify_retry;}
    if(state->trust.verify(ssl))return ssl_verify_ok;
  }catch(const std::exception& error){printf("{\"verification_exception\":\"%s\"}\n",error.what());}
  *alert=SSL_AD_CERTIFICATE_UNKNOWN;return ssl_verify_invalid;
}
static ssl_private_key_result_t native_sign(SSL* ssl,uint8_t* out,size_t* size,size_t maximum,uint16_t algorithm,const uint8_t* input,size_t input_len){
  try{auto state=client(ssl);if(state)return state->identity.sign(ssl,out,size,maximum,algorithm,input,input_len);}catch(...){status("sign_exception",ERROR_INTERNAL_ERROR);}return ssl_private_key_failure;
}
static ssl_private_key_result_t native_complete(SSL* ssl,uint8_t*,size_t*,size_t){auto state=client(ssl);if(state)++state->identity.completes;return ssl_private_key_failure;}
static CURLcode configure_context(CURL*,void* context,void* opaque){
  try{auto state=static_cast<Client*>(opaque);auto ctx=static_cast<SSL_CTX*>(context);if(!SSL_CTX_set_ex_data(ctx,context_index(),state))return CURLE_OUT_OF_MEMORY;
    if(state->trust.enabled)SSL_CTX_set_custom_verify(ctx,SSL_VERIFY_PEER,native_verify);
    if(state->use_store){
      auto pointer=state->identity.cert.value->pbCertEncoded;X509* leaf=d2i_X509(nullptr,const_cast<const BYTE**>(&pointer),state->identity.cert.value->cbCertEncoded);
      if(!leaf)return CURLE_SSL_CERTPROBLEM;int ok=SSL_CTX_use_certificate(ctx,leaf);X509_free(leaf);if(!ok)return CURLE_SSL_CERTPROBLEM;
      SSL_CTX_set_private_key_method(ctx,&key_method);
      const uint16_t rsa_pss[]{SSL_SIGN_RSA_PSS_RSAE_SHA256,SSL_SIGN_RSA_PSS_RSAE_SHA384,SSL_SIGN_RSA_PSS_RSAE_SHA512,SSL_SIGN_RSA_PKCS1_SHA256};
      const uint16_t rsa_pkcs1[]{SSL_SIGN_RSA_PKCS1_SHA256};const uint16_t ecdsa[]{SSL_SIGN_ECDSA_SECP256R1_SHA256};
      if(state->identity.rsa)ok=state->identity.forced?SSL_CTX_set_signing_algorithm_prefs(ctx,&state->identity.forced,1):state->identity.pkcs1?SSL_CTX_set_signing_algorithm_prefs(ctx,rsa_pkcs1,1):SSL_CTX_set_signing_algorithm_prefs(ctx,rsa_pss,4);
      else ok=SSL_CTX_set_signing_algorithm_prefs(ctx,ecdsa,1);
      if(!ok)return CURLE_SSL_CERTPROBLEM;
    }return CURLE_OK;
  }catch(const std::exception& error){printf("{\"context_exception\":\"%s\"}\n",error.what());return CURLE_SSL_CERTPROBLEM;}
}
int main(int argc,char** argv){
  if(argc!=11)return 2;std::string password;for(char c;std::cin.get(c)&&c!='\n';){if(password.size()==4096)return 2;password+=c;}if(!password.empty()&&password.back()=='\r')password.pop_back();
  if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;int rc=1;
  try{
    printf("{\"tls_provider\":\"%s\"}\n",curl_version_info(CURLVERSION_NOW)->ssl_version);
    auto a=std::make_unique<Client>();a->prepare(argv,password);
    if(std::string(argv[10])=="owners"){
      auto b=std::make_unique<Client>();b->prepare(argv,password);printf("{\"owner_key_handles_distinct\":%s}\n",a->identity.key!=b->identity.key?"true":"false");
      bool first=false,second=false;std::thread one([&]{first=a->perform(0,0);});std::thread two([&]{second=b->perform(1,0);});one.join();two.join();a.reset();bool reused=b->perform(1,1);b.reset();rc=first&&second&&reused?0:1;
    }else{bool first=a->perform(0,0),second=first&&a->perform(0,1);a.reset();rc=first&&second?0:1;}
  }catch(const std::exception& error){printf("{\"failure\":\"%s\"}\n",error.what());}
  curl_global_cleanup();if(!password.empty())SecureZeroMemory(password.data(),password.size());printf("{\"borrowed_identity_handles_released\":true}\n");return rc;
}
