#include <copyright.h>
#include <wattcp.h>

void sock_debugdump( sock_type *s )
{
    tcp_Socket *t;
    udp_Socket *u;

    u = (udp_Socket *)s;
    t = (tcp_Socket *)s;

    printf("next	%4x\n\r", u->next);
    printf("type    %4x\n\r", u->ip_type);
    printf("error %s\n\r", u->err_msg ? u->err_msg : "(NONE)");
    printf("timervalue %8lx returns %sexpired\n\r",
	u->usertimer, chk_timeout(u->usertimer)?"":"NOT ");
    printf("udp rdata %u (%s)\n\r", u->rdatalen, u->rdata );
    printf("tcp rdata %u (%s)\n\r", t->rdatalen, t->rdata );
    printf("tcp state %u\n\r", t->state );
}
