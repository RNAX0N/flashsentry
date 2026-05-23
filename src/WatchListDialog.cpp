#include "WatchListDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QUuid>
#include <QMessageBox>

namespace FlashSentry {

WatchListDialog::WatchListDialog(const QString& mountPoint, const QString& deviceDisplayName,
                                 WatchManifest manifest, QWidget* parent)
    : QDialog(parent)
    , m_mountPoint(mountPoint)
    , m_deviceName(deviceDisplayName)
    , m_manifest(std::move(manifest))
{
    setWindowTitle(QStringLiteral("Watch lists — %1").arg(deviceDisplayName));
    resize(640, 480);

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        QStringLiteral("Define file groups to verify with Merkle summaries on <b>%1</b> (%2)")
            .arg(deviceDisplayName, mountPoint)));

    auto* topRow = new QHBoxLayout;
    m_groupList = new QListWidget;
    m_groupList->setMinimumWidth(180);
    topRow->addWidget(m_groupList, 1);

    auto* editorBox = new QGroupBox(QStringLiteral("Group"));
    auto* editorLayout = new QVBoxLayout(editorBox);

    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("Name:")));
    m_groupNameEdit = new QLineEdit;
    nameRow->addWidget(m_groupNameEdit, 1);
    editorLayout->addLayout(nameRow);

    m_pathList = new QListWidget;
    editorLayout->addWidget(m_pathList, 1);

    auto* pathBtnRow = new QHBoxLayout;
    auto* addFileBtn = new QPushButton(QStringLiteral("Add files…"));
    auto* addDirBtn = new QPushButton(QStringLiteral("Add folder…"));
    auto* removePathBtn = new QPushButton(QStringLiteral("Remove"));
    connect(addFileBtn, &QPushButton::clicked, this, &WatchListDialog::onAddFiles);
    connect(addDirBtn, &QPushButton::clicked, this, &WatchListDialog::onAddDirectory);
    connect(removePathBtn, &QPushButton::clicked, this, &WatchListDialog::onRemovePath);
    pathBtnRow->addWidget(addFileBtn);
    pathBtnRow->addWidget(addDirBtn);
    pathBtnRow->addStretch();
    pathBtnRow->addWidget(removePathBtn);
    editorLayout->addLayout(pathBtnRow);

    topRow->addWidget(editorBox, 2);
    layout->addLayout(topRow, 1);

    auto* groupBtnRow = new QHBoxLayout;
    auto* addGroupBtn = new QPushButton(QStringLiteral("Add group"));
    auto* removeGroupBtn = new QPushButton(QStringLiteral("Remove group"));
    connect(addGroupBtn, &QPushButton::clicked, this, &WatchListDialog::onAddGroup);
    connect(removeGroupBtn, &QPushButton::clicked, this, &WatchListDialog::onRemoveGroup);
    groupBtnRow->addWidget(addGroupBtn);
    groupBtnRow->addWidget(removeGroupBtn);
    groupBtnRow->addStretch();
    layout->addLayout(groupBtnRow);

    auto* profileForm = new QFormLayout;
    m_profileCombo = new QComboBox;
    m_profileCombo->addItem(QStringLiteral("Watch manifest (fast)"), static_cast<int>(VerificationProfile::WatchManifest));
    m_profileCombo->addItem(QStringLiteral("Full partition (deep)"), static_cast<int>(VerificationProfile::FullPartition));
    m_profileCombo->addItem(QStringLiteral("Hybrid (watch + full hash)"), static_cast<int>(VerificationProfile::Hybrid));
    profileForm->addRow(QStringLiteral("Verification mode for this drive:"), m_profileCombo);
    layout->addLayout(profileForm);

    m_buildBtn = new QPushButton(QStringLiteral("Build Merkle baseline from current paths"));
    connect(m_buildBtn, &QPushButton::clicked, this, &WatchListDialog::onBuildBaseline);
    layout->addWidget(m_buildBtn);

    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch();
    auto* closeBtn = new QPushButton(QStringLiteral("Done"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    layout->addLayout(closeRow);

    connect(m_groupList, &QListWidget::currentRowChanged, this, &WatchListDialog::onGroupSelectionChanged);

    refreshGroupList();
    if (m_manifest.groups.isEmpty()) {
        onAddGroup();
    } else {
        m_groupList->setCurrentRow(0);
    }
}

void WatchListDialog::onAddGroup()
{
    saveEditorToCurrentGroup();
    WatchGroup g;
    g.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    g.name = QStringLiteral("Group %1").arg(m_manifest.groups.size() + 1);
    m_manifest.groups.append(g);
    refreshGroupList();
    m_groupList->setCurrentRow(m_manifest.groups.size() - 1);
}

void WatchListDialog::onRemoveGroup()
{
    const int row = m_groupList->currentRow();
    if (row < 0 || row >= m_manifest.groups.size()) {
        return;
    }
    m_manifest.groups.removeAt(row);
    m_currentGroupIndex = -1;
    refreshGroupList();
    if (!m_manifest.groups.isEmpty()) {
        m_groupList->setCurrentRow(0);
    }
}

void WatchListDialog::onGroupSelectionChanged()
{
    saveEditorToCurrentGroup();
    loadGroupIntoEditor(m_groupList->currentRow());
}

void WatchListDialog::onAddFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Select files to watch"), m_mountPoint);
    for (const QString& f : files) {
        QString rel = f;
        if (rel.startsWith(m_mountPoint)) {
            rel = rel.mid(m_mountPoint.size());
            if (rel.startsWith(QLatin1Char('/'))) {
                rel = rel.mid(1);
            }
        }
        m_pathList->addItem(rel);
    }
}

