﻿#include "gamelobby.h"
#include "chatroom.h"
#include "chessroom.h"
#include <QMessageBox>
#include <QDebug>
#include <QInputDialog>
#include <mutex>
#include "game.h"
#include <iostream>
#include <QTimer>

#define MAXSIZE 512

#pragma execution_character_set("utf-8")
static gameLobby *clientptr;
bool gameLobby::is_opened = false;
extern game *Game;

EloTier getEloTier(int elo)
{
    if (elo <= 800)
        return EloTier::BEGINNER;
    if (elo <= 1600)
        return EloTier::INTERMEDIATE;
    if (elo <= 2000)
        return EloTier::ADVANCED;
    if (elo <= 2400)
        return EloTier::EXPERT;
    return EloTier::MASTER;
}

gameLobby::gameLobby(QWidget *parent) : QGraphicsView(parent)
{
    gameLobby::is_opened = true;
    OnlineScene = new QGraphicsScene();
    OnlineScene->setSceneRect(0, 0, 1400, 940);
    chRoom = nullptr;
    setFixedSize(1400, 850);
    setScene(OnlineScene);
    clientptr = this;
    connect(this, SIGNAL(updateRooms(cJSON *)), this, SLOT(createRoomsList(cJSON *)));
    connect(this, SIGNAL(socketClosed()), this, SLOT(ServerClose()));
    connect(this, SIGNAL(socketClosedfailed()), this, SLOT(SocketBugs()));
    connect(this, SIGNAL(TimeoutJoin()), this, SLOT(JoinTimeOut()));
    connect(this, SIGNAL(ListFull()), this, SLOT(List_is_full()));
    connect(this, SIGNAL(Full()), this, SLOT(This_Game_isFull()));
    connect(this, SIGNAL(someoneLeave()), this, SLOT(Leave()));
    connect(this, SIGNAL(ShowGame()), Game, SLOT(SHOW()));
    connect(this, SIGNAL(PlayBlack(QString, QString)), Game, SLOT(playAsBlackOnline(QString, QString)));
    connect(this, SIGNAL(PlayWhite(QString, QString)), Game, SLOT(playAsWhiteOnline(QString, QString)));
    connect(this, SIGNAL(PlayBlackAgain()), Game, SLOT(playAsBlackOnline()));
    connect(this, SIGNAL(PlayWhiteAgain()), Game, SLOT(playAsWhiteOnline()));
    connect(this, SIGNAL(moveTo(onlineMove *)), Game, SLOT(receiveMove(onlineMove *)));
    connect(this, &gameLobby::inviteReceived, this, &gameLobby::handleInvite);
    connect(this, SIGNAL(askDraw()), Game, SLOT(askDraw()));
    connect(this, SIGNAL(Draw()), Game, SLOT(Draw()));
    connect(this, &gameLobby::updateOnlineUserList, this, &gameLobby::handleOnlineUserList, Qt::QueuedConnection);
    connect(this, &gameLobby::inviteResponse, this, &gameLobby::onInviteResponse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QDialog dialog;
    dialog.setWindowTitle("Connect");
    QLineEdit text1LineEdit;
    QLineEdit text2LineEdit;
    QLineEdit text3LineEdit;
    text1LineEdit.setText("127.0.0.1");
    QFormLayout layout(&dialog);
    layout.addRow("Server IP address:", &text1LineEdit);
    layout.addRow("User name:", &text2LineEdit);
    text3LineEdit.setEchoMode(QLineEdit::Password);
    layout.addRow("Password:", &text3LineEdit);
    QPushButton okButton("OK");
    layout.addRow(&okButton);
    QObject::connect(&okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString in_ip = text1LineEdit.text();
        QString in_id = text2LineEdit.text();
        QString in_pw = text3LineEdit.text();
        if (in_ip.isEmpty() || in_id.isEmpty() || in_pw.isEmpty())
        {
            connectError = true;
            return;
        }
        connectToServer(in_ip);
        if (connectError)
            return;
        if (!requestLogIn(in_id, in_pw))
        {
            connectError = true;
            return;
        }
        id_id = in_id;
    }
    else
    {
        connectError = true;
        return;
    }
    chRoom = new Chatroom(this);
    t1 = std::thread(ClientThread);
    titleText = new QGraphicsTextItem("Online Chess Lobby");
    QFont titleFont("arial", 50);
    titleText->setFont(titleFont);
    int xPos = width() / 2 - titleText->boundingRect().width() / 2;
    int yPos = 100;
    titleText->setPos(xPos, yPos);
    OnlineScene->addItem(titleText);

    QGraphicsTextItem *infoTitle = new QGraphicsTextItem("Logged in as: " + id_id + "\nElo: " + QString::number(id_elo));
    QFont infoFont("arial", 20);
    infoTitle->setFont(infoFont);
    infoTitle->setPos(50, 100);
    OnlineScene->addItem(infoTitle);

    playButton = new button("Return to Menu");
    int pxPos = 150;
    int pyPos = 750;
    playButton->setPos(pxPos, pyPos);
    connect(playButton, SIGNAL(clicked()), this, SLOT(ReturnToMenu()));
    playButton->hide();
    OnlineScene->addItem(playButton);
    listView = new QListView();
    onlineUserList = new QStandardItemModel();
    listView->setModel(onlineUserList);
    QGraphicsProxyWidget *proxyWidget = new QGraphicsProxyWidget();
    proxyWidget->setWidget(listView);
    proxyWidget->resize(200, 300);
    proxyWidget->setPos(50, 250);
    OnlineScene->addItem(proxyWidget);
    getOnUserBtn = new button("Refresh: Online Users");
    getOnUserBtn->setPos(50, 200);
    connect(getOnUserBtn, SIGNAL(clicked()), this, SLOT(GetOnlineUser()));
    OnlineScene->addItem(getOnUserBtn);

    createRoomBtn = new button("Create a Game Room");
    createRoomBtn->setPos(400, 750);
    connect(createRoomBtn, SIGNAL(clicked()), this, SLOT(CreateAGameRoom()));
    OnlineScene->addItem(createRoomBtn);

    matchRandomBtn = new button("Match by Random Player");
    matchRandomBtn->setPos(650, 750);
    connect(matchRandomBtn, SIGNAL(clicked()), this, SLOT(MatchRandomPlayer()));
    OnlineScene->addItem(matchRandomBtn);

    eloRandomBtn = new button("Match Player by Elo");
    eloRandomBtn->setPos(900, 750);
    connect(eloRandomBtn, SIGNAL(clicked()), this, SLOT(MatchEloPlayer()));
    OnlineScene->addItem(eloRandomBtn);

    OnlineScene->addItem(createRankingWidget());

    hostWindow();
    LobbySUI();
}

gameLobby::~gameLobby()
{
    gameLobby::is_opened = false;

    if (matchingDialog)
    {
        matchingDialog->hide();
        matchingDialog->deleteLater();
        matchingDialog = nullptr;
    }
    if (t1.joinable())
    {
        t1.join();
    }
    if (t2.joinable())
    {
        t2.join();
    }
    if (matchingDialog)
    {
        matchingDialog->deleteLater();
        matchingDialog = nullptr;
    }
}

void gameLobby::getTopRanking()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "GET_TOP_RANKING");
    char *JsonToSend = cJSON_Print(Mesg);
    qDebug() << strlen(JsonToSend);
    cJSON_Delete(Mesg);
    QStringList rankingList;
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) < 0)
    {
        QMessageBox::critical(NULL, "Error", "Cannot send GetTopRanking message!");
        return;
    }
    QThread::msleep(1000);
}

