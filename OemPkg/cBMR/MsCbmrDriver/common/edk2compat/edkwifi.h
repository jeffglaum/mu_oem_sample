#ifndef _EDK2COMPAT_WIFI_H_
#define _EDK2COMPAT_WIFI_H_

//
// Declared in MdePkg\Include\Protocol\WiFi2.h
//

#ifndef __EFI_WIFI2_PROTOCOL_H__
#define __EFI_WIFI2_PROTOCOL_H__

typedef struct _EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL;

///
/// EFI_80211_BSS_TYPE
///
typedef enum {
    IeeeInfrastructureBSS,
    IeeeIndependentBSS,
    IeeeMeshBSS,
    IeeeAnyBss
} EFI_80211_BSS_TYPE;

///
/// EFI_80211_CONNECT_NETWORK_RESULT_CODE
///
typedef enum {
    //
    // The connection establishment operation finished successfully.
    //
    ConnectSuccess,
    //
    // The connection was refused by the Network.
    //
    ConnectRefused,
    //
    // The connection establishment operation failed (i.e, Network is not
    // detected).
    //
    ConnectFailed,
    //
    // The connection establishment operation was terminated on timeout.
    //
    ConnectFailureTimeout,
    //
    // The connection establishment operation failed on other reason.
    //
    ConnectFailedReasonUnspecified
} EFI_80211_CONNECT_NETWORK_RESULT_CODE;

///
/// EFI_80211_MAC_ADDRESS
///
typedef struct {
    UINT8 Addr[6];
} EFI_80211_MAC_ADDRESS;

#define EFI_MAX_SSID_LEN 32

///
/// EFI_80211_SSID
///
typedef struct {
    //
    // Length in bytes of the SSId. If zero, ignore SSId field.
    //
    UINT8 SSIdLen;
    //
    // Specifies the service set identifier.
    //
    UINT8 SSId[EFI_MAX_SSID_LEN];
} EFI_80211_SSID;

///
/// EFI_80211_GET_NETWORKS_DATA
///
typedef struct {
    //
    // The number of EFI_80211_SSID in SSIDList. If zero, SSIDList should be
    // ignored.
    //
    UINT32 NumOfSSID;
    //
    // The SSIDList is a pointer to an array of EFI_80211_SSID instances. The
    // number of entries is specified by NumOfSSID. The array should only include
    // SSIDs of hidden networks. It is suggested that the caller inputs less than
    // 10 elements in the SSIDList. It is the caller's responsibility to free
    // this buffer.
    //
    EFI_80211_SSID SSIDList[1];
} EFI_80211_GET_NETWORKS_DATA;

///
/// EFI_80211_SUITE_SELECTOR
///
typedef struct {
    //
    // Organization Unique Identifier, as defined in IEEE 802.11 standard,
    // usually set to 00-0F-AC.
    //
    UINT8 Oui[3];
    //
    // Suites types, as defined in IEEE 802.11 standard.
    //
    UINT8 SuiteType;
} EFI_80211_SUITE_SELECTOR;

///
/// EFI_80211_AKM_SUITE_SELECTOR
///
typedef struct {
    //
    // Indicates the number of AKM suite selectors that are contained in
    // AKMSuiteList. If zero, the AKMSuiteList is ignored.
    //
    UINT16 AKMSuiteCount;
    //
    // A variable-length array of AKM suites, as defined in IEEE 802.11 standard,
    // Table 8-101. The number of entries is specified by AKMSuiteCount.
    //
    EFI_80211_SUITE_SELECTOR AKMSuiteList[1];
} EFI_80211_AKM_SUITE_SELECTOR;

///
/// EFI_80211_CIPHER_SUITE_SELECTOR
///
typedef struct {
    //
    // Indicates the number of cipher suites that are contained in
    // CipherSuiteList. If zero, the CipherSuiteList is ignored.
    //
    UINT16 CipherSuiteCount;
    //
    // A variable-length array of cipher suites, as defined in IEEE 802.11
    // standard, Table 8-99. The number of entries is specified by
    // CipherSuiteCount.
    //
    EFI_80211_SUITE_SELECTOR CipherSuiteList[1];
} EFI_80211_CIPHER_SUITE_SELECTOR;

