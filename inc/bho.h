#ifndef BHO_H
#define BHO_H

#include <Olectl.h>
#include <Wininet.h>

enum HttpStatus {
        Connect,
        OpenRequest,
        Close
};

struct HttpInfo {
        HttpStatus status_;
        HINTERNET http_handle_;
        size_t port_;
        size_t data_len_;
};

#endif  // End of header guard