QGraphicsProxyWidget *gameLobby::createRankingWidget()
{
    getTopRanking();

    QTableWidget *tableWidget = new QTableWidget(0, 2);
    QStringList headers;
    headers << "User ID" << "ELO";
    tableWidget->setHorizontalHeaderLabels(headers);
    tableWidget->setRowCount(rankingList.size());
    for (int row = 0; row < rankingList.size(); ++row)
    {
        QStringList columns = rankingList.at(row).split("#");
        for (int col = 0; col < columns.size(); ++col)
        {
            tableWidget->setItem(row, col, new QTableWidgetItem(columns.at(col)));
        }
    }

    QLabel *titleLabel = new QLabel("Top 50");
    QFont titleFont("arial", 20);
    titleLabel->setFont(titleFont);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(titleLabel);
    layout->addWidget(tableWidget);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    QGraphicsProxyWidget *proxyWidget = new QGraphicsProxyWidget;
    proxyWidget->setWidget(widget);
    proxyWidget->setPos(1100, 20);
    return proxyWidget;
}

bool gameLobby::requestLogIn(QString id, QString pw)
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "LOGIN");
    cJSON_AddStringToObject(Mesg, "UN", id.toStdString().c_str());
    cJSON_AddStringToObject(Mesg, "PW", pw.toStdString().c_str());
    char *JsonToSend = cJSON_Print(Mesg); // make the json as char*
    cJSON_Delete(Mesg);
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) < 0)
        return false;
    // receive response
    char buffer[MAXSIZE] = {0};
    for (int i = 0; i < 50; i++)
    {
        if (recv(Connection, buffer, sizeof(buffer), MSG_DONTWAIT) > 0)
        {
            qDebug() << buffer;
            cJSON *json, *json_type, *json_status, *json_elo;
            json = cJSON_Parse(buffer);
            json_type = cJSON_GetObjectItem(json, "Type");
            json_status = cJSON_GetObjectItem(json, "Status");

            std::string type = "";
            std::string status;
            if (json_type != NULL && json_status != NULL)
            {
                type = json_type->valuestring;
                status = json_status->valuestring;
            }
            if (type == "LOGIN_RES" && status == statusToString(StatusCode::OK))
            {
                json_elo = cJSON_GetObjectItem(json, "elo");
                std::string tmp = json_elo->valuestring;
                id_elo = std::stoi(tmp);
                cJSON_Delete(json);
                return true;
            }
            cJSON_Delete(json);
            if (status == statusToString(StatusCode::BAD_REQUEST))
                QMessageBox::critical(NULL, "Error", "Failed to Log In!\nIncorrect ID or password");
            else
                QMessageBox::critical(NULL, "Error", "Failed to Log In!\nAccount currently in use!");
            return false;
        }
        QThread::msleep(100);
    }

    QMessageBox::critical(NULL, "Error", "Log In timeout!");
    return false;
}