///
/// EFI_80211_NETWORK
///
typedef struct {
    //
    // Specifies the type of the BSS.
    //
    EFI_80211_BSS_TYPE BSSType;
    //
    // Specifies the SSID of the BSS.
    //
    EFI_80211_SSID SSId;
    //
    // Pointer to the AKM suites supported in the wireless network.
    //
    EFI_80211_AKM_SUITE_SELECTOR* AKMSuite;
    //
    // Pointer to the cipher suites supported in the wireless network.
    //
    EFI_80211_CIPHER_SUITE_SELECTOR* CipherSuite;
} EFI_80211_NETWORK;

///
/// EFI_80211_NETWORK_DESCRIPTION
///
typedef struct {
    //
    // Specifies the found wireless network.
    //
    EFI_80211_NETWORK Network;
    //
    // Indicates the network quality as a value between 0 to 100, where 100
    // indicates the highest network quality.
    //
    UINT8 NetworkQuality;
} EFI_80211_NETWORK_DESCRIPTION;

///
/// EFI_80211_GET_NETWORKS_RESULT
///
typedef struct {
    //
    // The number of EFI_80211_NETWORK_DESCRIPTION in NetworkDesc. If zero,
    // NetworkDesc should be ignored.
    //
    UINT8 NumOfNetworkDesc;
    //
    // The NetworkDesc is a pointer to an array of EFI_80211_NETWORK_DESCRIPTION
    // instances. It is caller's responsibility to free this buffer.
    //
    EFI_80211_NETWORK_DESCRIPTION NetworkDesc[1];
} EFI_80211_GET_NETWORKS_RESULT;

///
/// EFI_80211_GET_NETWORKS_TOKEN
///
typedef struct {
    //
    // If the status code returned by GetNetworks() is EFI_SUCCESS, then this
    // Event will be signaled after the Status field is updated by the EFI
    // Wireless MAC Connection Protocol II driver. The type of Event must be
    // EFI_NOTIFY_SIGNAL.
    //
    EFI_EVENT Event;
    //
    // Will be set to one of the following values:
    // EFI_SUCCESS: The operation completed successfully.
    // EFI_NOT_FOUND: Failed to find available wireless networks.
    // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
    // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
    // hardware or software state.
    // EFI_NOT_READY: The operation is started but not yet completed.
    //
    EFI_STATUS Status;
    //
    // Pointer to the input data for getting networks.
    //
    EFI_80211_GET_NETWORKS_DATA* Data;
    //
    // Indicates the scan result. It is caller's responsibility to free this
    // buffer.
    //
    EFI_80211_GET_NETWORKS_RESULT* Result;
} EFI_80211_GET_NETWORKS_TOKEN;

///
/// EFI_80211_CONNECT_NETWORK_DATA
///
typedef struct {
    //
    // Specifies the wireless network to connect to.
    //
    EFI_80211_NETWORK* Network;
    //
    // Specifies a time limit in seconds that is optionally present, after which
    // the connection establishment procedure is terminated by the UNDI driver.
    // This is an optional parameter and may be 0. Values of 5 seconds or higher
    // are recommended.
    //
    UINT32 FailureTimeout;
} EFI_80211_CONNECT_NETWORK_DATA;

///
/// EFI_80211_CONNECT_NETWORK_TOKEN
///
typedef struct {
    //
    // If the status code returned by ConnectNetwork() is EFI_SUCCESS, then this
    // Event will be signaled after the Status field is updated by the EFI
    // Wireless MAC Connection Protocol II driver. The type of Event must be
    // EFI_NOTIFY_SIGNAL.
    //
    EFI_EVENT Event;
    //
    // Will be set to one of the following values:
    // EFI_SUCCESS: The operation completed successfully.
    // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
    // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
    // hardware or software state.
    // EFI_NOT_READY: The operation is started but not yet completed.
    //
    EFI_STATUS Status;
    //
    // Pointer to the connection data.
    //
    EFI_80211_CONNECT_NETWORK_DATA* Data;
    //
    // Indicates the connection state.
    //
    EFI_80211_CONNECT_NETWORK_RESULT_CODE ResultCode;
} EFI_80211_CONNECT_NETWORK_TOKEN;

