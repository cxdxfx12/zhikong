#include "FormatUtils.h"
#include <QRegularExpression>

QString FormatUtils::formatValue(double value, const ColumnDef& col) {
    if (col.dataType == "INTEGER") {
        return QString::number(qRound64(value));
    }
    if (col.dataType == "REAL") {
        return QString::number(value, 'f', 2);
    }
    return QString::number(value);
}

QString FormatUtils::formatValue(double value, const QString& dataType, int decimals) {
    if (dataType == "INTEGER") {
        return QString::number(qRound64(value));
    }
    return QString::number(value, 'f', decimals);
}

QString FormatUtils::dateToString(const QDate& date) {
    return date.toString("yyyy-MM-dd");
}

QDate FormatUtils::stringToDate(const QString& str) {
    return QDate::fromString(str.trimmed(), "yyyy-MM-dd");
}

double FormatUtils::parseDouble(const QString& str) {
    QString s = str.trimmed();
    // Remove % suffix if present
    if (s.endsWith('%')) s.chop(1);
    // Remove commas
    s.remove(',');
    bool ok = false;
    double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

int FormatUtils::parseInt(const QString& str) {
    QString s = str.trimmed();
    s.remove(',');
    bool ok = false;
    int v = s.toInt(&ok);
    return ok ? v : 0;
}

QDate FormatUtils::parseDate(const QString& str) {
    QString s = str.trimmed();

    // Try various formats
    QDate d = QDate::fromString(s, "yyyy-M-d");
    if (d.isValid()) return d;

    d = QDate::fromString(s, "yyyy-MM-dd");
    if (d.isValid()) return d;

    d = QDate::fromString(s, "yyyy/M/d");
    if (d.isValid()) return d;

    d = QDate::fromString(s, "M/d/yyyy");
    if (d.isValid()) return d;

    // Excel serial date — no compensation needed (QXlsx provides raw cell value)
    bool ok = false;
    int serial = s.toInt(&ok);
    if (ok && serial > 0 && serial < 100000) {
        return QDate(1899, 12, 30).addDays(serial);
    }

    return QDate();
}

double FormatUtils::variantToDouble(const QVariant& v) {
    if (v.isNull()) return 0.0;
    // Direct numeric types — no string round-trip
    if (v.userType() == QMetaType::Double || v.userType() == QMetaType::Float)
        return v.toDouble();
    if (v.userType() == QMetaType::Int || v.userType() == QMetaType::LongLong
        || v.userType() == QMetaType::UInt || v.userType() == QMetaType::ULongLong)
        return v.toDouble();
    // String type — parse
    if (v.userType() == QMetaType::QString)
        return parseDouble(v.toString());
    // Bool
    if (v.userType() == QMetaType::Bool)
        return v.toBool() ? 1.0 : 0.0;
    // Fallback: try string conversion
    return parseDouble(v.toString());
}

QDate FormatUtils::variantToDate(const QVariant& v) {
    if (v.isNull()) return QDate();
    // Direct date types
    if (v.userType() == QMetaType::QDate)
        return v.toDate();
    if (v.userType() == QMetaType::QDateTime)
        return v.toDateTime().date();
    // Numeric — Excel serial date
    if (v.userType() == QMetaType::Double || v.userType() == QMetaType::Int
        || v.userType() == QMetaType::LongLong) {
        int serial = v.toInt();
        if (serial > 0 && serial < 100000)
            return QDate(1899, 12, 30).addDays(serial);
    }
    // String — try parsing
    return parseDate(v.toString());
}
