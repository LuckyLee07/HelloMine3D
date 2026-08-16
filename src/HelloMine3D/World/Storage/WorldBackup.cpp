#include "WorldBackup.h"

#include "ChunkStorageData.h"
#include "StorageTransaction.h"
#include "WorldSave.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char *BackupDirectoryName = "backups";
    constexpr const char *ManifestName = "manifest.hmb";
    constexpr const char *BackupPrefix = "backup-";
    constexpr const char *BackupPendingName = ".pending";
    constexpr const char *BackupFailedName = ".failed";
    constexpr const char *CorruptBackupName = ".corrupt.failed";
    constexpr const char *RestorePendingName = ".restore.pending";
    constexpr const char *RestoreFailedName = ".restore.failed";
    constexpr const char *RecoveryPendingName = "recovery.pending";
    constexpr const char *RecoveryFailedName = "recovery.failed";
    constexpr std::uintmax_t MaxManifestBytes = 1024 * 1024;

    struct BackupFile {
        std::string relativePath;
        std::vector<char> bytes;
        std::string hash;
        int chunkX = 0;
        int chunkZ = 0;
        bool isWorldMetadata = false;
        bool isChunk = false;
    };

    struct ParsedManifest {
        WorldBackupInfo info;
        std::vector<BackupFile> files;
    };

    void appendError(std::string &target, const std::string &message)
    {
        if (!target.empty()) {
            target += "; ";
        }
        target += message;
    }

    bool validatePolicy(const WorldBackupPolicy &policy, std::string &error)
    {
        if (policy.maxBackups == 0 || policy.maxTotalBytes == 0 ||
            policy.maxFiles == 0 || policy.maxFileBytes == 0) {
            error = "backup policy limits must be positive";
            return false;
        }
        return true;
    }

    template <typename Integer>
    bool parseUnsigned(const std::string &text, Integer &value)
    {
        if (text.empty() || text.front() == '+' || text.front() == '-') {
            return false;
        }
        Integer parsed = 0;
        const auto result =
            std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc() ||
            result.ptr != text.data() + text.size()) {
            return false;
        }
        value = parsed;
        return true;
    }

    bool parseSignedInt(const std::string &text, int &value)
    {
        if (text.empty() || text.front() == '+') {
            return false;
        }
        int parsed = 0;
        const auto result =
            std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc() ||
            result.ptr != text.data() + text.size()) {
            return false;
        }
        value = parsed;
        return true;
    }

    bool parseSigned64(const std::string &text, std::int64_t &value)
    {
        if (text.empty() || text.front() == '+') {
            return false;
        }
        std::int64_t parsed = 0;
        const auto result =
            std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc() ||
            result.ptr != text.data() + text.size()) {
            return false;
        }
        value = parsed;
        return true;
    }

    std::string hashBytes(const std::vector<char> &bytes)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char value : bytes) {
            hash ^= value;
            hash *= 1099511628211ull;
        }
        std::ostringstream output;
        output << std::hex << std::setfill('0') << std::setw(16) << hash;
        return output.str();
    }

    bool readFile(const fs::path &path, std::uintmax_t maximumBytes,
                  std::vector<char> &bytes, std::string &error,
                  bool allowEmpty = false)
    {
        std::error_code statusError;
        const fs::file_status status = fs::symlink_status(path, statusError);
        if (statusError || !fs::is_regular_file(status) ||
            fs::is_symlink(status)) {
            error = "expected a real regular file: " + path.string();
            return false;
        }
        std::error_code sizeError;
        const std::uintmax_t size = fs::file_size(path, sizeError);
        if (sizeError || (!allowEmpty && size == 0) ||
            size > maximumBytes ||
            size > static_cast<std::uintmax_t>(
                       std::numeric_limits<std::streamsize>::max()) ||
            size > static_cast<std::uintmax_t>(
                       std::numeric_limits<std::size_t>::max())) {
            error = "file size is outside the backup limit: " +
                    path.string();
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            error = "cannot open backup source: " + path.string();
            return false;
        }
        bytes.resize(static_cast<std::size_t>(size));
        input.read(bytes.data(), static_cast<std::streamsize>(size));
        if (!input || input.peek() != std::char_traits<char>::eof()) {
            error = "cannot read complete backup source: " + path.string();
            return false;
        }
        return true;
    }

    bool parseChunkName(const std::string &name, int &x, int &z)
    {
        constexpr const char *prefix = "chunk_";
        constexpr const char *suffix = ".hmcchunk";
        if (name.rfind(prefix, 0) != 0 ||
            name.size() <= std::char_traits<char>::length(prefix) +
                               std::char_traits<char>::length(suffix) ||
            name.compare(name.size() - std::char_traits<char>::length(suffix),
                         std::char_traits<char>::length(suffix), suffix) != 0) {
            return false;
        }
        const std::size_t bodyBegin =
            std::char_traits<char>::length(prefix);
        const std::size_t bodyEnd =
            name.size() - std::char_traits<char>::length(suffix);
        const std::size_t separator = name.find('_', bodyBegin);
        if (separator == std::string::npos || separator >= bodyEnd) {
            return false;
        }
        const std::string xText =
            name.substr(bodyBegin, separator - bodyBegin);
        const std::string zText =
            name.substr(separator + 1, bodyEnd - separator - 1);
        if (!parseSignedInt(xText, x) || !parseSignedInt(zText, z)) {
            return false;
        }
        return name == "chunk_" + std::to_string(x) + "_" +
                           std::to_string(z) + ".hmcchunk";
    }

    bool isTransactionArtifact(const std::string &name)
    {
        return name.size() > 8 &&
               (name.compare(name.size() - 8, 8, ".pending") == 0 ||
                name.compare(name.size() - 7, 7, ".failed") == 0);
    }

    bool validateFileFormat(const fs::path &path, const BackupFile &file,
                            int *worldVersion, std::string &error)
    {
        if (file.isWorldMetadata) {
            WorldSaveData data;
            if (!WorldSave::loadFromPath(path.string(), data, &error)) {
                error = "invalid world metadata: " + error;
                return false;
            }
            if (worldVersion != nullptr) {
                *worldVersion = data.version;
            }
            return true;
        }
        if (file.isChunk) {
            StoredChunkData data;
            if (!ChunkStorageData::loadChunkFile(
                    path.string(), file.chunkX, file.chunkZ, data, &error)) {
                error = "invalid chunk " + file.relativePath + ": " + error;
                return false;
            }
            return true;
        }
        error = "unsupported backup file: " + file.relativePath;
        return false;
    }

    bool collectWorldFiles(const fs::path &worldDirectory,
                           const WorldBackupPolicy &policy,
                           bool requireValidFormats,
                           std::vector<BackupFile> &files,
                           int &worldVersion, std::string &error,
                           bool requireMetadata = true)
    {
        files.clear();
        worldVersion = 0;
        const fs::path metadataPath = worldDirectory / "world.meta";
        std::error_code metadataStatusError;
        const fs::file_status metadataStatus =
            fs::symlink_status(metadataPath, metadataStatusError);
        if (metadataStatusError &&
            metadataStatusError != std::errc::no_such_file_or_directory) {
            error = "cannot inspect world metadata: " +
                    metadataStatusError.message();
            return false;
        }
        if (!metadataStatusError && fs::is_regular_file(metadataStatus) &&
            !fs::is_symlink(metadataStatus)) {
            BackupFile metadata;
            metadata.relativePath = "world.meta";
            metadata.isWorldMetadata = true;
            if (!readFile(metadataPath, policy.maxFileBytes, metadata.bytes,
                          error, !requireValidFormats)) {
                return false;
            }
            metadata.hash = hashBytes(metadata.bytes);
            if (requireValidFormats &&
                !validateFileFormat(metadataPath, metadata, &worldVersion,
                                    error)) {
                return false;
            }
            files.push_back(std::move(metadata));
        }
        else if (!metadataStatusError && fs::exists(metadataStatus)) {
            error = "world metadata is not a real file: " +
                    metadataPath.string();
            return false;
        }
        else if (requireMetadata) {
            error = "world metadata is missing or not a real file: " +
                    metadataPath.string();
            return false;
        }

        const fs::path chunksPath = worldDirectory / "chunks";
        std::error_code chunkStatusError;
        const fs::file_status chunkStatus =
            fs::symlink_status(chunksPath, chunkStatusError);
        if (!chunkStatusError && fs::exists(chunkStatus)) {
            if (!fs::is_directory(chunkStatus) || fs::is_symlink(chunkStatus)) {
                error = "chunk storage is not a real directory: " +
                        chunksPath.string();
                return false;
            }
            std::error_code iterateError;
            for (fs::directory_iterator iterator(chunksPath, iterateError), end;
                 iterator != end && !iterateError;
                 iterator.increment(iterateError)) {
                const fs::directory_entry &entry = *iterator;
                const std::string name = entry.path().filename().string();
                if (isTransactionArtifact(name)) {
                    continue;
                }
                int x = 0;
                int z = 0;
                if (!parseChunkName(name, x, z)) {
                    error = "unexpected file in chunk storage: " +
                            entry.path().string();
                    return false;
                }
                BackupFile chunk;
                chunk.relativePath = "chunks/" + name;
                chunk.chunkX = x;
                chunk.chunkZ = z;
                chunk.isChunk = true;
                if (!readFile(entry.path(), policy.maxFileBytes, chunk.bytes,
                              error, !requireValidFormats)) {
                    return false;
                }
                chunk.hash = hashBytes(chunk.bytes);
                if (requireValidFormats &&
                    !validateFileFormat(entry.path(), chunk, nullptr, error)) {
                    return false;
                }
                files.push_back(std::move(chunk));
            }
            if (iterateError) {
                error = "cannot enumerate chunk storage: " +
                        iterateError.message();
                return false;
            }
        }
        else if (chunkStatusError &&
                 chunkStatusError != std::errc::no_such_file_or_directory) {
            error = "cannot inspect chunk storage: " +
                    chunkStatusError.message();
            return false;
        }

        std::sort(files.begin(), files.end(),
                  [](const BackupFile &left, const BackupFile &right) {
                      return left.relativePath < right.relativePath;
                  });
        if (files.size() > policy.maxFiles) {
            error = "world file count exceeds the backup limit";
            return false;
        }
        std::uintmax_t total = 0;
        for (const BackupFile &file : files) {
            const std::uintmax_t size = file.bytes.size();
            if (size > policy.maxTotalBytes -
                           std::min(policy.maxTotalBytes, total)) {
                error = "world bytes exceed the backup limit";
                return false;
            }
            total += size;
            if (total > policy.maxTotalBytes) {
                error = "world bytes exceed the backup limit";
                return false;
            }
        }
        return true;
    }

    std::string backupId(std::uint64_t sequence)
    {
        std::ostringstream output;
        output << BackupPrefix << std::setfill('0') << std::setw(20)
               << sequence;
        return output.str();
    }

    bool parseBackupId(const std::string &id, std::uint64_t &sequence)
    {
        if (id.rfind(BackupPrefix, 0) != 0 ||
            id.size() != std::char_traits<char>::length(BackupPrefix) + 20) {
            return false;
        }
        const std::string number =
            id.substr(std::char_traits<char>::length(BackupPrefix));
        return parseUnsigned(number, sequence) && sequence > 0 &&
               id == backupId(sequence);
    }

    std::int64_t currentUtcSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::uintmax_t totalFileBytes(const std::vector<BackupFile> &files)
    {
        std::uintmax_t total = 0;
        for (const BackupFile &file : files) {
            total += file.bytes.size();
        }
        return total;
    }

    std::string serializeManifest(const WorldBackupInfo &info,
                                  const std::vector<BackupFile> &files)
    {
        std::ostringstream output;
        output << "HMBACKUP 1\n";
        output << "id " << info.id << '\n';
        output << "sequence " << info.sequence << '\n';
        output << "created_utc " << info.createdUtc << '\n';
        output << "world_version " << info.worldFormatVersion << '\n';
        output << "file_count " << files.size() << '\n';
        output << "total_bytes " << totalFileBytes(files) << '\n';
        for (const BackupFile &file : files) {
            output << "file " << file.relativePath << ' '
                   << file.bytes.size() << ' ' << file.hash << '\n';
        }
        return output.str();
    }

    bool parseManifest(const fs::path &backupDirectory,
                       const WorldBackupPolicy &policy,
                       ParsedManifest &manifest, std::string &error,
                       const std::string *expectedId = nullptr)
    {
        std::vector<char> bytes;
        const fs::path path = backupDirectory / ManifestName;
        if (!readFile(path, MaxManifestBytes, bytes, error)) {
            return false;
        }
        std::istringstream input(std::string(bytes.begin(), bytes.end()));
        std::string line;
        if (!std::getline(input, line) || line != "HMBACKUP 1") {
            error = "invalid backup manifest header";
            return false;
        }

        ParsedManifest parsed;
        std::size_t expectedFileCount = 0;
        std::uintmax_t expectedTotalBytes = 0;
        bool idSeen = false;
        bool sequenceSeen = false;
        bool createdSeen = false;
        bool versionSeen = false;
        bool countSeen = false;
        bool totalSeen = false;
        std::set<std::string> paths;
        while (std::getline(input, line)) {
            if (line.empty()) {
                error = "empty line in backup manifest";
                return false;
            }
            std::istringstream row(line);
            std::string key;
            std::string value;
            if (!(row >> key)) {
                error = "invalid backup manifest row";
                return false;
            }
            if (key == "id") {
                std::uint64_t idSequence = 0;
                if (idSeen || !(row >> value) ||
                    !parseBackupId(value, idSequence)) {
                    error = "invalid or duplicate backup id";
                    return false;
                }
                if (sequenceSeen && idSequence != parsed.info.sequence) {
                    error = "backup id and sequence differ";
                    return false;
                }
                parsed.info.sequence = idSequence;
                parsed.info.id = value;
                idSeen = true;
            }
            else if (key == "sequence") {
                std::uint64_t sequence = 0;
                if (sequenceSeen || !(row >> value) ||
                    !parseUnsigned(value, sequence)) {
                    error = "invalid or duplicate backup sequence";
                    return false;
                }
                if (idSeen && sequence != parsed.info.sequence) {
                    error = "backup id and sequence differ";
                    return false;
                }
                parsed.info.sequence = sequence;
                sequenceSeen = true;
            }
            else if (key == "created_utc") {
                if (createdSeen || !(row >> value) ||
                    !parseSigned64(value, parsed.info.createdUtc) ||
                    parsed.info.createdUtc <= 0) {
                    error = "invalid or duplicate backup timestamp";
                    return false;
                }
                createdSeen = true;
            }
            else if (key == "world_version") {
                if (versionSeen || !(row >> value) ||
                    !parseSignedInt(value, parsed.info.worldFormatVersion) ||
                    parsed.info.worldFormatVersion < 1 ||
                    parsed.info.worldFormatVersion >
                        WorldSaveFormatVersion) {
                    error = "invalid or duplicate backup world version";
                    return false;
                }
                versionSeen = true;
            }
            else if (key == "file_count") {
                if (countSeen || !(row >> value) ||
                    !parseUnsigned(value, expectedFileCount) ||
                    expectedFileCount == 0) {
                    error = "invalid or duplicate backup file count";
                    return false;
                }
                if (expectedFileCount > policy.maxFiles) {
                    error = "backup file count exceeds policy";
                    return false;
                }
                countSeen = true;
            }
            else if (key == "total_bytes") {
                if (totalSeen || !(row >> value) ||
                    !parseUnsigned(value, expectedTotalBytes) ||
                    expectedTotalBytes == 0) {
                    error = "invalid or duplicate backup byte count";
                    return false;
                }
                if (expectedTotalBytes > policy.maxTotalBytes) {
                    error = "backup byte count exceeds policy";
                    return false;
                }
                totalSeen = true;
            }
            else if (key == "file") {
                BackupFile file;
                std::uintmax_t expectedBytes = 0;
                std::string sizeText;
                if (!(row >> file.relativePath >> sizeText >> file.hash) ||
                    !parseUnsigned(sizeText, expectedBytes) ||
                    expectedBytes == 0 ||
                    expectedBytes > static_cast<std::uintmax_t>(
                                        std::numeric_limits<std::size_t>::max()) ||
                    file.hash.size() != 16 ||
                    !paths.insert(file.relativePath).second) {
                    error = "invalid backup file row";
                    return false;
                }
                if (expectedBytes > policy.maxFileBytes) {
                    error = "backup file size exceeds policy";
                    return false;
                }
                if (file.relativePath == "world.meta") {
                    file.isWorldMetadata = true;
                }
                else if (file.relativePath.rfind("chunks/", 0) == 0 &&
                         file.relativePath.find('/', 7) == std::string::npos &&
                         parseChunkName(file.relativePath.substr(7),
                                        file.chunkX, file.chunkZ)) {
                    file.isChunk = true;
                }
                else {
                    error = "unsafe backup file path: " + file.relativePath;
                    return false;
                }
                for (char valueChar : file.hash) {
                    if (!((valueChar >= '0' && valueChar <= '9') ||
                          (valueChar >= 'a' && valueChar <= 'f'))) {
                        error = "invalid backup file hash";
                        return false;
                    }
                }
                row >> std::ws;
                if (!row.eof()) {
                    error = "trailing backup file data";
                    return false;
                }
                file.bytes.resize(static_cast<std::size_t>(expectedBytes));
                parsed.files.push_back(std::move(file));
                continue;
            }
            else {
                error = "unknown backup manifest field: " + key;
                return false;
            }
            row >> std::ws;
            if (!row.eof()) {
                error = "trailing backup manifest data";
                return false;
            }
        }
        if (!input.eof() || !idSeen || !sequenceSeen || !createdSeen ||
            !versionSeen || !countSeen || !totalSeen ||
            parsed.files.size() != expectedFileCount ||
            parsed.info.id !=
                (expectedId == nullptr ? backupDirectory.filename().string()
                                       : *expectedId)) {
            error = "incomplete or mismatched backup manifest";
            return false;
        }
        if (parsed.info.id != backupId(parsed.info.sequence)) {
            error = "backup sequence is not canonical";
            return false;
        }
        std::sort(parsed.files.begin(), parsed.files.end(),
                  [](const BackupFile &left, const BackupFile &right) {
                      return left.relativePath < right.relativePath;
                  });
        if (parsed.files.empty() ||
            parsed.files.back().relativePath != "world.meta") {
            error = "backup manifest does not contain world.meta";
            return false;
        }
        parsed.info.fileCount = parsed.files.size();
        parsed.info.totalBytes = expectedTotalBytes;
        parsed.info.directoryPath = backupDirectory.string();
        manifest = std::move(parsed);
        return true;
    }

    bool loadBackup(const fs::path &backupDirectory,
                    const WorldBackupPolicy &policy, ParsedManifest &manifest,
                    std::string &error,
                    const std::string *expectedId = nullptr)
    {
        std::error_code statusError;
        const fs::file_status status =
            fs::symlink_status(backupDirectory, statusError);
        if (statusError || !fs::is_directory(status) ||
            fs::is_symlink(status)) {
            error = "backup is not a real directory: " +
                    backupDirectory.string();
            return false;
        }
        ParsedManifest parsed;
        if (!parseManifest(backupDirectory, policy, parsed, error,
                           expectedId)) {
            return false;
        }
        std::set<std::string> expectedPaths{ManifestName};
        for (const BackupFile &file : parsed.files) {
            expectedPaths.insert(file.relativePath);
        }
        std::set<std::string> actualPaths;
        std::error_code rootError;
        for (fs::recursive_directory_iterator iterator(
                 backupDirectory, fs::directory_options::none, rootError),
             end;
             iterator != end && !rootError; iterator.increment(rootError)) {
            const fs::path relative =
                fs::relative(iterator->path(), backupDirectory, rootError);
            if (rootError) {
                break;
            }
            const fs::file_status entryStatus =
                fs::symlink_status(iterator->path(), rootError);
            if (rootError || fs::is_symlink(entryStatus)) {
                break;
            }
            if (fs::is_directory(entryStatus)) {
                if (relative.generic_string() != "chunks") {
                    error = "unexpected directory in backup: " +
                            relative.generic_string();
                    return false;
                }
                continue;
            }
            if (!fs::is_regular_file(entryStatus)) {
                error = "unexpected non-file in backup: " +
                        relative.generic_string();
                return false;
            }
            actualPaths.insert(relative.generic_string());
        }
        if (rootError) {
            error = "cannot inspect backup inventory: " +
                    rootError.message();
            return false;
        }
        if (actualPaths != expectedPaths) {
            error = "backup inventory differs from manifest";
            return false;
        }
        std::uintmax_t actualTotal = 0;
        int worldVersion = 0;
        for (BackupFile &file : parsed.files) {
            const std::size_t expectedBytes = file.bytes.size();
            if (!readFile(backupDirectory / file.relativePath,
                          policy.maxFileBytes, file.bytes, error)) {
                return false;
            }
            if (file.bytes.size() != expectedBytes ||
                hashBytes(file.bytes) != file.hash) {
                error = "backup hash mismatch: " + file.relativePath;
                return false;
            }
            if (!validateFileFormat(backupDirectory / file.relativePath, file,
                                    &worldVersion, error)) {
                return false;
            }
            actualTotal += file.bytes.size();
        }
        if (actualTotal != parsed.info.totalBytes ||
            worldVersion != parsed.info.worldFormatVersion) {
            error = "backup totals or world version do not match manifest";
            return false;
        }
        manifest = std::move(parsed);
        return true;
    }

    bool removeTree(const fs::path &path, std::string &error)
    {
        std::error_code removeError;
        fs::remove_all(path, removeError);
        if (removeError) {
            error = "cannot remove " + path.string() + ": " +
                    removeError.message();
            return false;
        }
        return true;
    }

    bool replaceDirectory(const fs::path &source, const fs::path &target,
                          std::string &error)
    {
        if (!removeTree(target, error)) {
            return false;
        }
        std::error_code renameError;
        fs::rename(source, target, renameError);
        if (renameError) {
            error = "cannot publish directory " + target.string() + ": " +
                    renameError.message();
            return false;
        }
        return true;
    }

    bool ensureRealDirectory(const fs::path &path, std::string &error)
    {
        std::error_code statusError;
        const fs::file_status status = fs::symlink_status(path, statusError);
        if (!statusError && fs::exists(status)) {
            if (!fs::is_directory(status) || fs::is_symlink(status)) {
                error = "path is not a real directory: " + path.string();
                return false;
            }
            return true;
        }
        if (statusError &&
            statusError != std::errc::no_such_file_or_directory) {
            error = "cannot inspect directory " + path.string() + ": " +
                    statusError.message();
            return false;
        }
        std::error_code createError;
        fs::create_directories(path, createError);
        if (createError) {
            error = "cannot create directory " + path.string() + ": " +
                    createError.message();
            return false;
        }
        return true;
    }

    bool publishBytes(const fs::path &target, const std::vector<char> &bytes,
                      const BackupFile *formatFile,
                      WorldBackupMetrics &metrics, std::string &error,
                      bool requireFormatValidation = true)
    {
        if (!ensureRealDirectory(target.parent_path(), error)) {
            return false;
        }
        const std::string expectedHash = hashBytes(bytes);
        const auto validator =
            [expectedHash, formatFile, requireFormatValidation](
                const std::string &candidate, std::string &validationError) {
                std::vector<char> candidateBytes;
                if (!readFile(candidate,
                              std::numeric_limits<std::uintmax_t>::max(),
                              candidateBytes, validationError, true) ||
                    hashBytes(candidateBytes) != expectedHash) {
                    if (validationError.empty()) {
                        validationError = "candidate hash differs";
                    }
                    return false;
                }
                if (requireFormatValidation && formatFile != nullptr) {
                    return validateFileFormat(candidate, *formatFile, nullptr,
                                              validationError);
                }
                return true;
            };
        StorageTransactionMetrics transaction;
        if (!StorageTransaction::publish(target.string(), bytes, validator, {},
                                         &transaction)) {
            error = transaction.error;
            return false;
        }
        ++metrics.filesCopied;
        metrics.bytesCopied += bytes.size();
        return true;
    }

    bool writeFiles(const fs::path &directory,
                    const std::vector<BackupFile> &files,
                    WorldBackupMetrics &metrics, std::string &error,
                    bool validateFormats = true)
    {
        for (const BackupFile &file : files) {
            if (!publishBytes(directory / file.relativePath, file.bytes,
                              &file, metrics, error, validateFormats)) {
                error = "cannot stage " + file.relativePath + ": " + error;
                return false;
            }
        }
        return true;
    }

    bool listBackupsInternal(const fs::path &backupRoot,
                             const WorldBackupPolicy &policy,
                             std::vector<WorldBackupInfo> &backups,
                             std::string &error)
    {
        backups.clear();
        std::error_code statusError;
        const fs::file_status status =
            fs::symlink_status(backupRoot, statusError);
        if (statusError == std::errc::no_such_file_or_directory ||
            (!statusError && !fs::exists(status))) {
            return true;
        }
        if (statusError || !fs::is_directory(status) ||
            fs::is_symlink(status)) {
            error = "backup root is not a real directory";
            return false;
        }
        std::error_code iterateError;
        for (fs::directory_iterator iterator(backupRoot, iterateError), end;
             iterator != end && !iterateError;
             iterator.increment(iterateError)) {
            const std::string id = iterator->path().filename().string();
            if (!id.empty() && id.front() == '.') {
                continue;
            }
            std::uint64_t sequence = 0;
            if (!parseBackupId(id, sequence)) {
                error = "unexpected entry in backup root: " + id;
                return false;
            }
            ParsedManifest manifest;
            if (!loadBackup(iterator->path(), policy, manifest, error)) {
                error = "invalid backup " + id + ": " + error;
                return false;
            }
            backups.push_back(std::move(manifest.info));
        }
        if (iterateError) {
            error = "cannot enumerate backups: " + iterateError.message();
            return false;
        }
        std::sort(backups.begin(), backups.end(),
                  [](const WorldBackupInfo &left,
                     const WorldBackupInfo &right) {
                      return left.sequence < right.sequence;
                  });
        return true;
    }

    bool isPolicyRejection(const std::string &error)
    {
        return error.find("exceeds policy") != std::string::npos;
    }

    bool quarantineDirectory(const fs::path &source, const fs::path &target,
                             WorldBackupMetrics &metrics,
                             std::string &error)
    {
        if (!replaceDirectory(source, target, error)) {
            return false;
        }
        metrics.quarantinePath = target.string();
        return true;
    }

    bool sameFileSet(const std::vector<BackupFile> &left,
                     const std::vector<BackupFile> &right)
    {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (left[index].relativePath != right[index].relativePath ||
                left[index].hash != right[index].hash ||
                left[index].bytes != right[index].bytes) {
                return false;
            }
        }
        return true;
    }

    bool rollbackPrimary(const fs::path &worldDirectory,
                         const std::vector<BackupFile> &primaryFiles,
                         const std::set<std::string> &publishedPaths,
                         WorldBackupMetrics &metrics, std::string &error)
    {
        std::set<std::string> primaryPaths;
        for (const BackupFile &file : primaryFiles) {
            primaryPaths.insert(file.relativePath);
            if (!publishBytes(worldDirectory / file.relativePath, file.bytes,
                              &file, metrics, error, false)) {
                error = "rollback failed for " + file.relativePath + ": " +
                        error;
                return false;
            }
        }
        for (const std::string &path : publishedPaths) {
            if (primaryPaths.count(path) != 0) {
                continue;
            }
            std::error_code removeError;
            fs::remove(worldDirectory / path, removeError);
            if (removeError) {
                error = "rollback cannot remove new file " + path + ": " +
                        removeError.message();
                return false;
            }
        }
        metrics.rolledBack = true;
        return true;
    }
}