void gameLobby::connectToServer(QString serverIP)
{
    Connection = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1111);
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.toUtf8().constData()); // INADDR_ANY;

    if (::connect(Connection, (struct sockaddr *)&serverAddress, sizeof(serverAddress))) // If we have trouble in connect to the addr
    {
        QMessageBox::critical(NULL, "Error", "Failed to Connect");
        connectError = true;
        return;
    }
}

bool gameLobby::CloseConnection()
{
    ::close(Connection);
    return true;
}

bool gameLobby::sendMove(int FromX, int FromY, int ToX, int ToY)
{
    cJSON *Move;
    Move = cJSON_CreateObject();
    cJSON_AddStringToObject(Move, "Type", "MOVE");
    cJSON_AddNumberToObject(Move, "FromX", FromX);
    cJSON_AddNumberToObject(Move, "FromY", FromY);
    cJSON_AddNumberToObject(Move, "ToX", ToX);
    cJSON_AddNumberToObject(Move, "ToY", ToY);
    cJSON_AddNumberToObject(Move, "Castling", -1);
    char *JsonToSend = cJSON_Print(Move);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return false;
    return true;
}

bool gameLobby::sendMove(int FromX, int FromY, int ToX, int ToY, int castling)
{
    cJSON *Move;
    Move = cJSON_CreateObject();
    cJSON_AddStringToObject(Move, "Type", "MOVE");
    cJSON_AddNumberToObject(Move, "FromX", FromX);
    cJSON_AddNumberToObject(Move, "FromY", FromY);
    cJSON_AddNumberToObject(Move, "ToX", ToX);
    cJSON_AddNumberToObject(Move, "ToY", ToY);
    cJSON_AddNumberToObject(Move, "Castling", castling);
    char *JsonToSend = cJSON_Print(Move); // make the json as char*
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return false;
    return true;
}

void gameLobby::Signal_socketClosed()
{
    emit socketClosed();
}

void gameLobby::Signal_socketClosedfailed()
{
    emit socketClosedfailed();
}

void gameLobby::Signal_TimeoutJoin()
{
    emit TimeoutJoin();
}

void gameLobby::I_wannaPlayAgain()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "PLAY_AGAIN");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
}

void gameLobby::ReturnToMenu()
{
    if (!inRooms && !waiting && !host)
    {
        gameLobby::is_opened = false;
        Game->mainmenu();
        exitLobby();
        Game->show();
        this->close();
    }
}

void gameLobby::CancelHost()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "CANCEL_HOST");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL))
    {
        yourSide = -1;
        inRooms = false;
        waiting = false;
        host = false;
        showRooms();
    }
}

void gameLobby::GetOnlineUser()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "GET_ONLINE_USERS");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) <= 0)
    {
        qDebug() << "Send message to server failed!.";
    }
}

void gameLobby::CreateAGameRoom()
{
    if (!host && !waiting && !inRooms)
    {
        std::string user = id_id.toStdString() + "#" + std::to_string(id_elo);
        if (user.length() > 16)
            user = user.substr(0, 16);
        if (!CreateRoom(user))
        {
        }
    }
}

void gameLobby::ShowChatRoom()
{
    if (chRoom->isVisible())
        chRoom->hide();
    else
        chRoom->show();
}

bool gameLobby::backToLobby()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "BACK_TO_LOBBY");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return false;
    else
    {
        clientptr->host = false;
        clientptr->inRooms = false;
        clientptr->waiting = false;
        clientptr->yourSide = -1;
        showRooms();
        return true;
    }
}

void gameLobby::closeEvent(QCloseEvent *event)
{
    gameLobby::is_opened = false;
    if (inRooms)
        Leave();
    exitLobby();
    chRoom->close();
    if (matchingDialog)
    {
        matchingDialog->hide();
        matchingDialog->deleteLater();
        matchingDialog = nullptr;
    }
    event->accept();
}

bool gameLobby::sendMessage(const std::string &message, const std::string &username)
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "Message");
    cJSON_AddStringToObject(Mesg, "User", username.c_str());
    cJSON_AddStringToObject(Mesg, "Message", message.c_str());
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return false;
    return true;
}

bool gameLobby::CreateRoom(const std::string &user)
{
    host = true;
    waitingForJoin();
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "CREATEROOM");
    cJSON_AddStringToObject(Mesg, "User", user.c_str());
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return false;
    return true;
}

