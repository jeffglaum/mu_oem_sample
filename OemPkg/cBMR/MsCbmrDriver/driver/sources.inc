!IF 0

Copyright (c) Microsoft Corporation

Module Name:

    sources.inc

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.

!ENDIF

BOOTAPPNAME=cbmr

!include $(PROJECT_ROOT)\boot\environ\app\bootapp.inc
!include $(BASEDIR)\data\bootenv.inc
!include $(BASEDIR)\data\boot\bootlib.inc

!if $(ARM)
C_DEFINES=$(C_DEFINES) /d2loadstoremultiword-
!endif

# Compiler warning level 4
#
MSC_WARNING_LEVEL=/W4

BUILD_FOR_CORESYSTEM=1
USE_DEFAULT_WIN32_LIBS=0

BOOT_LOADER_CRITICAL=1

#
# Uncomment this define to generate .cod files
#
#USER_C_FLAGS=$(USER_C_FLAGS) /FAcs

# special compilation settings used to produce an EFI app
C_DEFINES=$(C_DEFINES) -DUNICODE
C_DEFINES=$(C_DEFINES) -D_UNICODE
C_DEFINES=$(C_DEFINES) -DWINNT
C_DEFINES=$(C_DEFINES) -DDEVL
C_DEFINES=$(C_DEFINES) -DNO_INTERFACE_DECL
C_DEFINES=$(C_DEFINES) -GL

INCLUDES=$(INCLUDES);\
    $(BOOT_DIR)\inc;\
    $(PROJECT_ROOT)\boot\environ\app\cbmr\common; \
    $(PROJECT_ROOT)\boot\environ\app\cbmr\xml\Include; \
    $(PROJECT_ROOT)\boot\environ\app\cbmr\xml\Include\Library; \


SOURCES=                        \
    ..\blmissing.c              \
    ..\cab_extract.c            \
    ..\cbmr_core.c              \
    ..\cbmr_driver.c            \
    ..\cbmr_protocols.c         \
    ..\dcat.c                   \
    ..\error.c                  \
    ..\http.c                   \
    ..\network_common.c         \
    ..\protocols.c              \
    ..\ramdisk.c                \
    ..\tls.c                    \
    ..\wim.c                    \
    ..\string_helper.c          \

COMPILER_FLAGS=\
-entry:CbmrDriverEntry \
-subsystem:EFI_BOOT_SERVICE_DRIVER

NTTARGETFILE1=\
    $(OBJ_PATH)\$(O)\$(BOOTAPPNAME).$(TARGETEXTENSION) \

!if "$(SECONDTARGETNAME)" != ""
NTTARGETFILE2=\
    $(NTTARGETFILE2) \
    $(OBJ_PATH)\$(O)\$(SECONDTARGETNAME).$(TARGETEXTENSION) \
!endif

!if "$(BOOTARCHNAME)" != ""
NTTARGETFILE2=\
    $(NTTARGETFILE2) \
    $(OBJ_PATH)\$(O)\$(BOOTARCHNAME).$(TARGETEXTENSION) \
!endif

