#pragma once

#include <unordered_map>

#include "ServerCommons.h"
#include "DequeThreadSafe.hpp"

#include "asio.hpp"
#include "asio/ts/socket.hpp"


class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(int id, asio::ip::tcp::socket socket, asio::io_context& context, DequeThreadSafe<TcpOwnedMessage>& in_queue)
        : m_id(id), m_socket(std::move(socket)), m_context(context), m_in_messages(in_queue) {
        printf("New connection arrived with id %d\n", id);
    }

    virtual ~TcpConnection() {
        printf("Connection with id %d closed\n", m_id);
        m_socket.close();
    }

public:
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

public:
    int get_id() const { return m_id; }
    bool is_connected() const { return m_socket.is_open(); }

    void connect() {
        try {
            if (m_socket.is_open()) {
                read_header();
            }
        }
        catch (const std::runtime_error& e) {
            printf("Unable to connect: %s\n", e.what());
        }
    }

    void disconnect() {
        if (is_connected()) {
            printf("Disconnecting connection with id %d\n", m_id);
            asio::post(m_context, [this]() {
                m_socket.close();
            });
        }
    }

    void send(TcpMessage msg) {
        asio::post(m_context, [this, msg]() {
            bool is_writing = !m_out_messages.is_empty();
            m_out_messages.push_back(msg);
            if (!is_writing) {
                write_header();
            }
        });
    }

private:
    void read_header() {
        asio::async_read(m_socket, asio::buffer(&m_in_buffer.header, sizeof(TcpHeader)),
            [this](const std::error_code& ec, std::size_t length) {
            if (!ec) {
                if (m_in_buffer.header.size > 0) {
                    m_in_buffer.body.resize(m_in_buffer.header.size);
                    read_body();
                }
                else {
                    add_to_queue();
                }
            }
            else if (ec != asio::error::eof) {
                printf("Unable to read header: %s\n", ec.message().c_str());
                disconnect();
            }
        });
    }

    void read_body() {
        asio::async_read(m_socket, asio::buffer(m_in_buffer.body.data(), m_in_buffer.body.size()),
            [this](const std::error_code& ec, std::size_t length) {
            if (!ec) {
                add_to_queue();
            }
            else if (ec != asio::error::eof) {
                printf("Unable to read body: %s\n", ec.message().c_str());
                disconnect();
            }
        });
    }

    void write_header() {
        asio::async_write(m_socket, asio::buffer(&m_out_messages.front().header, sizeof(TcpHeader)),
            [this](const std::error_code& ec, std::size_t length) {
            if (!ec) {
                if(m_out_messages.front().header.id == 2) {
					printf("End game sent\n");
				}
				else {
					printf("Message sent\n");
				}

                if (m_out_messages.front().header.size > 0) {
                    write_body();
                }
                else {
                    m_out_messages.pop_front();
                    if (!m_out_messages.is_empty()) {
                        write_header();
                    }
                }
            }
            else {
                printf("Unable to write header: %s\n", ec.message().c_str());
            }
        });
    }

    void write_body() {
        asio::async_write(m_socket, asio::buffer(m_out_messages.front().body.data(), m_out_messages.front().body.size()),
            [this](const std::error_code& ec, std::size_t length) {
            if (!ec) {
                m_out_messages.pop_front();
                if (!m_out_messages.is_empty()) {
                    write_header();
                }
            }
            else {
                printf("Unable to write body: %s\n", ec.message().c_str());
            }
        });
    }

    void add_to_queue() {
        auto msg    = TcpOwnedMessage();
        msg.header  = m_in_buffer.header;
        msg.body    = m_in_buffer.body;
        msg.owner   = shared_from_this();

        m_in_messages.push_back(msg);
        read_header();
    }

private:
    int m_id;
    asio::ip::tcp::socket m_socket;
    asio::io_context& m_context;

    DequeThreadSafe<TcpMessage> m_out_messages;
    DequeThreadSafe<TcpOwnedMessage>& m_in_messages;

    TcpMessage m_in_buffer;
};

class TcpServer {
public:
    TcpServer(const char* port, asio::io_context& context)
        : m_context(context),
        m_acceptor(context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), atoi(port))) {
        printf("Server running on port %d\n", atoi(port));
        listen();
        m_thread = std::thread([this]() { m_context.run(); });
    }

    virtual ~TcpServer() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

public:
    void send(TcpMessage msg, std::shared_ptr<TcpConnection> connection) {
		if (connection && connection->is_connected()) {
	        connection->send(msg);
		}
        else {
            on_disconnected(connection);
        }
	}

    void send_all(TcpMessage msg, std::shared_ptr<TcpConnection> ignore) {
         std::vector<int> to_delete;

		for (auto& pair : m_connections) {
            auto& connection = pair.second;
            bool is_alive = false;

            if (connection) {
                if (connection->is_connected()) {
                    is_alive = true;
                    if (connection != ignore) {
                        connection->send(msg);
                    }
                }
            }

            if(!is_alive) {
                to_delete.push_back(pair.first);
                on_disconnected(pair.second);
            }
		}

        for (int id : to_delete) {
            m_connections.erase(id);
        }
	}

    void update() {
        if (!m_in_messages.is_empty()) {
            on_message(m_in_messages.pop_front());
        }
    }

    virtual void on_message(TcpOwnedMessage message) = 0;
    virtual void on_connected(std::shared_ptr<TcpConnection> connection) = 0;
    virtual void on_disconnected(std::shared_ptr<TcpConnection> connection) = 0;

private:
    void listen() {
        m_acceptor.async_accept([this](const std::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                accept_connection(std::move(socket));
            }
            else {
                printf("Unable to accept connection: %s\n", ec.message().c_str());
            }
            listen();
        });
    }

    void accept_connection(asio::ip::tcp::socket socket) {
        auto connection = std::shared_ptr<TcpConnection>(
            new TcpConnection(m_connectionId++, std::move(socket), m_context, m_in_messages)
        );

        connection->connect();
        on_connected(connection);
        m_connections[connection->get_id()] = std::move(connection);
    }

protected:
    DequeThreadSafe<TcpOwnedMessage> m_in_messages;
    std::unordered_map<int, std::shared_ptr<TcpConnection>> m_connections;
    int m_connectionId = 1000;

private:
    asio::io_context& m_context;
    asio::ip::tcp::acceptor m_acceptor;
    std::thread m_thread;
};