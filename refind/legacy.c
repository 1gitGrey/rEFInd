/*
 * refind/legacy.c
 * Main code for the boot menu
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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

#include "legacy.h"
#include "global.h"
#include "screen.h"
#include "lib.h"
#include "refit_call_wrapper.h"

#include "../include/syslinux_mbr.h"

//
// legacy boot functions
//

static EFI_STATUS ActivateMbrPartition(IN EFI_BLOCK_IO *BlockIO, IN UINTN PartitionIndex)
{
    EFI_STATUS          Status;
    UINT8               SectorBuffer[512];
    MBR_PARTITION_INFO  *MbrTable, *EMbrTable;
    UINT32              ExtBase, ExtCurrent, NextExtCurrent;
    UINTN               LogicalPartitionIndex = 4;
    UINTN               i;
    BOOLEAN             HaveBootCode;

    // read MBR
    Status = refit_call5_wrapper(BlockIO->ReadBlocks, BlockIO, BlockIO->Media->MediaId, 0, 512, SectorBuffer);
    if (EFI_ERROR(Status))
        return Status;
    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55)
        return EFI_NOT_FOUND;  // safety measure #1

    // add boot code if necessary
    HaveBootCode = FALSE;
    for (i = 0; i < MBR_BOOTCODE_SIZE; i++) {
        if (SectorBuffer[i] != 0) {
            HaveBootCode = TRUE;
            break;
        }
    }
    if (!HaveBootCode) {
        // no boot code found in the MBR, add the syslinux MBR code
        SetMem(SectorBuffer, MBR_BOOTCODE_SIZE, 0);
        CopyMem(SectorBuffer, syslinux_mbr, SYSLINUX_MBR_SIZE);
    }

    // set the partition active
    MbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
    ExtBase = 0;
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80)
            return EFI_NOT_FOUND;   // safety measure #2
        if (i == PartitionIndex)
            MbrTable[i].Flags = 0x80;
        else if (PartitionIndex >= 4 && IS_EXTENDED_PART_TYPE(MbrTable[i].Type)) {
            MbrTable[i].Flags = 0x80;
            ExtBase = MbrTable[i].StartLBA;
        } else
            MbrTable[i].Flags = 0x00;
    }

    // write MBR
    Status = refit_call5_wrapper(BlockIO->WriteBlocks, BlockIO, BlockIO->Media->MediaId, 0, 512, SectorBuffer);
    if (EFI_ERROR(Status))
        return Status;

    if (PartitionIndex >= 4) {
        // we have to activate a logical partition, so walk the EMBR chain

        // NOTE: ExtBase was set above while looking at the MBR table
        for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
            // read current EMBR
            Status = refit_call5_wrapper(BlockIO->ReadBlocks, BlockIO, BlockIO->Media->MediaId, ExtCurrent, 512, SectorBuffer);
            if (EFI_ERROR(Status))
                return Status;
            if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55)
                return EFI_NOT_FOUND;  // safety measure #3

            // scan EMBR, set appropriate partition active
            EMbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
            NextExtCurrent = 0;
            for (i = 0; i < 4; i++) {
                if (EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80)
                    return EFI_NOT_FOUND;   // safety measure #4
                if (EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0)
                    break;
                if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {
                    // link to next EMBR
                    NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                    EMbrTable[i].Flags = (PartitionIndex >= LogicalPartitionIndex) ? 0x80 : 0x00;
                    break;
                } else {
                    // logical partition
                    EMbrTable[i].Flags = (PartitionIndex == LogicalPartitionIndex) ? 0x80 : 0x00;
                    LogicalPartitionIndex++;
                }
            }

            // write current EMBR
            Status = refit_call5_wrapper(BlockIO->WriteBlocks, BlockIO, BlockIO->Media->MediaId, ExtCurrent, 512, SectorBuffer);
            if (EFI_ERROR(Status))
                return Status;

            if (PartitionIndex < LogicalPartitionIndex)
                break;  // stop the loop, no need to touch further EMBRs
        }
        
    }

    return EFI_SUCCESS;
} /* static EFI_STATUS ActivateMbrPartition() */

