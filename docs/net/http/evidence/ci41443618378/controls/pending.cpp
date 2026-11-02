#include <curl/curl.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
static bool sent=false;
static int trace(CURL*,curl_infotype t,char* p,size_t n,void*) { if(t==CURLINFO_HEADER_OUT) sent=true; if(t==CURLINFO_TEXT) fwrite(p,1,n,stderr);return 0; }
static size_t body(char*,size_t a,size_t b,void*){return a*b;}
int main(int argc,char**argv){ curl_global_init(CURL_GLOBAL_ALL); auto*m=curl_multi_init();curl_multi_setopt(m,CURLMOPT_MAX_HOST_CONNECTIONS,2L); auto*a=curl_easy_init();auto*b=curl_easy_init();
for(auto*e:{a,b}){curl_easy_setopt(e,CURLOPT_WRITEFUNCTION,body);curl_easy_setopt(e,CURLOPT_HTTP_VERSION,(long)CURL_HTTP_VERSION_2TLS);curl_easy_setopt(e,CURLOPT_PIPEWAIT,argc>3?0L:1L);curl_easy_setopt(e,CURLOPT_DNS_SERVERS,argv[2]);curl_easy_setopt(e,CURLOPT_IPRESOLVE,(long)CURL_IPRESOLVE_V4);curl_easy_setopt(e,CURLOPT_VERBOSE,1L);curl_easy_setopt(e,CURLOPT_DEBUGFUNCTION,trace);curl_easy_setopt(e,CURLOPT_PROXY,"");curl_easy_setopt(e,CURLOPT_CONNECTTIMEOUT_MS,1000L);}
std::string first=std::string(argv[1])+"/slow",second=std::string(argv[1])+"/plain";curl_easy_setopt(a,CURLOPT_URL,first.c_str());curl_easy_setopt(b,CURLOPT_URL,second.c_str());curl_multi_add_handle(m,a); bool added=false;int rc=99;auto start=std::chrono::steady_clock::now();while(std::chrono::steady_clock::now()-start<std::chrono::seconds(3)){int running;curl_multi_perform(m,&running);if(!added){curl_multi_add_handle(m,b);added=true;}int n;while(auto*msg=curl_multi_info_read(m,&n))if(msg->msg==CURLMSG_DONE&&msg->easy_handle==b){rc=msg->data.result;goto done;}curl_multi_poll(m,nullptr,0,10,&n);}done:curl_multi_remove_handle(m,a);curl_multi_remove_handle(m,b);curl_easy_cleanup(a);curl_easy_cleanup(b);curl_multi_cleanup(m);curl_global_cleanup();printf("result=%d\n",rc);return rc;}
