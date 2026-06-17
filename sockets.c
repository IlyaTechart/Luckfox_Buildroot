#include "sockets.h"


void Socket_Netlink_Init(int* fd)
{
    struct sockaddr_nl addr;

    *fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (*fd < 0)
    { 
        perror("Ошибка создания сокета Netlink"); 
        return NULL; 
    }

    memset(&addr, 0, sizeof(struct sockaddr_nl));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 1;
    if (bind(*fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    { 
        perror("Ошибка bind Netlink"); 
        return NULL; 
    }
}