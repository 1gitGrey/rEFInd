/*
 * refit/config.c
 * Configuration file functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
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

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "config.h"
#include "screen.h"
#include "refit_call_wrapper.h"

// constants

#define CONFIG_FILE_NAME    L"refind.conf"
#define MAXCONFIGFILESIZE   (64*1024)

#define ENCODING_ISO8859_1  (0)
#define ENCODING_UTF8       (1)
#define ENCODING_UTF16_LE   (2)

//
// read a file into a buffer
//

static EFI_STATUS ReadFile(IN EFI_FILE_HANDLE BaseDir, CHAR16 *FileName, REFIT_FILE *File)
{
    EFI_STATUS      Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO   *FileInfo;
    UINT64          ReadSize;

    File->Buffer = NULL;
    File->BufferSize = 0;

    // read the file, allocating a buffer on the way
    Status = refit_call5_wrapper(BaseDir->Open, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (CheckError(Status, L"while loading the configuration file"))
        return Status;
    
    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        // TODO: print and register the error
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    if (ReadSize > MAXCONFIGFILESIZE)
        ReadSize = MAXCONFIGFILESIZE;
    FreePool(FileInfo);
    
    File->BufferSize = (UINTN)ReadSize;   // was limited to a few K before, so this is safe
    File->Buffer = AllocatePool(File->BufferSize);
    Status = refit_call3_wrapper(FileHandle->Read, FileHandle, &File->BufferSize, File->Buffer);
    if (CheckError(Status, L"while loading the configuration file")) {
        FreePool(File->Buffer);
        File->Buffer = NULL;
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return Status;
    }
    Status = refit_call1_wrapper(FileHandle->Close, FileHandle);
    
    // setup for reading
    File->Current8Ptr  = (CHAR8 *)File->Buffer;
    File->End8Ptr      = File->Current8Ptr + File->BufferSize;
    File->Current16Ptr = (CHAR16 *)File->Buffer;
    File->End16Ptr     = File->Current16Ptr + (File->BufferSize >> 1);
    
    // detect encoding
    File->Encoding = ENCODING_ISO8859_1;   // default: 1:1 translation of CHAR8 to CHAR16
    if (File->BufferSize >= 4) {
        if (File->Buffer[0] == 0xFF && File->Buffer[1] == 0xFE) {
            // BOM in UTF-16 little endian (or UTF-32 little endian)
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
            File->Current16Ptr++;
        } else if (File->Buffer[0] == 0xEF && File->Buffer[1] == 0xBB && File->Buffer[2] == 0xBF) {
            // BOM in UTF-8
            File->Encoding = ENCODING_UTF8;       // translate from UTF-8 to UTF-16
            File->Current8Ptr += 3;
        } else if (File->Buffer[1] == 0 && File->Buffer[3] == 0) {
            File->Encoding = ENCODING_UTF16_LE;   // use CHAR16 as is
        }
        // TODO: detect other encodings as they are implemented
    }
    
    return EFI_SUCCESS;
}

//
// get a single line of text from a file
//

static CHAR16 *ReadLine(REFIT_FILE *File)
{
    CHAR16  *Line, *q;
    UINTN   LineLength;
    
    if (File->Buffer == NULL)
        return NULL;
    
    if (File->Encoding == ENCODING_ISO8859_1 || File->Encoding == ENCODING_UTF8) {
        
        CHAR8 *p, *LineStart, *LineEnd;
        
        p = File->Current8Ptr;
        if (p >= File->End8Ptr)
            return NULL;
        
        LineStart = p;
        for (; p < File->End8Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End8Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current8Ptr = p;
        
        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;
        
        q = Line;
        if (File->Encoding == ENCODING_ISO8859_1) {
            for (p = LineStart; p < LineEnd; )
                *q++ = *p++;
        } else if (File->Encoding == ENCODING_UTF8) {
            // TODO: actually handle UTF-8
            for (p = LineStart; p < LineEnd; )
                *q++ = *p++;
        }
        *q = 0;
        
    } else if (File->Encoding == ENCODING_UTF16_LE) {
        
        CHAR16 *p, *LineStart, *LineEnd;
        
        p = File->Current16Ptr;
        if (p >= File->End16Ptr)
            return NULL;
        
        LineStart = p;
        for (; p < File->End16Ptr; p++)
            if (*p == 13 || *p == 10)
                break;
        LineEnd = p;
        for (; p < File->End16Ptr; p++)
            if (*p != 13 && *p != 10)
                break;
        File->Current16Ptr = p;
        
        LineLength = (UINTN)(LineEnd - LineStart) + 1;
        Line = AllocatePool(LineLength * sizeof(CHAR16));
        if (Line == NULL)
            return NULL;
        
        for (p = LineStart, q = Line; p < LineEnd; )
            *q++ = *p++;
        *q = 0;
        
    } else
        return NULL;   // unsupported encoding
    
    return Line;
}

//
// get a line of tokens from a file
//

UINTN ReadTokenLine(IN REFIT_FILE *File, OUT CHAR16 ***TokenList)
{
    BOOLEAN         LineFinished, IsQuoted = FALSE;
    CHAR16          *Line, *Token, *p;
    UINTN           TokenCount = 0;

    *TokenList = NULL;

    while (TokenCount == 0) {
        Line = ReadLine(File);
        if (Line == NULL)
            return(0);

        p = Line;
        LineFinished = FALSE;
        while (!LineFinished) {
            // skip whitespace
            while ((*p == ' ' || *p == '\t' || *p == '=' || *p == ',') && !IsQuoted)
                p++;
            if (*p == 0 || *p == '#')
                break;

            if (*p == '"') {
               IsQuoted = !IsQuoted;
               p++;
            } // if
            Token = p;

            // find end of token
            while (*p && *p != '"' && ((*p != ' ' && *p != '\t' && *p != '=' && *p != '#' && *p != ',') || IsQuoted)) {
               if ((*p == '/') && !IsQuoted) // Switch Unix-style to DOS-style directory separators
                  *p = '\\';
               p++;
            } // if
            if (*p == '"')
               IsQuoted = !IsQuoted;
            if (*p == 0 || *p == '#')
                LineFinished = TRUE;
            *p++ = 0;
            
            AddListElement((VOID ***)TokenList, &TokenCount, (VOID *)StrDuplicate(Token));
        }
        
        FreePool(Line);
    }
    return (TokenCount);
} /* ReadTokenLine() */