bool gameLobby::GetString()
{
    char buffer[MAXSIZE];
    int RetnCheck = recv(Connection, buffer, sizeof(buffer), NULL);
    if (RetnCheck < 0)
        return false;
    cJSON *json, *json_type;
    QString receivedMessage = QString::fromUtf8(buffer);
    qDebug() << "Received:" << receivedMessage;
    json = cJSON_Parse(buffer);
    json_type = cJSON_GetObjectItem(json, "Type");
    if (json_type == NULL)
    {
        this->chRoom->Showmessage(buffer);
        cJSON_Delete(json);
        return true;
    }
    std::string type = json_type->valuestring;
    if (type == "Message")
    {
        cJSON *Mess;
        Mess = cJSON_GetObjectItem(json, "Message");
        this->chRoom->Showmessage(Mess->valuestring);
        cJSON_Delete(json);
        return true;
    }
    else if (type == "List_of_Rooms")
    {
        emit updateRooms(json);
    }
    else if (type == "MOVE")
    {
        if (inRooms)
        {
            cJSON *FromX, *FromY, *ToX, *ToY, *Castling;
            FromX = cJSON_GetObjectItem(json, "FromX");
            FromY = cJSON_GetObjectItem(json, "FromY");
            ToX = cJSON_GetObjectItem(json, "ToX");
            ToY = cJSON_GetObjectItem(json, "ToY");
            Castling = cJSON_GetObjectItem(json, "Castling");
            onlineMove *Move = new onlineMove(FromX->valueint, FromY->valueint, ToX->valueint, ToY->valueint, Castling->valueint);
            cJSON_Delete(json);
            emit moveTo(Move);
            Move = NULL;
        }
    }
    else if (type == "GET_ONLINE_USERS_RES")
    {
        onlineUserList->clear();
        cJSON *Data = cJSON_GetObjectItem(json, "Data");
        QStringList onlineStr = QString::fromStdString(Data->valuestring).split(",");
        qDebug() << "Online Users:" << onlineStr;

        emit updateOnlineUserList(onlineStr);
        cJSON_Delete(json);
    }
    else if (type == "Result")
    {
        cJSON *json_result;
        json_result = cJSON_GetObjectItem(json, "elo");
        int result = json_result->valueint;
        id_elo += result;
        recent_elo = result;
        cJSON_Delete(json);
    }
    else if (type == "CREATEROOM_RES")
    {
        clientptr->host = true;
    }
    else if (type == "JOIN_ROOM_RES")
    {
        cJSON *Status;
        Status = cJSON_GetObjectItem(json, "Status");
        std::string status = Status->valuestring;
        if (status == statusToString(StatusCode::CONFLICT))
        {
            clientptr->waiting = false;
            emit Full();
        }
        else if (status == statusToString(StatusCode::OK))
        {
            yourSide = 1;
            inRooms = true;
            clientptr->waiting = false;
            cJSON *Name_Info;
            Name_Info = cJSON_GetObjectItem(json, "Name_Info");
            QString nameInfo = Name_Info->valuestring;
            qDebug() << nameInfo;
            emit PlayBlack(nameInfo, id_id + "#" + QString::number(id_elo));
            emit ShowGame();
        }
    }
    else if (type == "SEND_OPPONENT_JOINED")
    {
        yourSide = 0;
        inRooms = true;
        clientptr->waiting = false;
        cJSON *Name_Info;
        Name_Info = cJSON_GetObjectItem(json, "Name_Info");
        QString nameInfo = Name_Info->valuestring;
        emit PlayWhite(id_id + "#" + QString::number(id_elo), nameInfo);
        emit ShowGame();
    }
    else if (type == "INVITE_RECEIVED")
    {
        cJSON *dataJson = cJSON_GetObjectItem(json, "Data");
        if (!dataJson)
        {
            cJSON_Delete(json);
            return true;
        }
        QString data = QString::fromStdString(dataJson->valuestring);
        QStringList parts = data.split("#");
        if (parts.size() != 2)
        {
            cJSON_Delete(json);
            return true;
        }

        QString fromUser = parts[0];
        int gameID = parts[1].toInt();
        emit inviteReceived(fromUser, gameID);
        cJSON_Delete(json);
    }
    else if (type == "INVITE_RES")
    {
        cJSON *Status;
        Status = cJSON_GetObjectItem(json, "Status");
        std::string status = Status->valuestring;
        if (!Status)
        {
            cJSON_Delete(json);
            return true;
        }

        if (status == statusToString(StatusCode::OK))
        {
            qDebug() << "Invite sent successfully.";
            emit inviteResponse("Invite sent successfully.", true);
        }
        else if (status == statusToString(StatusCode::CONFLICT))
        {
            qDebug() << "Recipient is currently busy.";
            emit inviteResponse("Recipient is currently busy.", false);
        }
        else if (status == statusToString(StatusCode::NOT_FOUND))
        {
            qDebug() << "Recipient not found.";
            emit inviteResponse("Recipient not found.", false);
        }
        else if (status == statusToString(StatusCode::FORBIDDEN))
        {
            qDebug() << "You must be hosting a room to send an invite.";
            emit inviteResponse("You must be hosting a room to send an invite.", false);
        }
        else
        {
            qDebug() << "Unexpected response status";
            emit inviteResponse("Unexpected response from server.", false);
        }

        cJSON_Delete(json);
    }
    else if (type == "SEND_OPPONENT_LEAVED")
    {
        inRooms = false;
        waiting = false;
        host = false;
        yourSide = -1;

        QTimer::singleShot(0, this, [this]()
                           { emit someoneLeave(); });

        cJSON_Delete(json);
    }
    else if (type == "LOST_CONNECTION")
    {
        inRooms = false;
        waiting = false;
        host = false;
        yourSide = -1;

        if (matchingDialog)
        {
            matchingDialog->hide();
            matchingDialog->deleteLater();
            matchingDialog = nullptr;
        }

        QTimer::singleShot(0, this, [this]()
                           { emit someoneLeave(); });

        cJSON_Delete(json);
    }
    if (type == "SEND_PLAY_AGAIN")
    {
        if (yourSide == 1)
            emit PlayBlackAgain();
        else
            emit PlayWhiteAgain();
        cJSON_Delete(json);
    }
    else if (type == "ASK_DRAW")
    {
        std::cout << type;
        emit askDraw();
        cJSON_Delete(json);
    }
    else if (type == "DRAW")
    {
        cJSON *json_result;
        json_result = cJSON_GetObjectItem(json, "Confirm");
        int result = json_result->valueint;
        if (result)
            emit Draw();
        cJSON_Delete(json);
    }
    else if (type == "GET_TOP_RANKING_RES")
    {
        cJSON *System_Info;
        System_Info = cJSON_GetObjectItem(json, "Response");
        rankingList = QString::fromStdString(System_Info->valuestring).split(",");
        cJSON_Delete(json);
    }
    else if (type == "RANDOM_MATCH_FOUND")
    {
        waiting = false;
        if (matchingDialog)
        {
            matchingDialog->hide();
            matchingDialog->deleteLater();
            matchingDialog = nullptr;
        }
        cJSON *opponent = cJSON_GetObjectItem(json, "Opponent");
        cJSON *side = cJSON_GetObjectItem(json, "Side");

        if (!opponent || !side)
        {
            qDebug() << "Invalid match data received";
            cJSON_Delete(json);
            return true;
        }

        QString opponentName = QString(opponent->valuestring);
        QString sideStr = QString(side->valuestring);

        inRooms = true;
        if (sideStr == "white")
        {
            yourSide = 0;
            emit PlayWhite(id_id + "#" + QString::number(id_elo), opponentName);
        }
        else
        {
            yourSide = 1;
            emit PlayBlack(opponentName, id_id + "#" + QString::number(id_elo));
        }
        emit ShowGame();
        cJSON_Delete(json);
    }
    else if (type == "ELO_MATCH_FOUND")
    {
        waiting = false;

        cJSON *opponent = cJSON_GetObjectItem(json, "Opponent");
        cJSON *side = cJSON_GetObjectItem(json, "Side");
        cJSON *opponentElo = cJSON_GetObjectItem(json, "OpponentElo");

        if (eloMatchingDialog)
        {
            eloMatchingDialog->close();
            eloMatchingDialog->deleteLater();
            eloMatchingDialog = nullptr;
        }

        inRooms = true;

        QString opponentName = QString::fromStdString(opponent->valuestring);
        QString sideStr = QString(side->valuestring);
        if (sideStr == "white")
        {
            yourSide = 0;
            emit PlayWhite(id_id + "#" + QString::number(id_elo), opponentName);
        }
        else
        {
            yourSide = 1;
            emit PlayBlack(opponentName, id_id + "#" + QString::number(id_elo));
        }
        emit ShowGame();
        cJSON_Delete(json);
    }
    return true;
}

