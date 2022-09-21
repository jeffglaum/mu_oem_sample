//
// Global includes
//
#include "cbmrincludes.h"
#include "cbmr_config.h"
#include "file.h"

#ifndef UEFI_BUILD_SYSTEM
#ifdef DEBUGMODE
#include "windbgserver.h"
#endif
#endif

#define WRITE_TIMEOUT             1000
#define LOG_DESTINATION_DIRECTORY L"\\cbmr\\logs"

//
// Variables
//
static EFI_DEBUGPORT* gEfiDebugPortProtocol = NULL;
static EFI_FILE_PROTOCOL* gsDebugFile = NULL;
static CHAR8 gsModuleName[20];
static CHAR16 gsModuleNameWide[20];
static CHAR16 gsFileName[256];
static UINTN gsMaxUefiVariableSize = 10 * 1024; // 10K
static CHAR8* gsUefiVariable = NULL;
static UINTN gsUefiVariableIndex = 0;

#ifndef UEFI_BUILD_SYSTEM
#ifdef DEBUGMODE
static EFI_MS_WINDBG_SERVER_PROTOCOL* gEfiMsWindbgServerProtocol = NULL;
#endif
#endif

//
// Prototypes
//
static VOID DebugWriteToSerialPort(_In_z_ CHAR8* Buffer, _In_ UINTN BufferLength);
static VOID DebugWriteToFile(_In_z_ CHAR8* Buffer, _In_ UINTN BufferLength);
static VOID DebugWrite(_In_z_ CHAR8* Str, _In_ UINTN BufferLength);
static EFI_STATUS DebugOpenFile();
static EFI_STATUS DebugInitBootlibrary();
static BOOLEAN IsDebugFlagEnabled(DEBUG_FLAGS DebugFlag);
static CHAR8* GetDebugFlagStr(DEBUG_FLAGS DebugFlag);
static CHAR16* GetDebugFlagStrU(DEBUG_FLAGS DebugFlag);

//
// Interfaces
//

EFI_STATUS EFIAPI DebugInit(CHAR8* ModuleName)
{
    EFI_STATUS Status = EFI_SUCCESS;

    AsciiStrnCpy(gsModuleName, ModuleName, AsciiStrnLenS(ModuleName, _countof(gsModuleName) - 1));
    AsciiStrToUnicodeStr(gsModuleName, gsModuleNameWide);

#ifndef UEFI_BUILD_SYSTEM
#ifdef DEBUGMODE
    if (gCbmrConfig.SpewTarget & SPEW_DEBUGGER) {
        Status = gBS->LocateProtocol(&(EFI_GUID)EFI_MS_WINDBG_SERVER_PROTOCOL_GUID,
                                     NULL,
                                     (VOID**)&gEfiMsWindbgServerProtocol);
        if (EFI_ERROR(Status)) {
            Status = EFI_SUCCESS;
            gEfiMsWindbgServerProtocol = NULL;
        } else {
            //
            // Dump .reload command
            //

            gEfiMsWindbgServerProtocol->DumpImageInfo(gEfiMsWindbgServerProtocol, gImageHandle);
        }
    }
#endif
#endif

    if ((gCbmrConfig.SpewTarget & SPEW_FILE) == SPEW_FILE && gsDebugFile == NULL) {
        Status = DebugOpenFile();
        if (EFI_ERROR(Status)) {
            gsDebugFile = NULL;
            gST->ConOut->OutputString(gST->ConOut,
                                      L"Failed to open/create debug.log file");
            return Status;
        }
    }

    if (gCbmrConfig.SpewTarget & SPEW_UEFI_VAR) {
        gsUefiVariable = AllocateZeroPool(gsMaxUefiVariableSize); // Ignore failure
    }

    if (gCbmrConfig.SpewTarget & SPEW_SERIAL) {
        Status = gBS->LocateProtocol(&gEfiDebugPortProtocolGuid,
                                     NULL,
                                     (VOID**)&gEfiDebugPortProtocol);
        if (EFI_ERROR(Status)) {
            gEfiDebugPortProtocol = NULL;
            return Status;
        }

        gEfiDebugPortProtocol->Reset(gEfiDebugPortProtocol);
    }

    return Status;
}

VOID EFIAPI DebugPrintFormatted(_In_opt_ DEBUG_FLAGS DebugFlag,
                                _In_opt_ CHAR8* Function,
                                _In_opt_ UINTN Line,
                                _In_z_ _Printf_format_string_ CHAR8* Fmt,
                                ...)
{
    CHAR8 Buffer[512];
    UINTN PrefixLength = 0;
    UINTN Remaining = 0;
    va_list ArgList;

    if (!IsDebugFlagEnabled(DebugFlag))
        return;

    if (Function != NULL && Line != UINTN_MAX) { // Normal prints. Non XXXX_RAW macros
        StringPrintfA(Buffer,
                      _countof(Buffer),
#ifdef UEFI_BUILD_SYSTEM
                      "[%a] %a %-4u %-25a | ",
#else
                      (CHAR8*)"[%s] %s %-4zu %-25s | ",
#endif
                      gsModuleName,
                      GetDebugFlagStr(DebugFlag),
                      Line,
                      Function);
        PrefixLength = AsciiStrnLenS(Buffer, _countof(Buffer));
    }

    Remaining = _countof(Buffer) - PrefixLength;

    va_start(ArgList, Fmt);
    StringVPrintfA(&Buffer[PrefixLength], Remaining, (CHAR8*)Fmt, ArgList);
    va_end(ArgList);

    if (Buffer) {
        DebugWrite(Buffer, _countof(Buffer));
    }
}

