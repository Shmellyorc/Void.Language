// Existing curl store selection with explicit CA and full peer/host verification.
#include <windows.h>
#include <curl/curl.h>
#include <cstdio>
#include <string>
#include <iostream>
static size_t discard(char*,size_t n,size_t m,void*){return n*m;}
int main(int argc,char** argv){
  if(argc==2&&std::string(argv[1])=="--version"){puts(curl_version());return 0;}
  if(argc!=6)return 2;
  if(curl_global_init(CURL_GLOBAL_DEFAULT))return 2;
  CURL* easy=curl_easy_init();if(!easy)return 2;
  char error[CURL_ERROR_SIZE]{};
  curl_easy_setopt(easy,CURLOPT_ERRORBUFFER,error);
  curl_easy_setopt(easy,CURLOPT_URL,argv[1]);
  curl_easy_setopt(easy,CURLOPT_CAINFO,argv[2]);
  curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1L);
  curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2L);
  curl_easy_setopt(easy,CURLOPT_PROXY,"");
  curl_easy_setopt(easy,CURLOPT_CONNECTTIMEOUT_MS,5000L);
  curl_easy_setopt(easy,CURLOPT_TIMEOUT_MS,10000L);
  curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,discard);
  curl_easy_setopt(easy,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_1);
  long tls=std::string(argv[4])=="1.3" ? CURL_SSLVERSION_TLSv1_3|CURL_SSLVERSION_MAX_TLSv1_3 : CURL_SSLVERSION_TLSv1_2|CURL_SSLVERSION_MAX_TLSv1_2;
  curl_easy_setopt(easy,CURLOPT_SSLVERSION,tls);
  std::string password;
  if(std::string(argv[3])!="none")curl_easy_setopt(easy,CURLOPT_SSLCERT,argv[3]);
  if(std::string(argv[5])=="p12"){
    std::getline(std::cin,password);if(!password.empty()&&password.back()=='\r')password.pop_back();curl_easy_setopt(easy,CURLOPT_SSLCERTTYPE,"P12");
    curl_easy_setopt(easy,CURLOPT_KEYPASSWD,password.c_str());
  }
  int rc=0;
  for(int i=0;i<2;i++){
    CURLcode s=curl_easy_perform(easy);long connections=0;
    curl_easy_getinfo(easy,CURLINFO_NUM_CONNECTS,&connections);
    printf("{\"curl_status\":%d,\"new_connections\":%ld,\"request\":%d}\n",static_cast<int>(s),connections,i);
    if(s){fprintf(stderr,"%s\n",error);rc=1;break;}
  }
  curl_easy_cleanup(easy);if(!password.empty())SecureZeroMemory(password.data(),password.size());
  curl_global_cleanup();return rc;
}