static VOID FreeTokenLine(IN OUT CHAR16 ***TokenList, IN OUT UINTN *TokenCount)
{
    // TODO: also free the items
    FreeList((VOID ***)TokenList, TokenCount);
}

//
// handle a parameter with a single integer argument
//

static VOID HandleInt(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT UINTN *Value)
{
    if (TokenCount < 2) {
        return;
    }
    if (TokenCount > 2) {
        return;
    }
    *Value = Atoi(TokenList[1]);
}

//
// handle a parameter with a single string argument
//

static VOID HandleString(IN CHAR16 **TokenList, IN UINTN TokenCount, OUT CHAR16 **Value)
{
    if (TokenCount < 2) {
        return;
    }
    if (TokenCount > 2) {
        return;
    }
    *Value = StrDuplicate(TokenList[1]);
}

//
// read config file
//

VOID ReadConfig(VOID)
{
    EFI_STATUS      Status;
    REFIT_FILE      File;
    CHAR16          **TokenList;
    CHAR16          *FlagName;
    UINTN           TokenCount, i;

    if (!FileExists(SelfDir, CONFIG_FILE_NAME)) {
        Print(L"Configuration file missing!\n");
        return;
    }

    Status = ReadFile(SelfDir, CONFIG_FILE_NAME, &File);
    if (EFI_ERROR(Status))
        return;

    for (;;) {
        TokenCount = ReadTokenLine(&File, &TokenList);
        if (TokenCount == 0)
            break;
        
        if (StriCmp(TokenList[0], L"timeout") == 0) {
            HandleInt(TokenList, TokenCount, &(GlobalConfig.Timeout));
            
        } else if (StriCmp(TokenList[0], L"disable") == 0) {
            for (i = 1; i < TokenCount; i++) {
                FlagName = TokenList[i];
                if (StriCmp(FlagName, L"shell") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_SHELL;
                } else if (StriCmp(FlagName, L"tools") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_TOOLS;
                } else if (StriCmp(FlagName, L"singleuser") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_SINGLEUSER;
                } else if (StriCmp(FlagName, L"hwtest") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_HWTEST;
                } else if (StriCmp(FlagName, L"all") == 0) {
                    GlobalConfig.DisableFlags = DISABLE_ALL;
                } else {
                    Print(L" unknown disable flag: '%s'\n", FlagName);
                }
            }

        } else if (StriCmp(TokenList[0], L"scanfor") == 0) {
           for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
              if (i < TokenCount)
                 GlobalConfig.ScanFor[i] = TokenList[i][0];
              else
                 GlobalConfig.ScanFor[i] = ' ';
           }
        } else if (StriCmp(TokenList[0], L"hideui") == 0) {
            for (i = 1; i < TokenCount; i++) {
                FlagName = TokenList[i];
                if (StriCmp(FlagName, L"banner") == 0) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_BANNER;
                } else if (StriCmp(FlagName, L"shell") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_SHELL;
                } else if (StriCmp(FlagName, L"tools") == 0) {
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_TOOLS;
                } else if (StriCmp(FlagName, L"funcs") == 0) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_FUNCS;
                } else if (StriCmp(FlagName, L"label") == 0) {
                    GlobalConfig.HideUIFlags |= HIDEUI_FLAG_LABEL;
                } else if (StriCmp(FlagName, L"all") == 0) {
                    GlobalConfig.HideUIFlags = HIDEUI_ALL;
                    GlobalConfig.DisableFlags |= DISABLE_FLAG_SHELL | DISABLE_FLAG_TOOLS;
                } else {
                    Print(L" unknown hideui flag: '%s'\n", FlagName);
                }
            }
            
        } else if (StriCmp(TokenList[0], L"banner") == 0) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.BannerFileName));

        } else if (StriCmp(TokenList[0], L"selection_small") == 0) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.SelectionSmallFileName));

        } else if (StriCmp(TokenList[0], L"selection_big") == 0) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.SelectionBigFileName));

        } else if (StriCmp(TokenList[0], L"default_selection") == 0) {
            HandleString(TokenList, TokenCount, &(GlobalConfig.DefaultSelection));
            
        } else if (StriCmp(TokenList[0], L"textonly") == 0) {
            GlobalConfig.TextOnly = TRUE;

        } else if ((StriCmp(TokenList[0], L"}") == 0) || (StriCmp(TokenList[0], L"loader") == 0) ||
                   (StriCmp(TokenList[0], L"icon") == 0) || (StriCmp(TokenList[0], L"options") == 0)) {
           // Do nothing; handled by ScanUserConfigured()
        }

        FreeTokenLine(&TokenList, &TokenCount);
    }

    FreePool(File.Buffer);
} /* VOID ReadConfig() */

