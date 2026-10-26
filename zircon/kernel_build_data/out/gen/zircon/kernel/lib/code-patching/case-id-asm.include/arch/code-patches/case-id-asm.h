// This file is generated.  DO NOT EDIT!

#ifndef _ARCH_CODE_PATCHES_CASE_ID_ASM_H_
#define _ARCH_CODE_PATCHES_CASE_ID_ASM_H_ 1

#ifdef __aarch64__
#define CASE_ID_SELF_TEST 0
#define CASE_ID_SMCCC_CONDUIT 0x1
#define CASE_ID_SMCCC_WORKAROUND_FUNCTION 0x2
#endif  // __aarch64__
#ifdef __riscv
#define CASE_ID_SELF_TEST 0
#endif  // __riscv
#ifdef __x86_64__
#define CASE_ID_SELF_TEST 0
#define CASE_ID_SWAPGS_MITIGATION 0x1
#define CASE_ID_MDS_TAA_MITIGATION 0x2
#define CASE_ID__X86_COPY_TO_OR_FROM_USER 0x3
#define CASE_ID___X86_INDIRECT_THUNK_R11 0x4
#define CASE_ID___UNSANITIZED_MEMCPY 0x5
#define CASE_ID___UNSANITIZED_MEMSET 0x6
#endif  // __x86_64__

#endif  // _ARCH_CODE_PATCHES_CASE_ID_ASM_H_