///
/// EFI_80211_DISCONNECT_NETWORK_TOKEN
///
typedef struct {
    //
    // If the status code returned by DisconnectNetwork() is EFI_SUCCESS, then
    // this Event will be signaled after the Status field is updated by the EFI
    // Wireless MAC Connection Protocol II driver. The type of Event must be
    // EFI_NOTIFY_SIGNAL.
    //
    EFI_EVENT Event;
    //
    // Will be set to one of the following values:
    // EFI_SUCCESS: The operation completed successfully
    // EFI_DEVICE_ERROR: An unexpected network or system error occurred.
    // EFI_ACCESS_DENIED: The operation is not completed due to some underlying
    // hardware or software state.
    //
    EFI_STATUS Status;
} EFI_80211_DISCONNECT_NETWORK_TOKEN;

/**
  Request a survey of potential wireless networks that administrator can later
  elect to try to join.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for getting wireless
                                  network.

  @retval EFI_SUCCESS             The operation started, and an event will
                                  eventually be raised for the caller.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters is not
                                  supported by this implementation.
  @retval EFI_ALREADY_STARTED     The operation of getting wireless network is
                                  already started.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef EFI_STATUS(EFIAPI* EFI_WIRELESS_MAC_CONNECTION_II_GET_NETWORKS)(
    IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL* This,
    IN EFI_80211_GET_NETWORKS_TOKEN* Token);

/**
  Connect a wireless network specified by a particular SSID, BSS type and
  Security type.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for connecting wireless
                                  network.

  @retval EFI_SUCCESS             The operation started successfully. Results
                                  will be notified eventually.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not
                                  supported by this implementation.
  @retval EFI_ALREADY_STARTED     The connection process is already started.
  @retval EFI_NOT_FOUND           The specified wireless network is not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef EFI_STATUS(EFIAPI* EFI_WIRELESS_MAC_CONNECTION_II_CONNECT_NETWORK)(
    IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL* This,
    IN EFI_80211_CONNECT_NETWORK_TOKEN* Token);

/**
  Request a disconnection with current connected wireless network.

  @param[in]  This                Pointer to the
                                  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL
                                  instance.
  @param[in]  Token               Pointer to the token for disconnecting
                                  wireless network.

  @retval EFI_SUCCESS             The operation started successfully. Results
                                  will be notified eventually.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  Token is NULL.
  @retval EFI_UNSUPPORTED         One or more of the input parameters are not
                                  supported by this implementation.
  @retval EFI_NOT_FOUND           Not connected to a wireless network.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be
                                  allocated.

**/
typedef EFI_STATUS(EFIAPI* EFI_WIRELESS_MAC_CONNECTION_II_DISCONNECT_NETWORK)(
    IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL* This,
    IN EFI_80211_DISCONNECT_NETWORK_TOKEN* Token);

///
/// The EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL provides network management
/// service interfaces for 802.11 network stack. It is used by network
/// applications (and drivers) to establish wireless connection with a wireless
/// network.
///
struct _EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL {
    EFI_WIRELESS_MAC_CONNECTION_II_GET_NETWORKS GetNetworks;
    EFI_WIRELESS_MAC_CONNECTION_II_CONNECT_NETWORK ConnectNetwork;
    EFI_WIRELESS_MAC_CONNECTION_II_DISCONNECT_NETWORK DisconnectNetwork;
};

#endif // __EFI_WIFI2_PROTOCOL_H__

//
// Declared in MdePkg\Include\Protocol\Supplicant.h
//

#ifndef __EFI_SUPPLICANT_PROTOCOL_H__
#define __EFI_SUPPLICANT_PROTOCOL_H__

typedef struct _EFI_SUPPLICANT_PROTOCOL EFI_SUPPLICANT_PROTOCOL;

///
/// EFI_SUPPLICANT_CRYPT_MODE
///
typedef enum {
    //
    // Encrypt data provided in the fragment buffers.
    //
    EfiSupplicantEncrypt,
    //
    // Decrypt data provided in the fragment buffers.
    //
    EfiSupplicantDecrypt,
} EFI_SUPPLICANT_CRYPT_MODE;

