#pragma once

#include "IsoCatalog.h"

#include <QString>

namespace FlashSpartan {
namespace IsoCatalogInternal {

QString normalizeFingerprint(const QString& fp);
QString rockyAlmaVersionPath(const QString& major, const QString& minor);
QString raspiosMirrorTree(const QString& suffix);

IsoPublisherMatch makeArch(const QString& fileName, const QString& versionTag);
IsoPublisherMatch makeUbuntu(const QString& fileName, const QString& version);
IsoPublisherMatch makeDebian(const QString& fileName, const QString& version);
IsoPublisherMatch makeLinuxMint(const QString& fileName, const QString& majorVersion);
IsoPublisherMatch makeOpenSuseLeap(const QString& fileName, const QString& version);
IsoPublisherMatch makeOpenSuseTumbleweed(const QString& fileName);
IsoPublisherMatch makeManjaro(const QString& fileName, const QString& edition, const QString& version);
IsoPublisherMatch makeFedora(const QString& fileName, const QString& version);
IsoPublisherMatch makeCdimageUbuntuFlavor(const QString& publisherId, const QString& publisherName,
                                          const QString& cdimagePath, const QString& fileName,
                                          const QString& version);
IsoPublisherMatch makeKali(const QString& fileName, const QString& version);
IsoPublisherMatch makeRocky(const QString& fileName, const QString& versionPath);
IsoPublisherMatch makeAlmaLinux(const QString& fileName, const QString& versionPath);
IsoPublisherMatch makePopOs(const QString& fileName, const QString& version, const QString& variant,
                            const QString& build);
IsoPublisherMatch makePopOsSimple(const QString& fileName, const QString& version);
IsoPublisherMatch makeCentOsStream(const QString& fileName, const QString& major);
IsoPublisherMatch makeElementary(const QString& fileName, const QString& version);
IsoPublisherMatch makeGaruda(const QString& fileName, const QString& edition, const QString& date);
IsoPublisherMatch makeCachyOs(const QString& fileName, const QString& variant, const QString& date);
IsoPublisherMatch makeNobara(const QString& fileName);
IsoPublisherMatch makeRaspiosOs(const QString& fileName, const QString& date, const QString& suffix);
IsoPublisherMatch makeUbuntuRpi(const QString& fileName, const QString& version);
IsoPublisherMatch makeAlpine(const QString& fileName, const QString& version, const QString& arch);
IsoPublisherMatch makeVoidLinux(const QString& fileName);
IsoPublisherMatch makeArmbian(const QString& fileName);
IsoPublisherMatch makeNixos(const QString& fileName, const QString& channel, const QString& variant);
IsoPublisherMatch makeEndeavourOs(const QString& fileName, const QString& dateVersion);

} // namespace IsoCatalogInternal
} // namespace FlashSpartan
