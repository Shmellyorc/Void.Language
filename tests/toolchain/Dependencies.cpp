// Call the real exported APIs, including compression and XML allocation/free.
#include <zlib.h>
#include <zstd.h>
#include <libxml/parser.h>
#include <cstdio>
#include <cstring>

int main() {
    std::printf("zlib headers=%s runtime=%s; zstd headers=%s runtime=%s; LibXml2 headers=%s runtime=%s\n",
        ZLIB_VERSION,zlibVersion(),ZSTD_VERSION_STRING,ZSTD_versionString(),LIBXML_DOTTED_VERSION,xmlParserVersion);
    if(std::strcmp(ZLIB_VERSION,zlibVersion()) || ZSTD_VERSION_NUMBER!=ZSTD_versionNumber() ||
       std::strcmp(LIBXML_VERSION_STRING,xmlParserVersion)) return 1;
    const unsigned char input[]="VOID LLVM 23.1.4 dependency probe";
    unsigned char compressed[256],output[256];
    uLongf compressed_size=sizeof(compressed),output_size=sizeof(output);
    if(compress2(compressed,&compressed_size,input,sizeof(input),Z_DEFAULT_COMPRESSION)!=Z_OK ||
       uncompress(output,&output_size,compressed,compressed_size)!=Z_OK ||
       output_size!=sizeof(input) || std::memcmp(input,output,sizeof(input))) return 2;
    auto zstd_size=ZSTD_compress(compressed,sizeof(compressed),input,sizeof(input),1);
    if(ZSTD_isError(zstd_size)) return 3;
    auto decoded=ZSTD_decompress(output,sizeof(output),compressed,zstd_size);
    if(ZSTD_isError(decoded) || decoded!=sizeof(input) || std::memcmp(input,output,sizeof(input))) return 4;
    auto document=xmlReadMemory("<void/>",7,"probe.xml",nullptr,XML_PARSE_NONET);
    if(!document) return 5;
    const bool valid=xmlStrEqual(xmlDocGetRootElement(document)->name,BAD_CAST "void");
    xmlFreeDoc(document);
    xmlCleanupParser();
    return valid?0:6;
}
