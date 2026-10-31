/* VOID's default provider policy never writes secrets through SSLKEYLOGFILE.
 * This replaces private curl keylog hooks at build time; upstream files remain
 * byte-identical except for the three reviewed cookie enumeration corrections.
 */
#include "curl_setup.h"
#include "vtls/keylog.h"

void Curl_tls_keylog_open(void) {}
void Curl_tls_keylog_close(void) {}
bool Curl_tls_keylog_enabled(void) { return false; }
const char *Curl_tls_keylog_file_name(void) { return NULL; }
bool Curl_tls_keylog_write(const char *label, const unsigned char *client_random,
                          size_t random_size, const unsigned char *secret,
                          size_t secretlen)
{
  (void)label; (void)client_random; (void)random_size;
  (void)secret; (void)secretlen;
  return false;
}
bool Curl_tls_keylog_write_line(const char *line)
{
  (void)line;
  return false;
}
