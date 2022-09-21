/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cbmr.c

Abstract:

    This module implements the wrapper for UEFI
    EFI_HTTP_PROTOCOL.

Author:

    Jancarlo Perez (jpere) 29-March-2021

Environment:

    UEFI mode only.

--*/

//
// Global includes
//

#include "cbmrincludes.h"
#ifndef UEFI_BUILD_SYSTEM
#include "strsafe.h"
#endif

//
// Local includes
//

#include "http.h"
#include "cbmr_config.h"
#include "error.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

//
// Constants/Macros
//

#define HTTP_DEFAULT_RESPONSE_BUFFER_SIZE (0x100000) // 1048576
#define HTTP_REQUEST_WAIT_TIMEOUT         SEC_TO_100_NS(20)
#define HTTP_RESPONSE_WAIT_TIMEOUT        SEC_TO_100_NS(20)

//
// Structs
//

typedef struct _HTTP_CONTEXT {
#if 0
    EFI_HANDLE ParentHandle;

    //
    // DHCP protocol
    //

    DHCP_CONTEXT* Dhcp4Context;
#endif
    //
    // HTTP native protocols.
    //
    EFI_HANDLE HttpHandle;
    EFI_SERVICE_BINDING_PROTOCOL* HttpSvcBindingProtocol;
    EFI_HTTP_PROTOCOL* Http;
} HTTP_CONTEXT, *PHTTP_CONTEXT;

typedef struct _HTTP_REQUEST {
    PWSTR Url;

    BOOLEAN CallbackTriggered;

    EFI_HTTP_REQUEST_DATA Data;
    EFI_HTTP_HEADER Header;
    EFI_HTTP_MESSAGE Message;
    EFI_HTTP_TOKEN Token;
} HTTP_REQUEST, *PHTTP_REQUEST;

typedef struct _HTTP_RESPONSE {
    UINTN ContentLength;
    UINTN ContentDownloaded;
    UINTN TotalExpectedContentLength;

    BOOLEAN CallbackTriggered;

    EFI_HTTP_RESPONSE_DATA Data;
    EFI_HTTP_MESSAGE Message;
    EFI_HTTP_TOKEN Token;

    HTTP_REQUEST* Request;
} HTTP_RESPONSE, *PHTTP_RESPONSE;

