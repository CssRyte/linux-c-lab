#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(
            stderr,
            "Usage: %s <port> <max_connections> <hold_seconds>\n",
            argv[0]
        );

        fprintf(
            stderr,
            "Example: %s 8080 200 30\n",
            argv[0]
        );

        return 1;
    }

    int port = atoi(argv[1]);
    int max_conn = atoi(argv[2]);
    int hold_seconds = atoi(argv[3]);

    if (max_conn <= 0 || max_conn > 10000) {
        fprintf(
            stderr,
            "max_connections must be between 1 and 10000\n"
        );
        return 1;
    }

    if (hold_seconds <= 0) {
        fprintf(
            stderr,
            "hold_seconds must be > 0\n"
        );
        return 1;
    }

    const char *target_ip = "127.0.0.1";

    int *socks = calloc(max_conn, sizeof(int));

    if (!socks) {
        perror("calloc");
        return 1;
    }

    printf(
        "Starting localhost-only connection test to %s:%d\n",
        target_ip,
        port
    );

    for (int i = 0; i < max_conn; i++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);

        if (s < 0) {
            perror("socket");
            socks[i] = -1;
            continue;
        }

        struct sockaddr_in srv;

        memset(&srv, 0, sizeof(srv));

        srv.sin_family = AF_INET;
        srv.sin_port = htons(port);

        inet_pton(AF_INET, target_ip, &srv.sin_addr);

        if (connect(
                s,
                (struct sockaddr*)&srv,
                sizeof(srv)
            ) < 0) {

            fprintf(
                stderr,
                "connect failed: %s\n",
                strerror(errno)
            );

            close(s);
            socks[i] = -1;
            usleep(1000);
            continue;
        }

        socks[i] = s;

        if ((i + 1) % 50 == 0 ||
            i == max_conn - 1) {

            printf(
                "Opened %d/%d connections\n",
                i + 1,
                max_conn
            );
        }

        usleep(1000);
    }

    printf(
        "Holding %d connections for %d seconds (localhost only).\n",
        max_conn,
        hold_seconds
    );

    sleep(hold_seconds);

    for (int i = 0; i < max_conn; i++) {
        if (socks[i] >= 0) {
            close(socks[i]);
        }
    }

    free(socks);

    printf("Done. Closed all sockets.\n");

    return 0;
}
