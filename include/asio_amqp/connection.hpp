#pragma once
#include <asio_amqp/config.hpp>

#include <future>

#include <asio_amqp/connection_service.hpp>
#include <asio_amqp/future.hpp>

namespace asio_amqp {
    struct connection
    {
        using service_type = connection_service;
        using impl_type = service_type::impl_type;
        using impl_ptr_type = service_type::impl_ptr_type;

        using protocol_type = service_type::protocol_type;
        using socket_type = service_type::socket_type;
        using resolver_type = service_type::resolver_type;
        using query_type = service_type::query_type;
        
        using connect_result_type = service_type::connect_result_type;
        
        
        connection(asio::io_service& io_service)
        : _service { std::addressof(asio::use_service<connection_service>(io_service)) }
        , _impl { get_service().create() }
        {
        }
        
        // rule of 5
        connection(const connection&) = delete;
        connection(connection&& r) = default;
        connection& operator=(const connection&) = delete;
        connection& operator=(connection&& r) = default;
        ~connection() noexcept
        {
            system::error_code sink;
            close(sink);
        }
        
        // utility
        

        // connect
        template<class Handler>
        void async_connect_transport(query_type&& query, Handler&& handler)
        {
            auto deferred_handler = make_future_handler<connect_result_type>(get_io_service(),
                                                                             std::forward<Handler>(handler));
            if (!_impl)
            {
                deferred_handler(system::system_error(logic_error_code::zombie));
            }
            else
            {
                _impl->async_connect_transport(std::move(query), std::move(deferred_handler));
            }
        }

        template<class Handler>
        void async_connect(AMQP::Login login,
                           std::string vhost, Handler&& handler)
        {
            auto deferred_handler = make_future_handler<connect_result_type>(get_io_service(),
                                                                             std::forward<Handler>(handler));
            if (!_impl)
            {
                deferred_handler(system::system_error(logic_error_code::zombie));
            }
            else
            {
                _impl->async_connect(std::move(login),
                                     std::move(vhost),
                                     std::move(deferred_handler));
            }
        }
        
        // cancel all outstanding handlers
        system::error_code cancel(system::error_code& ec = system::throws);

        // destroy the connection if running and release all resources
        void close(system::error_code& ec = system::throws);
        
        asio::io_service& get_io_service()
        {
            return get_service().get_io_service();
        }
        
        impl_ptr_type const& get_impl_ptr() const {
            return _impl;
        }

        connection_service& get_service() noexcept {
            return *_service;
        }

    private:
        impl_type& get_implementation() noexcept {
            return *_impl;
        }
        
        
    private:
        connection_service* _service;
        impl_ptr_type _impl;
    };
    
    
    // implemantatiokn
    
    
    
    system::error_code connection::cancel(system::error_code& ec)
    {
        if(_impl)
        {
            get_service().cancel(_impl, ec);
        }
        else {
            assign_error(ec, logic_error_code::zombie);
        }
        return ec;
    }
    
    void connection::close(system::error_code& ec)
    {
        if (_impl)
        {
            cancel(ec);
            get_service().close(_impl, ec);
        }
    }
    
}