// // clang-format off
// static ENUM_TO_STRING HttpStatusMap[] = {
//     {HTTP_STATUS_UNSUPPORTED_STATUS,                STRINGIFY(HTTP_STATUS_UNSUPPORTED_STATUS)},
//     {HTTP_STATUS_100_CONTINUE,                      STRINGIFY(HTTP_STATUS_100_CONTINUE)},
//     {HTTP_STATUS_101_SWITCHING_PROTOCOLS,           STRINGIFY(HTTP_STATUS_101_SWITCHING_PROTOCOLS)},
//     {HTTP_STATUS_200_OK,                            STRINGIFY(HTTP_STATUS_200_OK)},
//     {HTTP_STATUS_201_CREATED,                       STRINGIFY(HTTP_STATUS_201_CREATED)},
//     {HTTP_STATUS_202_ACCEPTED,                      STRINGIFY(HTTP_STATUS_202_ACCEPTED)},
//     {HTTP_STATUS_203_NON_AUTHORITATIVE_INFORMATION, STRINGIFY(HTTP_STATUS_203_NON_AUTHORITATIVE_INFORMATION)},
//     {HTTP_STATUS_204_NO_CONTENT,                    STRINGIFY(HTTP_STATUS_204_NO_CONTENT)},
//     {HTTP_STATUS_205_RESET_CONTENT,                 STRINGIFY(HTTP_STATUS_205_RESET_CONTENT)},
//     {HTTP_STATUS_206_PARTIAL_CONTENT,               STRINGIFY(HTTP_STATUS_206_PARTIAL_CONTENT)},
//     {HTTP_STATUS_300_MULTIPLE_CHOICES,              STRINGIFY(HTTP_STATUS_300_MULTIPLE_CHOICES)},
//     {HTTP_STATUS_301_MOVED_PERMANENTLY,             STRINGIFY(HTTP_STATUS_301_MOVED_PERMANENTLY)},
//     {HTTP_STATUS_302_FOUND,                         STRINGIFY(HTTP_STATUS_302_FOUND)},
//     {HTTP_STATUS_303_SEE_OTHER,                     STRINGIFY(HTTP_STATUS_303_SEE_OTHER)},
//     {HTTP_STATUS_304_NOT_MODIFIED,                  STRINGIFY(HTTP_STATUS_304_NOT_MODIFIED)},
//     {HTTP_STATUS_305_USE_PROXY,                     STRINGIFY(HTTP_STATUS_305_USE_PROXY)},
//     {HTTP_STATUS_307_TEMPORARY_REDIRECT,            STRINGIFY(HTTP_STATUS_307_TEMPORARY_REDIRECT)},
//     {HTTP_STATUS_400_BAD_REQUEST,                   STRINGIFY(HTTP_STATUS_400_BAD_REQUEST)},
//     {HTTP_STATUS_401_UNAUTHORIZED,                  STRINGIFY(HTTP_STATUS_401_UNAUTHORIZED)},
//     {HTTP_STATUS_402_PAYMENT_REQUIRED,              STRINGIFY(HTTP_STATUS_402_PAYMENT_REQUIRED)},
//     {HTTP_STATUS_403_FORBIDDEN,                     STRINGIFY(HTTP_STATUS_403_FORBIDDEN)},
//     {HTTP_STATUS_404_NOT_FOUND,                     STRINGIFY(HTTP_STATUS_404_NOT_FOUND)},
//     {HTTP_STATUS_405_METHOD_NOT_ALLOWED,            STRINGIFY(HTTP_STATUS_405_METHOD_NOT_ALLOWED)},
//     {HTTP_STATUS_406_NOT_ACCEPTABLE,                STRINGIFY(HTTP_STATUS_406_NOT_ACCEPTABLE)},
//     {HTTP_STATUS_407_PROXY_AUTHENTICATION_REQUIRED, STRINGIFY(HTTP_STATUS_407_PROXY_AUTHENTICATION_REQUIRED)},
//     {HTTP_STATUS_408_REQUEST_TIME_OUT,              STRINGIFY(HTTP_STATUS_408_REQUEST_TIME_OUT)},
//     {HTTP_STATUS_409_CONFLICT,                      STRINGIFY(HTTP_STATUS_409_CONFLICT)},
//     {HTTP_STATUS_410_GONE,                          STRINGIFY(HTTP_STATUS_410_GONE)},
//     {HTTP_STATUS_411_LENGTH_REQUIRED,               STRINGIFY(HTTP_STATUS_411_LENGTH_REQUIRED)},
//     {HTTP_STATUS_412_PRECONDITION_FAILED,           STRINGIFY(HTTP_STATUS_412_PRECONDITION_FAILED)},
//     {HTTP_STATUS_413_REQUEST_ENTITY_TOO_LARGE,      STRINGIFY(HTTP_STATUS_413_REQUEST_ENTITY_TOO_LARGE)},
//     {HTTP_STATUS_414_REQUEST_URI_TOO_LARGE,         STRINGIFY(HTTP_STATUS_414_REQUEST_URI_TOO_LARGE)},
//     {HTTP_STATUS_415_UNSUPPORTED_MEDIA_TYPE,        STRINGIFY(HTTP_STATUS_415_UNSUPPORTED_MEDIA_TYPE)},
//     {HTTP_STATUS_416_REQUESTED_RANGE_NOT_SATISFIED, STRINGIFY(HTTP_STATUS_416_REQUESTED_RANGE_NOT_SATISFIED)},
//     {HTTP_STATUS_417_EXPECTATION_FAILED,            STRINGIFY(HTTP_STATUS_417_EXPECTATION_FAILED)},
//     {HTTP_STATUS_500_INTERNAL_SERVER_ERROR,         STRINGIFY(HTTP_STATUS_500_INTERNAL_SERVER_ERROR)},
//     {HTTP_STATUS_501_NOT_IMPLEMENTED,               STRINGIFY(HTTP_STATUS_501_NOT_IMPLEMENTED)},
//     {HTTP_STATUS_502_BAD_GATEWAY,                   STRINGIFY(HTTP_STATUS_502_BAD_GATEWAY)},
//     {HTTP_STATUS_503_SERVICE_UNAVAILABLE,           STRINGIFY(HTTP_STATUS_503_SERVICE_UNAVAILABLE)},
//     {HTTP_STATUS_504_GATEWAY_TIME_OUT,              STRINGIFY(HTTP_STATUS_504_GATEWAY_TIME_OUT)},
//     {HTTP_STATUS_505_HTTP_VERSION_NOT_SUPPORTED,    STRINGIFY(HTTP_STATUS_505_HTTP_VERSION_NOT_SUPPORTED)},
//     {HTTP_STATUS_308_PERMANENT_REDIRECT,            STRINGIFY(HTTP_STATUS_308_PERMANENT_REDIRECT)},
// };
// // clang-format on