void gameLobby::createRoomsList(cJSON *json)
{

    qDeleteAll(chessroomS);
    chessroomS.clear();

    cJSON *List;
    List = cJSON_GetObjectItem(json, "List");
    ChessRoom *NewChess;
    int array_size = cJSON_GetArraySize(List);
    cJSON *chessroomINFO = NULL;

    if (array_size == 0)
    {
        cJSON_Delete(json);
        return;
    }

    cJSON_ArrayForEach(chessroomINFO, List)
    {
        bool playing = false;
        cJSON *Name = cJSON_GetObjectItem(chessroomINFO, "name");
        cJSON *ID = cJSON_GetObjectItem(chessroomINFO, "id");
        cJSON *isPlaying = cJSON_GetObjectItem(chessroomINFO, "isplay");

        if (isPlaying->valueint)
            playing = true;
        else
            playing = false;
        NewChess = new ChessRoom(this, Name->valuestring, ID->valueint, playing);
        if (playing)
        {
            cJSON *P2 = cJSON_GetObjectItem(chessroomINFO, "p2name");
            NewChess->player2(P2->valuestring);
        }

        chessroomS.append(NewChess);
    }
    cJSON_Delete(json);
    showRooms();
}

void gameLobby::showRooms()
{
    LobbySUI();
    waitingForJoin();

    int len = chessroomS.length();
    for (int i = 0; i < len; i++)
    {
        connect(chessroomS[i], SIGNAL(clicked(int)), this, SLOT(sendJointRequest(int)));
        chessroomS[i]->setPos(300 + (i % 3) * 300, 300 + (i / 3) * 240);
        this->OnlineScene->addItem(chessroomS[i]);
    }
}

