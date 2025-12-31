/***************************************************************************//**
 * @file sl_cpc_security_config.h
 * @brief CPC Security Configuration - DISABLED STUB
 *
 * This is a stub configuration file for when CPC security is disabled.
 * The SDK headers still include this file even when security is off.
 ******************************************************************************/

#ifndef SL_CPC_SECURITY_CONFIG_H
#define SL_CPC_SECURITY_CONFIG_H

// CPC Security is DISABLED - this is a stub file
#define SL_CPC_SECURITY_ENABLED                     0
#define SL_CPC_SECURITY_BINDING_KEY_METHOD          SL_CPC_SECURITY_BINDING_KEY_PLAINTEXT_SHARE

// Stub defines to satisfy header requirements
#ifndef SL_CPC_SECURITY_BINDING_KEY_PLAINTEXT_SHARE
#define SL_CPC_SECURITY_BINDING_KEY_PLAINTEXT_SHARE 0
#endif

#ifndef SL_CPC_SECURITY_BINDING_KEY_ECDH
#define SL_CPC_SECURITY_BINDING_KEY_ECDH            1
#endif

#endif // SL_CPC_SECURITY_CONFIG_H
