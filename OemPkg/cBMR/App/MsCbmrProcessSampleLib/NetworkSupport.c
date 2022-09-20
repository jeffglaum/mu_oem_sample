/** @file NetworkSupport.c

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The library is intended to be a sample of how to initiate the cBMR (Cloud Bare Metal Recovery) process and this file
  specifically contains the primary entry function to initialize the network.
**/

#include "CbmrProcessCommon.h"

// Event used when a network protocol process is blocked by another in use process
EFI_EVENT gEventFlag = NULL;

/**
  Network event callback to support WaitForDataNotify ().  The callback will close the triggering event and if the
  handle matches the global event flag, the flag will be cleared indicating to WaitForDataNotify() that it can
  continue execution.

  @param[in]  Event     Handle to the event to be closed
  @param[in]  Context   Unused, set to NULL by the register function.
**/
VOID
EFIAPI
NetworkEventCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context)
{
  // Close the event triggering this callback
  gBS->CloseEvent (Event);
  
  // If the event matches the flag, clear it.
  if (Event == gEventFlag) {
    gEventFlag = NULL;
  }
}

/**
  If either the SetData or GetData functions in the IP4 protocol return EFI_NOT_READY, that means the command was
  blocked by an already executing process.

  This function is used after getting the not ready return and will register with the protocol for an event, block
  execution flow (with timeout), and proceed once the protocol signals the blocking process is finished.

  @param[in]  Ip4Config2Protocol  Pointer to the IP4 configuration protocol
  @param[in]  DataType            Config data type to associate with the event.  See the UEFI spec for more info.
  @param[in]  TimeoutInSeconds    Number of seconds to wait for the event before returning EFI_TIMEOUT

  @return     EFI_STATUS
**/
EFI_STATUS
EFIAPI
WaitForDataNotify (
  IN EFI_IP4_CONFIG2_PROTOCOL   *Ip4Config2Protocol,
  IN EFI_IP4_CONFIG2_DATA_TYPE  DataType,
  IN UINT32                     TimeoutInSeconds)
{
  volatile EFI_EVENT *EventFlagPtr = &gEventFlag;
  EFI_EVENT Event;
  UINTN TimeoutCount;
  EFI_STATUS Status;

  // Create a notify event to wait on.
  Event = NULL;
  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL,
                             TPL_CALLBACK,
                             NetworkEventCallback,
                             NULL,
                             &Event);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  // Initialize the event flag with this event value
  *EventFlagPtr = Event;

  // Register the event with the IP4 protocol to signal when the async process is done
  Status = Ip4Config2Protocol->RegisterDataNotify(Ip4Config2Protocol,
                                                  DataType,
                                                  Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR:  EFI_IP4_CONFIG2_PROTOCOL::RegisterDataNotify() - Status %r\n", Status));
    gBS->CloseEvent (Event);
    return Status;
  }

  // Each loop delays 1mS, so TimeoutCount is (seconds * 1000)
  TimeoutCount = TimeoutInSeconds * 100;
  Status = EFI_SUCCESS;

  // Wait for the event callback to clear the gEventFlag variable
  while (*EventFlagPtr != NULL) {

    // Check for timeout then stall 10mS
    if (TimeoutCount == 0) {
      Status = EFI_TIMEOUT;
      break;
    }
    gBS->Stall(10 * 1000);
    TimeoutCount--;
  }

  // Unregister the event from the IP4 protocol
  Ip4Config2Protocol->UnregisterDataNotify (Ip4Config2Protocol,
                                            DataType,
                                            Event);

  // If the event did not happen, close the event
  if (*EventFlagPtr == NULL) {
    gBS->CloseEvent (Event);
    *EventFlagPtr = NULL;
  }

  // Return success or timeout
  return Status;
}

// Very simple wrapper for EFI_IP4_CONFIG2_PROTOCOL::SetData that on a not ready return, waits (with timout) for any
// blocking process to finish.
EFI_STATUS
EFIAPI
AsynchronousIP4CfgSetData (
  IN EFI_IP4_CONFIG2_PROTOCOL   *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE  DataType,
  IN UINTN                      DataSize,
  IN VOID                       *Data,
  IN UINT32                     TimeoutInSeconds)
{
  EFI_STATUS Status;

  // Initial call
  Status = This->SetData (This, DataType, DataSize, Data);

  // If not ready, block until ready
  if (Status == EFI_NOT_READY) {
    DEBUG ((DEBUG_INFO, "[cBMR] EFI_IP4_CONFIG2_PROTOCOL::SetData() blocked by an existing process\n"));
    DEBUG ((DEBUG_INFO, "       Waiting up to %d seconds...\n", TimeoutInSeconds));
    Status = WaitForDataNotify (This, DataType, TimeoutInSeconds);
  }

  // Return status
  return Status;
}

