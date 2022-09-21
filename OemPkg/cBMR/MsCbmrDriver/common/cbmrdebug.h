#ifndef _DEBUG_H_
#define _DEBUG_H_

typedef enum _DEBUG_FLAGS {
    FLAG_DEBUG_ERROR,
    FLAG_DEBUG_WARNING,
    FLAG_DEBUG_INFO,
    FLAG_DEBUG_VERBOSE,

    FLAG_DEBUG_MAX,
} DEBUG_FLAGS;

EFI_STATUS EFIAPI DebugInit(CHAR8* ModuleName);
VOID EFIAPI DebugPrintFormatted(_In_opt_ DEBUG_FLAGS DebugFlag,
                                _In_opt_ CHAR8* Function,
                                _In_opt_ UINTN Line,
                                _In_z_ _Printf_format_string_ CHAR8* Fmt,
                                ...);
VOID EFIAPI DebugPrintFormattedU(_In_opt_ DEBUG_FLAGS DebugFlag,
                                 _In_opt_ CHAR16* Function,
                                 _In_opt_ UINTN Line,
                                 _In_z_ _Printf_format_string_ CHAR16* FmtW,
                                 ...);
VOID EFIAPI DebugClose();

// clang-format off

#ifndef UEFI_BUILD_SYSTEM
#define DBG_ERROR(Format, ...)            DebugPrintFormatted(FLAG_DEBUG_ERROR,       (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_WARNING(Format, ...)          DebugPrintFormatted(FLAG_DEBUG_WARNING,     (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_INFO(Format, ...)             DebugPrintFormatted(FLAG_DEBUG_INFO,        (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_VERBOSE(Format, ...)          DebugPrintFormatted(FLAG_DEBUG_VERBOSE,     (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)

#define DBG_ERROR_U(Format, ...)          DebugPrintFormattedU(FLAG_DEBUG_ERROR,      (CHAR16 *)__FUNCTIONW__,__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_WARNING_U(Format, ...)        DebugPrintFormattedU(FLAG_DEBUG_WARNING,    (CHAR16 *)__FUNCTIONW__,__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_INFO_U(Format, ...)           DebugPrintFormattedU(FLAG_DEBUG_INFO,       (CHAR16 *)__FUNCTIONW__,__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_VERBOSE_U(Format, ...)        DebugPrintFormattedU(FLAG_DEBUG_VERBOSE,    (CHAR16 *)__FUNCTIONW__,__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)

#define DBG_ERROR_RAW(Format, ...)        DebugPrintFormatted(FLAG_DEBUG_ERROR,       NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_WARNING_RAW(Format, ...)      DebugPrintFormatted(FLAG_DEBUG_WARNING,     NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_INFO_RAW(Format, ...)         DebugPrintFormatted(FLAG_DEBUG_INFO,        NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_VERBOSE_RAW(Format, ...)      DebugPrintFormatted(FLAG_DEBUG_VERBOSE,     NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)

#define DBG_ERROR_RAW_U(Format, ...)      DebugPrintFormattedU(FLAG_DEBUG_ERROR,      NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_WARNING_RAW_U(Format, ...)    DebugPrintFormattedU(FLAG_DEBUG_WARNING,    NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_INFO_RAW_U(Format, ...)       DebugPrintFormattedU(FLAG_DEBUG_INFO,       NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_VERBOSE_RAW_U(Format, ...)    DebugPrintFormattedU(FLAG_DEBUG_VERBOSE,    NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)

#else
#define DBG_ERROR(Format, ...)            DebugPrintFormatted(FLAG_DEBUG_ERROR,       (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_WARNING(Format, ...)          DebugPrintFormatted(FLAG_DEBUG_WARNING,     (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_INFO(Format, ...)             DebugPrintFormatted(FLAG_DEBUG_INFO,        (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)
#define DBG_VERBOSE(Format, ...)          DebugPrintFormatted(FLAG_DEBUG_VERBOSE,     (CHAR8 *)__FUNCTION__, __LINE__,  (CHAR8 *)Format  "\r\n",  __VA_ARGS__)

#define DBG_ERROR_U(Format, ...)          DebugPrintFormattedU(FLAG_DEBUG_ERROR,      (CHAR16 *)L"__FUNCTIONW__",__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_WARNING_U(Format, ...)        DebugPrintFormattedU(FLAG_DEBUG_WARNING,    (CHAR16 *)L"__FUNCTIONW__",__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_INFO_U(Format, ...)           DebugPrintFormattedU(FLAG_DEBUG_INFO,       (CHAR16 *)L"__FUNCTIONW__",__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)
#define DBG_VERBOSE_U(Format, ...)        DebugPrintFormattedU(FLAG_DEBUG_VERBOSE,    (CHAR16 *)L"__FUNCTIONW__",__LINE__,  (CHAR16 *)Format  L"\r\n", __VA_ARGS__)

#define DBG_ERROR_RAW(Format, ...)        DebugPrintFormatted(FLAG_DEBUG_ERROR,       NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_WARNING_RAW(Format, ...)      DebugPrintFormatted(FLAG_DEBUG_WARNING,     NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_INFO_RAW(Format, ...)         DebugPrintFormatted(FLAG_DEBUG_INFO,        NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)
#define DBG_VERBOSE_RAW(Format, ...)      DebugPrintFormatted(FLAG_DEBUG_VERBOSE,     NULL,         UINTN_MAX, (CHAR8 *)Format,          __VA_ARGS__)

#define DBG_ERROR_RAW_U(Format, ...)      DebugPrintFormattedU(FLAG_DEBUG_ERROR,      NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_WARNING_RAW_U(Format, ...)    DebugPrintFormattedU(FLAG_DEBUG_WARNING,    NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_INFO_RAW_U(Format, ...)       DebugPrintFormattedU(FLAG_DEBUG_INFO,       NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#define DBG_VERBOSE_RAW_U(Format, ...)    DebugPrintFormattedU(FLAG_DEBUG_VERBOSE,    NULL,         UINTN_MAX, (CHAR16 *)Format,          __VA_ARGS__)
#endif

#define DBG_CMD_RAW_U(str) gST->ConOut->OutputString(gST->ConOut, str);

// clang-format on

#endif // _DEBUG_H_
