// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QFrame>

#include <coreplugin/icontext.h>

class StudioQuickWidget;

namespace QmlDesigner {

class CollectionDetailsModel;
class CollectionDetailsSortFilterModel;
class CollectionListModel;
class CollectionView;
class ModelNode;

class CollectionWidget : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(bool targetNodeSelected MEMBER m_targetNodeSelected NOTIFY targetNodeSelectedChanged)

public:
    CollectionWidget(CollectionView *view);
    void contextHelp(const Core::IContext::HelpCallback &callback) const;

    QPointer<CollectionListModel> listModel() const;
    QPointer<CollectionDetailsModel> collectionDetailsModel() const;

    void reloadQmlSource();

    virtual QSize minimumSizeHint() const;

    Q_INVOKABLE bool loadJsonFile(const QUrl &url, const QString &collectionName = {});
    Q_INVOKABLE bool loadCsvFile(const QUrl &url, const QString &collectionName = {});
    Q_INVOKABLE bool isJsonFile(const QUrl &url) const;
    Q_INVOKABLE bool isCsvFile(const QUrl &url) const;
    Q_INVOKABLE bool isValidUrlToImport(const QUrl &url) const;

    Q_INVOKABLE bool importFile(const QString &collectionName,
                                const QUrl &url,
                                const bool &firstRowIsHeader = true);

    Q_INVOKABLE void addCollectionToDataStore(const QString &collectionName);
    Q_INVOKABLE void assignCollectionToSelectedNode(const QString collectionName);
    Q_INVOKABLE void openCollection(const QString &collectionName);
    Q_INVOKABLE ModelNode dataStoreNode() const;

    void warn(const QString &title, const QString &body);
    void setTargetNodeSelected(bool selected);

    void deleteSelectedCollection();

signals:
    void targetNodeSelectedChanged(bool);

private:
    QString generateUniqueCollectionName(const ModelNode &node, const QString &name);

    QPointer<CollectionView> m_view;
    QPointer<CollectionListModel> m_listModel;
    QPointer<CollectionDetailsModel> m_collectionDetailsModel;
    std::unique_ptr<CollectionDetailsSortFilterModel> m_collectionDetailsSortFilterModel;
    QScopedPointer<StudioQuickWidget> m_quickWidget;
    bool m_targetNodeSelected = false;
};

} // namespace QmlDesigner