// Very simple wrapper for EFI_IP4_CONFIG2_PROTOCOL::GetData that on a not ready return, waits (with timout) for any
// blocking process to finish then re-attempts the get call up to 2 more times if not ready is returned again.
EFI_STATUS
EFIAPI
AsynchronousIP4CfgGetData (
  IN EFI_IP4_CONFIG2_PROTOCOL   *This,
  IN EFI_IP4_CONFIG2_DATA_TYPE  DataType,
  IN OUT UINTN                  *DataSize,
  IN VOID                       *Data,
  IN UINT32                     TimeoutInSeconds)
{
  EFI_STATUS Status;
  UINTN Attempt;

  // Initial call
  Status = This->GetData (This, DataType, DataSize, Data);

  // Loop while not ready and attempts are < 3
  for (Attempt = 0; Attempt < 3 && Status == EFI_NOT_READY; Attempt++) {

    if (Attempt > 0) {
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_IP4_CONFIG2_PROTOCOL::GetData() indicated data is ready, but returned EFI_NOT_READY\n"));
    }
    DEBUG ((DEBUG_INFO, "[cBMR] EFI_IP4_CONFIG2_PROTOCOL::GetData() blocked by an existing process\n"));
    DEBUG ((DEBUG_INFO, "       Waiting up to %d seconds...\n", TimeoutInSeconds));

    // Block until ready
    Status = WaitForDataNotify (This, DataType, TimeoutInSeconds);
    if (EFI_ERROR(Status)) {
      break;
    }

    // Re-try the get call
    Status = This->GetData (This, DataType, DataSize, Data);
  }

  // Return status
  return Status;
}

/**
  Debug prints the IP4 Config Interface Info structure

  @param[in]  Ip4Config2Protocol  Pointer to the IP4 configuration protocol
  @param[in]  InterfaceInfo       Buffer containing interface into to debug print

  @retval     EFI_STATUS
**/
VOID
EFIAPI
DebugPrintNetworkInfo (
  IN EFI_IP4_CONFIG2_PROTOCOL       *Ip4Config2Protocol,
  IN EFI_IP4_CONFIG2_INTERFACE_INFO *InterfaceInfo)
{
  UINTN Size, x;

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));
  DEBUG ((DEBUG_INFO, "    Interface Name:           %s\n", InterfaceInfo->Name));
  DEBUG ((DEBUG_INFO, "    RFC 1700 Hardware Type:   0x%02x\n", InterfaceInfo->IfType));
  DEBUG ((DEBUG_INFO, "    HW MAC Address:           %02X", InterfaceInfo->HwAddress.Addr[0]));
  for (x = 1; x < InterfaceInfo->HwAddressSize; x++) {
    DEBUG ((DEBUG_INFO, "-%02X", InterfaceInfo->HwAddress.Addr[x]));
  }
  DEBUG ((DEBUG_INFO, "\n"));
  DEBUG ((DEBUG_INFO, "    IPv4 Address:             %d.%d.%d.%d\n", InterfaceInfo->StationAddress.Addr[0],
                                                                     InterfaceInfo->StationAddress.Addr[1],
                                                                     InterfaceInfo->StationAddress.Addr[2],
                                                                     InterfaceInfo->StationAddress.Addr[3]));
  DEBUG ((DEBUG_INFO, "    Sub-Net Mask:             %d.%d.%d.%d\n", InterfaceInfo->SubnetMask.Addr[0],
                                                                     InterfaceInfo->SubnetMask.Addr[1],
                                                                     InterfaceInfo->SubnetMask.Addr[2],
                                                                     InterfaceInfo->SubnetMask.Addr[3]));
  Size = InterfaceInfo->RouteTableSize / sizeof(EFI_IP4_ROUTE_TABLE);
  for (x = 0; x < Size; x++) {
    DEBUG ((DEBUG_INFO, "    Routing Table %d:\n", x + 1));
    DEBUG ((DEBUG_INFO, "        Sub-Net Address:        %d.%d.%d.%d\n", InterfaceInfo->RouteTable[x].SubnetAddress.Addr[0],
                                                                         InterfaceInfo->RouteTable[x].SubnetAddress.Addr[1],
                                                                         InterfaceInfo->RouteTable[x].SubnetAddress.Addr[2],
                                                                         InterfaceInfo->RouteTable[x].SubnetAddress.Addr[3]));
    DEBUG ((DEBUG_INFO, "        Sub-Net Mask:           %d.%d.%d.%d\n", InterfaceInfo->RouteTable[x].SubnetMask.Addr[0],
                                                                         InterfaceInfo->RouteTable[x].SubnetMask.Addr[1],
                                                                         InterfaceInfo->RouteTable[x].SubnetMask.Addr[2],
                                                                         InterfaceInfo->RouteTable[x].SubnetMask.Addr[3]));
    DEBUG ((DEBUG_INFO, "        Gateway Address:        %d.%d.%d.%d\n", InterfaceInfo->RouteTable[x].GatewayAddress.Addr[0],
                                                                         InterfaceInfo->RouteTable[x].GatewayAddress.Addr[1],
                                                                         InterfaceInfo->RouteTable[x].GatewayAddress.Addr[2],
                                                                         InterfaceInfo->RouteTable[x].GatewayAddress.Addr[3]));
  }
}

