
#include "TicTocServer.hpp"


int main() {
    asio::io_context context;
    TicTocServer server("30000", context);

    while (true) {
        server.update();
        server.check_loose();
    }

    return 0;
}
