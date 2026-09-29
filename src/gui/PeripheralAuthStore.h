#pragma once

#include "core/PeripheralRemovalGuard.h"

#include <QString>
#include <functional>
#include <optional>

class QObject;

namespace AetherSDR {

// Desktop Keychain adapter for the three direct 4O3A accessory connections.
// This stays in the UI shell: the engine owns the handshake, while the desktop
// owns OS credential access. Never persisted in AppSettings configuration;
// without QtKeychain the codes stay in its session vault.
class PeripheralAuthStore {
public:
    using Device = PeripheralRemovalGuard::Device;
    enum class LoadStatus { Found, Missing, Unavailable };
    struct LoadResult {
        QString code;
        LoadStatus status{LoadStatus::Missing};
    };

    // endpoint is the connected peer IP and port, never a discovery claim.
    static QString endpoint(const QString& peerAddress, quint16 port);
    static void load(Device device, const QString& endpoint, QObject* context,
                     std::function<void(const LoadResult&)> callback);
    // The callback reports persistence. A session-only save returns false for
    // a nonempty code; clearing the session value returns true.
    static void save(Device device, const QString& endpoint, const QString& code,
                     QObject* context,
                     std::function<void(bool)> callback = {});
    enum class ClearResult { Cleared, SessionCleared, Failed };
    // SessionCleared means the OS backend is unavailable: local state is
    // cleared, but deletion of a previously persisted secret is unconfirmed.
    static void clear(Device device, QObject* context,
                      std::function<void(ClearResult)> callback);
    // Metadata only: never prompts the OS vault or exposes a saved secret.
    struct CodeAvailability {
        LoadStatus status;
        bool persistent;
    };
    static std::optional<CodeAvailability> cachedStatus(Device device, const QString& endpoint);
    static bool persistentStoreAvailable();
    static bool validCode(const QString& code);
};

} // namespace AetherSDR
