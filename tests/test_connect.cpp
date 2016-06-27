#include <gtest/gtest.h>
#include <asio_amqp/connection.hpp>
#include <asio_amqp/channel.hpp>
#include <future>
#include <condition_variable>
#include <chrono>
#include <exception>
#include <boost/variant.hpp>
#include <valuelib/debug/unwrap.hpp>


using namespace std::literals;
static constexpr auto default_timeout = 360s;
using DefaultDuration = decltype(default_timeout);

template<class Duration = DefaultDuration>
::testing::AssertionResult tick(asio_amqp::asio::io_service& io_service, Duration timeout = default_timeout)
{
    using clock = std::chrono::high_resolution_clock;
    using namespace std::literals;
    
    auto first = clock::now();
    auto last = first + timeout;
    auto ticks = 0;
    do
    {
        try {
            if (io_service.stopped()) {
                io_service.reset();
            }
            ticks += io_service.poll_one();
        }
        catch(...) {
            return ::testing::AssertionFailure() << value::debug::unwrap(std::current_exception());
        }
    } while (not ticks and (clock::now() < last));

    if (ticks)
        return ::testing::AssertionSuccess();
    else
        return ::testing::AssertionFailure() << "timeout";
}

template<class F>
testing::AssertionResult
no_exception(F&& f)
{
    try {
        f();
        return testing::AssertionSuccess();
    }
    catch(...)
    {
        return testing::AssertionFailure() << value::debug::unwrap(std::current_exception());
        
    }
}

template<class F>
testing::AssertionResult
gives_system_error(asio_amqp::system::error_code ec, F&& f)
{
    try {
        f();
        return testing::AssertionFailure() << "no exception thrown";
    }
    catch(const asio_amqp::system::system_error& err)
    {
        if (err.code() == ec)
            return testing::AssertionSuccess();
        return testing::AssertionFailure() << "wrong error code: " << value::debug::unwrap(err);
    }
    catch(...)
    {
        return testing::AssertionFailure() << "wrong exception type: " << value::debug::unwrap(std::current_exception());
    }
    
}

template<class Exception, class F>
testing::AssertionResult
throws_exception(F&& f)
{
    try {
        f();
        return testing::AssertionFailure() << "no exception thrown";
    }
    catch(const Exception& err)
    {
        return testing::AssertionSuccess();
    }
    catch(...)
    {
        return testing::AssertionFailure() << "wrong exception type: " << value::debug::unwrap(std::current_exception());
    }
    
}

template<class T, class...Args>
void emplace_self(T& t, Args&&...args) {
    t = T(std::forward<Args>(args)...);
}


TEST(test_connection, basic_test)
{
    asio_amqp::asio::io_service io_service;
    asio_amqp::connection conn(io_service);
    
    asio_amqp::future<asio_amqp::connection::connect_result_type> shared_result;
    auto connect_handler = [&](auto& result) mutable {
        shared_result = std::move(result);
    };
    conn.async_connect_transport(asio_amqp::connection::query_type("localhost", "5672"), connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE(no_exception([&]{shared_result.get();}));
}

TEST(test_connection, resolve_failure)
{
    asio_amqp::asio::io_service io_service;
    asio_amqp::connection conn(io_service);
    
    asio_amqp::future<asio_amqp::connection::connect_result_type> shared_result;
    auto connect_handler = [&](auto& result) mutable {
        shared_result = std::move(result);
    };
    conn.async_connect_transport(asio_amqp::connection::query_type("nonexistentaddress.local", "5672"), connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE((throws_exception<asio_amqp::resolve_failure>([&]{shared_result.get();})));
}

TEST(test_connection, logon_failure)
{
    asio_amqp::asio::io_service io_service;
    asio_amqp::connection conn(io_service);
    
    asio_amqp::future<asio_amqp::connection::connect_result_type> shared_result;
    auto connect_handler = [&](auto& result) mutable {
        shared_result = std::move(result);
    };
    conn.async_connect_transport(asio_amqp::connection::query_type("localhost", "5672"), connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE(no_exception([&]{shared_result.get();}));
    
    emplace_self(shared_result);
    conn.async_connect(AMQP::Login("secrtest", "secrtestwrongpassword"), "secrtest",
                        connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE(throws_exception<asio_amqp::connection_failure>([&]{shared_result.get();}));
}

TEST(test_connection, create_channel)
{
    asio_amqp::asio::io_service io_service;
    asio_amqp::connection conn(io_service);
    
    asio_amqp::future<asio_amqp::connection::connect_result_type> shared_result;
    auto connect_handler = [&](auto& result) mutable {
        shared_result = std::move(result);
    };
    conn.async_connect_transport(asio_amqp::connection::query_type("localhost", "5672"), connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE(no_exception([&]{shared_result.get();}));
    
    emplace_self(shared_result);
    conn.async_connect(AMQP::Login("secrtest", "secrtest"), "secrtest",
                       connect_handler);
    ASSERT_TRUE(tick(io_service));
    ASSERT_TRUE(shared_result.valid());
    ASSERT_TRUE(no_exception([&]{shared_result.get();}));
    
    asio_amqp::channel chan(io_service, conn);
    chan.async_open([](asio_amqp::future<unsigned int>& channel_id) {
        
    });
    auto open_channel_handler = [](std::future<unsigned int>& result)
    {
    };
    

    
}