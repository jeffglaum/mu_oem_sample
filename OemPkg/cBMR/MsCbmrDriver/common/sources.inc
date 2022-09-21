TARGETTYPE=LIBRARY

!include $(BASEDIR)\data\boot\efiboot.inc
!include $(BASEDIR)\data\boot\bootlib.inc

# Compiler warning level 4
#
MSC_WARNING_LEVEL=/W4

BUILD_FOR_CORESYSTEM=1
USE_DEFAULT_WIN32_LIBS=0

BOOT_LOADER_CRITICAL=1

LIBNAME=cbmr_common

INCLUDES=\
    $(INCLUDES); \
    $(PROJECT_ROOT)\boot\environ\app\cbmr\common; \
    $(PROJECT_ROOT)\boot\environ\app\cbmr\windbgserver; \

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

COMPILER_FLAGS= \
    -subsystem:BOOT_APPLICATION

USE_NOLIBS=1


SOURCES=                                          \
    ..\buffer.c                                   \
    ..\cbmr_config.c                              \
    ..\cbmrdebug.c                                \
    ..\edk2compat.c                               \
    ..\file.c                                     \
    ..\guids.c                                    \
    ..\safe_arithmetic.c                          \
    ..\utils.c                                    \