//
// Prototypes
//
static VOID EFIAPI HttpRequestCallback(IN EFI_EVENT Event, IN VOID* Context);
static VOID EFIAPI HttpResponseCallback(IN EFI_EVENT Event, IN VOID* Context);
static EFI_STATUS EFIAPI HttpSendRequest(_In_ PHTTP_CONTEXT Context, _Inout_ PHTTP_REQUEST Request);
static EFI_STATUS EFIAPI HttpGetResponse(_Inout_ PHTTP_CONTEXT Context,
                                         _Inout_ PHTTP_RESPONSE Response);
static EFI_STATUS EFIAPI HttpPoll(_In_ PHTTP_CONTEXT Context,
                                  _In_ BOOLEAN* StateVariable,
                                  _In_ UINTN TimeoutInNs);
static VOID EFIAPI HttpDumpHeaders(_In_ EFI_HTTP_MESSAGE* Message);
static EFI_STATUS EFIAPI HttpReadHeaders(_In_ PHTTP_RESPONSE Response);
static EFI_STATUS EFIAPI HttpInit(_In_ PHTTP_CONTEXT Context);

//
// Interfaces
//

static EFI_STATUS EFIAPI HttpPoll(_In_ PHTTP_CONTEXT Context,
                                  _In_ BOOLEAN* StateVariable,
                                  _In_ UINTN TimeoutInNs)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_EVENT TimerEvent = NULL;

    Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CreateEvent() failed 0x%zx", Status);
        goto Exit;
    }

    Status = gBS->SetTimer(TimerEvent, TimerRelative, TimeoutInNs);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("SetTimer() failed 0x%zx", Status);
        goto Exit;
    }

    while (*StateVariable == FALSE && gBS->CheckEvent(TimerEvent) == EFI_NOT_READY) {
        Status = Context->Http->Poll(Context->Http);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Poll() failed 0x%zx", Status);
            goto Exit;
        }
    }

Exit:
    //
    // Time lapsed and state variable is still not set
    //

    if (gBS->CheckEvent(TimerEvent) == EFI_SUCCESS && *StateVariable == FALSE) {
        Status = EFI_TIMEOUT;
    }

    if (TimerEvent != NULL) {
        gBS->SetTimer(TimerEvent, TimerCancel, 0);
        gBS->CloseEvent(TimerEvent);
    }

    return Status;
}

static VOID EFIAPI HttpDumpHeaders(_In_ EFI_HTTP_MESSAGE* Message)
{
    if (Message->HeaderCount == 0)
        return;

    DBG_INFO("HTTP Headers:", NULL);
    for (UINTN Index = 0; Index < Message->HeaderCount; Index++) {
        DBG_INFO("     %s: %s",
                 Message->Headers[Index].FieldName,
                 Message->Headers[Index].FieldValue);
    }
}

