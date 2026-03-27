#ifndef SERVICE_SKELETON_H
#define SERVICE_SKELETON_H

#include <asyncbsdsocket/poller.h>

namespace ara
{
    namespace com
    {
        /// @brief Framework base class for all generated ara::com skeletons.
        ///
        /// Per AUTOSAR SWS_CM §8.4, the concrete Skeleton class is generated
        /// per-service from the ARXML service interface description and belongs
        /// to the SW-component layer.  This class provides the API contract
        /// (OfferService / StopOfferService) that all generated skeletons must
        /// conform to.
        class ServiceSkeleton
        {
        protected:
            AsyncBsdSocketLib::Poller *const mPoller;

            explicit ServiceSkeleton(
                AsyncBsdSocketLib::Poller *poller) noexcept
                : mPoller{poller}
            {
            }

        public:
            virtual ~ServiceSkeleton() noexcept = default;

            virtual void OfferService() = 0;
            virtual void StopOfferService() = 0;

            ServiceSkeleton(const ServiceSkeleton &) = delete;
            ServiceSkeleton &operator=(const ServiceSkeleton &) = delete;
        };
    }
}

#endif
