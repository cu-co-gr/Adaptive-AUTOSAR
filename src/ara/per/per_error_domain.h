#ifndef PER_ERROR_DOMAIN_H
#define PER_ERROR_DOMAIN_H

#include <exception>
#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace per
    {
        /// @brief Persistency internal error type
        enum class PerErrc : ara::core::ErrorDomain::CodeType
        {
            kStorageNotFound = 1,         ///!< Storage database not found
            kKeyNotFound = 2,             ///!< Key does not exist in storage
            kIllegalWriteAccess = 3,      ///!< Write access not permitted
            kPhysicalStorageFailure = 4,  ///!< I/O error on backing file
            kIntegrityCorrupted = 5,      ///!< Storage file is corrupt or unparseable
            kOutOfStorageSpace = 6,       ///!< No space left on device
            kInitValueNotAvailable = 7    ///!< No default/init value configured
        };

        /// @brief Persistency error domain
        /// @note The class is not fully aligned with ARA standard.
        class PerErrorDomain final : public ara::core::ErrorDomain
        {
        private:
            static const ara::core::ErrorDomain::IdType cId{0x8000000000000601};
            const char *cName{"Per"};

            static PerErrorDomain *mInstance;

            constexpr PerErrorDomain() noexcept : ara::core::ErrorDomain(cId)
            {
            }

        public:
            PerErrorDomain(PerErrorDomain &other) = delete;
            void operator=(const PerErrorDomain &) = delete;

            const char *Name() const noexcept override;
            const char *Message(ara::core::ErrorDomain::CodeType errorCode) const noexcept override;
            void ThrowAsException(const ara::core::ErrorCode &errorCode) const noexcept(false) override;

            /// @brief Get the global persistency error domain singleton
            /// @returns Pointer to the singleton persistency error domain
            static ara::core::ErrorDomain *GetPerDomain();

            /// @brief Make an error code based on the given persistency error type
            /// @param code Persistency error code input
            /// @returns Created error code
            ara::core::ErrorCode MakeErrorCode(PerErrc code) noexcept;
        };

        /// @brief Free function to create a persistency error code
        /// @param code PerErrc enumeration value
        /// @returns Corresponding ara::core::ErrorCode
        ara::core::ErrorCode MakeErrorCode(PerErrc code) noexcept;
    }
}

#endif
