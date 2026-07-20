#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QIcon>
#include "db/Database.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("快递日报数据管理系统");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("杭州喵喵至家网络有限公司");
    app.setWindowIcon(QIcon(":/icon.png"));

    // Load global stylesheet
    QFile styleFile(":/styles.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // Open / create database in exe directory
    auto& db = Database::instance();
    if (!db.open("express_daily.db")) {
        QMessageBox::critical(nullptr, "数据库错误",
            "无法打开数据库:\n" + db.lastError() +
            "\n\n请确认程序所在目录有写入权限。");
        return 1;
    }

    // Initialize schema
    if (!db.initialize()) {
        QMessageBox::critical(nullptr, "数据库错误", "数据库初始化失败。");
        return 1;
    }

    // Launch main window
    MainWindow window;
    window.show();

    return app.exec();
}
