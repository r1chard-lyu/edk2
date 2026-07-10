## @file
# MdeModule DSC include file for [LibraryClasses*] section of all Architectures.
#
# This file can be included to the [LibraryClasses*] section(s) of a platform
# DSC file by using "!include MdeModulePkg/MdeModuleLibs.dsc.inc" to specify
# the library instances of some EDKII basic/common library classes provided
# by MdeModulePkg.
#
# Copyright (c) 2026, SUSE LLC. All rights reserved.<BR>
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[LibraryClasses]
  GptLib|MdeModulePkg/Library/GptLib/GptLib.inf
