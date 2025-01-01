#pragma once

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <iostream>
#include "Games.h"
#include "Player.h"
#include <unordered_map>
#include "cJSON/cJSON.h"
#include <thread>
#include <QVector>
#include "sqlconnector.h"
#include <mutex>

using namespace std;

typedef shared_ptr<Game> onlineGame;
typedef shared_ptr<Player> player;

struct Account
{
    QString ID;
    QString PassW;
    int elo;
    bool login = false;
    Account() {};
    Account(QString id, QString pw, int e)
    {
        ID = id;
        PassW = pw;
        elo = e;
    }
};
enum class StatusCode
{
    // Successful
    OK = 200,
    CREATED = 201,

    // Client Errors
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,

    CONFLICT = 409,
    SERVER_ERROR = 500,
    SERVICE_UNAVAIABLE = 503
};

enum class EloTier
{
    BEGINNER = 0,     // 0-800
    INTERMEDIATE = 1, // 801-1600
    ADVANCED = 2,     // 1601-2000
    EXPERT = 3,       // 2001-2400
    MASTER = 4        // 2400+
};

class Server
{
public:
    Server(int PORT, bool BroadcastPublically = false);
    bool ListenForNewConnection();

private:
    bool SendString(int ID, string &_string);
    bool GetString(int ID, string &_string);
    void sendMessToClients(string Message);
    bool Processinfo(int ID);
    bool CreateGameList(string &_string);
    bool sendSystemInfo(int ID, string InfoType, string addKey = "", string addValue = "");
    bool systemSend(int ID, string InfoType, string addKey = "", string addValue = "");
    bool sendResponse(int ID, string type, StatusCode status, string addKeys = "", string addValues = "");
    bool sendGameList(int ID);
    void deleteGame(int ID);
    void ClientHandlerThread(int arg);
    void GetAllAccounts();
    bool Signup(QString username, QString password, int elo);
    int NameToElo(string);
    int CalculateElo(int playerA, int playerB, float result);
    void UpdateElo(string, int);
    QString GetTopRanking();
    EloTier getEloTier(int elo) const;
    int getPlayerElo(const QString &username);

private:
    unordered_map<int, int> Connections;
    int TotalConnections = 0;
    int allID = 0;
    struct sockaddr_in servaddr;
    unsigned int addrlen = sizeof(servaddr);
    int sListen;
    int GameNum = 0;
    unordered_map<int, onlineGame> GameList;
    unordered_map<int, player> PlayerList;
    unordered_map<int, QString> OnlineUserList;
    std::thread threadList[512];
    QVector<Account> accList;
    QString accsFilePath;
    std::mutex mutexLock;
    bool sendMatchNotification(int playerID, const std::string &side, const std::string &opponent, int opponentElo);
};

static Server *serverptr;