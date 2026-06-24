/** @file
  GUID Partition Table handling.

  Copyright (c) 2026, SUSE LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>

typedef struct {
  EFI_PARTITION_TABLE_HEADER    Header;
  VOID                          *PartitionEntries;
  UINTN                         PartitionEntryArraySize;
} GPT_VALIDATED_TABLE;

typedef struct {
  EFI_PARTITION_TABLE_HEADER    PrimaryHeader;
  EFI_PARTITION_TABLE_HEADER    BackupHeader;
  VOID                          *PartitionEntries;
  UINTN                         PartitionEntryArraySize;
} GPT_CANONICAL_VIEW;

/**
  Validate the protective MBR on a block device.

  @param[in] BlockIo  Parent Block I/O protocol.
  @param[in] DiskIo   Parent Disk I/O protocol.

  @retval EFI_SUCCESS           The protective MBR is valid.
  @retval EFI_INVALID_PARAMETER A required parameter is NULL.
  @retval EFI_VOLUME_CORRUPTED  The protective MBR is invalid.
  @retval other                 The disk read failed.
**/
EFI_STATUS
EFIAPI
GptValidateProtectiveMbr (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN EFI_DISK_IO_PROTOCOL   *DiskIo
  );

/**
  Read and validate one GPT header and its partition entry array.

  HeaderLba and AlternateLba are trusted locations supplied by the caller.
  The on-disk header must name exactly those locations.

  @param[in]  BlockIo      Parent Block I/O protocol.
  @param[in]  DiskIo       Parent Disk I/O protocol.
  @param[in]  HeaderLba    Required location of this GPT header.
  @param[in]  AlternateLba Required location of its alternate header.
  @param[out] Table        Validated table. The caller must release it with
                           GptFreeValidatedTable().

  @retval EFI_SUCCESS           The table is valid.
  @retval EFI_INVALID_PARAMETER A required parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @retval EFI_VOLUME_CORRUPTED  The table is invalid.
  @retval other                 A disk read failed.
**/
EFI_STATUS
EFIAPI
GptReadAndValidateTable (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  IN  EFI_LBA                HeaderLba,
  IN  EFI_LBA                AlternateLba,
  OUT GPT_VALIDATED_TABLE    *Table
  );

/**
  Parse and validate a complete GPT into one canonical view.

  This validates the protective MBR, primary and backup tables, fixed header
  locations, entry array bounds and CRCs, and pair consistency.

  @param[in]  BlockIo Parent Block I/O protocol.
  @param[in]  DiskIo  Parent Disk I/O protocol.
  @param[out] GptView Canonical GPT view. The caller must release it with
                      GptFreeCanonicalView().

  @retval EFI_SUCCESS           The complete GPT is valid and consistent.
  @retval EFI_INVALID_PARAMETER A required parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @retval EFI_VOLUME_CORRUPTED  The GPT is invalid or inconsistent.
  @retval other                 A disk read failed.
**/
EFI_STATUS
EFIAPI
GptParseAndValidate (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT GPT_CANONICAL_VIEW     *GptView
  );

VOID
EFIAPI
GptFreeValidatedTable (
  IN OUT GPT_VALIDATED_TABLE  *Table
  );

VOID
EFIAPI
GptFreeCanonicalView (
  IN OUT GPT_CANONICAL_VIEW  *GptView
  );
