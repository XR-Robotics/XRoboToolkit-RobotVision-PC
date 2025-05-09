//Network data receiver implementation for camera streaming using ASIO
#include "CameraDataReceiver.h"

CameraDataReceiver::CameraDataReceiver(const std::string& ip, int port)
    : m_ip(ip), m_port(port), m_acceptor(m_io_context, asio::ip::tcp::endpoint(asio::ip::make_address(m_ip), m_port)), m_should_exit(false) {
}

void CameraDataReceiver::run(DataCallback callback) {
    m_dataCallback = callback;
    std::cout << "CameraDataReceiver::run() started" << std::endl;

    // Start accepting connections
    acceptConnections();

    // Start IO context event loop
    m_io_context.run();
    std::cout << "CameraDataReceiver: io_context run completed" << std::endl;
}

void CameraDataReceiver::stop() {
    std::cout << "CameraDataReceiver::stop() called, setting exit flag" << std::endl;
    m_should_exit = true;

    try {
        if (m_acceptor.is_open()) {
            asio::error_code ec;
            // First cancel all pending async operations
            m_acceptor.cancel(ec);
            m_acceptor.close(ec);
            if (ec) {
                std::cerr << "Error closing acceptor: " << ec.message() << std::endl;
            }
            std::cout << "CameraDataReceiver: acceptor closed" << std::endl;
        }

        // Stop io_context, this will cause all unfinished async operations to be cancelled
        m_io_context.stop();
        std::cout << "CameraDataReceiver: io_context stopped" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during stop: " << e.what() << std::endl;
    }
}

void CameraDataReceiver::acceptConnections() {
    if (m_should_exit) {
        std::cout << "CameraDataReceiver::acceptConnections() exiting due to stop flag" <<std::endl;
        return;
    }

    auto socket = std::make_shared<asio::ip::tcp::socket>(m_io_context);

    m_acceptor.async_accept(*socket, [this, socket](const asio::error_code& error) {
        if (!error) {
            std::cout << "Client connected" << std::endl;
            handleClient(socket);
        } else {
                std::cout << "Accept error: " << error.message() << std::endl;
        }

        if (!m_should_exit) {
            acceptConnections();
        } else {
            std::cout << "CameraDataReceiver: stopping accept loop due to stop flag" << std::endl;
        }
    });
}

void CameraDataReceiver::handleClient(std::shared_ptr<asio::ip::tcp::socket> socket) {
    while (socket->is_open() && !m_should_exit) {
        try {
            // First receive data length (4-byte uint32_t)
            uint32_t dataLength = 0;
            asio::error_code error;

            // Read fixed length header information
            size_t header_bytes_read = asio::read(*socket, asio::buffer(&dataLength, sizeof(dataLength)), error);

            if (error) {
                std::cout << "Error reading length: " << error.message() << std::endl;
                break;
            }

            // Create buffer based on received length
            std::vector<char> buffer(dataLength);

            // Read data of specified length
            size_t bytes_read = asio::read(*socket, asio::buffer(buffer), error);

            if (error) {
                std::cout << "Error reading data: " << error.message() << std::endl;
                break;
            }

            m_dataCallback(buffer.data(), bytes_read);
        }
        catch (const std::exception& e) {
            std::cerr << "Error handling client: " << e.what() << std::endl;
        }
    }

    // Socket will be automatically closed when shared_ptr is destructed
}
