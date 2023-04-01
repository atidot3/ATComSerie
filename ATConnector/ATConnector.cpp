#include <coroutine>
#include <iostream>
#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind/bind.hpp>

#include "ATDevice.h"

boost::asio::awaitable<int> async_main(const int& ac, const char** av, boost::asio::thread_pool& pool)
{
    // Rand seed initialize
    srand(static_cast<unsigned int>(time(NULL)));

    boost::system::error_code ec;
    FakeATDevice device(pool.get_executor(), "com3", 8600);
    co_await device.connect(ec);
    if (ec)
    {
        std::cerr << "Error while connecting to the ATDevice COM port: " << device.GetComPort() << " " << ec.message() << "\n";
        co_return -1;
    }

    auto res = co_await device.sendCommand(ec, "AT\r\n");
    if (ec)
    {
        std::cerr << "Error while sending command to the ATDevice COM port: " << device.GetComPort() << " " << ec.message() << "\n";
        co_return -1;
    }

    // do whatever you want with res
    // send it to you at command handler or whatso ever
    // make a ATCommand response class [ ... ]
    const std::string_view response_data(res.begin(), res.end());
    std::cout << "ATResponse: " << response_data << std::endl;

    co_await device.disconnect();

    co_return 0;
}

int main(const int& ac, const char** av)
{
    boost::asio::thread_pool pool{ 8 };

    try
    {
        // install sighandlers
        boost::asio::signal_set signals(pool, SIGINT, SIGTERM);
        signals.async_wait(boost::bind(&boost::asio::thread_pool::stop, &pool));
        boost::asio::co_spawn(pool, async_main(ac, av, pool), boost::asio::detached);
        pool.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        pool.join();
    }

    return 0;
}