// Adds the options from a SINGLE loaders.conf stanza to a new loader entry and returns
// that entry. The calling function is then responsible for adding the entry to the
// list of entries.
static LOADER_ENTRY * AddStanzaEntries(REFIT_FILE *File, REFIT_VOLUME *Volume, CHAR16 *Title) {
   CHAR16       **TokenList;
   UINTN        TokenCount;
   LOADER_ENTRY *Entry;
   BOOLEAN      DefaultsSet = FALSE;

   // prepare the menu entry
   Entry = AllocateZeroPool(sizeof(LOADER_ENTRY));
   Entry->Title           = StrDuplicate(Title);
   Entry->me.Title        = PoolPrint(L"Boot %s from %s", (Title != NULL) ? Title : L"Unknown", Volume->VolName);
   Entry->me.Tag          = TAG_LOADER;
   Entry->me.Row          = 0;
   Entry->me.BadgeImage   = Volume->VolBadgeImage;
   Entry->LoaderPath      = NULL; // This MUST be set via a "loader" line for normal functioning!
   Entry->DevicePath      = NULL; // This MUST be set via a "loader" line for normal functioning!
   Entry->VolName         = Volume->VolName;
   Entry->UseGraphicsMode = FALSE;
   Entry->Enabled         = TRUE;
   Entry->OSType          = ' ';

   while (((TokenCount = ReadTokenLine(File, &TokenList)) > 0) && (StriCmp(TokenList[0], L"}") != 0)) {
      if (StriCmp(TokenList[0], L"loader") == 0) { // set the boot loader filename
         Entry->LoaderPath = StrDuplicate(TokenList[1]);
         Entry->DevicePath = FileDevicePath(Volume->DeviceHandle, Entry->LoaderPath);
         SetLoaderDefaults(Entry, TokenList[1], Volume);
         FreePool(Entry->LoadOptions);
         Entry->LoadOptions = NULL; // Discard default options, if any
         DefaultsSet = TRUE;
      } else if (StriCmp(TokenList[0], L"icon") == 0) {
         FreePool(Entry->me.Image);
         Entry->me.Image = LoadIcns(Volume->RootDir, TokenList[1], 128);
         if (Entry->me.Image == NULL) {
            Entry->me.Image = DummyImage(128);
         }
      } else if (StriCmp(TokenList[0], L"initrd") == 0) {
         MergeStrings(&Entry->LoadOptions, L"initrd=", L' ');
         MergeStrings(&Entry->LoadOptions, TokenList[1], 0);
      } else if (StriCmp(TokenList[0], L"options") == 0) {
         MergeStrings(&Entry->LoadOptions, TokenList[1], L' ');
      } else if (StriCmp(TokenList[0], L"ostype") == 0) {
         if (TokenCount > 1) {
            Entry->OSType = TokenList[1][0];
         }
      } else if (StriCmp(TokenList[0], L"graphics") == 0) {
         Entry->UseGraphicsMode = (StriCmp(TokenList[1], L"on") == 0);
      } else if (StriCmp(TokenList[0], L"disabled") == 0) {
         Entry->Enabled = FALSE;
      } // set options to pass to the loader program
      FreeTokenLine(&TokenList, &TokenCount);
   } // while()
   if (!DefaultsSet)
      SetLoaderDefaults(Entry, L"\\EFI\\BOOT\\nemo.efi", Volume); // user included no entry; use bogus one

   return(Entry);
} // static VOID AddStanzaEntries()