static EFI_STATUS EFIAPI HttpReadHeaders(_In_ PHTTP_RESPONSE Response)
{
    EFI_STATUS Status = EFI_SUCCESS;

    HttpDumpHeaders(&Response->Message);
    if (Response->ContentLength == 0) {
        for (UINTN Index = 0; Index < Response->Message.HeaderCount; Index++) {
            if (AsciiStrCmp((CHAR8*)Response->Message.Headers[Index].FieldName,
                            (CHAR8*)HTTP_HEADER_CONTENT_LENGTH) == 0) {
                Response->ContentLength = (ULONG)AsciiStrDecimalToUintn(
                    (CHAR8*)Response->Message.Headers[Index].FieldValue);
            }
        }
    }

    return Status;
}

static EFI_STATUS EFIAPI HttpInit(_In_ PHTTP_CONTEXT Context)
{
    EFI_STATUS Status = EFI_SUCCESS;
    // EFI_HANDLE* DeviceHandles = NULL;
    // UINTN HandleCount = 0;

    EFI_SERVICE_BINDING_PROTOCOL* ServiceBinding = NULL;
    EFI_HANDLE Handle = NULL;
    EFI_HTTP_PROTOCOL* HttpProtocol = NULL;

#if 0
    //
    // Locate all DHCP SB handles
    //

    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiHttpServiceBindingProtocolGuid,
                                     NULL, // No SearchKey for this search type.
                                     &HandleCount,
                                     &DeviceHandles);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to locate DHCP4 service binding protocol handles, 0x%zx", Status);
        goto Exit;
    }

    //
    // Locate DHCP SB handles with HTTP protocol
    //

    for (UINTN Index = 0; Index < HandleCount; Index++) {
        Status = ServiceBindingOpenProtocol(DeviceHandles[Index],
                                            &gEfiHttpServiceBindingProtocolGuid,
                                            &gEfiHttpProtocolGuid,
                                            &Context->HttpSvcBindingProtocol,
                                            &Context->HttpHandle,
                                            (PVOID*)&Context->Http);
        if (!EFI_ERROR(Status)) {
            Context->ParentHandle = DeviceHandles[Index];
            break;
        }
    }
#endif

    Status = gBS->LocateProtocol(&gEfiHttpServiceBindingProtocolGuid, NULL, (void**)&ServiceBinding);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Error 0x%zx", Status);
        goto Exit;
    }

    Status = ServiceBinding->CreateChild(ServiceBinding, &Handle);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Error 0x%zx", Status);
        goto Exit;
    }

    Status = gBS->OpenProtocol(Handle,
                               &gEfiHttpProtocolGuid,
                               (void**)&HttpProtocol,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Error 0x%zx", Status);
        goto Exit;
    }

    Context->HttpSvcBindingProtocol = ServiceBinding;
    Context->HttpHandle = Handle;
    Context->Http = HttpProtocol;

Exit:
    // gBS->FreePool(DeviceHandles);
    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_HTTP_INITIALIZATION_FAILED);
    }

    return Status;
}

UINTN EFIAPI HttpGetContentLength(_In_ PHTTP_RESPONSE Response)
{
    return Response->ContentLength;
}

UINTN EFIAPI HttpGetChunkSize(_In_ PHTTP_RESPONSE Response)
{
    return Response->Message.BodyLength;
}

UINT8* EFIAPI HttpGetChunk(_In_ PHTTP_RESPONSE Response)
{
    return Response->Message.Body;
}

