#include "stack.h"
/* Standalone provider experiment, not VOID Runtime or a public cookie API.
 * Supply an explicitly built static curl 8.23.0. No network or cookie files.
 * Allocator substitution is confined to this single-threaded test process. */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

static size_t allocation_calls, fail_at, injected;

static int reject_allocation(void) {
    ++allocation_calls;
    if (fail_at && allocation_calls == fail_at) {
        ++injected;
        return 1;
    }
    return 0;
}
static void *probe_malloc(size_t size) {
    return reject_allocation() ? NULL : malloc(size);
}
static void *probe_calloc(size_t count, size_t size) {
    return reject_allocation() ? NULL : calloc(count, size);
}
static void *probe_realloc(void *value, size_t size) {
    return reject_allocation() ? NULL : realloc(value, size);
}
static char *probe_strdup(const char *value) {
    if (reject_allocation()) return NULL;
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy) memcpy(copy, value, length);
    return copy;
}

static void checked(CURLcode code) {
    if (code != CURLE_OK) {
        fprintf(stderr, "Unexpected setup error: %d\n", (int)code);
        exit(2);
    }
}

/* Return 0 for the reproduced ambiguous result; 1 for checked OOM reporting. */
static int inspect(CURL *easy, const char *name, size_t ordinal,
                   size_t expected_count, int expect_over_bytes) {
    struct curl_slist *list = NULL;
    allocation_calls = injected = 0;
    fail_at = ordinal;
    CURLcode code = curl_easy_getinfo(easy, CURLINFO_COOKIELIST, &list);
    fail_at = 0;
    size_t calls = allocation_calls, failures = injected;
    size_t count = 0, bytes = 0;
    for (struct curl_slist *entry = list; entry; entry = entry->next) {
        ++count;
        bytes += strlen(entry->data) + 1;
    }
    int checked_oom = ordinal && code == CURLE_OUT_OF_MEMORY && !list;
    int ambiguous = ordinal && code == CURLE_OK && !list;
    printf("{\"case\":\"%s\",\"fail_ordinal\":%zu,\"api_code\":%d,"
           "\"list_null\":%s,\"allocations\":%zu,\"injected_failures\":%zu,"
           "\"cookie_count\":%zu,\"serialized_bytes_including_nul\":%zu,"
           "\"checked_oom\":%s,\"ambiguous_empty\":%s}\n",
           name, ordinal, (int)code, list ? "false" : "true", calls,
           failures, count, bytes, checked_oom ? "true" : "false",
           ambiguous ? "true" : "false");
    curl_slist_free_all(list);
    if (ordinal) {
        if (failures != 1 || (!checked_oom && !ambiguous)) exit(3);
    } else if (code || count != expected_count ||
               (!!expect_over_bytes != (bytes > 1048576))) {
        exit(4);
    }
    return checked_oom;
}

static void populate(CURL *easy, unsigned count, size_t value_size) {
    char line[5001];
    for (unsigned index = 0; index < count; ++index) {
        int used = snprintf(line, sizeof(line),
                            "accounting.invalid\tFALSE\t/\tFALSE\t0\tc%04u\t", index);
        if (used < 0 || (size_t)used + value_size >= sizeof(line)) exit(5);
        memset(line + used, 'x', value_size);
        line[used + value_size] = 0;
        checked(curl_easy_setopt(easy, CURLOPT_COOKIELIST, line));
    }
}

int main(int argc, char **argv) {
    if (provider_stack()) return 11;
    int embedding_host = argc == 2 && strcmp(argv[1], "--embedding-host") == 0;
    if (argc != 1 && !embedding_host) return 9;
    if (embedding_host) checked(curl_global_init(CURL_GLOBAL_DEFAULT));
    checked(curl_global_init_mem(CURL_GLOBAL_DEFAULT, probe_malloc, free,
                                probe_realloc, probe_strdup, probe_calloc));
    const curl_version_info_data *version = curl_version_info(CURLVERSION_NOW);
    if (!version || version->version_num != 0x081700) return 6;
    printf("{\"transport\":\"%s\",\"standalone_cookie_probe_only\":true,"
           "\"features\":%d,\"curl_off_t_bytes\":%zu}\n",
           version->version, version->features, sizeof(curl_off_t));
    CURL *easy = curl_easy_init();
    if (!easy) return 7;
    checked(curl_easy_setopt(easy, CURLOPT_COOKIEFILE, ""));
    if (embedding_host) {
        populate(easy, 1, 1);
        allocation_calls = injected = 0;
        fail_at = 1;
        struct curl_slist *list = NULL;
        CURLcode code = curl_easy_getinfo(easy, CURLINFO_COOKIELIST, &list);
        fail_at = 0;
        printf("{\"case\":\"embedding_host_initialized_first\",\"api_code\":%d,"
               "\"list_null\":%s,\"replacement_allocator_calls\":%zu,"
               "\"injected_failures\":%zu}\n", (int)code,
               list ? "false" : "true", allocation_calls, injected);
        if (code || !list || allocation_calls || injected) return 10;
        curl_slist_free_all(list);
        curl_easy_cleanup(easy);
        curl_global_cleanup();
        curl_global_cleanup();
        return 0;
    }
    inspect(easy, "empty_control", 0, 0, 0);

    populate(easy, 1025, 1);
    inspect(easy, "over_count_control", 0, 1025, 0);
    size_t count_allocations = allocation_calls;
    size_t count_ordinals[] = {1, 2, 3, count_allocations / 2,
                              count_allocations - 1, count_allocations};
    int corrected = 0;
    for (size_t i = 0; i < sizeof(count_ordinals) / sizeof(count_ordinals[0]); ++i)
        corrected += inspect(easy, "over_count_oom", count_ordinals[i], 0, 0);
    inspect(easy, "over_count_after_oom", 0, 1025, 0);
    checked(curl_easy_setopt(easy, CURLOPT_COOKIELIST, "ALL"));
    inspect(easy, "cleared_control", 0, 0, 0);

    populate(easy, 300, 4070);
    inspect(easy, "over_bytes_control", 0, 300, 1);
    size_t bytes_allocations = allocation_calls;
    size_t bytes_ordinals[] = {1, 2, 3, bytes_allocations / 2,
                              bytes_allocations - 1, bytes_allocations};
    for (size_t i = 0; i < sizeof(bytes_ordinals) / sizeof(bytes_ordinals[0]); ++i)
        corrected += inspect(easy, "over_bytes_oom", bytes_ordinals[i], 0, 0);
    inspect(easy, "over_bytes_after_oom", 0, 300, 1);
    checked(curl_easy_setopt(easy, CURLOPT_COOKIELIST, "ALL"));

    for (unsigned index = 0; index < 2048; ++index) populate(easy, 1, 1);
    inspect(easy, "replaced_2048_times_control", 0, 1, 0);
    checked(curl_easy_setopt(easy, CURLOPT_COOKIELIST,
        "accounting.invalid\tFALSE\t/\tFALSE\t1\tc0000\tx"));
    inspect(easy, "expired_to_empty_control", 0, 0, 0);
    curl_easy_cleanup(easy);
    curl_global_cleanup();
    if (corrected != 0 && corrected != 12) return 8;
    printf("{\"result\":\"%s\",\"oom_cases\":12}\n",
           corrected ? "checked_oom_in_candidate_patch" : "original_ambiguity_reproduced");
    return 0;
}
