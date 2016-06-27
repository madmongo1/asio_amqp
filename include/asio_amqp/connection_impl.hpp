#pragma once
#include <asio_amqp/config.hpp>
#include <asio_amqp/future.hpp>
#include <amqpcpp.h>

#include <asio_amqp/detail/sender.hpp>
#include <asio_amqp/detail/receiver.hpp>

#include <memory>
#include <mutex>
#include <valuelib/stdext/invoke.hpp>


namespace asio_amqp {
    
    struct connection_failure : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    
    struct connection_impl
    : ::AMQP::ConnectionHandler
    , std::enable_shared_from_this<connection_impl>
    {
        using protocol_type = asio::ip::tcp;
        using socket_type = protocol_type::socket;
        using resolver_type = protocol_type::resolver;
        using query_type = resolver_type::query;
        
        using connect_result_type = void;
        
        enum class state_type {
            stopped,
            resolving,
            transport_up,
            connecting,
            connected,
            error
        };
        
        connection_impl(asio::io_service& service_dispatcher)
        : _socket(service_dispatcher)
        {}
        
        virtual ~connection_impl() = default;
        
        bool is_open() {
            return _socket.is_open();
        }
        
        system::error_code close(system::error_code& ec) {
            return _socket.close(ec);
        }
        
        system::error_code cancel(system::error_code& ec) {
            return _socket.cancel(ec);
        }
        
        template<class Handler>
        void async_connect_transport(query_type&& query, Handler&& handler)
        {
            post_self([this,
                       query = std::move(query),
                       handler = std::move(handler)] () mutable
            {
                this->impl_async_connect_transport(std::move(query),
                                             std::move(handler));
            });
            
        }
        
        template<class Handler>
        void async_connect(AMQP::Login&& login, std::string&& vhost,
                                     Handler&& handler)
        {
            _socket.get_io_service().post([self = this->shared_from_this(),
                                           login = std::move(login),
                                           vhost = std::move(vhost),
                                           handler = std::move(handler)] () mutable
            {
                auto lock = self->get_lock();
                self->impl_async_connect(std::move(login),
                                                   std::move(vhost),
                                                   std::move(handler));
            });
        }
        
        
                                      
        
        
        socket_type& socket() {
            return _socket;
        }
        
        
        

        using mutex_type = std::recursive_mutex;
        using lock_type = std::unique_lock<mutex_type>;

        lock_type get_lock() const {
            return lock_type(_mutex);
        }
        
        AMQP::Connection* connection_ptr() const {
            return _connection.get();
        }
        
        
        template<class F>
        void post_self(F&& f)
        {
            _socket.get_io_service().post([this,
                                           self = this->shared_from_this(),
                                           f = std::move(f)] () mutable
                                          {
                                              value::stdext::invoke(f);
                                          });
        }

    private:
        
        /// @pre this has a valid shared reference
        template<class Handler>
        void impl_async_connect_transport(query_type&& query,
                                          Handler&& handler)
        {
            auto lock = get_lock();
            if (_state == state_type::stopped)
            {
                _state = state_type::resolving;
                async_resolve_and_connect(this->socket(),
                                          std::move(query),
                                          [this,
                                           self = this->shared_from_this(),
                                           handler = std::move(handler)]
                                          (auto& result) mutable
                                          {
                                              this->impl_handle_transport_connect(result,
                                                                                  std::move(handler));
                                          });
            }
            else
            {
                handler(system::system_error(logic_error_code::wrong_state_for_connect));
            }
        }
        
        /// @pre lock.owns_lock() == true
        /// @pre result.valid() == true
        template<class Handler>
        void impl_handle_transport_connect(std::future<void>& result,
                                           Handler&& handler)
        {
            assert(result.valid());
            try {
                result.get();
                auto lock = get_lock();
                _state = state_type::transport_up;
                lock.unlock();
                handler();
            }
            catch(...)
            {
                auto lock = get_lock();
                _state = state_type::error;
                lock.unlock();
                handler(std::current_exception());
            }
        }

        


        /// @pre lock is already taken
        template<class Handler>
        void impl_async_connect(AMQP::Login&& login, std::string&& vhost,
                                          Handler&& handler)
        {
            auto lock = get_lock();
            switch(_state)
            {
                case state_type::transport_up: {
                    _state = state_type::connecting;
                    _connect_handler = std::move(handler);
                    _connection = std::make_unique<AMQP::Connection>(this,
                                                                     login,
                                                                     vhost);
                    expect_response();
                } break;

                default:
                    handler(system::system_error(logic_error_code::wrong_state_for_connect));
                    break;
            }
        }

        
        /// @pre mutex is taken
        void onData(AMQP::Connection *connection, const char *buffer, size_t size) override
        {
            _sender.queue_for_send(buffer, buffer + size);
        }
        
        /// @pre mutex is taken
        void onConnected(AMQP::Connection *connection) override
        {
            assert(_state == state_type::connecting);
            auto copy = decltype(_connect_handler)();
            std::swap(copy, _connect_handler);
            copy();
        }
        
        /// @pre mutex is taken
        void onClosed(AMQP::Connection* connection) override
        {
            assert(false);
        }
        
        
        void onError(AMQP::Connection *connection, const char* message) override
        {
            if (_state == state_type::connecting)
            {
                auto exec = std::move(_connect_handler);
                exec(connection_failure(message));
            }
        }
        
        void expect_response()
        {
            if (_connection && _connection->waiting() && !_receiver.busy())
            {
                _receiver.async_read(_socket,
                                     [this, self = shared_from_this()](const auto& ec,
                                                                       auto received)
                                 {
                                     this->handle_read(ec, received);
                                 });
            }
        }
        
        void handle_read(system::error_code const& ec, std::size_t bytes_read)
        {
            auto lock = get_lock();
            if (ec) {
                _connection->close();
            }
            else {
                for(;;)
                {
                    auto buffer = _receiver.data();
                    auto data = asio::buffer_cast<const char*>(buffer);
                    auto length = asio::buffer_size(buffer);
                    auto consumed = _connection->parse(data, length);
                    if (consumed) {
                        _receiver.consume(consumed);
                    }
                    else {
                        break;
                    }
                }
                expect_response();
            }
            
        }
        
        
        template<class F, class...Args>
        auto with_lock(F&& f, Args&&...args)
        {
            auto lock = get_lock();
            return value::stdext::invoke(std::forward<F>(f),
                                         std::forward<Args>(args)...);
        }
        
        
        mutable mutex_type _mutex;
        state_type _state = state_type::stopped;
        
        
        socket_type _socket;
        detail::sender<socket_type> _sender { _socket };
        detail::receiver _receiver;
        std::unique_ptr<AMQP::Connection> _connection;
        future_handler<void> _connect_handler;
        
        
    };
}