const char *worldBackupFaultPointName(WorldBackupFaultPoint point) noexcept
{
    switch (point) {
    case WorldBackupFaultPoint::None:
        return "none";
    case WorldBackupFaultPoint::BeforeBackupPublish:
        return "before-backup-publish";
    case WorldBackupFaultPoint::BeforeRestoreValidation:
        return "before-restore-validation";
    case WorldBackupFaultPoint::BeforeRestorePublish:
        return "before-restore-publish";
    case WorldBackupFaultPoint::AfterFirstRestorePublish:
        return "after-first-restore-publish";
    }
    return "unknown";
}

WorldBackup::WorldBackup(std::string worldDirectory, WorldBackupPolicy policy)
    : m_worldDirectory(std::move(worldDirectory))
    , m_policy(policy)
{
}

bool WorldBackup::createBackup(WorldBackupInfo *created,
                               WorldBackupMetrics *metricsOutput,
                               const WorldBackupOptions &options) const
{
    WorldBackupMetrics metrics;
    const auto started = std::chrono::steady_clock::now();
    const auto finish = [&](bool result) {
        metrics.totalMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count();
        if (metricsOutput != nullptr) {
            *metricsOutput = metrics;
        }
        return result;
    };
    if (!validatePolicy(m_policy, metrics.error)) {
        return finish(false);
    }

    std::vector<BackupFile> files;
    int worldVersion = 0;
    if (!collectWorldFiles(m_worldDirectory, m_policy, true, files,
                           worldVersion, metrics.error)) {
        return finish(false);
    }
    const fs::path backupRoot = backupRootDirectory();
    if (!ensureRealDirectory(backupRoot, metrics.error)) {
        return finish(false);
    }
    std::vector<WorldBackupInfo> existing;
    if (!listBackupsInternal(backupRoot, m_policy, existing, metrics.error)) {
        return finish(false);
    }
    const std::uint64_t sequence =
        existing.empty() ? 1 : existing.back().sequence + 1;
    if (sequence == 0) {
        metrics.error = "backup sequence exhausted";
        return finish(false);
    }

    const fs::path pending = backupRoot / BackupPendingName;
    const fs::path failed = backupRoot / BackupFailedName;
    std::error_code pendingError;
    if (fs::exists(pending, pendingError)) {
        if (!quarantineDirectory(pending, failed, metrics, metrics.error)) {
            return finish(false);
        }
    }
    else if (pendingError) {
        metrics.error = "cannot inspect backup candidate: " +
                        pendingError.message();
        return finish(false);
    }
    if (!ensureRealDirectory(pending, metrics.error)) {
        return finish(false);
    }
    const auto failCandidate = [&]() {
        std::string quarantineError;
        if (!quarantineDirectory(pending, failed, metrics,
                                 quarantineError)) {
            appendError(metrics.error, quarantineError);
        }
        return finish(false);
    };

    if (!writeFiles(pending, files, metrics, metrics.error)) {
        return failCandidate();
    }
    WorldBackupInfo info;
    info.id = backupId(sequence);
    info.sequence = sequence;
    info.createdUtc = currentUtcSeconds();
    info.worldFormatVersion = worldVersion;
    info.fileCount = files.size();
    info.totalBytes = totalFileBytes(files);
    const std::string manifestText = serializeManifest(info, files);
    const std::vector<char> manifestBytes(manifestText.begin(),
                                          manifestText.end());
    if (!publishBytes(pending / ManifestName, manifestBytes, nullptr, metrics,
                      metrics.error, false)) {
        return failCandidate();
    }
    ParsedManifest candidate;
    if (!loadBackup(pending, m_policy, candidate, metrics.error, &info.id) ||
        candidate.info.sequence != info.sequence ||
        candidate.info.createdUtc != info.createdUtc ||
        candidate.info.worldFormatVersion != info.worldFormatVersion ||
        !sameFileSet(candidate.files, files)) {
        if (metrics.error.empty()) {
            metrics.error = "backup candidate differs from live generation";
        }
        return failCandidate();
    }
    metrics.candidateValidated = true;
    if (options.faultPoint == WorldBackupFaultPoint::BeforeBackupPublish) {
        metrics.error = std::string("injected fault at ") +
                        worldBackupFaultPointName(options.faultPoint);
        return failCandidate();
    }
    const fs::path finalPath = backupRoot / info.id;
    std::error_code finalError;
    if (fs::exists(finalPath, finalError) || finalError) {
        metrics.error = "backup destination already exists";
        return failCandidate();
    }
    std::error_code renameError;
    fs::rename(pending, finalPath, renameError);
    if (renameError) {
        metrics.error = "cannot publish backup directory: " +
                        renameError.message();
        return failCandidate();
    }
    info.directoryPath = finalPath.string();

    std::vector<WorldBackupInfo> backups;
    if (!listBackupsInternal(backupRoot, m_policy, backups, metrics.error)) {
        return finish(false);
    }
    std::uintmax_t totalBytes = 0;
    for (const WorldBackupInfo &backup : backups) {
        if (backup.totalBytes >
            std::numeric_limits<std::uintmax_t>::max() - totalBytes) {
            totalBytes = std::numeric_limits<std::uintmax_t>::max();
        }
        else {
            totalBytes += backup.totalBytes;
        }
    }
    while (backups.size() > m_policy.maxBackups ||
           totalBytes > m_policy.maxTotalBytes) {
        if (backups.empty() || backups.front().id == info.id) {
            metrics.error = "new backup cannot fit the configured limits";
            return finish(false);
        }
        const WorldBackupInfo oldest = backups.front();
        if (!removeTree(oldest.directoryPath, metrics.error)) {
            return finish(false);
        }
        totalBytes -= oldest.totalBytes;
        backups.erase(backups.begin());
    }

    metrics.published = true;
    if (created != nullptr) {
        *created = info;
    }
    return finish(true);
}

