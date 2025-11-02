#include "md5.h"
#include <openssl/evp.h>

std::string util::md5Hash(const std::string &str)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, str.data(), str.size());
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(digest_len * 2);
    for (unsigned int i = 0; i < digest_len; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 0xF]);
    }
    return std::forward<std::string>(out);
}