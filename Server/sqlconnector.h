#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

class SqlConnector
{
public:
    SqlConnector();
    ~SqlConnector();
    bool openConnection();
    void closeConnection();
    QSqlQuery executeQuery(const QString &queryString);

private:
    QSqlDatabase db;
};