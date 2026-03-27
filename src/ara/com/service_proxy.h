#ifndef SERVICE_PROXY_H
#define SERVICE_PROXY_H

#include <asyncbsdsocket/poller.h>

namespace ara
{
    namespace com
    {
        /// @brief Framework base class for all generated ara::com proxies.
        ///
        /// Per AUTOSAR SWS_CM §8.4, the concrete Proxy class is generated
        /// per-service from the ARXML service interface description and belongs
        /// to the SW-component layer.  This class provides the API contract
        /// that all generated proxies must conform to.
        class ServiceProxy
        {
        protected:
            AsyncBsdSocketLib::Poller *const mPoller;

            explicit ServiceProxy(
                AsyncBsdSocketLib::Poller *poller) noexcept
                : mPoller{poller}
            {
            }

        public:
            virtual ~ServiceProxy() noexcept = default;

            ServiceProxy(const ServiceProxy &) = delete;
            ServiceProxy &operator=(const ServiceProxy &) = delete;
        };
    }
}

#endif
