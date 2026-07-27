#pragma once

#include <QJsonObject>
#include <QString>

// Phase 3 verification harness — runs entirely offline with a mock SSE byte
// stream, no API key needed. Returns true iff every assertion holds.
//
// What it checks:
//   1. contentDelta fires once per text fragment (tokens arrive incrementally).
//   2. toolCallComposing fires when a tool call's first chunk lands (args still
//      streaming) — but toolCallReady does NOT fire yet.
//   3. toolCallReady fires ONLY after finish_reason == "tool_calls" is seen,
//      carrying the fully-assembled arguments as a valid QJsonObject.
//   4. A tool call whose arguments JSON is malformed is rejected, not
//   dispatched.
//   5. No dispatch ever happens on a partial arguments buffer.
//
// Invoke via `starryagent --test-pipeline`; exits 0 on success, non-zero on
// fail.
bool runPipelineSmokeTest();
