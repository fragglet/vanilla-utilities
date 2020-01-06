
#ifndef _wattcp_icmp_h
#define _wattcp_icmp_h

//#include <wattcp/wattcp.h>
#include <wattcp.h>

typedef struct icmp_unused {
    byte 	type;
    byte	code;
    word	checksum;
    longword	unused;
    in_Header	ip;
    byte	spares[ 8 ];
};

typedef struct icmp_pointer {
    byte	type;
    byte	code;
    word	checksum;
    byte	pointer;
    byte	unused[ 3 ];
    in_Header	ip;
};
typedef struct icmp_ip {
    byte	type;
    byte	code;
    word	checksum;
    longword	ipaddr;
    in_Header	ip;
};
typedef struct icmp_echo {
    byte	type;
    byte	code;
    word	checksum;
    word	identifier;
    word	sequence;
    longword index;
};

typedef struct icmp_timestamp {
    byte	type;
    byte	code;
    word	checksum;
    word	identifier;
    word	sequence;
    longword	original;	/* original timestamp */
    longword	receive;	/* receive timestamp */
    longword	transmit;	/* transmit timestamp */
};

typedef struct icmp_info {
    byte	type;
    byte	code;
    word	checksum;
    word	identifier;
    word	sequence;
};

typedef union  {
	struct icmp_unused	unused;
	struct icmp_pointer	pointer;
	struct icmp_ip		ip;
	struct icmp_echo	echo;
	struct icmp_timestamp	timestamp;
	struct icmp_info	info;
} icmp_pkt;

typedef struct _pkt {
    in_Header 	in;
    icmp_pkt 	icmp;
    in_Header	data;
};

#define ICMPTYPE_ECHOREPLY       0
#define ICMPTYPE_UNREACHABLE     3
#define ICMPTYPE_TIMEEXCEEDED    11

typedef enum ICMP_UnreachableCodes
{
   ICMP_UNREACH_NET = 0,
   ICMP_UNREACH_HOST = 1,
   ICMP_UNREACH_PROTO = 2,
   ICMP_UNREACH_PORT = 3,
   ICMP_UNREACH_FRAGNEEDED = 4,
   ICMP_UNREACH_SRCROUTEFAILED = 5
};

typedef enum ICMP_TimeExceededCodes
{
   ICMP_EXCEEDED_TTL = 0,
   ICMP_EXCEEDED_FRAGREASM = 1
};

/* a user-installed ICMP handler */
typedef int (*icmp_handler_type)( in_Header *ip );

/* install a user ICMP handler, or NULL to disable */
void set_icmp_handler( icmp_handler_type user_handler );

#endif /* ndef _wattcp_icmp_h */
