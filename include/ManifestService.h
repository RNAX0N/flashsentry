#pragma once

#include "Types.h"

#include <QString>
#include <QStringList>

namespace FlashSpartan {

/**
 * @brief Build and verify Merkle-backed watch manifests on mounted volumes.
 */
class ManifestService {
public:
  struct BuildResult {
    bool success = false;
    QString errorMessage;
    WatchGroup group;
  };

  struct VerifyResult {
    bool success = false;
    bool matches = false;
    QString computedRootHex;
    QString expectedRootHex;
    QStringList changedPaths;
    QStringList missingPaths;
    QStringList addedPaths;
    QString errorMessage;
    uint64_t filesChecked = 0;
    uint64_t durationMs = 0;
  };

  static QString hashFileContents(const QString& absolutePath, QString* errorOut = nullptr);

  static BuildResult buildGroup(const QString& mountPoint, const WatchGroup& spec);

  static VerifyResult verifyGroup(const QString& mountPoint, const WatchGroup& baseline);

  static VerifyResult verifyManifest(const QString& mountPoint, const WatchManifest& manifest);

  static QString manifestRootHex(const WatchManifest& manifest);

  static WatchManifest rebuildManifestRoots(const QString& mountPoint, const WatchManifest& spec);
};

} // namespace FlashSpartan
