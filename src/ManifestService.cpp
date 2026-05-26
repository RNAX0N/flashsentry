#include "ManifestService.h"
#include "MerkleTree.h"

#include <openssl/evp.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>

namespace FlashSentry {

namespace {

QString normalizeMount(const QString& mountPoint)
{
    QString m = QDir::fromNativeSeparators(mountPoint);
    while (m.endsWith(QLatin1Char('/')) && m.size() > 1) {
        m.chop(1);
    }
    return m;
}

bool isWithinMount(const QString& mountPoint, const QString& path)
{
    const QString mount = normalizeMount(QDir::cleanPath(QDir::fromNativeSeparators(mountPoint)));
    const QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return cleanPath == mount || cleanPath.startsWith(mount + QLatin1Char('/'));
}

QString relativePathUnder(const QString& mountPoint, const QString& absolutePath)
{
    const QString mount = normalizeMount(mountPoint);
    const QString abs = QDir::cleanPath(QDir::fromNativeSeparators(absolutePath));
    if (abs == mount) {
        return {};
    }
    if (abs.startsWith(mount + QLatin1Char('/'))) {
        return abs.mid(mount.size() + 1);
    }
    return {};
}

QStringList collectFilesForPaths(const QString& mountPoint, const QStringList& paths, QString* errorOut)
{
    QStringList files;
    const QString normalizedMount = normalizeMount(mountPoint);
    const QString mount = QFileInfo(normalizedMount).canonicalFilePath();
    if (mount.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Mount point does not exist: %1").arg(mountPoint);
        }
        return {};
    }

    QDir root(mount);
    if (!root.exists()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Mount point does not exist: %1").arg(mountPoint);
        }
        return {};
    }

    for (const QString& path : paths) {
        QString trimmed = path.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        QString absolute = trimmed;
        if (!QDir::isAbsolutePath(trimmed)) {
            absolute = root.absoluteFilePath(trimmed);
        }
        const QString cleanAbsolute = QDir::cleanPath(QDir::fromNativeSeparators(absolute));
        if (!isWithinMount(mount, cleanAbsolute)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Watch path escapes mount point: %1").arg(trimmed);
            }
            return {};
        }

        QFileInfo info(cleanAbsolute);
        if (!info.exists()) {
            continue;
        }
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || !isWithinMount(mount, canonical)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Watch path escapes mount point: %1").arg(trimmed);
            }
            return {};
        }

        if (info.isFile()) {
            files.append(canonical);
            continue;
        }
        if (info.isDir()) {
            QDirIterator it(canonical, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString filePath = it.next();
                const QString fileCanonical = QFileInfo(filePath).canonicalFilePath();
                if (fileCanonical.isEmpty() || !isWithinMount(mount, fileCanonical)) {
                    if (errorOut) {
                        *errorOut = QStringLiteral("Watched file escapes mount point: %1").arg(filePath);
                    }
                    return {};
                }
                files.append(fileCanonical);
            }
        }
    }

    files.sort();
    files.removeDuplicates();
    return files;
}

WatchGroup finalizeGroup(const QString& mountPoint, const QString& groupId, const QString& name,
                         const QStringList& watchPaths, const QVector<MerkleTree::Leaf>& leaves)
{
    WatchGroup group;
    group.id = groupId;
    group.name = name;
    group.watchPaths = watchPaths;
    group.builtAt = QDateTime::currentDateTimeUtc();

    for (const MerkleTree::Leaf& leaf : leaves) {
        WatchFileEntry entry;
        entry.relativePath = leaf.relativePath;
        entry.contentHash = leaf.contentHashHex;
        QFileInfo fi(normalizeMount(mountPoint) + QLatin1Char('/') + leaf.relativePath);
        if (fi.exists()) {
            entry.sizeBytes = static_cast<uint64_t>(fi.size());
            entry.modifiedUtc = fi.lastModified().toUTC();
        }
        group.files.append(entry);
    }

    group.merkleRoot = MerkleTree::rootHex(leaves);
    return group;
}

} // namespace

QString ManifestService::hashFileContents(const QString& absolutePath, QString* errorOut)
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return {};
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL context failed");
        }
        return {};
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL hash initialization failed");
        }
        return {};
    }

    QByteArray buffer;
    buffer.resize(1024 * 1024);
    while (true) {
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n < 0) {
            EVP_MD_CTX_free(ctx);
            if (errorOut) {
                *errorOut = file.errorString();
            }
            return {};
        }
        if (n == 0) {
            break;
        }
        if (EVP_DigestUpdate(ctx, buffer.constData(), static_cast<size_t>(n)) != 1) {
            EVP_MD_CTX_free(ctx);
            if (errorOut) {
                *errorOut = QStringLiteral("OpenSSL hash update failed");
            }
            return {};
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &len) != 1) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL hash finalization failed");
        }
        return {};
    }
    EVP_MD_CTX_free(ctx);

    return MerkleTree::toHex(QByteArray(reinterpret_cast<const char*>(hash), static_cast<int>(len)));
}