VOID EFIAPI DebugPrintFormattedU(_In_opt_ DEBUG_FLAGS DebugFlag,
                                 _In_opt_ CHAR16* Function,
                                 _In_opt_ UINTN Line,
                                 _In_z_ _Printf_format_string_ CHAR16* FmtW,
                                 ...)
{
    CHAR16 BufferWide[512];
    UINTN PrefixLength = 0;
    UINTN Remaining = 0;
    va_list ArgList;

    if (!IsDebugFlagEnabled(DebugFlag))
        return;

    if (Function != NULL && Line != UINTN_MAX) { // Normal prints. Non XXXX_RAW macros
        StringPrintfW(BufferWide,
                      _countof(BufferWide),
#ifdef UEFI_BUILD_SYSTEM
                      L"[%s] %s %-4u %-25s | ",
#else
                      (CHAR16*)L"[%s] %s %-4zu %-25s | ",
#endif
                      gsModuleNameWide,
                      GetDebugFlagStrU(DebugFlag),
                      Line,
                      Function);
        PrefixLength = StrnLenS(BufferWide, _countof(BufferWide));
    }

    Remaining = _countof(BufferWide) - PrefixLength;

    va_start(ArgList, FmtW);
    StringVPrintfW(&BufferWide[PrefixLength], Remaining, (CHAR16*)FmtW, ArgList);
    va_end(ArgList);

    if (BufferWide) {
        CHAR8 Buffer[1024];
        UnicodeStrToAsciiStr(BufferWide, Buffer);
        DebugWrite(Buffer, _countof(Buffer));
    }
}

static EFI_STATUS DebugCopyLogsToRamdisk()
{
#define STUBOS_VOLUME_LABEL L"STUBOS"
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* Source = NULL;
    EFI_FILE_PROTOCOL* Dest = NULL;

    Status = FileLocateAndOpen(gsFileName, EFI_FILE_MODE_READ, &Source);
    if (EFI_ERROR(Status)) {
        DBG_ERROR_U(L"FileLocateAndOpen() failed. Unable to locate %s 0x%zx", gsFileName, Status);
        Status = EFI_SUCCESS;
        goto Exit;
    }

    Status = FileCreateSubdirectories(STUBOS_VOLUME_LABEL,
                                      LOG_DESTINATION_DIRECTORY,
                                      _countof(LOG_DESTINATION_DIRECTORY),
                                      &Dest);
    if (EFI_ERROR(Status)) {
        DBG_ERROR_U(L"FileCreateSubdirectories() failed for %s with status 0x%zx",
                    LOG_DESTINATION_DIRECTORY,
                    Status);
        goto Exit;
    }

    Status = FileCopy(Source, Dest);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCopy() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    if (Source != NULL) {
        Source->Close(Source);
    }

    if (Dest != NULL) {
        Dest->Close(Dest);
    }

    return Status;
}

VOID EFIAPI DebugClose()
{
    if (gCbmrConfig.SpewTarget & SPEW_FILE) {
        if (gsDebugFile) {
            gsDebugFile->Flush(gsDebugFile);
            gsDebugFile->Close(gsDebugFile);
            gsDebugFile = NULL;
        }

        DebugCopyLogsToRamdisk();
    }

    FreePool(gsUefiVariable);
    gsUefiVariable = NULL;
}

//
// Local functions
//

