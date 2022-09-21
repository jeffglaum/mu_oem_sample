#ifndef _EDK2COMPAT_H_
#define _EDK2COMPAT_H_

#include "edk2compat\baselib.h"
#ifndef UEFI_BUILD_SYSTEM
#include "edk2compat\guids.h"
#include "edk2compat\bootmgrpolicy.h"
#include "edk2compat\blockio.h"
#include "edk2compat\devicepath.h"
#include "edk2compat\dxe.h"
#include "edk2compat\hiifont.h"
#include "edk2compat\hidatabase.h"
#include "edk2compat\ramdisk.h"
#include "edk2compat\smbios.h"
#include "edk2compat\tcg.h"
#include "edk2compat\tcp.h"
#include "edk2compat\timestamp.h"
#include "edk2compat\uefigpt.h"
#include "edk2compat\edkwifi.h"
#include "edk2compat\shell.h"
#endif

// Below header is not publicly exposed in UEFI
#include "edk2compat\wificonnectmanagerdxe.h"
#endif // _EDK2COMPAT_H_