///
/// EFI_SUPPLICANT_DATA_TYPE
///
typedef enum {
    //
    // Session Configuration
    //

    //
    // Current authentication type in use. The corresponding Data is of type
    // EFI_80211_AKM_SUITE_SELECTOR.
    //
    EfiSupplicant80211AKMSuite,
    //
    // Group data encryption type in use. The corresponding Data is of type
    // EFI_SUPPLICANT_CIPHER_SUITE_SELECTOR.
    //
    EfiSupplicant80211GroupDataCipherSuite,
    //
    // Pairwise encryption type in use. The corresponding Data is of type
    // EFI_80211_CIPHER_SUITE_SELECTOR.
    //
    EfiSupplicant80211PairwiseCipherSuite,
    //
    // PSK password. The corresponding Data is a NULL-terminated ASCII string.
    //
    EfiSupplicant80211PskPassword,
    //
    // Target SSID name. The corresponding Data is of type EFI_80211_SSID.
    //
    EfiSupplicant80211TargetSSIDName,
    //
    // Station MAC address. The corresponding Data is of type
    // EFI_80211_MAC_ADDRESS.
    //
    EfiSupplicant80211StationMac,
    //
    // Target SSID MAC address. The corresponding Data is 6 bytes MAC address.
    //
    EfiSupplicant80211TargetSSIDMac,

    //
    // Session Information
    //

    //
    // 802.11 PTK. The corresponding Data is of type EFI_SUPPLICANT_KEY.
    //
    EfiSupplicant80211PTK,
    //
    // 802.11 GTK. The corresponding Data is of type EFI_SUPPLICANT_GTK_LIST.
    //
    EfiSupplicant80211GTK,
    //
    // Supplicant state. The corresponding Data is
    // EFI_EAPOL_SUPPLICANT_PAE_STATE.
    //
    EfiSupplicantState,
    //
    // 802.11 link state. The corresponding Data is EFI_80211_LINK_STATE.
    //
    EfiSupplicant80211LinkState,
    //
    // Flag indicates key is refreshed. The corresponding Data is
    // EFI_SUPPLICANT_KEY_REFRESH.
    //
    EfiSupplicantKeyRefresh,

    //
    // Session Configuration
    //

    //
    // Supported authentication types. The corresponding Data is of type
    // EFI_80211_AKM_SUITE_SELECTOR.
    //
    EfiSupplicant80211SupportedAKMSuites,
    //
    // Supported software encryption types provided by supplicant driver. The
    // corresponding Data is of type EFI_80211_CIPHER_SUITE_SELECTOR.
    //
    EfiSupplicant80211SupportedSoftwareCipherSuites,
    //
    // Supported hardware encryption types provided by wireless UNDI driver. The
    // corresponding Data is of type EFI_80211_CIPHER_SUITE_SELECTOR.
    //
    EfiSupplicant80211SupportedHardwareCipherSuites,

    //
    // Session Information
    //

    //
    // 802.11 Integrity GTK. The corresponding Data is of type
    // EFI_SUPPLICANT_GTK_LIST.
    //
    EfiSupplicant80211IGTK,
    //
    // 802.11 PMK. The corresponding Data is 32 bytes pairwise master key.
    //
    EfiSupplicant80211PMK,
    EfiSupplicantDataTypeMaximum
} EFI_SUPPLICANT_DATA_TYPE;

///
/// EFI_80211_LINK_STATE
///
typedef enum {
    //
    // Indicates initial start state, unauthenticated, unassociated.
    //
    Ieee80211UnauthenticatedUnassociated,
    //
    // Indicates authenticated, unassociated.
    //
    Ieee80211AuthenticatedUnassociated,
    //
    // Indicates authenticated and associated, but pending RSN authentication.
    //
    Ieee80211PendingRSNAuthentication,
    //
    // Indicates authenticated and associated.
    //
    Ieee80211AuthenticatedAssociated
} EFI_80211_LINK_STATE;

///
/// EFI_SUPPLICANT_KEY_TYPE (IEEE Std 802.11 Section 6.3.19.1.2)
///
typedef enum { Group, Pairwise, PeerKey, IGTK } EFI_SUPPLICANT_KEY_TYPE;

///
/// EFI_SUPPLICANT_KEY_DIRECTION (IEEE Std 802.11 Section 6.3.19.1.2)
///
typedef enum {
    //
    // Indicates that the keys are being installed for the receive direction.
    //
    Receive,
    //
    // Indicates that the keys are being installed for the transmit direction.
    //
    Transmit,
    //
    // Indicates that the keys are being installed for both the receive and
    // transmit directions.
    //
    Both
} EFI_SUPPLICANT_KEY_DIRECTION;