static EFI_STATUS DebugOpenFile()
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystem = NULL;
    EFI_FILE_PROTOCOL* Root = NULL;
    EFI_LOADED_IMAGE* LoadedImage = NULL;
    EFI_TIME EfiTime = {0};

    Status = gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Open SIMPLE_FILE_SYSTEM_PROTOCOL for the volume from which the
    // current image is loaded
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                                 &gEfiSimpleFileSystemProtocolGuid,
                                 (void**)&SimpleFileSystem);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    gRT->GetTime(&EfiTime, NULL);
    StringPrintfW(gsFileName,
                  _countof(gsFileName),
                  (CHAR16*)L"%s_%02u%02u%02u_%02u%02u%02u.log",
                  gsModuleNameWide,
                  (UINT16)(EfiTime.Year % 100),
                  EfiTime.Month,
                  EfiTime.Day,
                  EfiTime.Hour,
                  EfiTime.Minute,
                  EfiTime.Second);

    Status = Root->Open(Root,
                        &gsDebugFile,
                        gsFileName,
                        EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                        0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

static VOID DebugWriteToSerialPort(_In_z_ CHAR8* Buffer, _In_ UINTN BufferLength)
{
    UINTN Length;
    EFI_STATUS Status;

    if (gEfiDebugPortProtocol == NULL)
        return;

    //
    // EFI_DEBUGPORT_PROTOCOL.Write is called until all message is sent.
    //

    while (BufferLength > 0) {
        Length = BufferLength;
        Status = gEfiDebugPortProtocol->Write(gEfiDebugPortProtocol,
                                              WRITE_TIMEOUT,
                                              &Length,
                                              (VOID*)Buffer);
        if (EFI_ERROR(Status) || BufferLength < Length)
            break;
        Buffer += Length;
        BufferLength -= Length;
    }
}

static VOID DebugWriteToFile(_In_z_ CHAR8* Buffer, _In_ UINTN BufferLength)
{
    UINTN Length;
    EFI_STATUS Status;

    if (gsDebugFile == NULL)
        return;

    while (BufferLength > 0) {
        Length = BufferLength;
        Status = gsDebugFile->Write(gsDebugFile, &Length, (VOID*)Buffer);
        if (EFI_ERROR(Status) || BufferLength < Length)
            break;
        Buffer += Length;
        BufferLength -= Length;
    }

    gsDebugFile->Flush(gsDebugFile);
}

static VOID DebugWriteToUefiVariable(_In_z_ CHAR8* Buffer, _In_ UINTN BufferLength)
{
    if (gsUefiVariable == NULL) {
        return;
    }

    for (UINTN Index = 0; Index < BufferLength; Index++) {
        gsUefiVariable[gsUefiVariableIndex] = Buffer[Index];
        gsUefiVariableIndex = (gsUefiVariableIndex + 1) % gsMaxUefiVariableSize;
    }

    // Write logs to Uefi variable
    gRT->SetVariable(L"CbmrUefiLogs",
                     &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                     EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                         EFI_VARIABLE_RUNTIME_ACCESS,
                     gsMaxUefiVariableSize,
                     gsUefiVariable);
}

static VOID DebugWrite(_In_z_ CHAR8* Str, _In_ UINTN BufferLength)
{
    UINTN StrLength = AsciiStrnLenS(Str, BufferLength);

    if (gCbmrConfig.SpewTarget & SPEW_SERIAL) {
        DebugWriteToSerialPort(Str, StrLength);
    }

#ifndef UEFI_BUILD_SYSTEM
#ifdef DEBUGMODE
    if (gCbmrConfig.SpewTarget & SPEW_DEBUGGER) {
        if (gEfiMsWindbgServerProtocol != NULL) {
            gEfiMsWindbgServerProtocol->Print(gEfiMsWindbgServerProtocol, Str);
        }
    }
#endif
#endif

    if (gCbmrConfig.SpewTarget & SPEW_CONSOLE) {
        CHAR16 BufferWide[512];
        AsciiStrToUnicodeStr(Str, BufferWide);
        gST->ConOut->OutputString(gST->ConOut, BufferWide);
    }

    if (gCbmrConfig.SpewTarget & SPEW_FILE) {
        DebugWriteToFile(Str, StrLength);
    }

    if (gCbmrConfig.SpewTarget & SPEW_UEFI_VAR) {
        DebugWriteToUefiVariable(Str, StrLength);
    }
}

static const struct {
    UINTN BitMask;
    CHAR8* FlagName;
    CHAR16* FlagNameW;
} DebugFlags[FLAG_DEBUG_MAX] = {
    [FLAG_DEBUG_ERROR] = {1U << FLAG_DEBUG_ERROR, t("ERROR"), T("ERROR")},
    [FLAG_DEBUG_WARNING] = {1U << FLAG_DEBUG_WARNING, t("WARNING"), T("WARNING")},
    [FLAG_DEBUG_INFO] = {1U << FLAG_DEBUG_INFO, t("INFO"), T("INFO")},
    [FLAG_DEBUG_VERBOSE] = {1U << FLAG_DEBUG_VERBOSE, t("VERBOSE"), T("VERBOSE")},
};

static BOOLEAN IsDebugFlagEnabled(DEBUG_FLAGS DebugFlag)
{
    if ((DebugFlag >= _countof(DebugFlags)) || (DebugFlag < 0))
        return FALSE;

    if ((DebugFlags[DebugFlag].BitMask & gCbmrConfig.DebugMask) == 0)
        return FALSE;

    return TRUE;
}

static CHAR8* GetDebugFlagStr(DEBUG_FLAGS DebugFlag)
{
    if ((DebugFlag >= _countof(DebugFlags)) || (DebugFlag < 0))
        return t("UNKNOWN");

    if (DebugFlags[DebugFlag].BitMask == 0)
        return t("UNKNOWN");

    return DebugFlags[DebugFlag].FlagName;
}

static CHAR16* GetDebugFlagStrU(DEBUG_FLAGS DebugFlag)
{
    if ((DebugFlag >= _countof(DebugFlags)) || (DebugFlag < 0))
        return T("UNKNOWN");

    if (DebugFlags[DebugFlag].BitMask == 0)
        return T("UNKNOWN");

    return DebugFlags[DebugFlag].FlagNameW;
}
