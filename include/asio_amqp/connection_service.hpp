#pragma once
#include <asio_amqp/async_resolve_and_connect.hpp>
#include <asio_amqp/config.hpp>
#include <asio_amqp/error.hpp>

#include <boost/asio.hpp>
#include <asio_amqp/connection_impl.hpp>
#include <thread>
#include <memory>

namespace asio_amqp {
    
    struct connection_service
    : asio::detail::service_base<connection_service>
    {
        using protocol_type = connection_impl::protocol_type;
        using socket_type = connection_impl::socket_type;
        using resolver_type = connection_impl::resolver_type;
        using query_type = connection_impl::query_type;
        
        using impl_type = connection_impl;
        using impl_ptr_type = std::shared_ptr<impl_type>;
        
        using connect_result_type = connection_impl::connect_result_type;
        
        
        connection_service(asio::io_service& client_dispatcher)
        : asio::detail::service_base<connection_service>(client_dispatcher)
        {
            _service_thread = std::thread(&connection_service::run, this);
        }
        
        auto create()
        {
            return std::make_shared<impl_type>(_service_dispatcher);
        }
        
        system::error_code cancel(const impl_ptr_type& impl, system::error_code& ec)
        {
            if (not zombie_check(impl, ec))
            {
                return impl->cancel(ec);
            }
            return ec;
        }
        
        system::error_code close(const impl_ptr_type& impl, system::error_code& ec)
        {
            if (not zombie_check(impl, ec))
            {
                impl->close(ec);
            }
            return ec;
        }
        
        asio::io_service& service_dispatcher() {
            return _service_dispatcher;
        }
        
        
    private:
        virtual void shutdown_service() override
        {
            _service_dispatcher.stop();
            if (_service_thread.joinable()) {
                _service_thread.join();
            }
        }
        
        void run();
        
        system::error_code& zombie_check(const impl_ptr_type& impl,
                                         system::error_code& ec = system::throws)
        {
            if (!impl.get()) {
                return assign_error(ec, logic_error_code::zombie);
            }
            ec.clear();
            return ec;
        }
        
    private:
        asio::io_service _service_dispatcher;
        asio::io_service::work _service_work { _service_dispatcher };
        std::thread _service_thread;
    };
}