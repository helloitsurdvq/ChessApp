#include "sqlconnector.h"

SqlConnector::SqlConnector()
{
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName("Chess_db");
    db.setUserName("root");
    db.setPassword("password");
}

SqlConnector::~SqlConnector()
{
    if (db.isOpen())
    {
        db.close();
    }
}

bool SqlConnector::openConnection()
{
    return db.open();
}

void SqlConnector::closeConnection()
{
    db.close();
}

QSqlQuery SqlConnector::executeQuery(const QString &queryString)
{
    QSqlQuery query;
    query.exec(queryString);
    return query;
}