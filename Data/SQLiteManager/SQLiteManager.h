#pragma once

#include <QString>
#include <QSqlDatabase>

#include "Model/MarketInput/MarketInput.h"
#include "Model/MarketResult/MarketResult.h"
#include "Model/SettlementResult/SettlementResult.h"

namespace pms {

class SQLiteManager {
public:
    explicit SQLiteManager(const QString& databasePath);
    ~SQLiteManager();

    bool open(QString* errorMessage = nullptr);
    void close();
    bool isOpen() const;

    bool initializeSchema(QString* errorMessage = nullptr);

    bool saveMarketInput(const MarketInput& input, QString* errorMessage = nullptr);
    bool saveMarketResult(const MarketResult& result,
                          const SettlementResult& settlement,
                          QString* errorMessage = nullptr);

    QString databasePath() const;

private:
    bool executeStatement(const QString& sql, QString* errorMessage);

    QString databasePath_;
    QSqlDatabase database_;
};

} // namespace pms
