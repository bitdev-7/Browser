#ifndef BROWSERCLIENT_H
#define BROWSERCLIENT_H

#include <time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <QObject>
#include <QUdpSocket>

using boost::asio::deadline_timer;

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "GDI32.lib")
#pragma comment(lib, "Advapi32.lib")

#ifdef _DEBUG
#pragma comment (lib, "libeay32MTd.lib")
#pragma comment (lib, "ssleay32MTd.lib")
#else
#pragma comment (lib, "libeay32MT.lib")
#pragma comment (lib, "ssleay32MT.lib")
#endif




/***
browser -> BROWSER_PACKET -> server -> SERVICE_PACKET -> service
(PNG_SVR, DRV_LST, FLD_LST, FLD_CRT, FIL_GET, FIL_PUT, MUS_CTL, KBD_CTL, SCR_GET, SCR_OFF)
browser <- SERVICE_CONTENTS <- server <- SERVICE_PACKET <- service
(PNG_SVR, SVC_ADD, SVC_DEL, DRV_LIST, FLD_LST, FLD_CRT, FIL_GET, FIL_PUT, SCR_GET)
***/

#define PACKET_TYPE_PNG_SVR 0
#define PACKET_TYPE_SVC_ADD 1
#define PACKET_TYPE_SVC_DEL 2
#define PACKET_TYPE_DRV_LST 3
#define PACKET_TYPE_FLD_LST 4
#define PACKET_TYPE_FLD_CRT 5
#define PACKET_TYPE_FIL_GET 6
#define PACKET_TYPE_FIL_PUT 7
#define PACKET_TYPE_MUS_CTL 8
#define PACKET_TYPE_KBD_CTL 9
#define PACKET_TYPE_SCR_GET 10
#define PACKET_TYPE_SCR_OFF 11

#define LOCAL_IPC_B2P_PORT  8888
#define LOCAL_IPC_P2B_PORT  7777

#pragma pack(push, 1)

/// service respond packet
typedef struct _service_contents
{
    uint32_t            packet_size;    // data size
    uint16_t            packet_type;    // packet type
    //void*             packet_data;    // data contents
}SERVICE_CONTENTS;

/// service packet data structure to be sent
typedef struct _service_request
{
    uint32_t            browser_sock;  // browser socket
    SERVICE_CONTENTS    service_data;  // service contents
}SERVICE_PACKET;

/// browser packet data structure
typedef struct _browser_packet
{
    uint32_t            service_sock;  // target socket
    SERVICE_PACKET      service_request;
}BROWSER_PACKET;

#pragma pack(pop)

#define BROWSER_PACKET_OVERHEAD    sizeof(BROWSER_PACKET)
#define SERVICE_PACKET_OVERHEAD    sizeof(SERVICE_PACKET)
#define SERVICE_CONTENTS_OVERHEAD  sizeof(SERVICE_CONTENTS)

#define BUFFER_SIZE                16384	// 16KB
#define BROWSER_PACKET_MAX_DATA    (BUFFER_SIZE - BROWSER_PACKET_OVERHEAD - MAX_PATH)

#define PING_SENDING_PERIOD      10    // 1s

#define NOTIFY_MESSAGE(msg)		do { time_t now = time(NULL); struct tm tstruct = *localtime(&now);		 \
                                     char buf[80]; strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);  \
                                     std::ofstream logfile; logfile.open("rdpproxy.log", std::ios_base::app); \
                                     logfile << "[" << buf << "] " << msg << std::endl; logfile.close(); \
                                } while(0)

class BrowserClient:public QObject
{
    Q_OBJECT
public:
    explicit BrowserClient(boost::asio::io_service& io_service, boost::asio::ssl::context& context, boost::asio::ip::tcp::resolver::iterator endpoint_iterator, QObject*parent = 0);
    ~BrowserClient();

    void handle_connect(const boost::system::error_code& error);
    void handle_handshake(const boost::system::error_code& error);
    void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred);
    void relay_to_browser(char* buffer, size_t bytes_transferred);
    void start_read();
    void start_write();
    void check_deadline(deadline_timer* deadline);
    void await_output();
    void deliver(const std::string& msg);
    bool stopped() const;
    void stop();
    void handle_b2p_read_timer();
    void handle_ping_timer();
    void m_thread_receiver();
public slots:

    void relay_to_server();
private:
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_remote_socket;
    boost::posix_time::seconds m_interval;			// 1 second
    deadline_timer  m_ping_timer;
    char m_send_buff[BUFFER_SIZE];
    char m_read_buff[BUFFER_SIZE];
    char m_ping_buff[BROWSER_PACKET_OVERHEAD];
    QUdpSocket *m_b2p_sock;
    QUdpSocket *m_p2b_sock;

    std::deque<std::string> output_queue_;
    deadline_timer  non_empty_output_queue_;
    deadline_timer  m_b2p_read_timer;

};

#endif // BROWSERCLIENT_H