// early 2006 Core Duo / Core Solo models
static UINT8 LegacyLoaderDevicePath1Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF9, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// mid-2006 Mac Pro (and probably other Core 2 models)
static UINT8 LegacyLoaderDevicePath2Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// mid-2007 MBP ("Santa Rosa" based models)
static UINT8 LegacyLoaderDevicePath3Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// early-2008 MBA
static UINT8 LegacyLoaderDevicePath4Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// late-2008 MB/MBP (NVidia chipset)
static UINT8 LegacyLoaderDevicePath5Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x40, 0xCB, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xBF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};

static EFI_DEVICE_PATH *LegacyLoaderList[] = {
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath1Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath2Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath3Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath4Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath5Data,
    NULL
};

#define MAX_DISCOVERED_PATHS (16)

static VOID StartLegacy(IN LEGACY_ENTRY *Entry)
{
    EFI_STATUS          Status;
    EG_IMAGE            *BootLogoImage;
    UINTN               ErrorInStep = 0;
    EFI_DEVICE_PATH     *DiscoveredPathList[MAX_DISCOVERED_PATHS];

    BeginExternalScreen(TRUE, L"Booting Legacy OS");

    BootLogoImage = LoadOSIcon(Entry->Volume->OSIconName, L"legacy", TRUE);
    if (BootLogoImage != NULL)
        BltImageAlpha(BootLogoImage,
                      (UGAWidth  - BootLogoImage->Width ) >> 1,
                      (UGAHeight - BootLogoImage->Height) >> 1,
                      &StdBackgroundPixel);

    if (Entry->Volume->IsMbrPartition)
        ActivateMbrPartition(Entry->Volume->WholeDiskBlockIO, Entry->Volume->MbrPartitionIndex);

    ExtractLegacyLoaderPaths(DiscoveredPathList, MAX_DISCOVERED_PATHS, LegacyLoaderList);

    Status = StartEFIImageList(DiscoveredPathList, Entry->LoadOptions, NULL, L"legacy loader", &ErrorInStep);
    if (Status == EFI_NOT_FOUND) {
        if (ErrorInStep == 1) {
            Print(L"\nPlease make sure that you have the latest firmware update installed.\n");
        } else if (ErrorInStep == 3) {
            Print(L"\nThe firmware refused to boot from the selected volume. Note that external\n"
                  L"hard drives are not well-supported by Apple's firmware for legacy OS booting.\n");
        }
    }
    FinishExternalScreen();
} /* static VOID StartLegacy() */

static LEGACY_ENTRY * AddLegacyEntry(IN CHAR16 *LoaderTitle, IN REFIT_VOLUME *Volume)
{
    LEGACY_ENTRY            *Entry, *SubEntry;
    REFIT_MENU_SCREEN       *SubScreen;
    CHAR16                  *VolDesc;
    CHAR16                  ShortcutLetter = 0;

    if (LoaderTitle == NULL) {
        if (Volume->OSName != NULL) {
            LoaderTitle = Volume->OSName;
            if (LoaderTitle[0] == 'W' || LoaderTitle[0] == 'L')
                ShortcutLetter = LoaderTitle[0];
        } else
            LoaderTitle = L"Legacy OS";
    }
    if (Volume->VolName != NULL)
        VolDesc = Volume->VolName;
    else
        VolDesc = (Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" : L"HD";

    // prepare the menu entry
    Entry = AllocateZeroPool(sizeof(LEGACY_ENTRY));
    Entry->me.Title        = PoolPrint(L"Boot %s from %s", LoaderTitle, VolDesc);
    Entry->me.Tag          = TAG_LEGACY;
    Entry->me.Row          = 0;
    Entry->me.ShortcutLetter = ShortcutLetter;
    Entry->me.Image        = LoadOSIcon(Volume->OSIconName, L"legacy", FALSE);
    Entry->me.BadgeImage   = Volume->VolBadgeImage;
    Entry->Volume          = Volume;
    Entry->LoadOptions     = (Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" :
        ((Volume->DiskKind == DISK_KIND_EXTERNAL) ? L"USB" : L"HD");
    Entry->Enabled         = TRUE;

    // create the submenu
    SubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));
    SubScreen->Title = PoolPrint(L"Boot Options for %s on %s", LoaderTitle, VolDesc);
    SubScreen->TitleImage = Entry->me.Image;

    // default entry
    SubEntry = AllocateZeroPool(sizeof(LEGACY_ENTRY));
    SubEntry->me.Title        = PoolPrint(L"Boot %s", LoaderTitle);
    SubEntry->me.Tag          = TAG_LEGACY;
    SubEntry->Volume          = Entry->Volume;
    SubEntry->LoadOptions     = Entry->LoadOptions;
    AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
    
    AddMenuEntry(SubScreen, &MenuEntryReturn);
    Entry->me.SubScreen = SubScreen;
    AddMenuEntry(&MainMenu, (REFIT_MENU_ENTRY *)Entry);
    return Entry;
} /* static LEGACY_ENTRY * AddLegacyEntry() */