// Read the user-configured loaders file, loaders.conf, and add or delete
// entries based on the contents of that file....
VOID ScanUserConfigured(VOID)
{
   EFI_STATUS        Status;
   REFIT_FILE        File;
   REFIT_VOLUME      *Volume;
   CHAR16            **TokenList;
   CHAR16            Title[256];
   UINTN             TokenCount;
   LOADER_ENTRY      *Entry;
   REFIT_MENU_SCREEN *SubScreen;

   if (FileExists(SelfDir, CONFIG_FILE_NAME)) {
      Status = ReadFile(SelfDir, CONFIG_FILE_NAME, &File);
      if (EFI_ERROR(Status))
         return;

      Volume = SelfVolume;
      // TODO: Figure out how to set volumes (on per-image basis, preferably)

      while ((TokenCount = ReadTokenLine(&File, &TokenList)) > 0) {
         if ((StriCmp(TokenList[0], L"menuentry") == 0) && (TokenCount > 1)) {
            StrCpy(Title, TokenList[1]);
            Entry = AddStanzaEntries(&File, Volume, Title);
            if (Entry->Enabled) {
               SubScreen = GenerateSubScreen(Entry, Volume);
               AddPreparedLoaderEntry(Entry, SubScreen);
            } else {
               FreePool(Entry);
            } // if/else
         } // if
         FreeTokenLine(&TokenList, &TokenCount);
      } // while()
   } // if()
} // VOID ScanUserConfigured()
