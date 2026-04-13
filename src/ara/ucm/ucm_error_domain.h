#ifndef UCM_ERROR_DOMAIN_H
#define UCM_ERROR_DOMAIN_H

#include <exception>
#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace ucm
    {
        /// @brief UCM (Update and Configuration Management) internal error type
        enum class UcmErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidTransferId = 1,    ///!< Transfer ID is unknown or already finalised
            kTransferNotComplete = 2,  ///!< TransferExit called before all data received
            kPackageMalformed = 3,     ///!< SW Package cannot be parsed or extracted
            kActivationFailed = 4,     ///!< Activation step failed (binary copy or manifest)
            kRollbackFailed = 5,       ///!< Rollback could not restore previous state
            kInsufficientSpace = 6,    ///!< Not enough disk space for the SW Package
            kOperationInProgress = 7   ///!< UCM is already processing another transfer
        };

        /// @brief UCM error domain
        /// @note The class is not fully aligned with ARA standard.
        class UcmErrorDomain final : public ara::core::ErrorDomain
        {
        private:
            static const ara::core::ErrorDomain::IdType cId{0x8000000000000701};
            const char *cName{"Ucm"};

            static UcmErrorDomain *mInstance;

            constexpr UcmErrorDomain() noexcept : ara::core::ErrorDomain(cId)
            {
            }

        public:
            UcmErrorDomain(UcmErrorDomain &other) = delete;
            void operator=(const UcmErrorDomain &) = delete;

            const char *Name() const noexcept override;
            const char *Message(ara::core::ErrorDomain::CodeType errorCode) const noexcept override;
            void ThrowAsException(const ara::core::ErrorCode &errorCode) const noexcept(false) override;

            /// @brief Get the global UCM error domain singleton
            /// @returns Pointer to the singleton UCM error domain
            static ara::core::ErrorDomain *GetUcmDomain();

            /// @brief Make an error code based on the given UCM error type
            /// @param code UCM error code input
            /// @returns Created error code
            ara::core::ErrorCode MakeErrorCode(UcmErrc code) noexcept;
        };

        /// @brief Free function to create a UCM error code
        /// @param code UcmErrc enumeration value
        /// @returns Corresponding ara::core::ErrorCode
        ara::core::ErrorCode MakeErrorCode(UcmErrc code) noexcept;

    } // namespace ucm
} // namespace ara

#endif
