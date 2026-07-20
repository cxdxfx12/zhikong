#pragma once
#include <QDialog>
#include <QDateEdit>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFormLayout>
#include <QMap>
#include <QWidget>
#include "model/ColumnDef.h"
#include "model/Entity.h"

class DataEntryDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { ModeNew, ModeEdit };

    explicit DataEntryDialog(QWidget* parent = nullptr);

    void setMode(Mode mode);
    void setDate(const QDate& date);
    void setEditTarget(int entityId, const QString& entityType, const QDate& date);

signals:
    void dataSaved();

private slots:
    void onTypeOrEntityChanged();
    void onSave();
    void onCopyPreviousDay();

private:
    void setupUI();
    void rebuildForm(int entityTypeId);
    void clearForm();
    void loadExistingData(int entityId, const QDate& date);
    void populateFieldsFromPreviousDay(int entityId, const QDate& prevDate);

    Mode m_mode = ModeNew;
    int m_editEntityId = 0;

    // Top controls
    QDateEdit* m_dateEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_entityCombo = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_copyPrevBtn = nullptr;

    // Dynamic form area
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_formWidget = nullptr;
    QFormLayout* m_formLayout = nullptr;

    // editor widget → columnId mapping
    QMap<int, QWidget*> m_editorMap;

    // Current state
    QVector<Entity> m_allEntities;
    QVector<ColumnDef> m_columns;
    bool m_updatingUI = false;  // guard against recursive signals
};