bool WorldBackup::listBackups(std::vector<WorldBackupInfo> &backups,
                              std::string *errorMessage) const
{
    std::string error;
    bool result = validatePolicy(m_policy, error);
    if (result) {
        result = listBackupsInternal(backupRootDirectory(), m_policy, backups,
                                     error);
    }
    else {
        backups.clear();
    }
    if (errorMessage != nullptr) {
        *errorMessage = error;
    }
    return result;
}

bool WorldBackup::restoreBackup(const std::string &backupIdValue,
                                const WorldBackupOptions &options,
                                WorldBackupMetrics *metricsOutput) const
{
    WorldBackupMetrics metrics;
    const auto started = std::chrono::steady_clock::now();
    const auto finish = [&](bool result) {
        metrics.totalMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count();
        if (metricsOutput != nullptr) {
            *metricsOutput = metrics;
        }
        return result;
    };
    if (!validatePolicy(m_policy, metrics.error)) {
        return finish(false);
    }
    std::uint64_t sequence = 0;
    if (!parseBackupId(backupIdValue, sequence)) {
        metrics.error = "backup id is not canonical";
        return finish(false);
    }
    const fs::path backupRoot = backupRootDirectory();
    const fs::path source = backupRoot / backupIdValue;
    ParsedManifest manifest;
    if (!loadBackup(source, m_policy, manifest, metrics.error)) {
        metrics.error = "backup validation failed: " + metrics.error;
        if (isPolicyRejection(metrics.error)) {
            return finish(false);
        }
        std::error_code sourceError;
        const fs::file_status sourceStatus =
            fs::symlink_status(source, sourceError);
        if (!sourceError && fs::is_directory(sourceStatus) &&
            !fs::is_symlink(sourceStatus)) {
            std::string quarantineError;
            if (!quarantineDirectory(source, backupRoot / CorruptBackupName,
                                     metrics, quarantineError)) {
                appendError(metrics.error, quarantineError);
            }
        }
        return finish(false);
    }

    const fs::path pending = backupRoot / RestorePendingName;
    const fs::path failed = backupRoot / RestoreFailedName;
    std::error_code pendingError;
    if (fs::exists(pending, pendingError)) {
        if (!quarantineDirectory(pending, failed, metrics, metrics.error)) {
            return finish(false);
        }
    }
    else if (pendingError) {
        metrics.error = "cannot inspect restore candidate: " +
                        pendingError.message();
        return finish(false);
    }
    if (!ensureRealDirectory(pending, metrics.error)) {
        return finish(false);
    }
    const auto failCandidate = [&]() {
        std::string quarantineError;
        if (!quarantineDirectory(pending, failed, metrics,
                                 quarantineError)) {
            appendError(metrics.error, quarantineError);
        }
        return finish(false);
    };
    if (!writeFiles(pending, manifest.files, metrics, metrics.error)) {
        return failCandidate();
    }
    if (options.faultPoint ==
        WorldBackupFaultPoint::BeforeRestoreValidation) {
        metrics.error = std::string("injected fault at ") +
                        worldBackupFaultPointName(options.faultPoint);
        return failCandidate();
    }
    std::vector<BackupFile> stagedFiles;
    int stagedVersion = 0;
    if (!collectWorldFiles(pending, m_policy, true, stagedFiles,
                           stagedVersion, metrics.error) ||
        stagedVersion != manifest.info.worldFormatVersion ||
        !sameFileSet(stagedFiles, manifest.files)) {
        if (metrics.error.empty()) {
            metrics.error = "restore candidate differs from backup";
        }
        return failCandidate();
    }
    metrics.candidateValidated = true;
    if (options.faultPoint == WorldBackupFaultPoint::BeforeRestorePublish) {
        metrics.error = std::string("injected fault at ") +
                        worldBackupFaultPointName(options.faultPoint);
        return failCandidate();
    }

    std::vector<BackupFile> primaryFiles;
    int ignoredVersion = 0;
    std::string primaryError;
    if (!collectWorldFiles(m_worldDirectory, m_policy, false, primaryFiles,
                           ignoredVersion, primaryError, false)) {
        metrics.error = "cannot preserve current primary: " + primaryError;
        return failCandidate();
    }
    const fs::path recoveryPending =
        fs::path(m_worldDirectory) / RecoveryPendingName;
    const fs::path recoveryFailed =
        fs::path(m_worldDirectory) / RecoveryFailedName;
    if (!removeTree(recoveryPending, metrics.error) ||
        !ensureRealDirectory(recoveryPending, metrics.error) ||
        !writeFiles(recoveryPending, primaryFiles, metrics, metrics.error,
                    false) ||
        !replaceDirectory(recoveryPending, recoveryFailed, metrics.error)) {
        return failCandidate();
    }

    std::set<std::string> publishedPaths;
    bool publishFailed = false;
    for (const BackupFile &file : stagedFiles) {
        if (!publishBytes(fs::path(m_worldDirectory) / file.relativePath,
                          file.bytes, &file, metrics, metrics.error)) {
            publishFailed = true;
            break;
        }
        publishedPaths.insert(file.relativePath);
        if (options.faultPoint ==
            WorldBackupFaultPoint::AfterFirstRestorePublish) {
            metrics.error = std::string("injected fault at ") +
                            worldBackupFaultPointName(options.faultPoint);
            publishFailed = true;
            break;
        }
    }

    if (!publishFailed) {
        std::set<std::string> restoredPaths;
        for (const BackupFile &file : stagedFiles) {
            restoredPaths.insert(file.relativePath);
        }
        for (const BackupFile &file : primaryFiles) {
            if (restoredPaths.count(file.relativePath) != 0) {
                continue;
            }
            std::error_code removeError;
            fs::remove(fs::path(m_worldDirectory) / file.relativePath,
                       removeError);
            if (removeError) {
                metrics.error = "cannot remove newer primary file " +
                                file.relativePath + ": " +
                                removeError.message();
                publishFailed = true;
                break;
            }
        }
    }

    if (publishFailed) {
        std::string rollbackError;
        if (!rollbackPrimary(m_worldDirectory, primaryFiles, publishedPaths,
                             metrics, rollbackError)) {
            appendError(metrics.error, rollbackError);
        }
        return failCandidate();
    }

    if (!removeTree(pending, metrics.error)) {
        return finish(false);
    }
    metrics.published = true;
    return finish(true);
}

std::string WorldBackup::backupRootDirectory() const
{
    return (fs::path(m_worldDirectory) / BackupDirectoryName).string();
}

std::string WorldBackup::recoveryQuarantineDirectory() const
{
    return (fs::path(m_worldDirectory) / RecoveryFailedName).string();
}
