#pragma once

#include <QJsonObject>

#include "backend/StarryAgentBackendGlobal.h"

// Phase 4 verification — exercises the ToolRegistry end-to-end with no API
// key needed. Ensures the .starryagent tree exists (running --setup first if
// needed), then:
//   1. Prints the OpenAI `tools` array (proves tools.jsonc loads, schemas
//      serialize to valid JSON, all three tools are registered).
//   2. Dispatches `overwrite` to create a test file, waits for toolFinished.
//   3. Dispatches `edit` to modify that file, waits for toolFinished.
//   4. Dispatches `exec` to run a shell command, waits for toolFinished.
//
// Returns true iff every tool returned a non-error result. Invoke via
// `starryagent --test-tools`.
STARRYAGENT_BACKEND_EXPORT bool runToolsSmokeTest();
