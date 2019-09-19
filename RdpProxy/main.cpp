#include "browserclient.h"

#define DEFAULT_SERVER_ADDR "192.168.6.130"
#define DEFAULT_SERVER_PORT "8080"

int main(int argc, const char *argv[])
{
    char *addr, *port;
    if (argc > 2) {
        addr = strdup(argv[1]);
        port = strdup(argv[2]);
    }
    else {
        addr = strdup(DEFAULT_SERVER_ADDR);
        port = strdup(DEFAULT_SERVER_PORT);
    }
    std::cerr << "Starting " << argv[0] << " " << addr << ":" << port << std::endl;
    remove("rdpproxy.log");
    while (1) {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(addr, port);
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
        boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12);
        BrowserClient bc(io_service, ctx, iterator);
        io_service.run();
    }
    return 0;
}