static VOID ScanLegacyVolume(REFIT_VOLUME *Volume, UINTN VolumeIndex) {
   UINTN VolumeIndex2;
   BOOLEAN ShowVolume, HideIfOthersFound;

   ShowVolume = FALSE;
   HideIfOthersFound = FALSE;
   if (Volume->IsAppleLegacy) {
      ShowVolume = TRUE;
      HideIfOthersFound = TRUE;
   } else if (Volume->HasBootCode) {
      ShowVolume = TRUE;
      if (Volume->BlockIO == Volume->WholeDiskBlockIO &&
         Volume->BlockIOOffset == 0 &&
         Volume->OSName == NULL)
         // this is a whole disk (MBR) entry; hide if we have entries for partitions
      HideIfOthersFound = TRUE;
   }
   if (HideIfOthersFound) {
      // check for other bootable entries on the same disk
      for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
         if (VolumeIndex2 != VolumeIndex && Volumes[VolumeIndex2]->HasBootCode &&
            Volumes[VolumeIndex2]->WholeDiskBlockIO == Volume->WholeDiskBlockIO)
            ShowVolume = FALSE;
      }
   }

   if (ShowVolume)
      AddLegacyEntry(NULL, Volume);
} // static VOID ScanLegacyVolume()

// Scan attached optical discs for legacy (BIOS) boot code
// and add anything found to the list....
static VOID ScanLegacyDisc(VOID)
{
   UINTN                   VolumeIndex;
   REFIT_VOLUME            *Volume;

   for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
      Volume = Volumes[VolumeIndex];
      if (Volume->DiskKind == DISK_KIND_OPTICAL)
         ScanLegacyVolume(Volume, VolumeIndex);
   } // for
} /* static VOID ScanLegacyDisc() */

// Scan internal hard disks for legacy (BIOS) boot code
// and add anything found to the list....
static VOID ScanLegacyInternal(VOID)
{
    UINTN                   VolumeIndex;
    REFIT_VOLUME            *Volume;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];
        if (Volume->DiskKind == DISK_KIND_INTERNAL)
            ScanLegacyVolume(Volume, VolumeIndex);
    } // for
} /* static VOID ScanLegacyInternal() */

// Scan external disks for legacy (BIOS) boot code
// and add anything found to the list....
static VOID ScanLegacyExternal(VOID)
{
   UINTN                   VolumeIndex;
   REFIT_VOLUME            *Volume;

   for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
      Volume = Volumes[VolumeIndex];
      if (Volume->DiskKind == DISK_KIND_EXTERNAL)
         ScanLegacyVolume(Volume, VolumeIndex);
   } // for
} /* static VOID ScanLegacyExternal() */