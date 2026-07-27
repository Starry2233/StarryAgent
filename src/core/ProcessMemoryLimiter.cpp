#include "ProcessMemoryLimiter.h"

#include <QString>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <sys/resource.h>
#endif

namespace ProcessMemoryLimiter
{

bool applyMegabytes(int megabytes, QString *errorMessage)
{
    if (megabytes <= 0)
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("memory limit must be greater than 0 MB");
        return false;
    }

#ifdef Q_OS_WIN
    static HANDLE jobHandle = nullptr;
    if (!jobHandle)
    {
        jobHandle = CreateJobObjectW(nullptr, nullptr);
        if (!jobHandle)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("CreateJobObjectW failed: %1")
                                    .arg(GetLastError());
            return false;
        }
        if (!AssignProcessToJobObject(jobHandle, GetCurrentProcess()))
        {
            const DWORD err = GetLastError();
            if (err != ERROR_ACCESS_DENIED)
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("AssignProcessToJobObject failed: %1")
                            .arg(err);
                return false;
            }
        }
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    info.ProcessMemoryLimit = static_cast<SIZE_T>(megabytes) * 1024u * 1024u;
    if (!SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation,
                                 &info, sizeof(info)))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("SetInformationJobObject failed: %1")
                                .arg(GetLastError());
        return false;
    }
    return true;
#elif defined(Q_OS_UNIX)
    const rlim_t limitBytes =
        static_cast<rlim_t>(megabytes) * 1024ull * 1024ull;
    struct rlimit limit;
    limit.rlim_cur = limitBytes;
    limit.rlim_max = limitBytes;
    if (setrlimit(RLIMIT_AS, &limit) != 0)
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("setrlimit(RLIMIT_AS) failed: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
#else
    if (errorMessage)
        *errorMessage =
            QStringLiteral("memory limit is not implemented on this platform");
    return false;
#endif
}

} // namespace ProcessMemoryLimiter
