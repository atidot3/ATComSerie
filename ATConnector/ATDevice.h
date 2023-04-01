#pragma once

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/use_awaitable.hpp>


// this is just a simple class used to make unit tests you dont really need to make any inherance, just make a concept.
class IATDevice
{
public:
    IATDevice(boost::asio::any_io_executor exec, const std::string& portName, unsigned int baudRate)
        : _exec{ exec }
        , serialPort_{ _exec }
        , portName_{ portName }
        , baudRate_{ baudRate }
    {

    }

    const std::string_view GetComPort() const { return portName_; }
    virtual boost::asio::awaitable<void> connect(boost::system::error_code& ec) = 0;
    virtual boost::asio::awaitable<std::vector<char>> sendCommand(boost::system::error_code& ec, const std::string& command) = 0;
    virtual boost::asio::awaitable<void> disconnect() = 0;

protected:
    boost::asio::any_io_executor _exec;
    boost::asio::serial_port serialPort_;
    std::string portName_;
    unsigned int baudRate_;
};

class FakeATDevice : public IATDevice
{
public:
    FakeATDevice(boost::asio::any_io_executor exec, const std::string& portName, unsigned int baudRate)
        : IATDevice{ exec, portName, baudRate }
    {
    }

    virtual boost::asio::awaitable<void> connect(boost::system::error_code& ec) override
    {
        auto rand_val = rand() % 100 + 1;
        if (rand_val > 50)
        {
            std::cout << "fake connection success[" << rand_val << "]\n";
        }
        else
        {
            std::cout << "fake connection failed[" << rand_val << "]\n";
            ec = boost::asio::error::connection_refused;
        }

        co_return;
    }

    virtual boost::asio::awaitable<std::vector<char>> sendCommand(boost::system::error_code& ec, const std::string& command) override
    {
        auto rand_val = rand() % 100 + 1;
        if (rand_val > 50)
        {
            std::cout << "fake send success[" << rand_val << "]\n";
            const std::string fake_response_command = "+COPS: (2,\"RADIOLINJA\",\"RL\",\"24405\"),(0,\"TELE\",\"TELE\",\"24491\")\0";
            std::vector<char> vect(fake_response_command.length());
            vect.assign(fake_response_command.begin(), fake_response_command.end());

            co_return vect;
        }
        else
        {
            std::cout << "fake send failed[" << rand_val << "]\n";
            ec = boost::asio::error::connection_aborted;
        }

        co_return std::vector<char>();
    }

    virtual boost::asio::awaitable<void> disconnect() override
    {
        co_return;
    }
};

class ATDevice : public IATDevice
{
public:
    ATDevice(boost::asio::any_io_executor exec, const std::string& portName, unsigned int baudRate)
        : IATDevice{ exec, portName, baudRate }
    {
    }

    virtual boost::asio::awaitable<void> connect(boost::system::error_code& ec) override
    {
        try
        {
            serialPort_.open(portName_, ec);
            if (ec)
            {
                // failed to connect
                co_return;
            }

            // connection success, set transfert rate
            serialPort_.set_option(boost::asio::serial_port_base::baud_rate(baudRate_));
            co_return;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error while connecting: " << e.what() << std::endl;
            ec = boost::asio::error::connection_refused;
            co_return;
        }
    }

    virtual boost::asio::awaitable<std::vector<char>> sendCommand(boost::system::error_code& ec, const std::string& command) override
    {
        try
        {
            boost::system::error_code ec;
            std::vector<char> response(1024);

            auto [ec_write, bytes_sent] = co_await boost::asio::async_write(serialPort_, boost::asio::buffer(command), boost::asio::as_tuple(boost::asio::use_awaitable));
            if (ec_write)
            {
                // failed to read, connection is most likely dropped
                ec = ec_write;
                co_return response;
            }
            
            auto [ec_read, bytesRead] = co_await boost::asio::async_read_until(serialPort_, boost::asio::dynamic_buffer(response), "\r\nOK\r\n", boost::asio::as_tuple(boost::asio::use_awaitable));
            if (ec_read)
            {
                // failed to read, connection is most likely dropped
                ec = ec_read;
                co_return response;
            }
            response.resize(bytesRead - 6);
            co_return response;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error while sending command: " << e.what() << std::endl;
            ec = boost::asio::error::connection_aborted;
            co_return std::vector<char>();
        }
    }

    virtual boost::asio::awaitable<void> disconnect() override
    {
        try
        {
            serialPort_.close();
            co_return;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error while disconnecting: " << e.what() << std::endl;
            co_return;
        }
    }
};