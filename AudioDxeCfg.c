/*
 * File: AudioDxeCfg.c
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

#include "AudioDxeCfg.h"

STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *mSimpleTextIn        = NULL;

STATIC CHAR16                           *mDefaultDevices[EfiAudioIoDeviceMaximum]  = { L"Line", L"Speaker", L"Headphones", L"SPDIF", L"Mic", L"HDMI", L"Other" };
STATIC CHAR16                           *mLocations[EfiAudioIoLocationMaximum]     = { L"N/A", L"rear", L"front", L"left", L"right", L"top", L"bottom", L"other" };
STATIC CHAR16                           *mSurfaces[EfiAudioIoSurfaceMaximum]       = { L"external", L"internal", L"other" };

STATIC UINT8                            mDeviceVolume         = (UINT8)(EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME / 2);
STATIC AUDIO_DEVICE                     *mDevices             = NULL;
STATIC AUDIO_DEVICE                     *mCurrentDevice       = NULL;
STATIC UINTN                            mDevicesCount         = 0;

STATIC UINT8                            *mBuffer              = NULL;
STATIC UINT32                           mBufferSize           = 0;
STATIC EFI_AUDIO_IO_PROTOCOL_FREQ       mFrequency            = 0;
STATIC EFI_AUDIO_IO_PROTOCOL_BITS       mBits                 = 0;
STATIC UINT8                            mChannels             = 0;

STATIC
VOID
FlushKeystrokes (
  VOID
  )
{
  EFI_STATUS      Status;
  EFI_INPUT_KEY   InputKey;

  //

  // Check if parameters are valid.
  if (mSimpleTextIn == NULL) {
    return;
  }

  // Flush any keystrokes.
  do {
    gBS->Stall (100);
    Status = mSimpleTextIn->ReadKeyStroke (mSimpleTextIn, &InputKey);
  } while (!EFI_ERROR (Status));
}

STATIC
EFI_STATUS
WaitForKey (
  OUT CHAR16    *KeyValue
  )
{
  EFI_STATUS      Status;
  UINTN           EventIndex;
  EFI_INPUT_KEY   InputKey;

  //

  // Check if parameters are valid.
  if ((mSimpleTextIn == NULL) || (KeyValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    // Wait for key.
    gBS->WaitForEvent (1, &(mSimpleTextIn->WaitForKey), &EventIndex);

    // Get key value.
    Status = mSimpleTextIn->ReadKeyStroke (mSimpleTextIn, &InputKey);

    if (!EFI_ERROR (Status)) {
      // If \n, wait again.
      if (InputKey.UnicodeChar == L'\n') {
        return WaitForKey (KeyValue);
      }

      // Success.
      *KeyValue = InputKey.UnicodeChar;

      return EFI_SUCCESS;
    }
  }

  return EFI_DEVICE_ERROR;
}

STATIC
EFI_STATUS
GetAudioDecoder (
  VOID
  )
{
  EFI_STATUS                  Status;
  EFI_AUDIO_DECODE_PROTOCOL   *AudioDecodeProtocol;

  //

  Status = gBS->LocateProtocol (
    &gEfiAudioDecodeProtocolGuid,
    NULL,
    (VOID **)&AudioDecodeProtocol
    );
  if (!EFI_ERROR (Status)) {
    Status = AudioDecodeProtocol->DecodeAny (
      AudioDecodeProtocol,
      &mChimeData[0],
      (UINT32)mChimeDataLength,
      (VOID **)&mBuffer,
      &mBufferSize,
      &mFrequency,
      &mBits,
      &mChannels
      );
    if (EFI_ERROR (Status)) {
      Print (L"Decoding audio buffer fail - %r\n", Status);
    }
  } else {
    Print (L"Cannot locate audio decoder protocol - %r\n", Status);
  }

  return Status;
}


STATIC
EFI_STATUS
GetOutputDevices (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *AudioIoHandles;
  UINTN                         AudioIoHandleCount;
  EFI_AUDIO_IO_PROTOCOL         *AudioIo;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  EFI_AUDIO_IO_PROTOCOL_PORT    *OutputPorts;
  UINTN                         OutputPortsCount;
  UINTN                         h;

  // Devices.
  AUDIO_DEVICE    *OutputDevices;
  AUDIO_DEVICE    *OutputDevicesNew;
  UINTN           OutputDevicesCount;
  UINTN           OutputDeviceIndex;
  UINTN           o;

  //

  // Get Audio I/O protocols in system.
  AudioIoHandles      = NULL;
  AudioIoHandleCount  = 0;
  Status              = gBS->LocateHandleBuffer (ByProtocol, &gEfiAudioIoProtocolGuid, NULL, &AudioIoHandleCount, &AudioIoHandles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  OutputDevices       = NULL;
  OutputDevicesCount  = 0;
  OutputDeviceIndex   = 0;

  // Discover audio outputs in system.
  for (h = 0; h < AudioIoHandleCount; h++) {
    // Open Audio I/O protocol.
    Status = gBS->HandleProtocol (AudioIoHandles[h], &gEfiAudioIoProtocolGuid, (VOID**)&AudioIo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Get device path.
    Status = gBS->HandleProtocol (AudioIoHandles[h], &gEfiDevicePathProtocolGuid, (VOID**)&DevicePath);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Get output devices.
    Status = AudioIo->GetOutputs (AudioIo, &OutputPorts, &OutputPortsCount);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Increase total output devices.
    OutputDevicesNew = ReallocatePool (OutputDevicesCount * sizeof (AUDIO_DEVICE),
                          (OutputDevicesCount + OutputPortsCount) * sizeof (AUDIO_DEVICE),
                          OutputDevices);
    if (OutputDevicesNew == NULL) {
      FreePool (OutputPorts);
      Status = EFI_OUT_OF_RESOURCES;
      goto DONE_ERROR;
    }
    OutputDevices       = OutputDevicesNew;
    OutputDevicesCount += OutputPortsCount;

    // Get devices on this protocol.
    for (o = 0; o < OutputPortsCount; o++) {
      OutputDevices[OutputDeviceIndex].AudioIo          = AudioIo;
      OutputDevices[OutputDeviceIndex].DevicePath       = DevicePath;
      OutputDevices[OutputDeviceIndex].OutputPort       = OutputPorts[o];
      OutputDevices[OutputDeviceIndex].OutputPortIndex  = o;
      OutputDeviceIndex++;
    }

    // Free output ports.
    FreePool (OutputPorts);
  }

  // Success.
  mDevices        = OutputDevices;
  mDevicesCount   = OutputDevicesCount;
  mCurrentDevice  = &mDevices[0];

  Status = EFI_SUCCESS;

  goto DONE;

  DONE_ERROR:

  // Free devices.
  if (OutputDevices != NULL) {
    FreePool (OutputDevices);
  }

  DONE:

  // Free stuff.
  if (AudioIoHandles != NULL) {
    FreePool (AudioIoHandles);
  }

  return Status;
}

STATIC
EFI_DEVICE_PATH_PROTOCOL *
GetRootDevicePath (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *TmpDevicePath;
  VENDOR_DEVICE_PATH          *VendorDevicePath;

  //

  if (DevicePath != NULL) {
    TmpDevicePath = DuplicateDevicePath (DevicePath);
    if (TmpDevicePath != NULL) {
      VendorDevicePath  = (VENDOR_DEVICE_PATH *)FindDevicePathNodeWithType (
                                                  TmpDevicePath,
                                                  MESSAGING_DEVICE_PATH,
                                                  MSG_VENDOR_DP);
      if (VendorDevicePath != NULL) {
        SetDevicePathEndNode (VendorDevicePath);
        return TmpDevicePath;
      }

      FreePool (TmpDevicePath);
    }
  }

  return NULL;
}

STATIC
EFI_STATUS
PrintDevices (
  VOID
  )
{
  EFI_STATUS                Status;
  CHAR16                    KeyValue;
  UINTN                     i;
  UINTN                     s;
  UINTN                     Len;
  CHAR16                    *TextDevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *TmpDevicePath;

  //

  // Check that parameters are valid.
  if ((mSimpleTextIn == NULL) || (mDevices == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Print each device.
  Print (L"Output devices:\n");

  Len = StrLen (PROMPT_ANY_KEY);

  for (i = 0; i < mDevicesCount; i++) {
    // Every 10 devices, wait for keystroke.
    if ((i > 0) && ((i % 10) == 0)) {
      Print (PROMPT_ANY_KEY);

      Status = WaitForKey (&KeyValue);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      FlushKeystrokes ();

      // Clear prompt line.
      for (s = 0; s < Len; s++) {
        Print (L"\b");
      }
    }

    TmpDevicePath   = GetRootDevicePath (mDevices[i].DevicePath);
    TextDevicePath  = ConvertDevicePathToText ((TmpDevicePath != NULL) ? TmpDevicePath : mDevices[i].DevicePath, FALSE, FALSE);

    // Print device.
    Print (L"%lu. %s - %s %s (Port: %lu) - %s\n",
      i + 1,
      mDefaultDevices[mDevices[i].OutputPort.Device],
      mLocations[mDevices[i].OutputPort.Location],
      mSurfaces[mDevices[i].OutputPort.Surface],
      mDevices[i].OutputPortIndex,
      TextDevicePath);

    if (TextDevicePath != NULL) {
      FreePool (TextDevicePath);
    }

    if (TmpDevicePath != NULL) {
      FreePool (TmpDevicePath);
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DumpDevices (
  VOID
  )
{
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
  EFI_FILE_PROTOCOL           *RootDir;
  EFI_FILE_PROTOCOL           *Dir;
  CHAR16                      *DirectoryName;
  UINTN                       i;
  UINTN                       Len;

  //

  Status = gBS->HandleProtocol (
    gImageHandle,
    &gEfiLoadedImageProtocolGuid,
    (VOID **)&LoadedImage
    );

  if (!EFI_ERROR (Status) && LoadedImage->DeviceHandle != NULL) {
    RootDir = LocateRootVolume (LoadedImage->DeviceHandle, NULL);

    // Attempt to get self-directory path

    DirectoryName = ConvertDevicePathToText (LoadedImage->FilePath, TRUE, FALSE);
    if (DirectoryName != NULL) {
      UnicodeUefiSlashes (DirectoryName);
      Len = StrLen (DirectoryName);
      for (i = Len; ((i > 0) && (DirectoryName[i] != L'\\')); i--);
      if (i > 0) {
        DirectoryName[i] = L'\0';
      } else {
        DirectoryName[0] = L'\\';
        DirectoryName[1] = L'\0';
      }
    }
  } else {
    RootDir       = NULL;
    DirectoryName = NULL;
  }

  if (RootDir == NULL) {
    Status = FindWritableFileSystem (&RootDir);
    if (EFI_ERROR (Status)) {
      Print (L"No usable filesystem for report - %r\n", Status);
      return EFI_NOT_FOUND;
    }
  }

  Status = SafeFileOpen (
    RootDir,
    &Dir,
    (DirectoryName != NULL) ? DirectoryName : L"\\",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
    EFI_FILE_DIRECTORY
    );
  if (!EFI_ERROR (Status)) {
    Status = OcAudioDump (Dir);
    Dir->Close (Dir);
  }

  RootDir->Close (RootDir);

  if (DirectoryName != NULL) {
    FreePool (DirectoryName);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PrintCurrentDevice (
  VOID
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *TmpDevicePath;
  CHAR16                    *TextDevicePath;

  //

  if (mCurrentDevice == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TmpDevicePath   = GetRootDevicePath (mCurrentDevice->DevicePath);
  TextDevicePath  = ConvertDevicePathToText ((TmpDevicePath != NULL) ? TmpDevicePath : mCurrentDevice->DevicePath, FALSE, FALSE);

  // Success.
  Print (L"Output: %s - %s %s (Port: %lu) - %s\n",
    mDefaultDevices[mCurrentDevice->OutputPort.Device],
    mLocations[mCurrentDevice->OutputPort.Location],
    mSurfaces[mCurrentDevice->OutputPort.Surface],
    mCurrentDevice->OutputPortIndex,
    TextDevicePath);

  if (TextDevicePath != NULL) {
    FreePool (TextDevicePath);
  }

  if (TmpDevicePath != NULL) {
    FreePool (TmpDevicePath);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PrintCurrentSetting (
  VOID
  )
{
  EFI_STATUS    Status;

  //

  Print (L"Volume: (%d)\n", mDeviceVolume);
  Print (L"Total devices: (%d)\n", mDevicesCount);
  Print (L"Sampler: size (%d) freq (%d) bits (%d) chan (%d)\n", mBufferSize, mFrequency, mBits, mChannels);

  Status = PrintCurrentDevice ();

  return Status;
}

STATIC
EFI_STATUS
SelectDevice (
  VOID
  )
{
  EFI_STATUS    Status;
  CHAR16        KeyValue;
  BOOLEAN       Backspace;
  CHAR16        CurrentBuffer[MAX_CHARS + 1];
  UINTN         CurrentCharCount;
  UINTN         DeviceIndex;

  //

  // Check that parameters are valid.
  if ((mSimpleTextIn == NULL) || (mDevices == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // We need a null terminator.
  CurrentBuffer[MAX_CHARS] = 0;

  // Prompt for device number.
  CurrentCharCount = 0;

  Print (L"Enter the device number (0-%lu): ", mDevicesCount);

  while (TRUE) {
    // Wait for key.
    Status = WaitForKey (&KeyValue);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Backspace = (KeyValue == L'\b');

    // If we are backspacing, clear selection.
    if ((CurrentCharCount != 0) && Backspace) {
      CurrentCharCount--;
      Print (L"\b \b");
      continue;
    }

    // If enter, break out.
    if (KeyValue == L'\r') {
      break;
    }

    // If not a number, ignore.
    if (!Backspace && ((KeyValue < L'0') || (KeyValue > '9'))) {
      continue;
    }

    // If no selection, we don't want to backspace.
    // If we are at the max, don't accept any more.
    if (((CurrentCharCount == 0) && Backspace) || (CurrentCharCount >= MAX_CHARS)) {
      continue;
    }

    // Get character.
    CurrentBuffer[CurrentCharCount] = KeyValue;
    Print (L"%c", CurrentBuffer[CurrentCharCount]);
    CurrentCharCount++;
  }
  Print (L"\n");

  // Clear out extra characters.
  SetMem (CurrentBuffer + CurrentCharCount, MAX_CHARS - CurrentCharCount, 0);

  // Get device index.
  DeviceIndex = StrDecimalToUintn (CurrentBuffer);
  if (DeviceIndex == 0) {
    DeviceIndex = 1;
  }
  DeviceIndex -= 1;

  // Ensure index is in range.
  if (DeviceIndex >= mDevicesCount) {
    Print (L"The selected device is not valid.\n");
    return EFI_SUCCESS;
  }

  mCurrentDevice = &mDevices[DeviceIndex];

  Status = PrintCurrentDevice ();

  return Status;
}

EFI_STATUS
EFIAPI
SelectVolume (
  VOID
  )
{
  EFI_STATUS    Status;
  CHAR16        KeyValue;
  BOOLEAN       Backspace;
  CHAR16        CurrentBuffer[MAX_CHARS + 1];
  UINTN         CurrentCharCount;
  UINTN         Volume;

  //

  // Check that parameters are valid.
  if (mSimpleTextIn == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // We need a null terminator.
  CurrentBuffer[MAX_CHARS] = 0;

  // Prompt for device number.
  CurrentCharCount = 0;

  Print (L"Enter the desired volume (0-100): ");

  while (TRUE) {
    // Wait for key.
    Status = WaitForKey (&KeyValue);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Backspace = (KeyValue == L'\b');

    // If we are backspacing, clear selection.
    if ((CurrentCharCount != 0) && Backspace) {
      CurrentCharCount--;
      Print (L"\b \b");
      continue;
    }

    // If enter, break out.
    if (KeyValue == L'\r') {
      break;
    }

    // If not a number, ignore.
    if (!Backspace && ((KeyValue < L'0') || (KeyValue > '9'))) {
      continue;
    }

    // If no selection, we don't want to backspace.
    // If we are at the max, don't accept any more.
    if (((CurrentCharCount == 0) && Backspace) || (CurrentCharCount >= MAX_CHARS)) {
      continue;
    }

    // Get character.
    CurrentBuffer[CurrentCharCount] = KeyValue;
    Print (L"%c", CurrentBuffer[CurrentCharCount]);
    CurrentCharCount++;
  }
  Print (L"\n");

  // Clear out extra characters.
  SetMem (CurrentBuffer + CurrentCharCount, MAX_CHARS - CurrentCharCount, 0);

  // Get device index.
  Volume = StrDecimalToUintn (CurrentBuffer);
  if (Volume > EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME) {
    Volume = EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME;
  }
  mDeviceVolume = (UINT8)Volume;

  // Success.
  Print (L"Volume set to %u\n", mDeviceVolume);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TestOutput (
  VOID
  )
{
  EFI_STATUS              Status;
  EFI_AUDIO_IO_PROTOCOL   *AudioIo;
  UINTN                   OutputIndex;

  //

  if (mCurrentDevice == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AudioIo     = mCurrentDevice->AudioIo;
  OutputIndex = mCurrentDevice->OutputPortIndex;

  // Setup playback.
  Print (L"Playing back audio...\n");

  Status = AudioIo->SetupPlayback (AudioIo, (UINT8)OutputIndex, mDeviceVolume, mFrequency, mBits, mChannels);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Play chime.
  return AudioIo->StartPlayback (AudioIo, mBuffer, mBufferSize, 0);
}

STATIC
VOID
DisplayMenu (
  VOID
  )
{
  // Clear screen.
  if (gST->ConOut != NULL) {
    gST->ConOut->ClearScreen (gST->ConOut);
  }

  // Print menu.
  Print (L"\n");
  Print (L"Configures the AudioDxe EFI driver.\n");
  Print (L"=========================================\n");
  Print (L"%c - List all audio outputs\n", BCFG_ARG_LIST);
  Print (L"%c - Dump audio outputs to file\n", BCFG_ARG_DUMP);
  Print (L"%c - Select audio output\n", BCFG_ARG_SELECT);
  Print (L"%c - Show current setting\n", BCFG_ARG_CURR);
  Print (L"%c - Change volume\n", BCFG_ARG_VOLUME);
  Print (L"%c - Test current audio output\n", BCFG_ARG_TEST);
  Print (L"%c - Quit\n", BCFG_ARG_QUIT);
  Print (L"\n");
  Print (L"Enter an option: ");
}

EFI_STATUS
EFIAPI
AudioDxeCfgMain (
  IN  EFI_HANDLE          ImageHandle,
  IN  EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS    Status;
  BOOLEAN       Backspace;
  CHAR16        KeyValue;
  CHAR16        Selection;

  //

  // Ensure ConIn is valid.
  if (gST->ConIn == NULL) {
    Print (L"There is no console input device.\n");
    Status = EFI_UNSUPPORTED;
    goto DONE;
  }
  mSimpleTextIn = gST->ConIn;

  // Get devices.
  Status = GetOutputDevices ();
  if (EFI_ERROR (Status)) {
    goto DONE;
  }

  // Get decoder, decoding sampler.
  Status = GetAudioDecoder ();
  if (EFI_ERROR (Status)) {
    goto DONE;
  }

  // Command loop.
  while (TRUE) {
    // Show menu.
    DisplayMenu ();

    // Flush any keystrokes.
    FlushKeystrokes ();

    // Handle keyboard input.
    Selection = CHAR_NULL;
    while (TRUE) {
      // Wait for key.
      Status = WaitForKey (&KeyValue);
      if (EFI_ERROR (Status)) {
        goto DONE;
      }

      Backspace = (KeyValue == L'\b');

      // If we are backspacing, clear selection.
      if ((Selection != CHAR_NULL) && Backspace) {
        Selection = CHAR_NULL;
        Print (L"\b \b");
        continue;
      }

      // If enter, break out.
      if (KeyValue == L'\r') {
        break;
      }

      // If lowercase letter, convert to uppercase.
      if ((KeyValue >= 'a') && (KeyValue <= 'z')) {
        KeyValue -= 32;
      }

      // If not a letter, ignore.
      if (!Backspace && ((KeyValue < L'A') || (KeyValue > 'Z'))) {
        continue;
      }

      // If no selection, we don't want to backspace.
      // If we already have a selection, don't accept any more.
      if ((Selection != CHAR_NULL) || Backspace) {
        continue;
      }

      // Get selection.
      Selection = KeyValue;
      Print (L"%c", Selection);
    }
    Print (L"\n\n");

    // Flush any keystrokes.
    FlushKeystrokes ();

    // Execute command.
    switch (Selection) {
      // List devices.
      case BCFG_ARG_LIST:
        Status = PrintDevices ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Dump devices.
      case BCFG_ARG_DUMP:
        Status = DumpDevices ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Select device.
      case BCFG_ARG_SELECT:
        Status = SelectDevice ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Print current setting.
      case BCFG_ARG_CURR:
        Status = PrintCurrentSetting ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Select volume.
      case BCFG_ARG_VOLUME:
        Status = SelectVolume ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Test playback.
      case BCFG_ARG_TEST:
        Status = TestOutput ();
        if (EFI_ERROR (Status)) {
          goto DONE;
        }
        break;

      // Quit.
      case BCFG_ARG_QUIT:
        Status = EFI_SUCCESS;
        goto DONE;

      default:
        Print (L"Invalid option.\n");
    }

    // Wait for keystroke.
    Print (L"\n");
    Print (PROMPT_ANY_KEY);

    WaitForKey (&KeyValue);

    FlushKeystrokes ();
  }

  DONE:

  if (mDevices != NULL) {
    FreePool (mDevices);
  }

  if (mBuffer != NULL) {
    FreePool (mBuffer);
  }

  // Show error.
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      Print (L"No audio outputs were found. Ensure AudioDxe is loaded.\n", Status);
    } else {
      Print (L"The command failed with error: %r\n", Status);
    }
  }

  return Status;
}
