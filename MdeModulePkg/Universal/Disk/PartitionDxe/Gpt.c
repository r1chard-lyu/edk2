/** @file
  Decode a hard disk partitioned with the GPT scheme in the UEFI 2.0
  specification.

  Caution: This file requires additional review when modified.
  This driver will have external input - disk partition.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

  PartitionInstallGptChildHandles() routine will read disk partition content and
  do basic validation before PartitionInstallChildHandle().

  GptLib and PartitionCheckGptEntry() validate the GPT table and GPT entries.

Copyright (c) 2018 Qualcomm Datacenter Technologies, Inc.
Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Partition.h"
#include "Gpt.h"

/**
  This routine will check GPT partition entry and return entry status.

  Caution: This function may receive untrusted input.
  The GPT partition entry is external input, so this routine
  will do basic validation for GPT partition entry and report status.

  @param[in]    PartHeader    Partition table header structure
  @param[in]    PartEntry     The partition entry array
  @param[out]   PEntryStatus  the partition entry status array
                              recording the status of each partition

**/
VOID
PartitionCheckGptEntry (
  IN  EFI_PARTITION_TABLE_HEADER  *PartHeader,
  IN  EFI_PARTITION_ENTRY         *PartEntry,
  OUT EFI_PARTITION_ENTRY_STATUS  *PEntryStatus
  );

/**
  Install child handles if the Handle supports GPT partition structure.

  Caution: This function may receive untrusted input.
  The GPT partition table is external input, so this routine
  will do basic validation for GPT partition table before install
  child handle for each GPT partition.

  @param[in]  This       Calling context.
  @param[in]  Handle     Parent Handle.
  @param[in]  DiskIo     Parent DiskIo interface.
  @param[in]  DiskIo2    Parent DiskIo2 interface.
  @param[in]  BlockIo    Parent BlockIo interface.
  @param[in]  BlockIo2   Parent BlockIo2 interface.
  @param[in]  DevicePath Parent Device Path.

  @retval EFI_SUCCESS           Valid GPT disk.
  @retval EFI_MEDIA_CHANGED     Media changed Detected.
  @retval other                 Not a valid GPT disk.

**/
EFI_STATUS
PartitionInstallGptChildHandles (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  EFI_DISK_IO_PROTOCOL         *DiskIo,
  IN  EFI_DISK_IO2_PROTOCOL        *DiskIo2,
  IN  EFI_BLOCK_IO_PROTOCOL        *BlockIo,
  IN  EFI_BLOCK_IO2_PROTOCOL       *BlockIo2,
  IN  EFI_DEVICE_PATH_PROTOCOL     *DevicePath
  )
{
  EFI_STATUS                   Status;
  UINT32                       BlockSize;
  EFI_LBA                      LastBlock;
  EFI_PARTITION_TABLE_HEADER   *PrimaryHeader;
  EFI_PARTITION_ENTRY          *PartEntry;
  EFI_PARTITION_ENTRY          *Entry;
  EFI_PARTITION_ENTRY_STATUS   *PEntryStatus;
  UINTN                        Index;
  EFI_STATUS                   GptValidStatus;
  HARDDRIVE_DEVICE_PATH        HdDev;
  EFI_PARTITION_INFO_PROTOCOL  PartitionInfo;
  GPT_CANONICAL_VIEW           GptView;

  PrimaryHeader = NULL;
  PartEntry     = NULL;
  PEntryStatus  = NULL;
  ZeroMem (&GptView, sizeof (GptView));

  BlockSize = BlockIo->Media->BlockSize;
  LastBlock = BlockIo->Media->LastBlock;

  DEBUG ((DEBUG_INFO, " BlockSize : %d \n", BlockSize));
  DEBUG ((DEBUG_INFO, " LastBlock : %lx \n", LastBlock));

  GptValidStatus = EFI_NOT_FOUND;

  Status = PartitionGetCanonicalGpt (BlockIo, DiskIo, &GptView);
  if (EFI_ERROR (Status)) {
    GptValidStatus = Status;
    goto Done;
  }

  PrimaryHeader = &GptView.PrimaryHeader;
  PartEntry     = (EFI_PARTITION_ENTRY *)GptView.PartitionEntries;
  DEBUG ((DEBUG_INFO, " Valid canonical primary and backup GPT\n"));

  DEBUG ((DEBUG_INFO, " Number of partition entries: %d\n", PrimaryHeader->NumberOfPartitionEntries));

  PEntryStatus = AllocateZeroPool (PrimaryHeader->NumberOfPartitionEntries * sizeof (EFI_PARTITION_ENTRY_STATUS));
  if (PEntryStatus == NULL) {
    DEBUG ((DEBUG_ERROR, "Allocate pool error\n"));
    goto Done;
  }

  //
  // Check the integrity of partition entries
  //
  PartitionCheckGptEntry (PrimaryHeader, PartEntry, PEntryStatus);

  //
  // If we got this far the GPT layout of the disk is valid and we should return true
  //
  GptValidStatus = EFI_SUCCESS;

  //
  // Create child device handles
  //
  for (Index = 0; Index < PrimaryHeader->NumberOfPartitionEntries; Index++) {
    Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)PartEntry + Index * PrimaryHeader->SizeOfPartitionEntry);
    if (CompareGuid (&Entry->PartitionTypeGUID, &gEfiPartTypeUnusedGuid) ||
        PEntryStatus[Index].OutOfRange ||
        PEntryStatus[Index].Overlap ||
        PEntryStatus[Index].OsSpecific
        )
    {
      //
      // Don't use null EFI Partition Entries, Invalid Partition Entries or OS specific
      // partition Entries
      //
      continue;
    }

    ZeroMem (&HdDev, sizeof (HdDev));
    HdDev.Header.Type    = MEDIA_DEVICE_PATH;
    HdDev.Header.SubType = MEDIA_HARDDRIVE_DP;
    SetDevicePathNodeLength (&HdDev.Header, sizeof (HdDev));

    HdDev.PartitionNumber = (UINT32)Index + 1;
    HdDev.MBRType         = MBR_TYPE_EFI_PARTITION_TABLE_HEADER;
    HdDev.SignatureType   = SIGNATURE_TYPE_GUID;
    HdDev.PartitionStart  = Entry->StartingLBA;
    HdDev.PartitionSize   = Entry->EndingLBA - Entry->StartingLBA + 1;
    CopyMem (HdDev.Signature, &Entry->UniquePartitionGUID, sizeof (EFI_GUID));

    ZeroMem (&PartitionInfo, sizeof (EFI_PARTITION_INFO_PROTOCOL));
    PartitionInfo.Revision = EFI_PARTITION_INFO_PROTOCOL_REVISION;
    PartitionInfo.Type     = PARTITION_TYPE_GPT;
    if (CompareGuid (&Entry->PartitionTypeGUID, &gEfiPartTypeSystemPartGuid)) {
      PartitionInfo.System = 1;
    }

    CopyMem (&PartitionInfo.Info.Gpt, Entry, sizeof (EFI_PARTITION_ENTRY));

    DEBUG ((DEBUG_INFO, " Index : %d\n", (UINT32)Index));
    DEBUG ((DEBUG_INFO, " Start LBA : %lx\n", (UINT64)HdDev.PartitionStart));
    DEBUG ((DEBUG_INFO, " End LBA : %lx\n", (UINT64)Entry->EndingLBA));
    DEBUG ((DEBUG_INFO, " Partition size: %lx\n", (UINT64)HdDev.PartitionSize));
    DEBUG ((DEBUG_INFO, " Start : %lx", MultU64x32 (Entry->StartingLBA, BlockSize)));
    DEBUG ((DEBUG_INFO, " End : %lx\n", MultU64x32 (Entry->EndingLBA, BlockSize)));

    Status = PartitionInstallChildHandle (
               This,
               Handle,
               DiskIo,
               DiskIo2,
               BlockIo,
               BlockIo2,
               DevicePath,
               (EFI_DEVICE_PATH_PROTOCOL *)&HdDev,
               &PartitionInfo,
               Entry->StartingLBA,
               Entry->EndingLBA,
               BlockSize,
               &Entry->PartitionTypeGUID
               );
  }

  DEBUG ((DEBUG_INFO, "Prepare to Free Pool\n"));

