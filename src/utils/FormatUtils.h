#pragma once
#include <QString>
#include <QDate>
#include <QVariant>
#include "model/ColumnDef.h"

class FormatUtils {
public:
    // Format a value based on column definition's data type
    static QString formatValue(double value, const ColumnDef& col);
    static QString formatValue(double value, const QString& dataType, int decimals = 2);

    // Date utilities
    static QString dateToString(const QDate& date);
    static QDate stringToDate(const QString& str);

    // Parse helpers for Excel import
    static double parseDouble(const QString& str);
    static double variantToDouble(const QVariant& v);  // direct QVariant → double
    static int parseInt(const QString& str);
    static QDate parseDate(const QString& str);
    static QDate variantToDate(const QVariant& v);      // direct QVariant → QDate
};
