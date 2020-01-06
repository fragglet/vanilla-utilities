#include <wattcp.h>

void debugpsocketlen( void )
{
    printf("tcp_Socket : %u bytes \n", sizeof( tcp_Socket ));
    printf("udp_Socket : %u bytes \n", sizeof( udp_Socket ));
}
