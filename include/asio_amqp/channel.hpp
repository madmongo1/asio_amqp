#pragma once
#include <asio_amqp/connection.hpp>
#include <asio_amqp/future.hpp>

namespace asio_amqp {

    template<class T, class Handler>
    struct future_result
    : std::enable_shared_from_this<future_result<T, Handler>>
    {
        future_result(Handler handler)
        : _handler(std::move(handler))
        {}
        
        template<class...Ts>
        void set_value(Ts&&...ts) {
            _promise.set_value(std::forward<Ts>(ts)...);
            trigger();
        }
        
        template<class...Ts>
        void set_exception(Ts&&...ts) {
            _promise.set_exception(std::make_exception_ptr(std::forward<Ts>(ts)...));
            trigger();
        }
        
        void set_exception(std::exception_ptr pe) {
            _promise.set_exception(std::move(pe));
            trigger();
        }
        
    private:
        void trigger() {
            this->_handler(std::shared_ptr<std::future<T>>(this->shared_from_this(), &this->_future));
        }
        
        std::promise<T> _promise;
        std::future<T> _future { _promise.get_future };
        Handler _handler;
    };
    
    template<class T>
    struct handler_pair
    {
        
    };

    struct channel_impl
    : std::enable_shared_from_this<channel_impl>
    {
        channel_impl(std::shared_ptr<connection_impl> connection)
        : _connection(connection)
        {
            
        }
        
        enum class state {
            closed,
            opening,
            open,
            shutdown
        };
        
        template<class Handler>
        void async_open(Handler&& handler)
        {
            _connection->post_self([this,
                                    self = this->shared_from_this(),
                                    handler = std::move(handler)]
            {
                if (_state != state::closed) {
                    handler(std::logic_error("wrong state"));
                }
                // this happens in the context of the connection's thread
                _channel.emplace(_connection->connection_ptr());
                
            });
        }
        
        void close()
        {
            
        }
        

        std::shared_ptr<connection_impl> _connection;
        optional<AMQP::Channel> _channel;
        state _state = state::closed;
    };
    
    template<class T, class Handler>
    auto make_future_result(Handler&& handler)
    {
        using handler_type = std::decay_t<Handler>;
        using value_type = T;
        using future_result_type = future_result<value_type, handler_type>;
        return std::make_shared<future_result_type>(std::forward<Handler>(handler));
    }

    struct channel_identifier
    {
        constexpr channel_identifier(std::uint16_t ident) : _ident(ident) {}
        constexpr operator std::uint16_t() const { return _ident; }
    private:
        const std::uint16_t _ident;
    };

	struct channel
	{
        /// Open a channel on a connection which reports results to the same io_service as
        /// referenced by the connection
        channel(connection& connection)
        : _owner(std::addressof(connection.get_io_service()))
        , _connection(std::addressof(connection))
        , _impl()
        {
            
        }
        
        /// Open a channel on a connection which reports results to a given io_service
        channel(asio::io_service& io_service, connection& connection)
        : _owner(std::addressof(io_service))
        , _connection(std::addressof(connection))
        , _impl()
        {
            
        }
        
        channel(channel&&) = default;
        channel& operator=(channel&& r) = default;
        
        ~channel()
        {
            if (_impl.get()) {
                _impl->close();
                _impl.reset();
            }
        }
        
        
        template<class Handler>
        void async_open(Handler&& handler)
        {
            auto my_handler = make_future_handler<unsigned int>(get_io_service(),
                                                                std::forward<Handler>(handler));
            if (_impl.get()) {
                my_handler(std::logic_error("already open"));
            }
            else
            {
                _impl = std::make_shared<channel_impl>(_connection->get_impl_ptr());
                _impl->async_open(std::move(my_handler));
            }
        }
        
        asio::io_service& get_io_service() const {
            return get_service().get_io_service();
        }
        
        connection_service& get_service() const {
            return _connection->get_service();
        }

        asio::io_service* _owner;
		connection* _connection;
		std::shared_ptr<channel_impl> _impl;
	};
}