void gameLobby::LobbySUI()
{
    if (!inRooms && !waiting && !host)
        playButton->show();
    else
        playButton->hide();
}

void gameLobby::waitingForJoin()
{
    if (host)
        hostWindow_show();
    else
        hostWindow_hide();
}

void gameLobby::hostWindow()
{
    rect = new QGraphicsRectItem();
    rect->setRect(0, 0, 450, 300);
    QBrush Abrush;
    Abrush.setStyle(Qt::SolidPattern);
    Abrush.setColor(QColor(199, 231, 253));
    rect->setBrush(Abrush);
    rect->setZValue(4);
    int pxPos = width() / 2 - rect->boundingRect().width() / 2;
    int pyPos = 250;
    rect->setPos(pxPos, pyPos);
    WindowTitle = new QGraphicsTextItem("Wating for other Players...");
    QFont titleFont("arial", 20);
    WindowTitle->setFont(titleFont);
    int axPos = width() / 2 - WindowTitle->boundingRect().width() / 2;
    int ayPos = 300;
    WindowTitle->setPos(axPos, ayPos);
    WindowTitle->setZValue(5);
    CancelBotton = new button("Cancel Host");
    int qxPos = width() / 2 - CancelBotton->boundingRect().width() / 2;
    int qyPos = 400;
    CancelBotton->setPos(qxPos, qyPos);
    CancelBotton->setZValue(5);
    connect(CancelBotton, SIGNAL(clicked()), this, SLOT(CancelHost()));
    hostWindow_hide();
    OnlineScene->addItem(rect);
    OnlineScene->addItem(WindowTitle);
    OnlineScene->addItem(CancelBotton);
}

void gameLobby::hostWindow_hide()
{
    rect->hide();
    WindowTitle->hide();
    CancelBotton->hide();
}

void gameLobby::hostWindow_show()
{
    rect->show();
    WindowTitle->show();
    CancelBotton->show();
}

void gameLobby::sendJointRequest(int ID)
{
    if (!inRooms && !waiting && !host)
    {
        this->SendRequestForJoining(ID);
    }
}

void gameLobby::SendRequestForJoining(int ID)
{
    cJSON *Request;
    Request = cJSON_CreateObject();
    cJSON_AddStringToObject(Request, "Type", "JOIN_ROOM");
    cJSON_AddStringToObject(Request, "User", (id_id + "#" + QString::number(id_elo)).toStdString().c_str());
    cJSON_AddNumberToObject(Request, "ID", ID);

    char *JsonToSend = cJSON_Print(Request);
    cJSON_Delete(Request);
    int RetnCheck = send(Connection, JsonToSend, strlen(JsonToSend), NULL);
    if (RetnCheck < 0)
        return;
    t2 = std::thread(WaitforResponseThread);
}

void gameLobby::sendInvite(const QString &username)
{
    static bool isInviteInProgress = false;
    if (isInviteInProgress)
        return;
    isInviteInProgress = true;
    qDebug() << "Sending invite to user:" << username;

    cJSON *inviteMessage = cJSON_CreateObject();
    cJSON_AddStringToObject(inviteMessage, "Type", "INVITE");
    cJSON_AddStringToObject(inviteMessage, "User", username.toStdString().c_str());

    char *jsonToSend = cJSON_Print(inviteMessage);
    cJSON_Delete(inviteMessage);

    if (!jsonToSend)
    {
        qDebug() << "Failed to serialize JSON object to string.";
        isInviteInProgress = false;
        return;
    }

    if (send(Connection, jsonToSend, strlen(jsonToSend), 0) == -1)
    {
        qDebug() << "Failed to send invite message.";
    }
    else
    {
        qDebug() << "Invite message sent to server successfully.";
    }

    free(jsonToSend);

    QTimer::singleShot(1000, this, [&]()
                       { isInviteInProgress = false; });
}

void gameLobby::exitLobby()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "EXIT");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    send(Connection, JsonToSend, strlen(JsonToSend), NULL);
}

void gameLobby::WaitforResponseThread()
{
    clientptr->waiting = true;
    QThread::msleep(4000);
    if (clientptr->waiting)
    {
        qDebug() << "Join Room time out!";
        clientptr->Signal_TimeoutJoin();
        clientptr->waiting = false;
    }
}

void gameLobby::JoinTimeOut()
{
    QMessageBox::information(NULL, "Time Out", "Join Room time out.");
}

void gameLobby::Leave()
{
    host = false;
    inRooms = false;
    waiting = false;
    yourSide = -1;
    if (matchingDialog)
    {
        matchingDialog->hide();
        matchingDialog->deleteLater();
        matchingDialog = nullptr;
    }
    cJSON *Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "LEAVE");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);

    if (Connection != -1)
    {
        send(Connection, JsonToSend, strlen(JsonToSend), 0);
        free(JsonToSend);
    }
    showRooms();
    QMessageBox::information(NULL, "Game is end", "The host or player already left the room.");
}

