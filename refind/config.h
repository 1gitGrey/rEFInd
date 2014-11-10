/*
 * refit/config.h
 * General header file
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

#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "efi.h"
#include "global.h"

//
// config module
//

typedef struct {
    UINT8   *Buffer;
    UINTN   BufferSize;
    UINTN   Encoding;
    CHAR8   *Current8Ptr;
    CHAR8   *End8Ptr;
    CHAR16  *Current16Ptr;
    CHAR16  *End16Ptr;
} REFIT_FILE;

#define DISABLE_FLAG_SHELL      (0x0001)
#define DISABLE_FLAG_TOOLS      (0x0002)
#define DISABLE_FLAG_SINGLEUSER (0x0004)
#define DISABLE_FLAG_HWTEST     (0x0008)
#define DISABLE_ALL             ((0xffff))

#define HIDEUI_FLAG_BANNER      (0x0001)
#define HIDEUI_FLAG_FUNCS       (0x0002)
#define HIDEUI_FLAG_LABEL       (0x0004)
#define HIDEUI_ALL              (0xffff)

VOID ReadConfig(VOID);
VOID ScanUserConfigured(VOID);

#endif

/* EOF */
