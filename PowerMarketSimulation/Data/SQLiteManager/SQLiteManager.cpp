#include "Data/SQLiteManager/SQLiteManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace pms {

namespace {

const char* kConnectionName = "pms_sqlite";

QString qs(const std::string& value) {
    return QString::fromStdString(value);
}

} // namespace

SQLiteManager::SQLiteManager(const QString& databasePath)
    : databasePath_(databasePath) {}

SQLiteManager::~SQLiteManager() {
    close();
}

bool SQLiteManager::open(QString* errorMessage) {
    if (database_.isOpen()) {
        return true;
    }

    if (QSqlDatabase::contains(kConnectionName)) {
        QSqlDatabase::removeDatabase(kConnectionName);
    }

    database_ = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    database_.setDatabaseName(databasePath_);
    if (!database_.open()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }
    return true;
}

void SQLiteManager::close() {
    if (!database_.isValid()) {
        return;
    }
    const QString connectionName = database_.connectionName();
    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

bool SQLiteManager::isOpen() const {
    return database_.isOpen();
}

bool SQLiteManager::executeStatement(const QString& sql, QString* errorMessage) {
    QSqlQuery query(database_);
    if (!query.exec(sql)) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool SQLiteManager::initializeSchema(QString* errorMessage) {
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "数据库未打开";
        }
        return false;
    }

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS generators ("
            "id TEXT PRIMARY KEY, p_min REAL, p_max REAL, mode TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS consumers ("
            "id TEXT PRIMARY KEY, fixed_demand REAL, mode TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS bid_segments ("
            "owner_id TEXT, time_slot INTEGER, segment_no INTEGER, "
            "quantity REAL, price REAL, "
            "PRIMARY KEY (owner_id, time_slot, segment_no))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS market_results ("
            "time_slot INTEGER PRIMARY KEY, mode TEXT, clearing_price REAL, "
            "clearing_volume REAL, feasible INTEGER, shortage REAL, message TEXT)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS generator_results ("
            "time_slot INTEGER, generator_id TEXT, output REAL, marginal_cost REAL, "
            "PRIMARY KEY (time_slot, generator_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS consumer_results ("
            "time_slot INTEGER, consumer_id TEXT, cleared_demand REAL, "
            "PRIMARY KEY (time_slot, consumer_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settlements ("
            "time_slot INTEGER, generator_id TEXT, output REAL, "
            "revenue REAL, cost REAL, profit REAL, "
            "PRIMARY KEY (time_slot, generator_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS consumer_payments ("
            "time_slot INTEGER, consumer_id TEXT, cleared_demand REAL, payment REAL, "
            "PRIMARY KEY (time_slot, consumer_id))")
    };

    for (const auto& statement : statements) {
        if (!executeStatement(statement, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool SQLiteManager::saveMarketInput(const MarketInput& input, QString* errorMessage) {
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "数据库未打开";
        }
        return false;
    }

    database_.transaction();

    for (const auto& generator : input.generators()) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO generators(id, p_min, p_max, mode) "
            "VALUES (?, ?, ?, ?)"));
        query.addBindValue(qs(generator.id()));
        query.addBindValue(generator.pMinMw());
        query.addBindValue(generator.pMaxMw());
        query.addBindValue(input.mode() == MarketMode::Quadratic
                               ? QStringLiteral("quadratic")
                               : QStringLiteral("piecewise"));
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }

        for (const auto& segment : generator.bidSheet().segments()) {
            QSqlQuery segmentQuery(database_);
            segmentQuery.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO bid_segments("
                "owner_id, time_slot, segment_no, quantity, price) "
                "VALUES (?, ?, ?, ?, ?)"));
            segmentQuery.addBindValue(qs(generator.id()));
            segmentQuery.addBindValue(input.timeSlot());
            segmentQuery.addBindValue(segment.segmentNo());
            segmentQuery.addBindValue(segment.quantityMw());
            segmentQuery.addBindValue(segment.priceYuanPerMwh());
            if (!segmentQuery.exec()) {
                if (errorMessage) {
                    *errorMessage = segmentQuery.lastError().text();
                }
                database_.rollback();
                return false;
            }
        }
    }

    for (const auto& consumer : input.consumers()) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO consumers(id, fixed_demand, mode) "
            "VALUES (?, ?, ?)"));
        query.addBindValue(qs(consumer.id()));
        query.addBindValue(consumer.fixedDemandMw());
        query.addBindValue(input.mode() == MarketMode::Quadratic
                               ? QStringLiteral("quadratic")
                               : QStringLiteral("piecewise"));
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    database_.commit();
    return true;
}