EFI_STATUS EFIAPI HttpCreate(_Outptr_ HTTP_CONTEXT** Context)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HTTP_CONTEXT* RetContext = NULL;

    if (Context == NULL) {
        DBG_ERROR("Context is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    // Initialize HTTP context
    RetContext = AllocateZeroPool(sizeof(HTTP_CONTEXT));
    if (RetContext == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DBG_ERROR("Unable to allocate HTTP_CONTEXT structure", NULL);
        goto Exit;
    }

    Status = HttpInit(RetContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpInit() failed : 0x%zx", Status);
        goto Exit;
    }

#if 0
    Status = DhcpInit(RetContext->ParentHandle, &RetContext->Dhcp4Context);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("DhcpInit() failed : 0x%zx", Status);
        goto Exit;
    }
#endif
    Status = HttpConfigure(RetContext, FALSE);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpConfigure() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Configured Http module", NULL);

    *Context = RetContext;
    return Status;

Exit:
    HttpFree(RetContext);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_HTTP_INSTANCE_CREATION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI HttpConfigure(_Inout_ PHTTP_CONTEXT Context, _In_ BOOLEAN ResetFirst)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_HTTP_CONFIG_DATA HttpConfig = {0};
    EFI_HTTPv4_ACCESS_POINT IPv4Node = {0};

#if 0
    // TODO: Add check if DHCP already started, to avoid reentering this path
    // every time.
    Status = DhcpStart(Context->Dhcp4Context);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("DhcpStart() failed : 0x%zx", Status);
        goto Exit;
    }
#endif

    if (ResetFirst) {
        Status = Context->Http->Configure(Context->Http, NULL);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HTTP Configure() to reset failed : 0x%zx", Status);
            goto Exit;
        }
    }

    // Configure Http
    HttpConfig.HttpVersion = HttpVersion11;
    HttpConfig.TimeOutMillisec = 0;
    HttpConfig.LocalAddressIsIPv6 = FALSE;
    ZeroMem(&IPv4Node, sizeof(IPv4Node));
    IPv4Node.UseDefaultAddress = TRUE;
    HttpConfig.AccessPoint.IPv4Node = &IPv4Node;

    Status = Context->Http->Configure(Context->Http, &HttpConfig);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HTTP Configure() failed : 0x%zx", Status);
        goto Exit;
    }

Exit:

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_HTTP_CONFIGURE_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI HttpFree(_In_ PHTTP_CONTEXT Context)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Context == NULL) {
        goto Exit;
    }

    if (Context->HttpHandle != NULL) {
        Status = gBS->CloseProtocol(Context->HttpHandle, &gEfiHttpProtocolGuid, gImageHandle, NULL);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CloseProtocol() failed : 0x%zx", Status);
            goto Exit;
        }

        Status = Context->HttpSvcBindingProtocol->DestroyChild(Context->HttpSvcBindingProtocol,
                                                               Context->HttpHandle);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("DestroyChild() failed : 0x%zx", Status);
            goto Exit;
        }
    }

    FreePool(Context);

Exit:
    return Status;
}

static EFI_STATUS EFIAPI HttpSendRequest(_In_ PHTTP_CONTEXT Context, _Inout_ PHTTP_REQUEST Request)
{
    EFI_STATUS Status = EFI_SUCCESS;

    DBG_INFO_U(L"HTTP request url: %s", Request->Url);

    Request->CallbackTriggered = FALSE;

    //
    // Send request
    //

    Status = Context->Http->Request(Context->Http, &Request->Token);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Request() failed 0x%zx Token Status = 0x%zx", Status, Request->Token.Status);
        goto Exit;
    }

    //
    // Poll for the request to complete
    //

    Status = HttpPoll(Context, &Request->CallbackTriggered, HTTP_REQUEST_WAIT_TIMEOUT);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpPoll() failed 0x%zx", Status);
        if (!Request->CallbackTriggered) {
            DBG_INFO("Cancelling the request", NULL);
            Status = Context->Http->Cancel(Context->Http, &Request->Token);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Cancel() failed 0x%zx Token Status = 0x%zx",
                          Status,
                          Request->Token.Status);
            }
        }
        goto Exit;
    }

Exit:
    return Status;
}