Done:
  if (PEntryStatus != NULL) {
    FreePool (PEntryStatus);
  }

  GptFreeCanonicalView (&GptView);
  return GptValidStatus;
}

/**
  This routine will check GPT partition entry and return entry status.

  Caution: This function may receive untrusted input.
  The GPT partition entry is external input, so this routine
  will do basic validation for GPT partition entry and report status.

  @param[in]    PartHeader    Partition table header structure
  @param[in]    PartEntry     The partition entry array
  @param[out]   PEntryStatus  the partition entry status array
                              recording the status of each partition

**/
VOID
PartitionCheckGptEntry (
  IN  EFI_PARTITION_TABLE_HEADER  *PartHeader,
  IN  EFI_PARTITION_ENTRY         *PartEntry,
  OUT EFI_PARTITION_ENTRY_STATUS  *PEntryStatus
  )
{
  EFI_LBA              StartingLBA;
  EFI_LBA              EndingLBA;
  EFI_PARTITION_ENTRY  *Entry;
  UINTN                Index1;
  UINTN                Index2;

  DEBUG ((DEBUG_INFO, " start check partition entries\n"));
  for (Index1 = 0; Index1 < PartHeader->NumberOfPartitionEntries; Index1++) {
    Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)PartEntry + Index1 * PartHeader->SizeOfPartitionEntry);
    if (CompareGuid (&Entry->PartitionTypeGUID, &gEfiPartTypeUnusedGuid)) {
      continue;
    }

    StartingLBA = Entry->StartingLBA;
    EndingLBA   = Entry->EndingLBA;
    if ((StartingLBA > EndingLBA) ||
        (StartingLBA < PartHeader->FirstUsableLBA) ||
        (StartingLBA > PartHeader->LastUsableLBA) ||
        (EndingLBA < PartHeader->FirstUsableLBA) ||
        (EndingLBA > PartHeader->LastUsableLBA)
        )
    {
      PEntryStatus[Index1].OutOfRange = TRUE;
      continue;
    }

    if ((Entry->Attributes & BIT1) != 0) {
      //
      // If Bit 1 is set, this indicate that this is an OS specific GUID partition.
      //
      PEntryStatus[Index1].OsSpecific = TRUE;
    }

    for (Index2 = Index1 + 1; Index2 < PartHeader->NumberOfPartitionEntries; Index2++) {
      Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)PartEntry + Index2 * PartHeader->SizeOfPartitionEntry);
      if (CompareGuid (&Entry->PartitionTypeGUID, &gEfiPartTypeUnusedGuid)) {
        continue;
      }

      if ((Entry->EndingLBA >= StartingLBA) && (Entry->StartingLBA <= EndingLBA)) {
        //
        // This region overlaps with the Index1'th region
        //
        PEntryStatus[Index1].Overlap = TRUE;
        PEntryStatus[Index2].Overlap = TRUE;
        continue;
      }
    }
  }

  DEBUG ((DEBUG_INFO, " End check partition entries\n"));
}