bool SQLiteManager::saveMarketResult(const MarketResult& result,
                                     const SettlementResult& settlement,
                                     QString* errorMessage) {
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "数据库未打开";
        }
        return false;
    }

    database_.transaction();

    const int timeSlot = result.timeSlot();
    {
        QSqlQuery cleanup(database_);
        cleanup.prepare(QStringLiteral("DELETE FROM market_results WHERE time_slot = ?"));
        cleanup.addBindValue(timeSlot);
        if (!cleanup.exec()) {
            if (errorMessage) {
                *errorMessage = cleanup.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    {
        QSqlQuery cleanup(database_);
        cleanup.prepare(QStringLiteral("DELETE FROM generator_results WHERE time_slot = ?"));
        cleanup.addBindValue(timeSlot);
        cleanup.exec();
    }
    {
        QSqlQuery cleanup(database_);
        cleanup.prepare(QStringLiteral("DELETE FROM consumer_results WHERE time_slot = ?"));
        cleanup.addBindValue(timeSlot);
        cleanup.exec();
    }
    {
        QSqlQuery cleanup(database_);
        cleanup.prepare(QStringLiteral("DELETE FROM settlements WHERE time_slot = ?"));
        cleanup.addBindValue(timeSlot);
        cleanup.exec();
    }
    {
        QSqlQuery cleanup(database_);
        cleanup.prepare(QStringLiteral("DELETE FROM consumer_payments WHERE time_slot = ?"));
        cleanup.addBindValue(timeSlot);
        cleanup.exec();
    }

    {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT INTO market_results("
            "time_slot, mode, clearing_price, clearing_volume, feasible, shortage, message) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"));
        query.addBindValue(timeSlot);
        query.addBindValue(QStringLiteral("piecewise"));
        query.addBindValue(result.clearingPriceYuanPerMwh());
        query.addBindValue(result.clearingVolumeMw());
        query.addBindValue(result.feasible() ? 1 : 0);
        query.addBindValue(result.shortageMw());
        query.addBindValue(qs(result.message()));
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    const auto& generatorResults = result.generatorResults();
    for (size_t i = 0; i < generatorResults.size(); ++i) {
        const auto& item = generatorResults[i];
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT INTO generator_results("
            "time_slot, generator_id, output, marginal_cost) VALUES (?, ?, ?, ?)"));
        query.addBindValue(timeSlot);
        query.addBindValue(qs(item.generatorId));
        query.addBindValue(item.outputMw);
        query.addBindValue(item.marginalCostYuanPerMwh);
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    const auto& consumerResults = result.consumerResults();
    for (const auto& item : consumerResults) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT INTO consumer_results("
            "time_slot, consumer_id, cleared_demand) VALUES (?, ?, ?)"));
        query.addBindValue(timeSlot);
        query.addBindValue(qs(item.consumerId));
        query.addBindValue(item.clearedDemandMw);
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    for (const auto& item : settlement.generatorSettlements()) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT INTO settlements("
            "time_slot, generator_id, output, revenue, cost, profit) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        query.addBindValue(timeSlot);
        query.addBindValue(qs(item.generatorId));
        query.addBindValue(item.outputMw);
        query.addBindValue(item.revenueYuan);
        query.addBindValue(item.costYuan);
        query.addBindValue(item.profitYuan);
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    for (const auto& item : settlement.consumerSettlements()) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral(
            "INSERT INTO consumer_payments("
            "time_slot, consumer_id, cleared_demand, payment) "
            "VALUES (?, ?, ?, ?)"));
        query.addBindValue(timeSlot);
        query.addBindValue(qs(item.consumerId));
        query.addBindValue(item.clearedDemandMw);
        query.addBindValue(item.paymentYuan);
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            database_.rollback();
            return false;
        }
    }

    database_.commit();
    return true;
}

QString SQLiteManager::databasePath() const {
    return databasePath_;
}

} // namespace pms