static EFI_STATUS EFIAPI HttpGetResponse(_Inout_ PHTTP_CONTEXT Context,
                                         _Inout_ PHTTP_RESPONSE Response)
{
    EFI_STATUS Status = EFI_SUCCESS;

    //
    // Get response
    //

    Response->CallbackTriggered = FALSE;

    Status = Context->Http->Response(Context->Http, &Response->Token);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Response() failed 0x%zx Token Status = 0x%zx",
                  Status,
                  Response->Token.Status);
        goto Exit;
    }

    //
    // Poll for the response
    //

    Status = HttpPoll(Context, &Response->CallbackTriggered, HTTP_RESPONSE_WAIT_TIMEOUT);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpPoll() failed 0x%zx", Status);
        if (!Response->CallbackTriggered) {
            DBG_INFO("Cancelling the response", NULL);
            Status = Context->Http->Cancel(Context->Http, &Response->Token);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Cancel() failed 0x%zx Token Status = 0x%zx",
                          Status,
                          Response->Token.Status);
            }
        }
        goto Exit;
    }

    // DBG_INFO("HTTP status: %s(%d)",
    //              HttpStatusMap[Response->Data.StatusCode].String,
    //              HttpStatusMap[Response->Data.StatusCode].Value);

    Response->ContentDownloaded += Response->Message.BodyLength;

    //
    // If caller specified TotalExpectedContentLength, prioritize checking
    // this value. This check is another defense-in-depth measure to prevent
    // us from downloading malicious content.
    //

    if (Response->TotalExpectedContentLength != 0 &&
        Response->ContentDownloaded > Response->TotalExpectedContentLength) {
        DBG_ERROR("Received unexpected number of bytes %zu. Expected %zu",
                  Response->ContentDownloaded,
                  Response->TotalExpectedContentLength);
        Status = EFI_ABORTED;
        goto Exit;
    }

    // DBG_INFO("HTTP Current Chunk Length %zu bytes", Response->Message.BodyLength);
    // DBG_INFO("HTTP Total Content Downloaded %zu bytes", Response->ContentDownloaded);

    HttpReadHeaders(Response);

    // DBG_INFO("HTTP Response->ContentLength %zu bytes", Response->ContentLength);

    FreePool(Response->Message.Headers);

    if (Response->ContentDownloaded == Response->ContentLength) {
        // Cancel any remaining http transfers
    }

Exit:
    return Status;
}

static EFI_STATUS HttpCreateRequestObject(_Inout_ PHTTP_CONTEXT Context,
                                          _In_ PWSTR Url,
                                          _In_ EFI_HTTP_METHOD Method,
                                          _In_ EFI_HTTP_HEADER* Headers,
                                          _In_ UINTN HeaderCount,
                                          _In_opt_ VOID* Body,
                                          _In_ UINTN BodyLength,
                                          _Outptr_ PHTTP_REQUEST* Request)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PHTTP_REQUEST RetRequest = NULL;

    UNREFERENCED_PARAMETER(Context);

    if (Url == NULL) {
        DBG_ERROR("Url is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    RetRequest = AllocateZeroPool(sizeof(HTTP_REQUEST));
    if (RetRequest == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DBG_ERROR("AllocatePool() failed to allocate HTTP_REQUEST", NULL);
        goto Exit;
    }

    RetRequest->Data.Method = Method;
    RetRequest->Data.Url = Url;

    RetRequest->Message.Data.Request = &RetRequest->Data;
    RetRequest->Message.HeaderCount = HeaderCount;
    RetRequest->Message.Headers = Headers;
    RetRequest->Message.BodyLength = BodyLength;
    RetRequest->Message.Body = Body;

    RetRequest->Token.Message = &RetRequest->Message;
    RetRequest->Token.Event = NULL;
    RetRequest->Token.Status = EFI_SUCCESS;

    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              HttpRequestCallback,
                              RetRequest,
                              &RetRequest->Token.Event);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CreateEvent() failed 0x%zx", Status);
        goto Exit;
    }

    RetRequest->Url = Url;

    *Request = RetRequest;
    return Status;

Exit:
    if (RetRequest && RetRequest->Token.Event) {
        gBS->CloseEvent(RetRequest->Token.Event);
    }

    //
    // Let caller free the HTTP headers
    //

    FreePool(RetRequest);

    return Status;
}

