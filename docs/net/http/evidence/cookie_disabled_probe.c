/* Native disabled-feature control for the exact proposed curl patch. */
#include <curl/curl.h>
#include <stdio.h>
#include <sys/resource.h>

int main(void) {
    struct rlimit stack;
    if (getrlimit(RLIMIT_STACK, &stack)) return 1;
    if (curl_global_init(CURL_GLOBAL_DEFAULT)) return 2;
    CURL *easy = curl_easy_init();
    if (!easy) return 3;
    struct curl_slist marker = {0};
    struct curl_slist *list = &marker;
    CURLcode code = curl_easy_getinfo(easy, CURLINFO_COOKIELIST, &list);
    printf("{\"code\":%d,\"list_null\":%s,\"stack_soft\":%llu,\"stack_hard\":%llu}\n",
           (int)code, list ? "false" : "true",
           (unsigned long long)stack.rlim_cur, (unsigned long long)stack.rlim_max);
    curl_easy_cleanup(easy);
    curl_global_cleanup();
    return code != CURLE_OK || list != NULL;
}