void gameLobby::List_is_full()
{
    QMessageBox::information(NULL, "List Full", "The max amount of Chess Games is 6.");
}

void gameLobby::This_Game_isFull()
{
    QMessageBox::information(NULL, "Game is Full", "This Game is already started.");
}

void gameLobby::ServerClose()
{
    QMessageBox::information(NULL, "Server closed", "Socket to the server was closed successfuly.");
}

void gameLobby::SocketBugs()
{
    QMessageBox::warning(NULL, "Socket has problem", "Socket was not able to be closed.");
}

void gameLobby::ClientThread()
{
    while (true)
    {
        if (!is_opened)
            return;
        if (!clientptr->GetString())
            break;
    }
    if (clientptr->CloseConnection())
        clientptr->Signal_socketClosed();
    clientptr->Signal_socketClosedfailed();
}

void gameLobby::EndGame(int color)
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "EndGame");
    cJSON_AddNumberToObject(Mesg, "Winner", color);
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    qDebug() << JsonToSend;
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL))
    {
    }
}

void gameLobby::I_wannaDraw()
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "ASK_DRAW");
    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    qDebug() << JsonToSend;
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL))
    {
    }
}

void gameLobby::handleInvite(const QString &fromUser, int gameID)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Game Invite");
    msgBox.setText(fromUser + " has invited you to play a game. Game ID: " + QString::number(gameID));
    QPushButton *acceptButton = msgBox.addButton("Accept", QMessageBox::AcceptRole);
    QPushButton *declineButton = msgBox.addButton("Decline", QMessageBox::RejectRole);
    msgBox.exec();

    if (msgBox.clickedButton() == acceptButton)
    {
        SendRequestForJoining(gameID);
        qDebug() << "Invite accepted from " << fromUser << " for game ID " << gameID;
    }
    else if (msgBox.clickedButton() == declineButton)
    {
        cJSON *response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "Type", "INVITE_RESPONSE");
        cJSON_AddStringToObject(response, "Response", "DECLINE");
        cJSON_AddStringToObject(response, "FromUser", fromUser.toStdString().c_str());
        cJSON_AddNumberToObject(response, "GameID", gameID);
        char *jsonToSend = cJSON_Print(response);
        send(Connection, jsonToSend, strlen(jsonToSend), 0);
        cJSON_Delete(response);

        qDebug() << "Invite declined from " << fromUser << " for game ID " << gameID;
    }
}

void gameLobby::handleOnlineUserList(const QStringList &users)
{
    onlineUserList->clear();

    for (const QString &username : users)
    {
        QWidget *userWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(userWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        QLabel *userLabel = new QLabel(username);
        layout->addWidget(userLabel);

        if (username != id_id)
        {
            QPushButton *inviteButton = new QPushButton("Invite");
            layout->addWidget(inviteButton);

            connect(inviteButton, &QPushButton::clicked, this, [this, username]()
                    { sendInvite(username); });
        }

        QStandardItem *item = new QStandardItem();
        onlineUserList->appendRow(item);
        QModelIndex index = onlineUserList->indexFromItem(item);
        listView->setIndexWidget(index, userWidget);
    }
}

void gameLobby::onInviteResponse(const QString &message, bool success)
{
    if (success)
    {
        QMessageBox::information(this, "Invite Status", message);
    }
    else
    {
        QMessageBox::warning(this, "Invite Status", message);
    }
}

void gameLobby::sendDraw(int reply)
{
    cJSON *Mesg;
    Mesg = cJSON_CreateObject();
    cJSON_AddStringToObject(Mesg, "Type", "DRAW");
    cJSON_AddNumberToObject(Mesg, "Confirm", reply);

    char *JsonToSend = cJSON_Print(Mesg);
    cJSON_Delete(Mesg);
    qDebug() << JsonToSend;
    if (send(Connection, JsonToSend, strlen(JsonToSend), NULL))
        emit Draw();
}

void gameLobby::MatchRandomPlayer()
{
    if (!host && !waiting && !inRooms)
    {
        matchingDialog = new QDialog(this);
        matchingDialog->setWindowTitle("Finding Match");

        QVBoxLayout *layout = new QVBoxLayout(matchingDialog);

        QLabel *waitLabel = new QLabel("Waiting for opponent...");
        layout->addWidget(waitLabel);

        QPushButton *cancelBtn = new QPushButton("Cancel");
        connect(cancelBtn, &QPushButton::clicked, this, &gameLobby::CancelRandomMatch);
        layout->addWidget(cancelBtn);

        cJSON *Mesg = cJSON_CreateObject();
        cJSON_AddStringToObject(Mesg, "Type", "RANDOM_MATCH");
        cJSON_AddStringToObject(Mesg, "User", (id_id + "#" + QString::number(id_elo)).toStdString().c_str());

        char *JsonToSend = cJSON_Print(Mesg);
        cJSON_Delete(Mesg);

        if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) < 0)
        {
            QMessageBox::critical(NULL, "Error", "Failed to send match request!");
            delete matchingDialog;
            return;
        }

        waiting = true;
        matchingDialog->show();
    }
}