///
/// EFI_SUPPLICANT_KEY_REFRESH
///
typedef struct {
    //
    // If TRUE, indicates GTK is just refreshed after a successful call to
    // EFI_SUPPLICANT_PROTOCOL.BuildResponsePacket().
    //
    BOOLEAN GTKRefresh;
} EFI_SUPPLICANT_KEY_REFRESH;

#define EFI_MAX_KEY_LEN 64

///
/// EFI_SUPPLICANT_KEY
///
typedef struct {
    //
    // The key value.
    //
    UINT8 Key[EFI_MAX_KEY_LEN];
    //
    // Length in bytes of the Key. Should be up to EFI_MAX_KEY_LEN.
    //
    UINT8 KeyLen;
    //
    // The key identifier.
    //
    UINT8 KeyId;
    //
    // Defines whether this key is a group key, pairwise key, PeerKey, or
    // Integrity Group.
    //
    EFI_SUPPLICANT_KEY_TYPE KeyType;
    //
    // The value is set according to the KeyType.
    //
    EFI_80211_MAC_ADDRESS Addr;
    //
    // The Receive Sequence Count value.
    //
    UINT8 Rsc[8];
    //
    // Length in bytes of the Rsc. Should be up to 8.
    //
    UINT8 RscLen;
    //
    // Indicates whether the key is configured by the Authenticator or
    // Supplicant. The value true indicates Authenticator.
    //
    BOOLEAN IsAuthenticator;
    //
    // The cipher suite required for this association.
    //
    EFI_80211_SUITE_SELECTOR CipherSuite;
    //
    // Indicates the direction for which the keys are to be installed.
    //
    EFI_SUPPLICANT_KEY_DIRECTION Direction;
} EFI_SUPPLICANT_KEY;

///
/// EFI_SUPPLICANT_GTK_LIST
///
typedef struct {
    //
    // Indicates the number of GTKs that are contained in GTKList.
    //
    UINT8 GTKCount;
    //
    // A variable-length array of GTKs of type EFI_SUPPLICANT_KEY. The number of
    // entries is specified by GTKCount.
    //
    EFI_SUPPLICANT_KEY GTKList[1];
} EFI_SUPPLICANT_GTK_LIST;

///
/// EFI_SUPPLICANT_FRAGMENT_DATA
///
typedef struct {
    //
    // Length of data buffer in the fragment.
    //
    UINT32 FragmentLength;
    //
    // Pointer to the data buffer in the fragment.
    //
    VOID* FragmentBuffer;
} EFI_SUPPLICANT_FRAGMENT_DATA;

/**
  BuildResponsePacket() is called during STA and AP authentication is in
  progress. Supplicant derives the PTK or session keys depend on type of
  authentication is being employed.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]       RequestBuffer  Pointer to the most recently received EAPOL
                                  packet. NULL means the supplicant need
                                  initiate the EAP authentication session and
                                  send EAPOL-Start message.
  @param[in]       RequestBufferSize
                                  Packet size in bytes for the most recently
                                  received EAPOL packet. 0 is only valid when
                                  RequestBuffer is NULL.
  @param[out]      Buffer         Pointer to the buffer to hold the built
                                  packet.
  @param[in, out]  BufferSize     Pointer to the buffer size in bytes. On
                                  input, it is the buffer size provided by the
                                  caller. On output, it is the buffer size in
                                  fact needed to contain the packet.

  @retval EFI_SUCCESS             The required EAPOL packet is built
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  RequestBuffer is NULL, but RequestSize is
                                  NOT 0.
                                  RequestBufferSize is 0.
                                  Buffer is NULL, but RequestBuffer is NOT 0.
                                  BufferSize is NULL.
  @retval EFI_BUFFER_TOO_SMALL    BufferSize is too small to hold the response
                                  packet.
  @retval EFI_NOT_READY           Current EAPOL session state is NOT ready to
                                  build ResponsePacket.

**/
typedef EFI_STATUS(EFIAPI* EFI_SUPPLICANT_BUILD_RESPONSE_PACKET)(IN EFI_SUPPLICANT_PROTOCOL* This,
                                                                 IN UINT8* RequestBuffer,
                                                                 OPTIONAL IN UINTN
                                                                     RequestBufferSize,
                                                                 OPTIONAL OUT UINT8* Buffer,
                                                                 IN OUT UINTN* BufferSize);

