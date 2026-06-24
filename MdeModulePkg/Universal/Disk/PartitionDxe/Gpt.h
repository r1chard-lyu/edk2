/** @file
  GPT recovery interfaces for PartitionDxe.

  Copyright (c) 2026, SUSE LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#pragma once

#include <Library/GptLib.h>

EFI_STATUS
PartitionGetCanonicalGpt (
  IN  EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN  EFI_DISK_IO_PROTOCOL   *DiskIo,
  OUT GPT_CANONICAL_VIEW     *GptView
  );
