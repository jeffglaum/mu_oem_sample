#ifndef _HTTP_H_
#define _HTTP_H_

#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTP_HEADER_CONTENT_TYPE   "Content-Type"
#define HTTP_HEADER_HOST           "Host"
#define HTTP_HEADER_USER_AGENT     "User-Agent"
#define HTTP_HEADER_ACCEPT         "Accept"

typedef struct _HTTP_CONTEXT HTTP_CONTEXT, *PHTTP_CONTEXT;
typedef struct _HTTP_RESPONSE HTTP_RESPONSE, *PHTTP_RESPONSE;

EFI_STATUS EFIAPI HttpCreate(_Outptr_ HTTP_CONTEXT** Context);
EFI_STATUS EFIAPI HttpConfigure(_Inout_ PHTTP_CONTEXT Context, _In_ BOOLEAN ResetFirst);
EFI_STATUS EFIAPI HttpIssueRequest(_In_ PHTTP_CONTEXT Context,
                                   _In_z_ PWSTR Url,
                                   _In_ UINTN UrlLength,
                                   _In_ EFI_HTTP_METHOD Method,
                                   _In_ EFI_HTTP_HEADER* Headers,
                                   _In_ UINTN HeaderCount,
                                   _In_opt_ VOID* Body,
                                   _In_ UINTN BodySize,
                                   _In_ UINTN TotalExpectedContentLength,
                                   _Outptr_ PHTTP_RESPONSE* Response);
EFI_STATUS EFIAPI HttpGetNext(_In_ PHTTP_CONTEXT Context, _In_ PHTTP_RESPONSE Response);
EFI_STATUS EFIAPI HttpFree(_In_ PHTTP_CONTEXT Context);
VOID EFIAPI HttpFreeHeaderFields(_In_ EFI_HTTP_HEADER* HeaderFields, _In_ UINTN FieldCount);
VOID EFIAPI HttpFreeResponse(_In_ PHTTP_CONTEXT Context, _In_ PHTTP_RESPONSE Response);
UINTN EFIAPI HttpGetContentLength(_In_ PHTTP_RESPONSE Response);
UINTN EFIAPI HttpGetChunkSize(_In_ PHTTP_RESPONSE Response);
UINT8* EFIAPI HttpGetChunk(_In_ PHTTP_RESPONSE Response);
#endif // _HTTP_H_