ManifestService::BuildResult ManifestService::buildGroup(const QString& mountPoint, const WatchGroup& spec)
{
    BuildResult result;
    QString err;
    const QStringList files = collectFilesForPaths(mountPoint, spec.watchPaths, &err);
    if (!err.isEmpty()) {
        result.errorMessage = err;
        return result;
    }
    if (files.isEmpty()) {
        result.errorMessage = QStringLiteral("No files found for watch paths");
        return result;
    }

    QVector<MerkleTree::Leaf> leaves;
    leaves.reserve(files.size());
    for (const QString& abs : files) {
        QString fileErr;
        const QString hash = hashFileContents(abs, &fileErr);
        if (hash.isEmpty()) {
            result.errorMessage = QStringLiteral("%1: %2").arg(abs, fileErr);
            return result;
        }
        MerkleTree::Leaf leaf;
        leaf.relativePath = relativePathUnder(mountPoint, abs);
        leaf.contentHashHex = hash;
        leaves.append(leaf);
    }

    result.group = finalizeGroup(mountPoint, spec.id, spec.name, spec.watchPaths, leaves);
    result.success = true;
    return result;
}

ManifestService::VerifyResult ManifestService::verifyGroup(const QString& mountPoint, const WatchGroup& baseline)
{
    VerifyResult result;
    QElapsedTimer timer;
    timer.start();

    WatchGroup spec = baseline;
    spec.watchPaths = baseline.watchPaths;
    const BuildResult built = buildGroup(mountPoint, spec);
    if (!built.success) {
        result.errorMessage = built.errorMessage;
        return result;
    }

    result.success = true;
    result.computedRootHex = built.group.merkleRoot;
    result.expectedRootHex = baseline.merkleRoot;
    result.filesChecked = static_cast<uint64_t>(built.group.files.size());
    result.durationMs = static_cast<uint64_t>(timer.elapsed());

    QHash<QString, QString> expected;
    for (const WatchFileEntry& e : baseline.files) {
        expected.insert(e.relativePath, e.contentHash);
    }
    QHash<QString, QString> actual;
    for (const WatchFileEntry& e : built.group.files) {
        actual.insert(e.relativePath, e.contentHash);
    }

    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        if (!actual.contains(it.key())) {
            result.missingPaths.append(it.key());
        } else if (actual.value(it.key()) != it.value()) {
            result.changedPaths.append(it.key());
        }
    }
    for (auto it = actual.constBegin(); it != actual.end(); ++it) {
        if (!expected.contains(it.key())) {
            result.addedPaths.append(it.key());
        }
    }

    result.matches = (result.computedRootHex == result.expectedRootHex)
                     && result.changedPaths.isEmpty()
                     && result.missingPaths.isEmpty()
                     && result.addedPaths.isEmpty();
    return result;
}

ManifestService::VerifyResult ManifestService::verifyManifest(const QString& mountPoint,
                                                              const WatchManifest& manifest)
{
    VerifyResult combined;
    combined.success = true;
    combined.matches = true;
    QElapsedTimer timer;
    timer.start();

    if (manifest.groups.isEmpty()) {
        combined.errorMessage = QStringLiteral("No watch groups configured");
        combined.matches = false;
        return combined;
    }

    for (const WatchGroup& group : manifest.groups) {
        if (group.merkleRoot.isEmpty()) {
            combined.matches = false;
            combined.errorMessage = QStringLiteral("Group '%1' has no baseline").arg(group.name);
            return combined;
        }
        const VerifyResult one = verifyGroup(mountPoint, group);
        if (!one.success) {
            return one;
        }
        combined.filesChecked += one.filesChecked;
        if (!one.matches) {
            combined.matches = false;
            for (const QString& p : one.changedPaths) {
                combined.changedPaths.append(group.name + QLatin1String(": ") + p);
            }
            for (const QString& p : one.missingPaths) {
                combined.missingPaths.append(group.name + QLatin1String(": ") + p);
            }
            for (const QString& p : one.addedPaths) {
                combined.addedPaths.append(group.name + QLatin1String(": ") + p);
            }
        }
    }

    combined.computedRootHex = manifestRootHex(manifest);
    combined.expectedRootHex = manifest.manifestRoot;
    combined.durationMs = static_cast<uint64_t>(timer.elapsed());
    return combined;
}

QString ManifestService::manifestRootHex(const WatchManifest& manifest)
{
    QVector<MerkleTree::Leaf> groupLeaves;
    QVector<WatchGroup> sorted = manifest.groups;
    std::sort(sorted.begin(), sorted.end(), [](const WatchGroup& a, const WatchGroup& b) {
        return a.id < b.id;
    });
    for (const WatchGroup& g : sorted) {
        MerkleTree::Leaf leaf;
        leaf.relativePath = g.id;
        leaf.contentHashHex = g.merkleRoot;
        groupLeaves.append(leaf);
    }
    return MerkleTree::rootHex(groupLeaves);
}

WatchManifest ManifestService::rebuildManifestRoots(const QString& mountPoint, const WatchManifest& spec)
{
    WatchManifest out = spec;
    out.groups.clear();
    for (const WatchGroup& g : spec.groups) {
        const BuildResult built = buildGroup(mountPoint, g);
        if (built.success) {
            out.groups.append(built.group);
        }
    }
    out.manifestRoot = manifestRootHex(out);
    out.updatedAt = QDateTime::currentDateTimeUtc();
    return out;
}

} // namespace FlashSentry
