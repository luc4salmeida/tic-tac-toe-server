#pragma once

#include <vector>
#include <stdexcept>
#include <memory>

class TcpConnection;

struct TcpHeader {
    uint32_t id = 0;
    uint32_t size = 0;
};

struct TcpMessage {
    TcpHeader header{};
    std::vector<char> body;

    void add_int(int value) {
        header.size += sizeof(value);
        size_t old_size = body.size();
        body.resize(body.size() + sizeof(value));
        memcpy(body.data() + old_size, &value, sizeof(value));
    }

    int pop_int() {
        if (body.size() < sizeof(int)) {
            throw std::runtime_error("No int to pop");
        }

        int ret;
        memcpy(&ret, body.data() + body.size() - sizeof(int), sizeof(int));

        body.resize(body.size() - sizeof(int));
        header.size -= sizeof(int);

        return ret;
    }
};

struct TcpOwnedMessage : public TcpMessage {
    std::shared_ptr<TcpConnection> owner = nullptr;
};