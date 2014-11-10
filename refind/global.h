/*
 * refit/global.h
 * Global header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012 Roderick W. Smith
 * 
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 * 
 */

#ifndef __GLOBAL_H_
#define __GLOBAL_H_

#include "efi.h"
#include "efilib.h"

#include "libeg.h"

#define REFIT_DEBUG (0)

#define TAG_ABOUT    (1)
#define TAG_RESET    (2)
#define TAG_SHUTDOWN (3)
#define TAG_TOOL     (4)
#define TAG_LOADER   (5)
#define TAG_LEGACY   (6)

#define NUM_SCAN_OPTIONS 10

//
// global definitions
//

// global types

typedef struct {
   UINT8 Flags;
   UINT8 StartCHS1;
   UINT8 StartCHS2;
   UINT8 StartCHS3;
   UINT8 Type;
   UINT8 EndCHS1;
   UINT8 EndCHS2;
   UINT8 EndCHS3;
   UINT32 StartLBA;
   UINT32 Size;
} MBR_PARTITION_INFO;

typedef struct {
   EFI_DEVICE_PATH     *DevicePath;
   EFI_HANDLE          DeviceHandle;
   EFI_FILE            *RootDir;
   CHAR16              *VolName;
   EG_IMAGE            *VolBadgeImage;
   UINTN               DiskKind;
   BOOLEAN             IsAppleLegacy;
   BOOLEAN             HasBootCode;
   CHAR16              *OSIconName;
   CHAR16              *OSName;
   BOOLEAN             IsMbrPartition;
   UINTN               MbrPartitionIndex;
   EFI_BLOCK_IO        *BlockIO;
   UINT64              BlockIOOffset;
   EFI_BLOCK_IO        *WholeDiskBlockIO;
   EFI_DEVICE_PATH     *WholeDiskDevicePath;
   MBR_PARTITION_INFO  *MbrPartitionTable;
} REFIT_VOLUME;

typedef struct _refit_menu_entry {
   CHAR16      *Title;
   UINTN       Tag;
   UINTN       Row;
   CHAR16      ShortcutDigit;
   CHAR16      ShortcutLetter;
   EG_IMAGE    *Image;
   EG_IMAGE    *BadgeImage;
   struct _refit_menu_screen *SubScreen;
} REFIT_MENU_ENTRY;

typedef struct _refit_menu_screen {
   CHAR16      *Title;
   EG_IMAGE    *TitleImage;
   UINTN       InfoLineCount;
   CHAR16      **InfoLines;
   UINTN       EntryCount;     // total number of entries registered
   REFIT_MENU_ENTRY **Entries;
   UINTN       TimeoutSeconds;
   CHAR16      *TimeoutText;
} REFIT_MENU_SCREEN;

typedef struct {
   REFIT_MENU_ENTRY me;
   CHAR16           *Title;
   CHAR16           *LoaderPath;
   CHAR16           *VolName;
   EFI_DEVICE_PATH  *DevicePath;
   BOOLEAN          UseGraphicsMode;
   BOOLEAN          Enabled;
   CHAR16           *LoadOptions;
   CHAR8            OSType;
} LOADER_ENTRY;

typedef struct {
   REFIT_MENU_ENTRY me;
   REFIT_VOLUME     *Volume;
   CHAR16           *LoadOptions;
   BOOLEAN          Enabled;
} LEGACY_ENTRY;

typedef struct {
   BOOLEAN     TextOnly;
   UINTN       Timeout;
   UINTN       DisableFlags;
   UINTN       HideUIFlags;
   BOOLEAN     Quiet;
   CHAR16      *BannerFileName;
   CHAR16      *SelectionSmallFileName;
   CHAR16      *SelectionBigFileName;
   CHAR16      *DefaultSelection;
   CHAR8       ScanFor[NUM_SCAN_OPTIONS]; // codes of types of loaders for which to scan
} REFIT_CONFIG;

// Global variables

extern EFI_HANDLE       SelfImageHandle;
extern EFI_LOADED_IMAGE *SelfLoadedImage;
extern EFI_FILE         *SelfRootDir;
extern EFI_FILE         *SelfDir;
extern CHAR16           *SelfDirPath;

extern REFIT_VOLUME     *SelfVolume;
extern REFIT_VOLUME     **Volumes;
extern UINTN            VolumesCount;

extern REFIT_CONFIG     GlobalConfig;

REFIT_MENU_SCREEN * GenerateSubScreen(LOADER_ENTRY *Entry, IN REFIT_VOLUME *Volume);
LOADER_ENTRY * MakeGenericLoaderEntry(VOID);
LOADER_ENTRY * PrepareLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume);
LOADER_ENTRY * AddLoaderEntry(IN CHAR16 *LoaderPath, IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume);
VOID SetLoaderDefaults(LOADER_ENTRY *Entry, CHAR16 *LoaderPath, IN REFIT_VOLUME *Volume);
LOADER_ENTRY * AddPreparedLoaderEntry(LOADER_ENTRY *Entry, REFIT_MENU_SCREEN *SubScreen);

#endif

/* EOF */
