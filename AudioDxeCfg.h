/*
 * File: AudioDxeCfg.h
 *
 * Description: AudioDxe configuration application.
 *
 * Copyright (c) 2018-2019 John Davis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _EFI_BOOT_CHIME_CFG_H_
#define _EFI_BOOT_CHIME_CFG_H_

// Common UEFI includes and library classes.
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
//#include <Library/BootChimeLib.h>
#include <Library/OcAudioLib.h>
#include <Library/OcDevicePathLib.h>
#include <Library/OcStringLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>

// Consumed protocols.
#include <Protocol/AudioDecode.h>
#include <Protocol/AudioIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>

#define PROMPT_ANY_KEY  L"Press any key to continue..."
#define BCFG_ARG_LIST   L'L'
#define BCFG_ARG_CURR   L'C'
#define BCFG_ARG_DUMP   L'D'
#define BCFG_ARG_SELECT L'S'
#define BCFG_ARG_VOLUME L'V'
#define BCFG_ARG_TEST   L'T'
#define BCFG_ARG_QUIT   L'Q'

#define MAX_CHARS       (12)

// Boot chime output device.
typedef struct {
  EFI_AUDIO_IO_PROTOCOL       *AudioIo;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  EFI_AUDIO_IO_PROTOCOL_PORT  OutputPort;
  UINTN                       OutputPortIndex;
} AUDIO_DEVICE;

// Chime data.
extern UINT8 mChimeData[];
extern UINTN mChimeDataLength;

#endif
