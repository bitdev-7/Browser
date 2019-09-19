#include "browserclient.h"

BrowserClient::BrowserClient(boost::asio::io_service& io_service, boost::asio::ssl::context& context, boost::asio::ip::tcp::resolver::iterator endpoint_iterator, QObject*parent)
    :  QObject(parent),m_remote_socket(io_service, context), m_interval(PING_SENDING_PERIOD),
      m_ping_timer(io_service, m_interval),
    non_empty_output_queue_(io_service),
    m_b2p_read_timer(io_service)
{
    m_p2b_sock = new QUdpSocket(this);
    m_b2p_sock = new QUdpSocket(this);

    if (m_b2p_sock->bind(QHostAddress::LocalHost, LOCAL_IPC_B2P_PORT)) {
        connect(m_b2p_sock, SIGNAL(readyRead()), this, SLOT(relay_to_server()));
        m_b2p_read_timer.expires_from_now(boost::posix_time::milliseconds(100));
        m_b2p_read_timer.async_wait(boost::bind(&BrowserClient::handle_b2p_read_timer, this));
    } else {
        NOTIFY_MESSAGE("Browser binding failed");
    }

    memset(&m_ping_buff[0], 0, BROWSER_PACKET_OVERHEAD);

    // The non_empty_output_queue_ deadline_timer is set to pos_infin whenever
    // the output queue is empty. This ensures that the output actor stays
    // asleep until a message is put into the queue.
    non_empty_output_queue_.expires_at(boost::posix_time::pos_infin);
    boost::asio::async_connect(m_remote_socket.lowest_layer(), endpoint_iterator,
                               boost::bind(&BrowserClient::handle_connect, this, boost::asio::placeholders::error));

}
BrowserClient::~BrowserClient()
{

}

void BrowserClient::handle_connect(const boost::system::error_code& error)
{
    if (!error) m_remote_socket.async_handshake(boost::asio::ssl::stream_base::client,
                  boost::bind(&BrowserClient::handle_handshake, this, boost::asio::placeholders::error));
    else stop();
}

void BrowserClient::handle_handshake(const boost::system::error_code& error)
{
    if (!error)
    {

        NOTIFY_MESSAGE("Connected to the server");

        // after handshaking established, send verification request

        char hostname[128] = { 0 };
        gethostname(&hostname[0], 128);
        sprintf_s(m_send_buff, BUFFER_SIZE, "GET /browser?uuid=%s HTTP/1.1\r\nHost: intelstar.rdp.com\r\n\r\n", hostname);

        output_queue_.push_back(m_send_buff);
        m_ping_timer.async_wait(boost::bind(&BrowserClient::handle_ping_timer,this));
        start_read();

        await_output();
    }
    else
    {
        stop();
    }
}
void BrowserClient::handle_ping_timer()
{
    NOTIFY_MESSAGE("Ping Send");
    deliver(m_ping_buff);
    m_ping_timer.expires_from_now(boost::posix_time::seconds(m_interval));
    m_ping_timer.async_wait(boost::bind(&BrowserClient::handle_ping_timer,this));
}

void BrowserClient::start_read()
{
    // Set a deadline for the read operation.
    NOTIFY_MESSAGE(std::string("Relay to browser: "));

    m_remote_socket.async_read_some(boost::asio::buffer(m_read_buff, BUFFER_SIZE),
        boost::bind(&BrowserClient::handle_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BrowserClient::start_write()
{

    // Start an asynchronous operation to send a message.
   // NOTIFY_MESSAGE(output_queue_.front() + "  " +std::to_string(output_queue_.front().size()));
    m_remote_socket.async_write_some(boost::asio::buffer(output_queue_.front() , output_queue_.front().size()),
        boost::bind(&BrowserClient::handle_write, this , boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void BrowserClient::await_output()
{
    if (stopped())
        return;
    if (output_queue_.empty())
    {
        non_empty_output_queue_.expires_at(boost::posix_time::pos_infin);
        non_empty_output_queue_.async_wait(
            boost::bind(&BrowserClient::await_output, this));
    }
    else
    {
        start_write();
    }
}

void BrowserClient::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
{
    if (stopped())
    {
        return;
    }
    if (!error)
    {
        NOTIFY_MESSAGE("handle_write: bytes_sent=" + std::to_string(bytes_transferred));
        output_queue_.pop_front();
        await_output();
    }
    else
    {
        stop();
    }
}

void BrowserClient::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{    
     if (stopped())
     {
         return;
     }

     if (!error)
     {
        NOTIFY_MESSAGE("handle_read: bytes_transfered=" + std::to_string(bytes_transferred));
        relay_to_browser(m_read_buff, bytes_transferred);  // send read buff to browser
        start_read();
    }
    else
    {
      stop();
    }
}

void BrowserClient::handle_b2p_read_timer()
{
    m_b2p_sock->waitForReadyRead(10);

    m_b2p_read_timer.expires_from_now(boost::posix_time::milliseconds(100));
    m_b2p_read_timer.async_wait(boost::bind(&BrowserClient::handle_b2p_read_timer, this));
}

void BrowserClient::stop()
{

    non_empty_output_queue_.cancel();
    m_remote_socket.get_io_service().stop();

}

bool BrowserClient::stopped() const
{
    return !m_remote_socket.lowest_layer().is_open();
}

void BrowserClient::relay_to_browser(char* buffer, size_t bytes_transferred)
{
    NOTIFY_MESSAGE(std::string("Relay to browser: ") + std::to_string(bytes_transferred));
    if (m_p2b_sock->writeDatagram(buffer, bytes_transferred, QHostAddress::LocalHost, LOCAL_IPC_P2B_PORT) == -1)
        NOTIFY_MESSAGE("Relay to browser failed");

}

void BrowserClient::relay_to_server()   //send buffer from browser to Server
{
    while (m_b2p_sock->hasPendingDatagrams())
    {
        qint64 bytes_to_read = m_b2p_sock->pendingDatagramSize();
        QByteArray buffer;
        buffer.resize(bytes_to_read);
        if (m_b2p_sock->readDatagram(buffer.data(), bytes_to_read, NULL, NULL) != -1)
        {
            char *browser_buff = buffer.data();
            // relay the cmd bytes
            NOTIFY_MESSAGE(std::string("Relay to Server: ") + std::to_string(bytes_to_read));
            std::string str(browser_buff, bytes_to_read);
            deliver(str);
        }
        else NOTIFY_MESSAGE("Relay to Server failed");
    }
}

void BrowserClient::deliver(const std::string& msg)
{   // put message into output_queue and awake m_non_empty_output_queue
    output_queue_.push_back(msg);
    non_empty_output_queue_.expires_at(boost::posix_time::neg_infin);
}
