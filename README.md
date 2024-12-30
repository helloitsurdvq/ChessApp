# Online Chess Application

Network Programming (IT4062E) Project - Group 1

## Features
- Establish a TCP client-server connection using sockets.
- Log all received and sent messages on the server for debugging and monitoring purposes.
- Enable user login, registration and authentication.
- Match players randomly or based on their skill level.
- Create game rooms, invite players to join and accept room invitations.
- Play chess with turn-based gameplay and enforce legal moves.
- Detect and announce the winner or loser for both players.
- Support draw proposals and handle them effectively.
- View a list of online players and player rankings based on their ratings.

## Setup
1. Install Qt development libraries
```shell
sudo apt install qtbase5-dev qt5-qmake libqt5sql5
```

2. Install MySQL connector
```shell
sudo apt install libqt5sql5-mysql  
```

3. Setup MySQL database
```mysql
CREATE DATABASE chess_db;
```

```mysql
CREATE TABLE accounts (
    username VARCHAR(255) PRIMARY KEY NOT NULL,
    password VARCHAR(255) NOT NULL,
    elo INT DEFAULT 1000
);
```

## Usage
1. Run the server
```bash
cd ChessServer
make
./ChessServer
```

2. Run the client
```bash
cd Client
make
./chessClient
```

3. Register/Login with username, password and the IP address of the server and enjoy the game!

## Team Members
<table>
    <tbody>
        <tr>
            <th align="center">Member name</th>
            <th align="center">Student ID</th>
        </tr>
        <tr>
            <td>Ngô Xuân Bách</td>
            <td align="center"> 20215181&nbsp;&nbsp;&nbsp;</td>
        </tr>
        <tr>
            <td>Đinh Việt Quang</td>
            <td align="center"> 20215235&nbsp;&nbsp;&nbsp;</td>
        </tr>
    </tbody>
</table>