void gameLobby::CancelRandomMatch()
{
    if (waiting)
    {
        try
        {
            cJSON *Mesg = cJSON_CreateObject();
            if (!Mesg)
            {
                throw std::runtime_error("Failed to create JSON object");
            }

            cJSON_AddStringToObject(Mesg, "Type", "CANCEL_RANDOM_MATCH");
            cJSON_AddStringToObject(Mesg, "User", (id_id + "#" + QString::number(id_elo)).toStdString().c_str());

            char *JsonToSend = cJSON_Print(Mesg);
            if (!JsonToSend)
            {
                cJSON_Delete(Mesg);
                throw std::runtime_error("Failed to print JSON");
            }

            if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) < 0)
            {
                free(JsonToSend);
                cJSON_Delete(Mesg);
                throw std::runtime_error("Failed to send cancel request");
            }

            free(JsonToSend);
            cJSON_Delete(Mesg);

            waiting = false;
            if (matchingDialog)
            {
                matchingDialog->close();
                delete matchingDialog;
                matchingDialog = nullptr;
            }
        }
        catch (const std::exception &e)
        {
            QMessageBox::critical(this, "Error", QString("Failed to cancel match: %1").arg(e.what()));
        }
    }
}

void gameLobby::MatchEloPlayer()
{
    if (!host && !waiting && !inRooms)
    {
        eloMatchingDialog = new QDialog(this);
        eloMatchingDialog->setWindowTitle("Finding ELO Match");

        QVBoxLayout *layout = new QVBoxLayout(eloMatchingDialog);

        QString tierName;
        EloTier tier = getEloTier(id_elo);
        switch (tier)
        {
        case EloTier::BEGINNER:
            tierName = "Beginner (0-800)";
            break;
        case EloTier::INTERMEDIATE:
            tierName = "Intermediate (801-1600)";
            break;
        case EloTier::ADVANCED:
            tierName = "Advanced (1601-2000)";
            break;
        case EloTier::EXPERT:
            tierName = "Expert (2001-2400)";
            break;
        case EloTier::MASTER:
            tierName = "Master (2400+)";
            break;
        }

        QLabel *waitLabel = new QLabel(QString("Finding opponent in %1 tier...\nYour ELO: %2").arg(tierName).arg(id_elo));
        layout->addWidget(waitLabel);

        QPushButton *cancelBtn = new QPushButton("Cancel");
        connect(cancelBtn, &QPushButton::clicked, this, &gameLobby::CancelEloMatch);
        layout->addWidget(cancelBtn);

        cJSON *Mesg = cJSON_CreateObject();
        cJSON_AddStringToObject(Mesg, "Type", "ELO_MATCH");
        cJSON_AddStringToObject(Mesg, "User", (id_id + "#" + QString::number(id_elo)).toStdString().c_str());

        char *JsonToSend = cJSON_Print(Mesg);
        cJSON_Delete(Mesg);

        if (send(Connection, JsonToSend, strlen(JsonToSend), NULL) < 0)
        {
            QMessageBox::critical(NULL, "Error", "Failed to send match request!");
            delete eloMatchingDialog;
            return;
        }

        waiting = true;
        eloMatchingDialog->show();
    }
}

void gameLobby::CancelEloMatch()
{
    if (waiting)
    {
        try
        {
            cJSON *Mesg = cJSON_CreateObject();
            if (!Mesg)
            {
                throw std::runtime_error("Failed to create JSON object");
            }
            cJSON_AddStringToObject(Mesg, "Type", "CANCEL_ELO_MATCH");
            cJSON_AddStringToObject(Mesg, "User", (id_id + "#" + QString::number(id_elo)).toStdString().c_str());

            char *JsonToSend = cJSON_Print(Mesg);
            if (!JsonToSend)
            {
                cJSON_Delete(Mesg);
                throw std::runtime_error("Failed to print JSON");
            }

            send(Connection, JsonToSend, strlen(JsonToSend), NULL);

            free(JsonToSend);
            cJSON_Delete(Mesg);

            waiting = false;
            if (eloMatchingDialog)
            {
                eloMatchingDialog->close();
                delete eloMatchingDialog;
                eloMatchingDialog = nullptr;
            }
        }
        catch (const std::exception &e)
        {
            QMessageBox::critical(this, "Error", QString("Failed to cancel match: %1").arg(e.what()));
        }
    }
}

std::string statusToString(StatusCode code)
{
    switch (code)
    {
    case StatusCode::OK:
        return "200";
    case StatusCode::CREATED:
        return "201";
    case StatusCode::BAD_REQUEST:
        return "400";
    case StatusCode::UNAUTHORIZED:
        return "401";
    case StatusCode::FORBIDDEN:
        return "403";
    case StatusCode::NOT_FOUND:
        return "404";
    case StatusCode::CONFLICT:
        return "409";
    case StatusCode::SERVER_ERROR:
        return "500";
    case StatusCode::SERVICE_UNAVAIABLE:
        return "503";
    default:
        return "Unknown Status";
    }
}