static EFI_STATUS HttpCreateResponseObject(_Inout_ PHTTP_CONTEXT Context,
                                           _In_ EFI_HTTP_METHOD Method,
                                           _Outptr_ PHTTP_RESPONSE* Response)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PHTTP_RESPONSE RetResponse = NULL;

    UNREFERENCED_PARAMETER(Context);

    RetResponse = AllocateZeroPool(sizeof(HTTP_RESPONSE));
    if (RetResponse == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DBG_ERROR("AllocatePool() failed to allocate HTTP_RESPONSE", NULL);
        goto Exit;
    }

    if (Method == HttpMethodHead) {
        RetResponse->Message.BodyLength = 0;
        RetResponse->Message.Body = NULL;
    } else {
        RetResponse->Message.BodyLength = HTTP_DEFAULT_RESPONSE_BUFFER_SIZE;
        RetResponse->Message.Body = AllocateZeroPool(RetResponse->Message.BodyLength);
        if (RetResponse->Message.Body == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            DBG_ERROR("AllocatePool() failed to allocate %zu bytes for http response",
                      RetResponse->Message.BodyLength);
            goto Exit;
        }
    }

    RetResponse->Data.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    RetResponse->Message.Data.Response = &RetResponse->Data;
    RetResponse->Message.HeaderCount = 0;
    RetResponse->Message.Headers = NULL;

    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              HttpResponseCallback,
                              RetResponse,
                              &RetResponse->Token.Event);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CreateEvent() failed 0x%zx", Status);
        goto Exit;
    }

    RetResponse->Token.Status = EFI_SUCCESS;
    RetResponse->Token.Message = &RetResponse->Message;

    *Response = RetResponse;
    return Status;

Exit:
    if (RetResponse) {
        FreePool(RetResponse->Message.Body);
        if (RetResponse->Token.Event) {
            gBS->CloseEvent(RetResponse->Token.Event);
        }
    }

    FreePool(RetResponse);

    return Status;
}

EFI_STATUS EFIAPI HttpIssueRequest(_In_ PHTTP_CONTEXT Context,
                                   _In_reads_z_(UrlLength) PWSTR Url,
                                   _In_ UINTN UrlLength,
                                   _In_ EFI_HTTP_METHOD Method,
                                   _In_ EFI_HTTP_HEADER* Headers,
                                   _In_ UINTN HeaderCount,
                                   _In_opt_ VOID* Body,
                                   _In_ UINTN BodyLength,
                                   _In_ UINTN TotalExpectedContentLength,
                                   _Outptr_ PHTTP_RESPONSE* Response)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PHTTP_REQUEST Request = NULL;
    PHTTP_RESPONSE RetResponse = NULL;
    CHAR16* HttpsUrl = NULL;

    if (Response == NULL || Url == NULL) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("Invalid parameters 0x%zx", Status);
        goto Exit;
    }

    if (gCbmrConfig.ForceHttps == TRUE) {
        if (StrStr(Url, L"http:") != NULL) {
            HttpsUrl = AllocateZeroPool(sizeof(CHAR16) * (UrlLength + 10));
            StrnCpy(HttpsUrl, L"https:", _countof(L"https:"));
            StrnCpy(HttpsUrl + 6, Url + 5, UrlLength - 5);
            Url = HttpsUrl;
            DBG_INFO_U(L"Patched outgoing url to be https: %s", Url);
        }
    }

    //
    // Due to what is seemingly a bug in UEFI Http implementation, we need to manually
    // reset and reconfigure HTTP instance whenever previous URL is different,
    // apparently. Otherwise, the EFI_HTT_PROTOCOL->Request() call fails with
    // EFI_ACCESS_DENIED.
    //

    Status = HttpConfigure(Context, TRUE);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpConfigure() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Create Request Object
    //

    Status = HttpCreateRequestObject(Context,
                                     Url,
                                     Method,
                                     Headers,
                                     HeaderCount,
                                     Body,
                                     BodyLength,
                                     &Request);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpCreateRequestObject() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Send Request
    //

    Status = HttpSendRequest(Context, Request);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpSendRequest() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Create Response Object
    //

    Status = HttpCreateResponseObject(Context, Method, &RetResponse);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpCreateResponseObject() failed 0x%zx", Status);
        goto Exit;
    }

    RetResponse->Request = Request;
    RetResponse->TotalExpectedContentLength = TotalExpectedContentLength;
    RetResponse->Data.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;
    RetResponse->Message.HeaderCount = 0;

    //
    // Get Response
    //

    Status = HttpGetResponse(Context, RetResponse);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpGetResponse() failed 0x%zx", Status);
        goto Exit;
    }

    *Response = RetResponse;

