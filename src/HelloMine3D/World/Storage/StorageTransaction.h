#ifndef STORAGETRANSACTION_H_INCLUDED
#define STORAGETRANSACTION_H_INCLUDED

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

enum class StorageFaultPoint {
    None,
    BeforeWrite,
    MidWrite,
    BeforeFlush,
    BeforeValidation,
    BeforeReplace
};

const char *storageFaultPointName(StorageFaultPoint point) noexcept;

struct StorageTransactionOptions {
    StorageFaultPoint faultPoint = StorageFaultPoint::None;
};

struct StorageTransactionMetrics {
    double prepareCompleteMilliseconds = 0.0;
    double writeCompleteMilliseconds = 0.0;
    double flushCompleteMilliseconds = 0.0;
    double validationCompleteMilliseconds = 0.0;
    double replaceCompleteMilliseconds = 0.0;
    double totalMilliseconds = 0.0;
    std::size_t bytesWritten = 0;
    bool durablyFlushed = false;
    bool candidateValidated = false;
    bool published = false;
    std::string quarantinePath;
    std::string error;
};

using StorageCandidateValidator =
    std::function<bool(const std::string &, std::string &)>;

/// Publishes one already-serialized file through a sibling pending candidate.
/// The target is replaced only after a durable file flush and full callback
/// validation. Failed candidates move to one bounded sibling quarantine.
class StorageTransaction {
  public:
    static bool publish(
        const std::string &targetPath, const std::vector<char> &payload,
        const StorageCandidateValidator &validator,
        const StorageTransactionOptions &options = {},
        StorageTransactionMetrics *metrics = nullptr);

    static std::string pendingPath(const std::string &targetPath);
    static std::string quarantinePath(const std::string &targetPath);
};

#endif // STORAGETRANSACTION_H_INCLUDED
