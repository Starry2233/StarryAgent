#pragma once

#include <QtCore/qglobal.h>

#if defined(STARRYAGENT_BACKEND_LIBRARY)
#  define STARRYAGENT_BACKEND_EXPORT Q_DECL_EXPORT
#else
#  define STARRYAGENT_BACKEND_EXPORT Q_DECL_IMPORT
#endif