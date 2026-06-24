/** @file
  Canonical GPT recovery for PartitionDxe.

  Copyright (c) 2026, SUSE LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Partition.h"
#include "Gpt.h"

STATIC
BOOLEAN
PartitionRestoreGptTable (
  IN EFI_BLOCK_IO_PROTOCOL       *BlockIo,
  IN EFI_DISK_IO_PROTOCOL        *DiskIo,
  IN EFI_PARTITION_TABLE_HEADER  *PartHeader
  )
{
  EFI_STATUS                  Status;
  UINTN                       BlockSize;
  EFI_PARTITION_TABLE_HEADER  *RestoredHeader;
  EFI_LBA                     PartitionEntryLba;
  UINT8                       *Entries;
  UINTN                       EntryArraySize;

  BlockSize     = BlockIo->Media->BlockSize;
  RestoredHeader = AllocateZeroPool (BlockSize);
  Entries        = NULL;
  if (RestoredHeader == NULL) {
    return FALSE;
  }

  PartitionEntryLba = (PartHeader->MyLBA == PRIMARY_PART_HEADER_LBA) ?
                      (PartHeader->LastUsableLBA + 1) :
                      (PRIMARY_PART_HEADER_LBA + 1);
  EntryArraySize = PartHeader->NumberOfPartitionEntries * PartHeader->SizeOfPartitionEntry;

  CopyMem (RestoredHeader, PartHeader, sizeof (*RestoredHeader));
  RestoredHeader->MyLBA             = PartHeader->AlternateLBA;
  RestoredHeader->AlternateLBA      = PartHeader->MyLBA;
  RestoredHeader->PartitionEntryLBA = PartitionEntryLba;
  RestoredHeader->Header.CRC32      = 0;
  RestoredHeader->Header.CRC32      = CalculateCrc32 (
                                        RestoredHeader,
                                        RestoredHeader->Header.HeaderSize
                                        );

  Entries = AllocatePool (EntryArraySize);
  if (Entries == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = DiskIo->ReadDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     MultU64x32 (PartHeader->PartitionEntryLBA, (UINT32)BlockSize),
                     EntryArraySize,
                     Entries
                     );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = DiskIo->WriteDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     MultU64x32 (PartitionEntryLba, (UINT32)BlockSize),
                     EntryArraySize,
                     Entries
                     );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = DiskIo->WriteDisk (
                     DiskIo,
                     BlockIo->Media->MediaId,
                     MultU64x32 (RestoredHeader->MyLBA, (UINT32)BlockSize),
                     BlockSize,
                     RestoredHeader
                     );

Done:
  if (Entries != NULL) {
    FreePool (Entries);
  }

  FreePool (RestoredHeader);
  return (BOOLEAN)!EFI_ERROR (Status);
}

EFI_STATUS
PartitionGetCanonicalGpt (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT GPT_CANONICAL_VIEW     *GptView
  )
{
  EFI_STATUS           Status;
  EFI_STATUS           PrimaryStatus;
  EFI_STATUS           BackupStatus;
  GPT_VALIDATED_TABLE  PrimaryTable;
  GPT_VALIDATED_TABLE  BackupTable;

  ZeroMem (&PrimaryTable, sizeof (PrimaryTable));
  ZeroMem (&BackupTable, sizeof (BackupTable));

  Status = GptParseAndValidate (BlockIo, DiskIo, GptView);
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  Status = GptValidateProtectiveMbr (BlockIo, DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PrimaryStatus = GptReadAndValidateTable (
                    BlockIo,
                    DiskIo,
                    PRIMARY_PART_HEADER_LBA,
                    BlockIo->Media->LastBlock,
                    &PrimaryTable
                    );
  BackupStatus = GptReadAndValidateTable (
                   BlockIo,
                   DiskIo,
                   BlockIo->Media->LastBlock,
                   PRIMARY_PART_HEADER_LBA,
                   &BackupTable
                   );

  if (EFI_ERROR (PrimaryStatus) && EFI_ERROR (BackupStatus)) {
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  if (!EFI_ERROR (PrimaryStatus) && !EFI_ERROR (BackupStatus)) {
    DEBUG ((DEBUG_ERROR, "Primary and backup GPT tables are inconsistent\n"));
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  if (EFI_ERROR (PrimaryStatus)) {
    DEBUG ((DEBUG_INFO, "Restore primary GPT table from validated backup\n"));
    if (!PartitionRestoreGptTable (BlockIo, DiskIo, &BackupTable.Header)) {
      Status = EFI_DEVICE_ERROR;
      goto Done;
    }
  } else {
    DEBUG ((DEBUG_INFO, "Restore backup GPT table from validated primary\n"));
    if (!PartitionRestoreGptTable (BlockIo, DiskIo, &PrimaryTable.Header)) {
      Status = EFI_DEVICE_ERROR;
      goto Done;
    }
  }

  Status = GptParseAndValidate (BlockIo, DiskIo, GptView);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Restored GPT pair failed canonical validation\n"));
  }

Done:
  GptFreeValidatedTable (&PrimaryTable);
  GptFreeValidatedTable (&BackupTable);
  return Status;
}
