// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractview.h"
#include "datastoremodelnode.h"
#include "modelnode.h"

#include <QJsonObject>

namespace QmlJS {
class Document;
}

namespace QmlDesigner {

class CollectionDetails;
class CollectionListModel;
class CollectionTask;
class CollectionWidget;
class DataStoreModelNode;

class CollectionView : public AbstractView
{
    Q_OBJECT

public:
    explicit CollectionView(ExternalDependenciesInterface &externalDependencies);

    bool hasWidget() const override;
    WidgetInfo widgetInfo() override;

    void modelAttached(Model *model) override;
    void modelAboutToBeDetached(Model *model) override;

    void selectedNodesChanged(const QList<ModelNode> &selectedNodeList,
                              const QList<ModelNode> &lastSelectedNodeList) override;

    void customNotification(const AbstractView *view,
                            const QString &identifier,
                            const QList<ModelNode> &nodeList,
                            const QList<QVariant> &data) override;

    void addResource(const QUrl &url, const QString &name);

    void assignCollectionToNode(const QString &collectionName, const ModelNode &node);
    void assignCollectionToSelectedNode(const QString &collectionName);
    void addNewCollection(const QString &collectionName, const QJsonObject &localCollection);

    void openCollection(const QString &collectionName);

    static void registerDeclarativeType();

    void resetDataStoreNode();
    ModelNode dataStoreNode() const;
    void ensureDataStoreExists();
    QString collectionNameFromDataStoreChildren(const PropertyName &childPropertyName) const;

private:
    friend class CollectionTask;

    NodeMetaInfo jsonCollectionMetaInfo() const;
    void ensureStudioModelImport();
    void onItemLibraryNodeCreated(const ModelNode &node);
    void onDocumentUpdated(const QSharedPointer<const QmlJS::Document> &doc);
    void addTask(QSharedPointer<CollectionTask> task);

    QPointer<CollectionWidget> m_widget;
    std::unique_ptr<DataStoreModelNode> m_dataStore;
    QSet<Utils::FilePath> m_expectedDocumentUpdates;
    QList<QSharedPointer<CollectionTask>> m_delayedTasks;
    QMetaObject::Connection m_documentUpdateConnection;
    bool m_libraryInfoIsUpdated = false;
    bool m_dataStoreTypeFound = false;
    bool m_rewriterAmended = false;
    int m_reloadCounter = 0;
};

class CollectionTask
{
public:
    CollectionTask(CollectionView *view, CollectionListModel *listModel);
    CollectionTask() = delete;
    virtual ~CollectionTask() = default;

    virtual void process() = 0;

protected:
    QPointer<CollectionView> m_collectionView;
    QPointer<CollectionListModel> m_listModel;
};

class DropListViewTask : public CollectionTask
{
public:
    DropListViewTask(CollectionView *view, CollectionListModel *listModel, const ModelNode &node);

    void process() override;

private:
    ModelNode m_node;
};

class AddCollectionTask : public CollectionTask
{
public:
    AddCollectionTask(CollectionView *view,
                      CollectionListModel *listModel,
                      const QJsonObject &localJsonObject,
                      const QString &collectionName);

    void process() override;

private:
    QJsonObject m_localJsonObject;
    QString m_name;
};

} // namespace QmlDesigner
