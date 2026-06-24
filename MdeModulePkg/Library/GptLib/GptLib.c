/** @file
  GUID Partition Table handling.

  Copyright (c) 2026, SUSE LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/Gpt.h>
#include <IndustryStandard/Mbr.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/GptLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SafeIntLib.h>

#define GPT_HEADER_REVISION_1_0  0x00010000

STATIC
UINT32
ReadUnalignedUint32 (
  IN CONST UINT8  Value[4]
  )
{
  return (UINT32)(
           (UINT32)Value[0] |
           ((UINT32)Value[1] << 8) |
           ((UINT32)Value[2] << 16) |
           ((UINT32)Value[3] << 24)
           );
}

STATIC
EFI_STATUS
GptReadDisk (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo,
  IN EFI_LBA                Lba,
  IN UINTN                  Size,
  OUT VOID                  *Buffer
  )
{
  RETURN_STATUS  Status;
  UINT64         Offset;

  Status = SafeUint64Mult (Lba, BlockIo->Media->BlockSize, &Offset);
  if (RETURN_ERROR (Status)) {
    return EFI_VOLUME_CORRUPTED;
  }

  return DiskIo->ReadDisk (
                   DiskIo,
                   BlockIo->Media->MediaId,
                   Offset,
                   Size,
                   Buffer
                   );
}

STATIC
BOOLEAN
GptHeaderCrcValid (
  IN OUT EFI_PARTITION_TABLE_HEADER  *Header,
  IN UINT32                            BlockSize
  )
{
  UINT32                      HeaderCrc;
  UINT32                      CalculatedCrc;

  if ((Header->Header.HeaderSize != sizeof (EFI_PARTITION_TABLE_HEADER)) ||
      (Header->Header.HeaderSize > BlockSize))
  {
    return FALSE;
  }

  HeaderCrc            = Header->Header.CRC32;
  Header->Header.CRC32 = 0;
  CalculatedCrc        = CalculateCrc32 (Header, Header->Header.HeaderSize);
  Header->Header.CRC32 = HeaderCrc;
  return (BOOLEAN)(CalculatedCrc == HeaderCrc);
}

STATIC
BOOLEAN
GptEntrySizeValid (
  IN UINT32  EntrySize
  )
{
  UINT32  SizeFactor;

  if (EntrySize < sizeof (EFI_PARTITION_ENTRY)) {
    return FALSE;
  }

  SizeFactor = EntrySize / sizeof (EFI_PARTITION_ENTRY);
  return (BOOLEAN)(
           ((EntrySize % sizeof (EFI_PARTITION_ENTRY)) == 0) &&
           ((SizeFactor & (SizeFactor - 1)) == 0)
           );
}

STATIC
EFI_STATUS
GptGetEntryArrayBounds (
  IN  CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN  EFI_BLOCK_IO_PROTOCOL             *BlockIo,
  OUT UINTN                             *ArraySize,
  OUT EFI_LBA                           *ArrayLastLba
  )
{
  RETURN_STATUS  Status;
  UINT64         ArraySize64;
  UINT64         ArrayBlockCount;
  UINT64         LastLba;

  if ((Header->NumberOfPartitionEntries == 0) ||
      !GptEntrySizeValid (Header->SizeOfPartitionEntry))
  {
    return EFI_VOLUME_CORRUPTED;
  }

  Status = SafeUint64Mult (
             Header->NumberOfPartitionEntries,
             Header->SizeOfPartitionEntry,
             &ArraySize64
             );
  if (RETURN_ERROR (Status) || (ArraySize64 > MAX_UINTN)) {
    return EFI_VOLUME_CORRUPTED;
  }

  ArrayBlockCount = DivU64x32Remainder (
                      ArraySize64,
                      BlockIo->Media->BlockSize,
                      NULL
                      );
  if ((ArraySize64 % BlockIo->Media->BlockSize) != 0) {
    ArrayBlockCount++;
  }

  if ((ArrayBlockCount == 0) ||
      RETURN_ERROR (SafeUint64Add (
                      Header->PartitionEntryLBA,
                      ArrayBlockCount - 1,
                      &LastLba
                      )) ||
      (LastLba > BlockIo->Media->LastBlock))
  {
    return EFI_VOLUME_CORRUPTED;
  }

  *ArraySize    = (UINTN)ArraySize64;
  *ArrayLastLba = LastLba;
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
GptHeaderFieldsValid (
  IN CONST EFI_PARTITION_TABLE_HEADER  *Header,
  IN EFI_BLOCK_IO_PROTOCOL             *BlockIo,
  IN EFI_LBA                           HeaderLba,
  IN EFI_LBA                           AlternateLba,
  IN EFI_LBA                           ArrayLastLba
  )
{
  BOOLEAN  IsPrimary;

  IsPrimary = (BOOLEAN)(HeaderLba == PRIMARY_PART_HEADER_LBA);

  if ((Header->Header.Signature != EFI_PTAB_HEADER_ID) ||
      (Header->Header.Revision != GPT_HEADER_REVISION_1_0) ||
      (Header->Header.Reserved != 0) ||
      (Header->MyLBA != HeaderLba) ||
      (Header->AlternateLBA != AlternateLba) ||
      (Header->FirstUsableLBA > Header->LastUsableLBA) ||
      (Header->LastUsableLBA >= BlockIo->Media->LastBlock))
  {
    return FALSE;
  }

  if (IsPrimary) {
    return (BOOLEAN)(
             (Header->PartitionEntryLBA > HeaderLba) &&
             (ArrayLastLba < Header->FirstUsableLBA)
             );
  }

  return (BOOLEAN)(
           (Header->PartitionEntryLBA > Header->LastUsableLBA) &&
           (ArrayLastLba < HeaderLba)
           );
}

EFI_STATUS
EFIAPI
GptValidateProtectiveMbr (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo
  )
{
  EFI_STATUS          Status;
  MASTER_BOOT_RECORD  *Mbr;
  UINTN               Index;
  UINT32              ProtectiveSize;

  if ((BlockIo == NULL) || (BlockIo->Media == NULL) || (DiskIo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (BlockIo->Media->BlockSize < sizeof (MASTER_BOOT_RECORD)) {
    return EFI_VOLUME_CORRUPTED;
  }

  Mbr = AllocatePool (BlockIo->Media->BlockSize);
  if (Mbr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GptReadDisk (BlockIo, DiskIo, 0, BlockIo->Media->BlockSize, Mbr);
  if (EFI_ERROR (Status)) {
    FreePool (Mbr);
    return Status;
  }

  Status = EFI_VOLUME_CORRUPTED;
  ProtectiveSize = (BlockIo->Media->LastBlock > MAX_UINT32) ?
                   MAX_UINT32 :
                   (UINT32)BlockIo->Media->LastBlock;
  if (Mbr->Signature == MBR_SIGNATURE) {
    for (Index = 0; Index < MAX_MBR_PARTITIONS; Index++) {
      if ((Mbr->Partition[Index].OSIndicator == PMBR_GPT_PARTITION) &&
          (Mbr->Partition[Index].BootIndicator == 0) &&
          (ReadUnalignedUint32 (Mbr->Partition[Index].StartingLBA) == 1) &&
          (ReadUnalignedUint32 (Mbr->Partition[Index].SizeInLBA) == ProtectiveSize))
      {
        Status = EFI_SUCCESS;
        break;
      }
    }
  }

  FreePool (Mbr);
  return Status;
}

EFI_STATUS
EFIAPI
GptReadAndValidateTable (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  IN  EFI_LBA                HeaderLba,
  IN  EFI_LBA                AlternateLba,
  OUT GPT_VALIDATED_TABLE    *Table
  )
{
  EFI_STATUS                  Status;
  EFI_PARTITION_TABLE_HEADER  *HeaderBlock;
  EFI_LBA                     ArrayLastLba;
  UINTN                       ArraySize;
  VOID                        *Entries;

  if ((BlockIo == NULL) || (BlockIo->Media == NULL) ||
      (DiskIo == NULL) || (Table == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Table, sizeof (*Table));
  if ((BlockIo->Media->BlockSize < sizeof (EFI_PARTITION_TABLE_HEADER)) ||
      (HeaderLba > BlockIo->Media->LastBlock) ||
      (AlternateLba > BlockIo->Media->LastBlock))
  {
    return EFI_VOLUME_CORRUPTED;
  }

  HeaderBlock = AllocateZeroPool (BlockIo->Media->BlockSize);
  if (HeaderBlock == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GptReadDisk (
             BlockIo,
             DiskIo,
             HeaderLba,
             BlockIo->Media->BlockSize,
             HeaderBlock
             );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (!GptHeaderCrcValid (HeaderBlock, BlockIo->Media->BlockSize)) {
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  Status = GptGetEntryArrayBounds (
             HeaderBlock,
             BlockIo,
             &ArraySize,
             &ArrayLastLba
             );
  if (EFI_ERROR (Status) ||
      !GptHeaderFieldsValid (
         HeaderBlock,
         BlockIo,
         HeaderLba,
         AlternateLba,
         ArrayLastLba
         ))
  {
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  Entries = AllocatePool (ArraySize);
  if (Entries == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GptReadDisk (
             BlockIo,
             DiskIo,
             HeaderBlock->PartitionEntryLBA,
             ArraySize,
             Entries
             );
  if (EFI_ERROR (Status)) {
    FreePool (Entries);
    goto Done;
  }

  if (CalculateCrc32 (Entries, ArraySize) != HeaderBlock->PartitionEntryArrayCRC32) {
    FreePool (Entries);
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  CopyMem (&Table->Header, HeaderBlock, sizeof (Table->Header));
  Table->PartitionEntries        = Entries;
  Table->PartitionEntryArraySize = ArraySize;
  Status                         = EFI_SUCCESS;

Done:
  FreePool (HeaderBlock);
  return Status;
}

STATIC
BOOLEAN
GptTablesConsistent (
  IN CONST GPT_VALIDATED_TABLE  *Primary,
  IN CONST GPT_VALIDATED_TABLE  *Backup
  )
{
  return (BOOLEAN)(
           CompareGuid (&Primary->Header.DiskGUID, &Backup->Header.DiskGUID) &&
           (Primary->Header.FirstUsableLBA == Backup->Header.FirstUsableLBA) &&
           (Primary->Header.LastUsableLBA == Backup->Header.LastUsableLBA) &&
           (Primary->Header.NumberOfPartitionEntries == Backup->Header.NumberOfPartitionEntries) &&
           (Primary->Header.SizeOfPartitionEntry == Backup->Header.SizeOfPartitionEntry) &&
           (Primary->Header.PartitionEntryArrayCRC32 == Backup->Header.PartitionEntryArrayCRC32) &&
           (Primary->PartitionEntryArraySize == Backup->PartitionEntryArraySize) &&
           (CompareMem (
              Primary->PartitionEntries,
              Backup->PartitionEntries,
              Primary->PartitionEntryArraySize
              ) == 0)
           );
}

EFI_STATUS
EFIAPI
GptParseAndValidate (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT GPT_CANONICAL_VIEW     *GptView
  )
{
  EFI_STATUS           Status;
  GPT_VALIDATED_TABLE  Primary;
  GPT_VALIDATED_TABLE  Backup;

  if (GptView == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (GptView, sizeof (*GptView));
  ZeroMem (&Primary, sizeof (Primary));
  ZeroMem (&Backup, sizeof (Backup));

  Status = GptValidateProtectiveMbr (BlockIo, DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GptReadAndValidateTable (
             BlockIo,
             DiskIo,
             PRIMARY_PART_HEADER_LBA,
             BlockIo->Media->LastBlock,
             &Primary
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = GptReadAndValidateTable (
             BlockIo,
             DiskIo,
             BlockIo->Media->LastBlock,
             PRIMARY_PART_HEADER_LBA,
             &Backup
             );
  if (EFI_ERROR (Status)) {
    GptFreeValidatedTable (&Primary);
    return Status;
  }

  if (!GptTablesConsistent (&Primary, &Backup)) {
    GptFreeValidatedTable (&Primary);
    GptFreeValidatedTable (&Backup);
    return EFI_VOLUME_CORRUPTED;
  }

  CopyMem (&GptView->PrimaryHeader, &Primary.Header, sizeof (Primary.Header));
  CopyMem (&GptView->BackupHeader, &Backup.Header, sizeof (Backup.Header));
  GptView->PartitionEntries        = Primary.PartitionEntries;
  GptView->PartitionEntryArraySize = Primary.PartitionEntryArraySize;
  Primary.PartitionEntries         = NULL;

  GptFreeValidatedTable (&Primary);
  GptFreeValidatedTable (&Backup);
  return EFI_SUCCESS;
}

VOID
EFIAPI
GptFreeValidatedTable (
  IN OUT GPT_VALIDATED_TABLE  *Table
  )
{
  if (Table == NULL) {
    return;
  }

  if (Table->PartitionEntries != NULL) {
    FreePool (Table->PartitionEntries);
  }

  ZeroMem (Table, sizeof (*Table));
}

VOID
EFIAPI
GptFreeCanonicalView (
  IN OUT GPT_CANONICAL_VIEW  *GptView
  )
{
  if (GptView == NULL) {
    return;
  }

  if (GptView->PartitionEntries != NULL) {
    FreePool (GptView->PartitionEntries);
  }

  ZeroMem (GptView, sizeof (*GptView));
}
