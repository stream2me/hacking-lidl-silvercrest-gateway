/**
 * @file sli_cpc_protocol_v6.h
 * @brief CPC Protocol Version 6 Backport Patch
 *
 * This header overrides the CPC protocol version from 5 to 6 for
 * compatibility with zigbeed 8.2 and modern cpcd versions.
 *
 * Include this header BEFORE any CPC headers in the build system,
 * or apply the patch directly to sli_cpc.h in the SDK.
 *
 * Analysis:
 * - GSDK 4.5.0 defines SLI_CPC_PROTOCOL_VERSION as 5
 * - Simplicity SDK 2025.6.2 defines it as 6
 * - The API (Property IDs, Commands, Capabilities) is IDENTICAL
 * - Only the version number needs to change for zigbeed 8.2 compatibility
 */

#ifndef SLI_CPC_PROTOCOL_V6_H
#define SLI_CPC_PROTOCOL_V6_H

// Override CPC protocol version for zigbeed 8.2 compatibility
// This must be defined BEFORE including sli_cpc.h
#ifdef SLI_CPC_PROTOCOL_VERSION
  #undef SLI_CPC_PROTOCOL_VERSION
#endif

#define SLI_CPC_PROTOCOL_VERSION (6)

// CPC Version info (from Simplicity SDK 2025.6.2)
#ifndef SL_CPC_VERSION_MAJOR
  #define SL_CPC_VERSION_MAJOR 4
#endif
#ifndef SL_CPC_VERSION_MINOR
  #define SL_CPC_VERSION_MINOR 7
#endif
#ifndef SL_CPC_VERSION_PATCH
  #define SL_CPC_VERSION_PATCH 1
#endif

#endif // SLI_CPC_PROTOCOL_V6_H