/**
  ProcessPacket() is called to Supplicant driver to encrypt or decrypt the data
  depending type of authentication type.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in, out]  FragmentTable  Pointer to a list of fragment. The caller
                                  will take responsible to handle the original
                                  FragmentTable while it may be reallocated in
                                  Supplicant driver.
  @param[in]       FragmentCount  Number of fragment.
  @param[in]       CryptMode      Crypt mode.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  FragmentTable is NULL.
                                  FragmentCount is NULL.
                                  CryptMode is invalid.
  @retval EFI_NOT_READY           Current supplicant state is NOT Authenticated.
  @retval EFI_ABORTED             Something wrong decryption the message.
  @retval EFI_UNSUPPORTED         This API is not supported.

**/
typedef EFI_STATUS(EFIAPI* EFI_SUPPLICANT_PROCESS_PACKET)(
    IN EFI_SUPPLICANT_PROTOCOL* This,
    IN OUT EFI_SUPPLICANT_FRAGMENT_DATA** FragmentTable,
    IN UINT32* FragmentCount,
    IN EFI_SUPPLICANT_CRYPT_MODE CryptMode);

/**
  Set Supplicant configuration data.

  @param[in]  This                Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]  DataType            The type of data.
  @param[in]  Data                Pointer to the buffer to hold the data.
  @param[in]  DataSize            Pointer to the buffer size in bytes.

  @retval EFI_SUCCESS             The Supplicant configuration data is set
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  Data is NULL.
                                  DataSize is 0.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.

**/
typedef EFI_STATUS(EFIAPI* EFI_SUPPLICANT_SET_DATA)(IN EFI_SUPPLICANT_PROTOCOL* This,
                                                    IN EFI_SUPPLICANT_DATA_TYPE DataType,
                                                    IN VOID* Data,
                                                    IN UINTN DataSize);

/**
  Get Supplicant configuration data.

  @param[in]       This           Pointer to the EFI_SUPPLICANT_PROTOCOL
                                  instance.
  @param[in]       DataType       The type of data.
  @param[out]      Data           Pointer to the buffer to hold the data.
                                  Ignored if DataSize is 0.
  @param[in, out]  DataSize       Pointer to the buffer size in bytes. On
                                  input, it is the buffer size provided by the
                                  caller. On output, it is the buffer size in
                                  fact needed to contain the packet.

  @retval EFI_SUCCESS             The Supplicant configuration data is got
                                  successfully.
  @retval EFI_INVALID_PARAMETER   One or more of the following conditions is
                                  TRUE:
                                  This is NULL.
                                  DataSize is NULL.
                                  Data is NULL if *DataSize is not zero.
  @retval EFI_UNSUPPORTED         The DataType is unsupported.
  @retval EFI_NOT_FOUND           The Supplicant configuration data is not
                                  found.
  @retval EFI_BUFFER_TOO_SMALL    The size of Data is too small for the
                                  specified configuration data and the required
                                  size is returned in DataSize.

**/
typedef EFI_STATUS(EFIAPI* EFI_SUPPLICANT_GET_DATA)(IN EFI_SUPPLICANT_PROTOCOL* This,
                                                    IN EFI_SUPPLICANT_DATA_TYPE DataType,
                                                    OUT UINT8* Data,
                                                    OPTIONAL IN OUT UINTN* DataSize);

///
/// The EFI_SUPPLICANT_PROTOCOL is designed to provide unified place for WIFI
/// and EAP security management. Both PSK authentication and 802.1X EAP
/// authentication can be managed via this protocol and driver or application
/// as a consumer can only focus on about packet transmitting or receiving.
///
struct _EFI_SUPPLICANT_PROTOCOL {
    EFI_SUPPLICANT_BUILD_RESPONSE_PACKET BuildResponsePacket;
    EFI_SUPPLICANT_PROCESS_PACKET ProcessPacket;
    EFI_SUPPLICANT_SET_DATA SetData;
    EFI_SUPPLICANT_GET_DATA GetData;
};

#endif // __EFI_SUPPLICANT_PROTOCOL_H__

#endif // _EDK2COMPAT_WIFI_H_
