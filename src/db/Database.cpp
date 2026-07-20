#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

Database& Database::instance() {
    static Database inst;
    return inst;
}

Database::~Database() {
    close();
}

QSqlDatabase& Database::db() {
    return m_db;
}

bool Database::open(const QString& dbPath) {
    if (m_db.isOpen()) return true;

    QString fullPath = dbPath;
    if (fullPath.isEmpty() || QFileInfo(fullPath).isRelative()) {
        QString dir = QCoreApplication::applicationDirPath();
        if (fullPath.isEmpty()) fullPath = "express_daily.db";
        fullPath = dir + "/" + fullPath;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(fullPath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qCritical() << "Failed to open database:" << m_lastError;
        return false;
    }

    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=DELETE");  // no WAL - avoid checkpoint issues
    q.exec("PRAGMA foreign_keys=ON");
    q.exec("PRAGMA synchronous=FULL");     // full sync to disk
    q.exec("PRAGMA busy_timeout=5000");

    qInfo() << "Database opened:" << fullPath;
    return true;
}

void Database::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::initialize() {
    if (!m_db.isOpen()) return false;

    QSqlQuery q(m_db);

    // ---- entity_types ----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS entity_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            sort_order INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now','localtime'))
        )
    )");

    q.exec("INSERT OR IGNORE INTO entity_types (name, sort_order) VALUES ('公司', 1)");
    q.exec("INSERT OR IGNORE INTO entity_types (name, sort_order) VALUES ('承包区', 2)");

    // ---- entities ----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS entities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type_id INTEGER NOT NULL REFERENCES entity_types(id) ON DELETE CASCADE,
            parent_id INTEGER DEFAULT NULL REFERENCES entities(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            sort_order INTEGER DEFAULT 0,
            is_active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT (datetime('now','localtime')),
            UNIQUE(type_id, parent_id, name)
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_entities_parent ON entities(parent_id)");

    q.exec("INSERT OR IGNORE INTO entities (id, type_id, parent_id, name, sort_order) VALUES (1, 1, NULL, '总部', 1)");
    q.exec("INSERT OR IGNORE INTO entities (id, type_id, parent_id, name, sort_order) VALUES (2, 2, 1, '承包区A', 1)");
    q.exec("INSERT OR IGNORE INTO entities (id, type_id, parent_id, name, sort_order) VALUES (3, 2, 1, '承包区B', 2)");
    q.exec("INSERT OR IGNORE INTO entities (id, type_id, parent_id, name, sort_order) VALUES (4, 2, 1, '承包区C', 3)");

    // ---- column_defs ----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS column_defs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            key TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            data_type TEXT NOT NULL DEFAULT 'INTEGER',
            unit TEXT DEFAULT '',
            entity_type_id INTEGER DEFAULT NULL,
            aggregate_type TEXT NOT NULL DEFAULT 'NONE',
            category TEXT DEFAULT '',
            is_core INTEGER DEFAULT 1,
            sort_order INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now','localtime'))
        )
    )");

    // ---- MIGRATION: ensure old DB has category column BEFORE any inserts ----
    {
        QSqlQuery mq(m_db);
        mq.exec("SELECT category FROM column_defs LIMIT 1");
        if (mq.lastError().isValid()) {
            q.exec("ALTER TABLE column_defs ADD COLUMN category TEXT DEFAULT ''");
            qInfo() << "Migration: added category column to column_defs";
        }
    }

    // Now safe to insert/update —category column is guaranteed to exist
    auto insertCol = [&](const QString& key, const QString& name, const QString& type,
                          const QString& unit, int entityTypeId, const QString& agg,
                          const QString& cat, int core, int order) {
        QSqlQuery iq(m_db);
        iq.prepare("INSERT OR IGNORE INTO column_defs (key, display_name, data_type, unit, entity_type_id, aggregate_type, category, is_core, sort_order) "
                   "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        iq.addBindValue(key); iq.addBindValue(name); iq.addBindValue(type);
        iq.addBindValue(unit);
        iq.addBindValue(entityTypeId > 0 ? QVariant(entityTypeId) : QVariant());
        iq.addBindValue(agg); iq.addBindValue(cat); iq.addBindValue(core); iq.addBindValue(order);
        iq.exec();
    };

    // ========== 业务 ==========
    insertCol("outbound",             "出港",         "INTEGER", "票",  0, "SUM",  "业务", 1, 1);
    insertCol("inbound",              "进港",         "INTEGER", "票",  0, "SUM",  "业务", 1, 2);
    insertCol("sign_rate_first",      "一阶签收率",    "REAL",   "%",   0, "AVG",  "业务", 1, 3);
    insertCol("sign_rate_second",     "二阶签收率",    "REAL",   "%",   0, "AVG",  "业务", 1, 4);
    insertCol("sign_rate",            "当天签收率",    "REAL",   "%",   0, "AVG",  "业务", 1, 5);
    insertCol("pickup_rate",          "揽收及时率",    "REAL",   "%",   0, "AVG",  "业务", 1, 6);
    insertCol("unsign_count",         "未按时签收量",  "INTEGER", "票",  0, "SUM",  "业务", 1, 7);
    insertCol("sign_penalty",         "签收率未达标考核","REAL",  "元",  1, "NONE", "业务", 0, 8);
    insertCol("intercept_rate",       "拦截件及时转出率","REAL",  "%",   1, "NONE", "业务", 0, 9);

    // ========== 客服 ==========
    insertCol("three_parts_count",    "三件数量",      "INTEGER","票",  0, "SUM",  "客服", 0, 10);
    insertCol("three_parts_compensation","三件总赔款线上","REAL", "元",  0, "SUM",  "客服", 0, 11);
    insertCol("private_compensation", "私了赔款",      "REAL",   "元",  1, "NONE", "客服", 0, 12);
    insertCol("work_order_fine",      "工单罚款",      "REAL",   "元",  1, "NONE", "客服", 0, 13);

    // ========== 操作 ==========
    insertCol("delivery_stay_rate",   "交货滞留率",    "REAL",   "%",   1, "NONE", "操作", 0, 14);
    insertCol("return_stay",          "回货滞留",      "INTEGER","票",  0, "SUM",  "操作", 0, 15);
    insertCol("delivery_schedule_score","分频次交货考核","REAL",  "分",  1, "NONE", "操作", 0, 16);
    insertCol("outbound_timely_rate1","出仓及时率1阶",  "REAL",   "%",   1, "NONE", "操作", 0, 17);
    insertCol("outbound_timely_rate2","出仓及时率2阶",  "REAL",   "%",   1, "NONE", "操作", 0, 18);
    insertCol("operation_complaint",  "运营投诉",      "INTEGER","票",  0, "SUM",  "操作", 0, 19);
    insertCol("outbound_misppm",      "出港错分",      "REAL",   "ppm", 1, "NONE", "操作", 0, 20);
    insertCol("auto_throughput",      "自动化吞吐量",   "INTEGER","票",  0, "SUM",  "操作", 0, 21);

    // ========== 小件员取派签质量 ==========
    insertCol("fake_sign",            "虚假签收",     "INTEGER", "票",  0, "SUM",  "小件员取派签质量", 1, 22);
    insertCol("miss_rate",            "爽约率",       "REAL",   "%",   0, "AVG",  "小件员取派签质量", 1, 23);
    insertCol("inbound_complaint_ppm","进港投诉率",    "REAL",   "ppm", 1, "NONE", "小件员取派签质量", 0, 24);
    insertCol("info_index_score",     "信息指数考核",  "INTEGER","票",  1, "NONE", "小件员取派签质量", 0, 25);
    insertCol("scatter_achievement_rate","散单达成率",  "REAL",   "%",   0, "AVG",  "小件员取派签质量", 0, 26);
    insertCol("first_pickup_rate",    "首揽及时率",    "REAL",   "%",   0, "AVG",  "小件员取派签质量", 0, 27);
    insertCol("customer_voice",       "客户声音",      "INTEGER","票",  0, "SUM",  "小件员取派签质量", 0, 28);
    insertCol("on_demand",            "按需派送",      "REAL",   "%",   0, "AVG",  "小件员取派签质量", 0, 29);
    insertCol("call_rate",            "电联",          "REAL",   "%",   0, "AVG",  "小件员取派签质量", 0, 30);
    insertCol("sms_rate",             "短信",          "REAL",   "%",   0, "AVG",  "小件员取派签质量", 0, 31);

    // ========== 运营 ==========
    insertCol("headquarter_fine",     "总部罚款",      "REAL",   "元",  1, "NONE", "运营", 0, 32);
    insertCol("kpi_score",            "网点KPI",      "REAL",   "分",  0, "AVG",  "运营", 1, 33);

    // ---- MIGRATION: update existing rows that may have wrong categories/names ----
    {
        auto updateCat = [&](const QString& key, const QString& cat) {
            QSqlQuery uq(m_db);
            uq.prepare("UPDATE column_defs SET category = ? WHERE key = ? AND (category IS NULL OR category = '')");
            uq.addBindValue(cat); uq.addBindValue(key); uq.exec();
        };
        updateCat("outbound",              "业务");
        updateCat("inbound",               "业务");
        updateCat("sign_rate_first",       "业务");
        updateCat("sign_rate_second",      "业务");
        updateCat("sign_rate",             "业务");
        updateCat("pickup_rate",           "业务");
        updateCat("unsign_count",          "业务");
        updateCat("sign_penalty",          "业务");
        updateCat("intercept_rate",        "业务");
        updateCat("three_parts_count",     "客服");
        updateCat("three_parts_compensation","客服");
        updateCat("private_compensation",  "客服");
        updateCat("work_order_fine",       "客服");
        updateCat("delivery_stay_rate",    "操作");
        updateCat("return_stay",           "操作");
        updateCat("delivery_schedule_score","操作");
        updateCat("outbound_timely_rate1", "操作");
        updateCat("outbound_timely_rate2", "操作");
        updateCat("operation_complaint",   "操作");
        updateCat("outbound_misppm",       "操作");
        updateCat("auto_throughput",      "操作");
        updateCat("fake_sign",             "小件员取派签质量");
        updateCat("miss_rate",             "小件员取派签质量");
        updateCat("inbound_complaint_ppm", "小件员取派签质量");
        updateCat("info_index_score",      "小件员取派签质量");
        updateCat("scatter_achievement_rate","小件员取派签质量");
        updateCat("first_pickup_rate",     "小件员取派签质量");
        updateCat("customer_voice",        "小件员取派签质量");
        updateCat("on_demand",             "小件员取派签质量");
        updateCat("call_rate",             "小件员取派签质量");
        updateCat("sms_rate",              "小件员取派签质量");
        updateCat("headquarter_fine",      "运营");
        updateCat("kpi_score",             "运营");

        auto updateName = [&](const QString& key, const QString& name) {
            QSqlQuery uq(m_db);
            uq.prepare("UPDATE column_defs SET display_name = ? WHERE key = ?");
            uq.addBindValue(name); uq.addBindValue(key); uq.exec();
        };
        updateName("outbound",              "出港");
        updateName("inbound",               "进港");
        updateName("three_parts_compensation","三件总赔款线上");
        updateName("outbound_timely_rate1", "出仓及时率1阶");
        updateName("outbound_timely_rate2", "出仓及时率2阶");
        updateName("outbound_misppm",       "出港错分");
        updateName("on_demand",             "按需派送");
    }

    // ---- daily_values ----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS daily_values (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            entity_id INTEGER NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
            report_date TEXT NOT NULL,
            column_id INTEGER NOT NULL REFERENCES column_defs(id) ON DELETE CASCADE,
            value REAL DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now','localtime')),
            updated_at TEXT DEFAULT (datetime('now','localtime')),
            UNIQUE(entity_id, report_date, column_id)
        )
    )");
    q.exec("CREATE INDEX IF NOT EXISTS idx_daily_entity_date ON daily_values(entity_id, report_date)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_daily_column ON daily_values(column_id)");

    return true;
}