Exit:
    FreePool(HttpsUrl);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_HTTP_REQUEST_ISSUE_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI HttpGetNext(_In_ PHTTP_CONTEXT Context, _In_ PHTTP_RESPONSE Response)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Response == NULL) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("Invalid parameters 0x%zx", Status);
        goto Exit;
    }

    // DBG_INFO("HTTP ContentDownloaded %zu bytes", Response->ContentDownloaded);
    // DBG_INFO("HTTP ContentLength %zu bytes", Response->ContentLength);

    if (Response->ContentDownloaded >= Response->ContentLength) {
        if (Response->ContentDownloaded > Response->ContentLength) {
            DBG_ERROR("Received unexpected number of bytes %zu. Expected ContentLength %zu",
                      Response->ContentDownloaded,
                      Response->ContentLength);

            Status = EFI_ABORTED;
            goto Exit;
        }

        Status = EFI_END_OF_FILE;
        goto Exit;
    }

    //
    // UEFI Spec: ..This allows the client to download a large file in chunks
    // instead of into one contiguous block of memory. Similar to HTTP request,
    // if Body is not NULL and BodyLength is non-zero and all other fields are
    // NULL or 0, the HTTP driver will queue a receive token to underlying TCP
    // instance. If data arrives in the receive buffer, up to BodyLength bytes
    // of data will be copied to Body. The HTTP driver will then update
    // BodyLength with the amount of bytes received and copied to Body.
    //
    // Hence setting below fields to zero
    //

    Response->Message.HeaderCount = 0;
    Response->Message.Headers = NULL;
    Response->Message.Data.Response = NULL;
    Response->Message.BodyLength = HTTP_DEFAULT_RESPONSE_BUFFER_SIZE;
    Status = HttpGetResponse(Context, Response);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpGetResponse() failed 0x%zx", Status);
        goto Exit;
    }

Exit:

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_HTTP_UNABLE_TO_READ_RESPONSE);
    }

    return Status;
}

static VOID HttpFreeRequest(_In_ PHTTP_CONTEXT Context, _In_ PHTTP_REQUEST Request)
{
    //
    // Cancel any pending transfers
    //

    Context->Http->Cancel(Context->Http, &Request->Token);

    gBS->CloseEvent(Request->Token.Event);

    FreePool(Request);
}

VOID EFIAPI HttpFreeResponse(_In_ PHTTP_CONTEXT Context, _In_ PHTTP_RESPONSE Response)
{
    if (Response == NULL) {
        return;
    }

    HttpFreeRequest(Context, Response->Request);

    //
    // Cancel any pending transfers
    //

    Context->Http->Cancel(Context->Http, &Response->Token);

    gBS->CloseEvent(Response->Token.Event);
    FreePool(Response->Message.Body);
    FreePool(Response);
}

//
// Local functions
//

static VOID EFIAPI HttpRequestCallback(IN EFI_EVENT Event, IN VOID* Context)
{
    UNREFERENCED_PARAMETER(Event);

    HTTP_REQUEST* Request = (HTTP_REQUEST*)Context;
    Request->CallbackTriggered = TRUE;
}

static VOID EFIAPI HttpResponseCallback(IN EFI_EVENT Event, IN VOID* Context)
{
    UNREFERENCED_PARAMETER(Event);

    HTTP_RESPONSE* Response = (HTTP_RESPONSE*)Context;
    Response->CallbackTriggered = TRUE;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif