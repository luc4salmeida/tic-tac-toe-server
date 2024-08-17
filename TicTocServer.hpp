#include "Server.hpp"
#include "PackageTypes.h"
#include "Lobby.hpp"

#include <map>
#include <deque>
#include <unordered_map>
#include <memory>

struct Match {
    int player1_id;
    int player2_id;
};

class TicTocServer : public TcpServer {
public:
    TicTocServer(const char* port, asio::io_context& context) :
        TcpServer(port, context) {}

    virtual ~TicTocServer() {
        m_lobbies.clear();
    }

    void on_message(TcpOwnedMessage msg) override {
        if (msg.header.id == PackageType::player_move) {
            on_player_move(msg);
        }
    }

    void on_connected(std::shared_ptr<TcpConnection> connection) override {
        m_room.push_back(connection);
        if (m_room.size() % 2 == 0) {
            auto lobby = std::make_unique<Lobby>();

            auto& p1 = m_room.front();
            auto& p2 = m_room.back();

            lobby->push_player(p1);
            lobby->push_player(p2);

            int lobby_id = m_last_lobby_id++;
            m_lobbies[lobby_id] = std::move(lobby);
            m_matches[lobby_id] = Match{ p1->get_id(), p2->get_id() };

            m_room.pop_front();
            m_room.pop_back();
        }
    }

    void on_disconnected(std::shared_ptr<TcpConnection> connection) override {
       int player_id = connection->get_id();

        for (const auto& pair : m_matches) {
            auto& match = pair.second;
            auto lobby_id = pair.first;
            if (match.player1_id == player_id || match.player2_id == player_id) {
                auto& lobby = m_lobbies[lobby_id];
                lobby->on_player_disconnected(player_id);
            }
        }
    }

    void on_player_move(TcpOwnedMessage msg) {
        int index = msg.pop_int();
        int player_id = msg.owner->get_id();

        for (const auto& pair : m_matches) {
            auto& match = pair.second;
            auto lobby_id = pair.first;
            if (match.player1_id == player_id || match.player2_id == player_id) {
                auto& lobby = m_lobbies[lobby_id];
                lobby->move(player_id, index);
            }
        }
    }

    void check_loose() {
        for (auto it = m_matches.begin(); it != m_matches.end();) {
            int lobby_id = it->first;
            auto& lobby = m_lobbies[lobby_id];
            if (!lobby->is_game_alive()) {
                close_lobby(lobby_id);
                it = m_matches.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void close_lobby(int lobby_id) {
        m_lobbies.erase(lobby_id);
    }

private:
    std::deque<std::shared_ptr<TcpConnection>> m_room;
    std::unordered_map<int, std::unique_ptr<Lobby>> m_lobbies;
    std::map<int, Match> m_matches;
    int m_last_lobby_id = 0;
};
