#ifndef PACKAGE_MANAGEMENT_TYPES_H
#define PACKAGE_MANAGEMENT_TYPES_H

/// @file package_management_types.h
/// @brief Re-exports UCM state types for the src-gen layer.
///
/// Hand-generated to simulate what an AUTOSAR code generator would produce
/// from ucm_service_interface.arxml.  Applications and platform services
/// include only this header — they do not depend on src/ara/ucm directly.

#include "ara/ucm/package_manager_state.h"

namespace package_management
{
    // Service constants (spec §ucm_service_interface.arxml)
    static const uint16_t cServiceId{0x00C0};       ///< Service ID 192
    static const uint16_t cMethodTransferStart{1};  ///< Method 0x0001
    static const uint16_t cMethodTransferData{2};   ///< Method 0x0002
    static const uint16_t cMethodTransferExit{3};   ///< Method 0x0003
    static const uint16_t cMethodProcessSwPackage{4};///< Method 0x0004
    static const uint16_t cMethodActivate{5};        ///< Method 0x0005
    static const uint16_t cMethodGetSwClusterInfo{6};///< Method 0x0006
    static const uint16_t cMethodGetCurrentStatus{7};///< Method 0x0007

    static const uint8_t cProtocolVersion{1};
    static const uint8_t cInterfaceVersion{1};
    static const uint16_t cClientId{0x0010};  ///< VUCM client ID

    /// @brief Wire-level error byte embedded at the start of every response.
    ///        0x00 = OK; non-zero = ara::ucm::UcmErrc value.
    using WireErrorCode = uint8_t;

    static const WireErrorCode cWireOk{0x00};

} // namespace package_management

#endif