/**
  Locates the first IP4 configuration policy protocol in the system.

  @param[out]  Ip4Config2ProtocolPtr  Pointer to receive the IP4 configuration protocol pointer

  @retval      EFI_STATUS
**/
EFI_STATUS
EFIAPI
LocateIp4ConfigProtocol (
  OUT EFI_IP4_CONFIG2_PROTOCOL  **Ip4Config2ProtocolPtr)
{
  EFI_HANDLE* Handles;
  UINTN HandleCount;
  EFI_STATUS Status;

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));

  // Find all network adapters that are bound to the IP4 Config Protocol
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiIp4Config2ProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  // This sample only supports 1 adapter.  If more are present in the system, this section needs to be expanded to
  // examine the path protocols bound to each handle to determine which one to use.
  if (HandleCount > 1) {
    DEBUG ((DEBUG_ERROR, "[cBMR] WARNING: Found %d EFI_IP4_CONFIG2_PROTOCOL handles\n", HandleCount));
    DEBUG ((DEBUG_ERROR, "                This sample app only supports 1 adapter\n"));
    DEBUG ((DEBUG_ERROR, "                Continuing to attempt connection with the first handle found\n"));
  }

  // Get the EFI_IP4_CONFIG2_PROTOCOL pointer from the handle
  Status = gBS->HandleProtocol (Handles[0], &gEfiIp4Config2ProtocolGuid, (VOID**)Ip4Config2ProtocolPtr);
  FreePool (Handles);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Sends a DHCP configuration request to the network

  @param[in]  Ip4Config2Protocol  IP4 configuration protocol

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ConfigureNetwork (
  IN EFI_IP4_CONFIG2_PROTOCOL  *Ip4Config2Protocol)
{
  EFI_IP4_CONFIG2_POLICY Policy;
  EFI_STATUS Status;
  UINTN Size;

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));

  // Perform a config read to determine if the network is already configured for DHCP
  Size = sizeof(EFI_IP4_CONFIG2_POLICY);
  Status = AsynchronousIP4CfgGetData (Ip4Config2Protocol,
                                      Ip4Config2DataTypePolicy,
                                      &Size,
                                      &Policy,
                                      FixedPcdGet32(PcdCbmrSetDhcpPolicyTimeout));
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_IP4_CONFIG2_PROTOCOL::GetData( Ip4Config2PolicyDhcp ) - Status %r\n", Status));
    return Status;
  }
  if (Policy == Ip4Config2PolicyDhcp) {
    return EFI_SUCCESS;
  }

  // If not, send the configuration policy request for DHCP
  Policy = Ip4Config2PolicyDhcp;
  Status = AsynchronousIP4CfgSetData (Ip4Config2Protocol,
                                      Ip4Config2DataTypePolicy,
                                      sizeof(EFI_IP4_CONFIG2_POLICY),
                                      &Policy,
                                      FixedPcdGet32(PcdCbmrSetDhcpPolicyTimeout));
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_IP4_CONFIG2_PROTOCOL::SetData( Ip4Config2PolicyDhcp ) - Status %r\n", Status));
    return Status;
  }

  // Perform another read to confirm the policy request was accepted
  Size = sizeof(EFI_IP4_CONFIG2_POLICY);
  Status = AsynchronousIP4CfgGetData (Ip4Config2Protocol,
                                      Ip4Config2DataTypePolicy,
                                      &Size,
                                      &Policy,
                                      FixedPcdGet32(PcdCbmrSetDhcpPolicyTimeout));
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_IP4_CONFIG2_PROTOCOL::GetData( Ip4Config2PolicyDhcp ) - Status %r\n", Status));
    return Status;
  }
  if (Policy != Ip4Config2PolicyDhcp) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_IP4_CONFIG2_PROTOCOL::GetData( Ip4Config2PolicyDhcp )\n"));
    DEBUG ((DEBUG_ERROR, "       Policy data was not committed to driver\n"));
    return EFI_PROTOCOL_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Polls the IP4 config protocol waiting for the server to provide a valid IP address.  Returns the interface info
  structure read once the address is valid.  The caller is responsible for freeing the returned buffer.

  @param[in]   Ip4Config2Protocol  IP4 configuration protocol
  @param[out]  InterfaceInfoPtr    Returns a pointer to an allocated buffer containing the interface info

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
WaitForIpAddress (
  IN  EFI_IP4_CONFIG2_PROTOCOL       *Ip4Config2Protocol,
  OUT EFI_IP4_CONFIG2_INTERFACE_INFO **InterfaceInfoPtr)
{
  EFI_IP4_CONFIG2_INTERFACE_INFO *Info;
  UINTN Timeout_mS;
  EFI_STATUS Status;
  UINTN Size;

#define TIMEOUT_LOOP_PAUSE_IN_mS    250

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));

  // Timeout loop
  Timeout_mS = FixedPcdGet32(PcdCbmrGetNetworkIPAddressTimeout) * 1000;
  while (Timeout_mS >= TIMEOUT_LOOP_PAUSE_IN_mS) {

    // Read the IP4 interface info.  Return size can vary, so read with 0 size first get the expected size
    Size = 0;
    Status = AsynchronousIP4CfgGetData (Ip4Config2Protocol,
                                        Ip4Config2DataTypeInterfaceInfo,
                                        &Size,
                                        NULL,
                                        FixedPcdGet32(PcdCbmrGetNetworkInterfaceInfoTimeout));
    if (Status != EFI_BUFFER_TOO_SMALL) {
      return Status;
    }

    // Allocate buffer requested from first call
    Info = (EFI_IP4_CONFIG2_INTERFACE_INFO*) AllocateZeroPool (Size);
    if (Info == NULL) {
      ASSERT (Info);
      return EFI_OUT_OF_RESOURCES;
    }

    // Perform a second call with the proper size buffer allocated
    Status = AsynchronousIP4CfgGetData (Ip4Config2Protocol,
                                        Ip4Config2DataTypeInterfaceInfo,
                                        &Size,
                                        Info,
                                        FixedPcdGet32(PcdCbmrGetNetworkInterfaceInfoTimeout));
    if (EFI_ERROR(Status)) {
      FreePool(Info);
      return Status;
    }

    // If the IP address is no longer zero, provide buffer to caller and exit success
    if (Info->StationAddress.Addr[0] != 0 ||
        Info->StationAddress.Addr[1] != 0 ||
        Info->StationAddress.Addr[2] != 0 ||
        Info->StationAddress.Addr[3] != 0) {

      *InterfaceInfoPtr = Info;
      return EFI_SUCCESS;
    }

    // If address is still zero, free the pool, stall, and loop
    FreePool(Info);
    gBS->Stall(TIMEOUT_LOOP_PAUSE_IN_mS * 1000);
    Timeout_mS -= TIMEOUT_LOOP_PAUSE_IN_mS;
  }

  // If here, the IP address never changed from 0's
  Status = EFI_TIMEOUT;
  DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: Failed to detect a valid IP address - Status %r\n", Status));
  return Status;
}

/**
  Primary function to initiate connection to a network

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ConnectToNetwork ()
{
  EFI_IP4_CONFIG2_PROTOCOL *Ip4Config2Protocol;
  EFI_IP4_CONFIG2_INTERFACE_INFO *InterfaceInfo;
  EFI_STATUS Status;

  //
  // Locate the IP4 configuration policy
  //

  Status = LocateIp4ConfigProtocol(&Ip4Config2Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Send a configuration request to the network
  //

  Status = ConfigureNetwork(Ip4Config2Protocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Wait for a valid IP address from the server
  //

  Status = WaitForIpAddress(Ip4Config2Protocol, &InterfaceInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Report the configuration of the network
  //

  DebugPrintNetworkInfo (Ip4Config2Protocol, InterfaceInfo);
  FreePool(InterfaceInfo);
  return EFI_SUCCESS;
}

