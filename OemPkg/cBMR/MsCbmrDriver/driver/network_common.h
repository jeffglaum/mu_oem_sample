#ifndef _NETWORK_COMMON_H_
#define _NETWORK_COMMON_H_

typedef struct _ENDPOINT_INFO {
    UINT8 AddressType;
    union {
        EFI_IPv4_ADDRESS Ipv4Address;
        EFI_IPv6_ADDRESS Ipv6Address;
    };
    UINT16 Port;
} ENDPOINT_INFO, *PENDPOINT_INFO;

#define DUMP_IP_PORT(message, x)                                  \
    {                                                             \
        if (0 == ((ENDPOINT_INFO*)(x))->AddressType) {            \
            DBG_INFO("%s: IPv4:%u.%u.%u.%u, Port:%u",             \
                     message,                                     \
                     ((ENDPOINT_INFO*)(x))->Ipv4Address.Addr[0],  \
                     ((ENDPOINT_INFO*)(x))->Ipv4Address.Addr[1],  \
                     ((ENDPOINT_INFO*)(x))->Ipv4Address.Addr[2],  \
                     ((ENDPOINT_INFO*)(x))->Ipv4Address.Addr[3],  \
                     ((ENDPOINT_INFO*)(x))->Port);                \
        } else if (1 == ((ENDPOINT_INFO*)(x))->AddressType) {     \
            DBG_INFO("%s: IPv6:%u.%u.%u.%u.%u.%u.%u.%u, Port:%u", \
                     message,                                     \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[0],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[1],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[2],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[3],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[4],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[5],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[6],  \
                     ((ENDPOINT_INFO*)(x))->Ipv6Address.Addr[7],  \
                     ((ENDPOINT_INFO*)(x))->Port);                \
        } else {                                                  \
            DBG_INFO("%s: Unsupported IP address type %u",        \
                     message,                                     \
                     ((ENDPOINT_INFO*)(x))->AddressType);         \
        }                                                         \
    }

#define DUMP_IP4_PORT(message, ip, port)              \
    {                                                 \
        {                                             \
            DBG_INFO("%s: IPv4:%u.%u.%u.%u, Port:%u", \
                     message,                         \
                     ip.Addr[0],                      \
                     ip.Addr[1],                      \
                     ip.Addr[2],                      \
                     ip.Addr[3],                      \
                     port);                           \
        }                                             \
    }

EFI_STATUS EFIAPI NetworkCommonInitStack();
BOOLEAN EFIAPI NetworkCommonIsEthernetHandle(_In_ EFI_HANDLE DeviceHandle);
BOOLEAN EFIAPI NetworkCommonIsEthernetMediaAttached(_In_ EFI_HANDLE DeviceHandle);
#endif // _NETWORK_COMMON_H_
