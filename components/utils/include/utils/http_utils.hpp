#pragma once

#include "esp_http_server.h"

/**
 * @brief Read full HTTP request body into buffer.
 *
 * - Reads up to dst_size - 1 bytes
 * - Always null-terminates
 * - Drains remaining body if truncated
 *
 * @param req HTTP request
 * @param dst Destination buffer
 * @param dst_size Size of destination buffer
 * @param out_len Optional: number of bytes written (excluding null terminator)
 * @return true on success, false on error
 */
bool http_read_body(httpd_req_t* req, char* dst, size_t dst_size, int* out_len);