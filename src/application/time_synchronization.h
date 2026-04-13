#ifndef TIME_SYNCHRONIZATION_H
#define TIME_SYNCHRONIZATION_H

/// @brief Time Synchronization placeholder application.
///
/// Minimal process used as a demonstration SW Package for UCM/VUCM
/// end-to-end testing.  It simply logs a heartbeat once per second.
///
/// In a real implementation this would implement ara::tsync interfaces.
namespace application
{
    int TimeSynchronizationMain();
}

#endif
