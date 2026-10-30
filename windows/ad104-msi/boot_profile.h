/* SPDX-License-Identifier: GPL-2.0-only */
/* Keep the local ESP identifier out of the public source tree. */
#pragma once
#if defined(GU_RTX4080S_MSI)
#include "rtx4080s_boot_profile.h"
#elif defined(GU_HOST_TEST_PROFILE)
#define GU_WINDOWS_PARTITION_INITIALIZER {0x11111111,0x2222,0x3333,{0x84,0x44,0x55,0x66,0x77,0x88,0x99,0xaa}}
#else
#include "local/boot_profile.h"
#ifndef GU_WINDOWS_PARTITION_INITIALIZER
#error "Configure the reviewed ESP GUID in local/boot_profile.h before building."
#endif
#endif
#ifndef GU_WINDOWS_PARTITION_NUMBER
#define GU_WINDOWS_PARTITION_NUMBER 2
#define GU_WINDOWS_PARTITION_START 1085440
#define GU_WINDOWS_PARTITION_SECTORS 202752
#define GU_WINDOWS_IMAGE_BYTES 3086728
#define GU_WINDOWS_IMAGE_SHA256 "b6f5ff8a5f605840796875259bba555d0642745a7ac8a5b38d16603768b22570"
#endif
