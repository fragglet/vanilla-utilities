#include <wattcp.h>


int sock_stats( sock_type *s, word *days, word *inactive, word *cwindow, word *avg, word *sd )
{
    if (s->tcp.ip_type == UDP_PROTO )
        return( 0 );

    if (days) *days = (word)(set_timeout(0)/0x1800b0L);
    if (inactive) *inactive = sock_inactive;
    if (cwindow) *cwindow = s->tcp.cwindow;
    if (avg)   *avg = s->tcp.vj_sa >> 3;
    if (sd)    *sd  = s->tcp.vj_sd >> 2;
    return (1);      // 94.11.19 -- what's return value mean?
}
