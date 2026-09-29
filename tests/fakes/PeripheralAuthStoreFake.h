#pragma once

namespace AetherSDR::FakePeripheralAuthStore {

// Controls only the dialog test's in-memory store implementation.
void setNextClearResult(bool ok);
void setBackendAvailable(bool available);
void deferClear(bool defer);
void finishClear();

}
