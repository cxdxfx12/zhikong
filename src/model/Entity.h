#pragma once
#include <QString>
#include <QVector>

struct Entity {
    int id = 0;
    int typeId = 0;
    int parentId = 0;
    QString typeName;
    QString name;
    QString parentName;
    int sortOrder = 0;
    bool isActive = true;

    bool isCompany() const { return typeName == u"公司"; }
    bool isContractor() const { return typeName == u"承包区"; }
    bool isTopLevel() const { return parentId == 0; }

    QString fullPath() const {
        if (isCompany()) return name;
        return parentName.isEmpty() ? name : parentName + " / " + name;
    }
};

struct EntityTreeNode {
    Entity entity;
    QVector<EntityTreeNode> children;
};
