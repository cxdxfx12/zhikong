# 快递日报数据管理系统 (ExpressDailyManager)

## 技术栈
- **语言**: C++17
- **UI**: Qt 6.11 (Widgets + Charts + Sql)
- **数据库**: SQLite 3 (QSQLITE 驱动, DELETE journal mode, 单文件)
- **Excel**: QXlsx (thirdparty/QXlsx, header-only 静态库)
- **构建**: CMake 3.20+, MinGW-w64 13.1.0

## 编译
```bash
cmake -B build -DCMAKE_PREFIX_PATH="/e/Qt/6.11.1/mingw_64" -G "MinGW Makefiles"
cmake --build build -j8
```

## 项目结构
```
src/
├── main.cpp                    # 入口，加载QSS，初始化DB
├── db/
│   ├── Database.h/cpp          # 单例，建表+初始数据+迁移
│   ├── EntityDao.h/cpp         # 公司/承包区 CRUD + 树形查询
│   ├── ColumnDao.h/cpp         # 列定义 CRUD
│   └── DailyDao.h/cpp          # 每日数据 CRUD，EAV转行查询
├── model/
│   ├── Entity.h                # 实体(公司/承包区) + 层级关系
│   ├── ColumnDef.h             # 列定义(含category/aggregateType)
│   ├── DailyValue.h            # 每日值 + DisplayRow
│   └── DisplayDataModel.h/cpp  # QAbstractTableModel，分类筛选+统计行
├── service/
│   ├── AggregationService.h/cpp # 承包区→公司自动汇总(SUM/AVG/NONE)
│   ├── ImportService.h/cpp     # Excel解析+缓存(parsePreview)→直接导入
│   ├── ExportService.h/cpp     # CSV/Excel 导出
│   └── ChartService.h/cpp      # 图表数据准备
├── ui/
│   ├── MainWindow.h/cpp        # 主窗口：菜单栏+工具栏+侧边栏+表格
│   ├── DataEntryDialog.h/cpp   # 动态表单(按列定义生成)
│   ├── EntityManageDialog.h/cpp # 实体树形管理
│   ├── ColumnManageDialog.h/cpp # 列定义CRUD+排序
│   ├── ImportDialog.h/cpp      # Excel导入预览+映射+确认
│   └── ChartDialog.h/cpp       # 7种图表+PowerBI风格+数据表
└── utils/
    └── FormatUtils.h/cpp       # 日期/数值解析(含Excel序列号转换)
```

## 代码规范
- 所有 SQL 使用 QString::arg() 字符串拼接，**不用 prepare/bind**
- DAO 层使用 `Database::instance().db()` 获取唯一连接
- 修改数据库表结构后，在 Database.cpp 的 migration 段加 ALTER TABLE
- 新增列定义在 Database.cpp 的 insertCol 调用中添加
- QSS 选择器必须限定到具体 widget (如 `QTableView#dataTable`)，禁止全局样式

## 关键业务逻辑
- **实体层级**: entities.parent_id → NULL=公司，非NULL=承包区(指向上级公司)
- **数据汇总**: column_defs.aggregate_type 决定公司数据如何从承包区计算 (SUM/AVG/NONE)
- **导入流程**: parsePreview 读Excel并缓存全部数据 → executeImportDirect 用缓存直接写库
- **表头匹配**: 清洗掉 `[,，、。\s()（）ppmPM]` 后先精准匹配，再最长子串匹配
- **Excel日期**: 序列号 → `QDate(1899,12,30).addDays(serial)`