void WatchListDialog::onAddDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select folder to watch"), m_mountPoint);
    if (dir.isEmpty()) {
        return;
    }
    QString rel = dir;
    if (rel.startsWith(m_mountPoint)) {
        rel = rel.mid(m_mountPoint.size());
        if (rel.startsWith(QLatin1Char('/'))) {
            rel = rel.mid(1);
        }
    }
    m_pathList->addItem(rel);
}

void WatchListDialog::onRemovePath()
{
    const auto items = m_pathList->selectedItems();
    for (QListWidgetItem* item : items) {
        delete m_pathList->takeItem(m_pathList->row(item));
    }
}

VerificationProfile WatchListDialog::selectedProfile() const
{
    const int idx = m_profileCombo->currentIndex();
    if (idx < 0) {
        return VerificationProfile::WatchManifest;
    }
    return static_cast<VerificationProfile>(m_profileCombo->itemData(idx).toInt());
}

void WatchListDialog::onBuildBaseline()
{
    saveEditorToCurrentGroup();
    if (m_manifest.groups.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("No groups"),
                             QStringLiteral("Add at least one watch group."));
        return;
    }
    for (const WatchGroup& g : m_manifest.groups) {
        if (g.watchPaths.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Empty group"),
                                 QStringLiteral("Group '%1' has no paths.").arg(g.name));
            return;
        }
    }
    m_baselineBuilt = true;
    emit buildBaselineRequested(m_manifest);
    QMessageBox::information(
        this, QStringLiteral("Building baseline"),
        QStringLiteral("FlashSentry will hash the selected paths and store Merkle roots. "
                       "This may take a moment."));
    accept();
}

void WatchListDialog::refreshGroupList()
{
    m_groupList->clear();
    for (const WatchGroup& g : m_manifest.groups) {
        m_groupList->addItem(g.name);
    }
}

void WatchListDialog::loadGroupIntoEditor(int index)
{
    m_currentGroupIndex = index;
    if (index < 0 || index >= m_manifest.groups.size()) {
        m_groupNameEdit->clear();
        m_pathList->clear();
        return;
    }
    const WatchGroup& g = m_manifest.groups.at(index);
    m_groupNameEdit->setText(g.name);
    m_pathList->clear();
    for (const QString& p : g.watchPaths) {
        m_pathList->addItem(p);
    }
}

void WatchListDialog::saveEditorToCurrentGroup()
{
    if (m_currentGroupIndex < 0 || m_currentGroupIndex >= m_manifest.groups.size()) {
        return;
    }
    m_manifest.groups[m_currentGroupIndex] = currentGroupFromEditor();
}

WatchGroup WatchListDialog::currentGroupFromEditor() const
{
    WatchGroup g;
    if (m_currentGroupIndex >= 0 && m_currentGroupIndex < m_manifest.groups.size()) {
        g.id = m_manifest.groups.at(m_currentGroupIndex).id;
    } else {
        g.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    g.name = m_groupNameEdit->text().trimmed();
    if (g.name.isEmpty()) {
        g.name = QStringLiteral("Unnamed group");
    }
    for (int i = 0; i < m_pathList->count(); ++i) {
        g.watchPaths.append(m_pathList->item(i)->text());
    }
    return g;
}

} // namespace FlashSentry
