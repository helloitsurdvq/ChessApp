#pragma once

#include <string>
#include <iostream>
#include <memory>
#include "Games.h"

typedef std::shared_ptr<Game> onlineGame;

class Player
{
public:
	Player(int id)
	{
		this->id = id;
	}
	void JoininGame(int GameID, onlineGame GAME)
	{
		this->GameID = GameID;
		ingame = true;
		this->game = GAME;
	}
	void hostGame(int gameID, onlineGame GAME)
	{
		this->GameID = gameID;
		this->game = GAME;
		ishost = true;
	}
	void returnToLobby()
	{
		ishost = false;
		ingame = false;
		GameID = -1;
		game = NULL;
	}
	bool isFree()
	{
		return !ishost && !ingame;
	}
	int AreYouInGame()
	{
		return GameID;
	}
	bool isOnlyInRoom()
	{
		return ishost && !ingame;
	}
	bool ishost = false;
	bool isWaitingForRandomMatch = false;
	bool isWaitingForEloMatch = false;
	int waitingEloTier = -1;
	bool isWaitingForMatch() const { return isWaitingForRandomMatch; }
	void setWaitingForMatch(bool waiting) { isWaitingForRandomMatch = waiting; }

	void setMatchingState(bool waiting, int tier = -1)
	{
		isWaitingForEloMatch = waiting;
		waitingEloTier = tier;
	}

	void resetMatchingState()
	{
		isWaitingForEloMatch = false;
		isWaitingForRandomMatch = false;
		waitingEloTier = -1;
	}

	bool canJoinGame() const
	{
		return !ishost && !ingame && !isWaitingForEloMatch && !isWaitingForRandomMatch;
	}

private:
	int id;
	int GameID = -1;
	onlineGame game = NULL;
	bool ingame = false;
};