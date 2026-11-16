/*
 * RosHttpDl - a tiny HTTP downloader for ReactOS 0.4.16.
 *
 * Why a custom tool instead of shipping GNU wget / curl:
 *   - MSVC builds need vcruntime140.dll + api-ms-win-crt-* (absent on bare
 *     ReactOS); the static MSVC CRT also imports the Vista+ kernel32 API
 *     InitializeCriticalSectionEx, which ReactOS 0.4.16 does not export.
 *   - GnuWin32 wget / stock curl drag in libintl, libiconv, libcurl, OpenSSL,
 *     zlib ... none of which exist on a bare ReactOS 0.4.16 install.
 *   - This tool is plain C99 built with MinGW-w64 i686 + "-static", so the
 *     resulting PE imports ONLY system DLLs that ReactOS ships:
 *       KERNEL32.dll, ADVAPI32.dll, WS2_32.dll, msvcrt.dll
 *     (verified by the CI "Verify PE imports" step).
 *
 * Supported usage (enough for the local http://10.0.2.2:8000 test share):
 *   wget  <url> [-O <file>] [--output-document=<file>] [-q]
 *   curl  <url> [-o <file>] [--output <file>] [-L] [-s]
 *   With no output option the file name is derived from the URL path.
 *   Only plain http:// URLs are supported (no TLS) - that is all the local
 *   HTTP share needs, and it keeps the binary free of OpenSSL deps.
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IO_BUF 16384

static void usage(const char *prog)
{
    fprintf(stderr,
        "%s - minimal HTTP downloader for ReactOS\n"
        "usage: %s <http://url> [-O <file> | -o <file> | --output-document=<file>]\n"
        "  -O <file>  write to <file>            (wget style)\n"
        "  -o <file>  write to <file>            (curl style)\n"
        "  -q, -s, -L, -nv, --quiet, --output ...  accepted/ignored\n"
        "  If no output name is given, the URL path's file name is used.\n"
        "  Only http:// (not https://) is supported.\n",
        prog, prog);
}

/* Split "http://host[:port]/path" into components. Returns 0 on success. */
static int parse_url(const char *url, char *host, size_t host_sz,
                     int *port, char *path, size_t path_sz)
{
    const char *p = url;
    const char *host_start, *host_end, *colon;

    if (strncmp(p, "http://", 7) != 0) {
        fprintf(stderr, "error: only http:// URLs are supported\n");
        return -1;
    }
    p += 7;

    host_start = p;
    host_end = strchr(p, '/');
    if (!host_end) {
        host_end = p + strlen(p);
        strcpy(path, "/");
    } else {
        /* host_end points at the '/' that terminates the authority section.
         * The request path is everything from that '/' to the URL end. */
        size_t plen = strlen(host_end);
        if (plen >= path_sz) {
            fprintf(stderr, "error: URL path too long\n");
            return -1;
        }
        memcpy(path, host_end, plen + 1);   /* includes NUL */
        if (path[0] != '/') {
            /* defensive: always keep an absolute path */
            memmove(path + 1, path, plen + 1);
            path[0] = '/';
        }
    }

    colon = NULL;
    for (const char *q = host_start; q < host_end; ++q) {
        if (*q == ':') { colon = q; break; }
    }

    if (colon) {
        size_t hlen = (size_t)(colon - host_start);
        if (hlen == 0 || hlen >= host_sz) {
            fprintf(stderr, "error: invalid host in URL\n");
            return -1;
        }
        memcpy(host, host_start, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0 || *port > 65535) *port = 80;
    } else {
        size_t hlen = (size_t)(host_end - host_start);
        if (hlen == 0 || hlen >= host_sz) {
            fprintf(stderr, "error: invalid host in URL\n");
            return -1;
        }
        memcpy(host, host_start, hlen);
        host[hlen] = '\0';
        *port = 80;
    }
    return 0;
}

/* Derive an output file name from a URL path. */
static void name_from_url(const char *url, char *out, size_t out_sz)
{
    const char *slash = strrchr(url, '/');
    const char *name = (slash && slash[1]) ? slash + 1 : "download.bin";
    size_t n = strlen(name);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, name, n);
    out[n] = '\0';
}

/* Case-insensitive substring search, returns pointer or NULL. */
static const char *strcasestr_local(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return hay;
    for (; *hay; ++hay) {
        size_t i;
        for (i = 0; i < nl; ++i) {
            if (tolower((unsigned char)hay[i]) != tolower((unsigned char)needle[i]))
                break;
        }
        if (i == nl) return hay;
    }
    return NULL;
}

