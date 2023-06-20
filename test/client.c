#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

int main() {
    // 创建socket
    int fd_client = socket(AF_INET, SOCK_RAW, 0);
    

    return 0;
}