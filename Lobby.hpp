#pragma once

#include "ServerCommons.h"
#include "Server.hpp"
#include "PackageTypes.h"

class Lobby {
public:
	Lobby() = default;
	virtual ~Lobby() {
		printf("Closing lobby\n");
	}

public:
	Lobby(const Lobby&) = delete;
	Lobby& operator=(const Lobby&) = delete;

public:
	void push_player(std::shared_ptr<TcpConnection> player) {
		if (player1 == nullptr) {
			player1 = player;
		}
		else if (player2 == nullptr) {
			player2 = player;
		}

		if (player1 != nullptr && player2 != nullptr) {
			printf("Game started\n");
			send_start(player1);
			send_start(player2);
		}
	}

	void move(int connection_id, int index) {
		int player_id = get_player_id(connection_id);

		if (player_id != turn) {
			return;
		}

		board[index] = player_id;
		turn = player_id == 1 ? 2 : 1;

		send_game_state();
	}

	void on_player_disconnected(int player_id) {
		end_game(player_id == 1 ? 2 : 1);
		is_game_over = true;
	}

	bool is_game_alive() const {
		return !is_game_over;
	}

private:
	void send_start(std::shared_ptr<TcpConnection> player) {
		auto msg = TcpMessage();
		msg.header.id = PackageType::start_game;
		msg.add_int(get_player_id(player->get_id()));
		player->send(msg);
	}

	int get_player_id(int connection_id) {
		if (connection_id == player1->get_id()) {
			return 1;
		}

		if (connection_id == player2->get_id()) {
			return 2;
		}

		return 0;
	}

	void send_game_state() {
		auto msg = TcpMessage();
		msg.header.id = PackageType::game_state;

		for (auto& cell : board) {
			msg.add_int(cell);
		}

		msg.add_int(turn);

		player1->send(msg);
		player2->send(msg);

		check_winner();
	}


	void check_winner() {
		if ((board[0] == 1 && board[1] == 1 && board[2] == 1) ||
			(board[3] == 1 && board[4] == 1 && board[5] == 1) ||
			(board[6] == 1 && board[7] == 1 && board[8] == 1) ||
			(board[0] == 1 && board[3] == 1 && board[6] == 1) ||
			(board[1] == 1 && board[4] == 1 && board[7] == 1) ||
			(board[2] == 1 && board[5] == 1 && board[8] == 1) ||
			(board[0] == 1 && board[4] == 1 && board[8] == 1) ||
			(board[2] == 1 && board[4] == 1 && board[6] == 1)) {
			return end_game(1);
		}

		if ((board[0] == 2 && board[1] == 2 && board[2] == 2) ||
			(board[3] == 2 && board[4] == 2 && board[5] == 2) ||
			(board[6] == 2 && board[7] == 2 && board[8] == 2) ||
			(board[0] == 2 && board[3] == 2 && board[6] == 2) ||
			(board[1] == 2 && board[4] == 2 && board[7] == 2) ||
			(board[2] == 2 && board[5] == 2 && board[8] == 2) ||
			(board[0] == 2 && board[4] == 2 && board[8] == 2) ||
			(board[2] == 2 && board[4] == 2 && board[6] == 2)) {
			return end_game(2);
		}

		for(auto& cell : board) {
			if (cell == 0) {
				return;
			}
		}
		
		end_game(0);
	}

	void end_game(int winner) {
		auto msg = TcpMessage();
		msg.header.id = PackageType::end_game;
		msg.add_int(winner);

		printf("Game ended with winner %d \n", winner);
		
		player1->send(msg);
		player2->send(msg);

		is_game_over = true;
	}


private:
	std::vector<int> board = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int turn = 1;
	bool is_game_over = false;

private:
	std::shared_ptr<TcpConnection> player1 = nullptr;
	std::shared_ptr<TcpConnection> player2 = nullptr;
};
