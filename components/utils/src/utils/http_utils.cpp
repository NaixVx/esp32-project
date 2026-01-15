#include "utils/http_utils.hpp"

#include <algorithm>

bool http_read_body(httpd_req_t* req, char* dst, size_t dst_size, int* out_len)
{
    size_t total = req->content_len;
    if (dst_size == 0)
        return false;

    size_t to_read = total;
    size_t written = 0;

    while (to_read > 0) {
        int chunk = httpd_req_recv(req, dst + written, std::min(to_read, dst_size - 1 - written));

        if (chunk <= 0)
            return false;

        written += chunk;
        to_read -= chunk;

        if (written >= dst_size - 1)
            break;
    }

    dst[written] = '\0';

    if (out_len)
        *out_len = static_cast<int>(written);

    // Drain remaining data if truncated
    while (to_read > 0) {
        char junk[64];
        int chunk = httpd_req_recv(req, junk, std::min<size_t>(to_read, sizeof(junk)));

        if (chunk <= 0)
            break;

        to_read -= chunk;
    }

    return true;
}
