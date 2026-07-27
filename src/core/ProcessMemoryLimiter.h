#pragma once

class QString;

namespace ProcessMemoryLimiter
{

bool applyMegabytes(int megabytes, QString *errorMessage = nullptr);

}