static int download(const char *url, const char *outfile)
{
    char host[256], path[2048];
    int port;
    WSADATA wsa;
    struct hostent *he;
    struct sockaddr_in addr;
    SOCKET s;
    char req[4096];
    char buf[IO_BUF];
    char hdr[IO_BUF];
    int hdr_len = 0, hdr_done = 0;
    long content_len = -1, total = 0;
    FILE *f = NULL;
    int rc = 1;

    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0)
        return 1;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "error: WSAStartup failed (%d)\n", WSAGetLastError());
        return 1;
    }

    he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "error: cannot resolve host '%s' (WSA error %d)\n",
                host, WSAGetLastError());
        goto cleanup;
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "error: socket() failed (%d)\n", WSAGetLastError());
        goto cleanup;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "error: cannot connect to %s:%d (WSA error %d)\n",
                host, port, WSAGetLastError());
        closesocket(s);
        goto cleanup;
    }

    sprintf(req,
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: RosHttpDl/1.0 (ReactOS)\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port);

    if (send(s, req, (int)strlen(req), 0) == SOCKET_ERROR) {
        fprintf(stderr, "error: send() failed (%d)\n", WSAGetLastError());
        closesocket(s);
        goto cleanup;
    }

    f = fopen(outfile, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", outfile);
        closesocket(s);
        goto cleanup;
    }

    fprintf(stderr, "downloading %s -> %s\n", url, outfile);

    for (;;) {
        int n = recv(s, buf, IO_BUF, 0);
        const char *chunk = buf;
        int chunk_len = 0;

        if (n == 0) break;                 /* server closed connection */
        if (n == SOCKET_ERROR) {
            fprintf(stderr, "error: recv() failed (%d)\n", WSAGetLastError());
            goto close_all;
        }

        if (!hdr_done) {
            const char *end;
            int take;

            if (hdr_len + n > IO_BUF) {
                fprintf(stderr, "error: HTTP header too large\n");
                goto close_all;
            }
            memcpy(hdr + hdr_len, buf, n);
            hdr_len += n;

            end = strstr(hdr, "\r\n\r\n");
            if (!end) continue;            /* header not complete yet */

            hdr_done = 1;
            {
                int status = 0;
                if (sscanf(hdr, "HTTP/%*d.%*d %d", &status) != 1 ||
                    status < 200 || status >= 300) {
                    fprintf(stderr, "error: unexpected HTTP response: %.80s\n", hdr);
                    goto close_all;
                }
            }
            {
                const char *cl = strcasestr_local(hdr, "content-length:");
                if (cl) content_len = atol(cl + strlen("content-length:"));
            }

            take = (int)((end + 4) - hdr);   /* consume header incl. terminator */
            chunk = buf + take;
            chunk_len = n - take;
            if (chunk_len < 0) chunk_len = 0;
        } else {
            chunk_len = n;
        }

        if (chunk_len > 0) {
            if (fwrite(chunk, 1, (size_t)chunk_len, f) != (size_t)chunk_len) {
                fprintf(stderr, "error: write failed\n");
                goto close_all;
            }
            total += chunk_len;
        }

        if (content_len >= 0 && total >= content_len) break;
    }

    if (total <= 0) {
        fprintf(stderr, "error: no data received\n");
        goto close_all;
    }

    fclose(f);
    f = NULL;
    closesocket(s);
    fprintf(stderr, "done: %ld bytes written to %s\n", total, outfile);
    rc = 0;
    goto cleanup;

close_all:
    if (f) fclose(f);
    closesocket(s);
cleanup:
    WSACleanup();
    return rc;
}

int main(int argc, char **argv)
{
    const char *url = NULL;
    const char *outfile = NULL;
    char derived[512];
    int i;

    if (argc < 2) { usage(argv[0]); return 1; }

    for (i = 1; i < argc; ++i) {
        const char *a = argv[i];

        if (!strcmp(a, "-O") || !strcmp(a, "-o") ||
            !strcmp(a, "--output") || !strcmp(a, "--output-document")) {
            if (i + 1 < argc) outfile = argv[++i];
        } else if (!strncmp(a, "--output-document=", 18)) {
            outfile = a + 18;
        } else if (!strncmp(a, "--output=", 9)) {
            outfile = a + 9;
        } else if (!strcmp(a, "-q") || !strcmp(a, "-s") || !strcmp(a, "-L") ||
                   !strcmp(a, "-nv") || !strcmp(a, "--quiet") ||
                   !strcmp(a, "--no-check-certificate")) {
            /* accepted and ignored */
        } else if (!strncmp(a, "http://", 7)) {
            url = a;
        } else if (a[0] == '-') {
            /* unknown switch: ignore to stay compatible with common flags */
        } else if (!url) {
            url = a;
        }
    }

    if (!url) { usage(argv[0]); return 1; }
    if (!outfile) {
        name_from_url(url, derived, sizeof(derived));
        outfile = derived;
    }

    return download(url, outfile);
}
