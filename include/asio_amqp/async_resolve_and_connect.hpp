#pragma once
#include <asio_amqp/config.hpp>
#include <asio_amqp/error.hpp>

#include <memory>
#include <string>
#include <future>



namespace asio_amqp
{
    template<class T>
    struct deferred_result
    {
        using value_type = T;
        using handler_sig = void(std::future<value_type>&);
        using handler_func = std::function<handler_sig>;
        
        deferred_result(handler_func func)
        : _handler(std::move(func))
        {}
        
        void set_exception(std::exception_ptr ep)
        {
            _promise.set_exception(std::move(ep));
            _handler(_future);
        }
        
        template<
        class Arg,
        std::enable_if_t<std::is_convertible<Arg, value_type>::value> * = nullptr
        >
        void set_value(Arg&& value)
        {
            _promise.set_value(std::forward<Arg>(value));
            _handler(_future);
        }
        
        template<class Handler>
        void set_handler(Handler&& handler)
        {
            _handler = handler_func(std::forward<Handler>(handler));
        }
        
        std::promise<T> _promise;
        std::future<T> _future { _promise.get_future() };
        handler_func _handler;
    };
    
    
    template<> struct deferred_result<void>
    {
        using value_type = void;
        using handler_sig = void(std::future<value_type>&);
        using handler_func = std::function<handler_sig>;
        
        deferred_result(handler_func func = {})
        : _handler(std::move(func))
        {}
        
        void set_exception(std::exception_ptr ep)
        {
            _promise.set_exception(std::move(ep));
            _handler(_future);
        }
        
        void set_value()
        {
            _promise.set_value();
            _handler(_future);
        }
        
        template<class Handler>
        void set_handler(Handler&& handler)
        {
            _handler = handler_func(std::forward<Handler>(handler));
        }
        
        std::promise<value_type> _promise;
        std::future<value_type> _future { _promise.get_future() };
        handler_func _handler;
    };
    

    template<class T, class...Args>
    std::exception_ptr make_failure(const boost::system::error_code& ec,
                                    Args&&...args)
    {
        try {
            throw boost::system::system_error(ec, ec.message());
        }
        catch(...)
        {
            try {
                std::throw_with_nested(T(std::forward<Args>(args)...));
            }
            catch(...)
            {
                return std::current_exception();
            }
        }
        return {};
    }

    
    
    struct resolve_and_connect_op
    : std::enable_shared_from_this<resolve_and_connect_op>
    {
        using async_result_type = deferred_result<void>;
        
        using protocol_type = asio::ip::tcp;
        using socket_type = protocol_type::socket;
        using resolver_type = protocol_type::resolver;
        using query_type = resolver_type::query;
        using iterator_type = resolver_type::iterator;
        
        resolve_and_connect_op(socket_type& socket,
                               query_type query)
        : _query(std::move(query))
        , _resolver(socket.get_io_service())
        , _socket(std::addressof(socket))
        {}
        
        template<class Handler>
        void run(Handler&& handler)
        {
            _result.set_handler(std::forward<Handler>(handler));
            _resolver.async_resolve(_query,
                                    [this, self = this->shared_from_this()]
                                    (auto const& ec, auto iter)
                                    {
                                        this->handle_resolve(ec, iter);
                                    });
        }
        
        void handle_resolve(boost::system::error_code const& ec, iterator_type iter)
        {
            if (ec) {
                auto context = "resolving " + _query.host_name() + ':'
                + _query.service_name();
                _result.set_exception(make_failure<resolve_failure>(ec,
                                                                    std::move(context)));
            }
            else {
                attempt_connect(iter);
            }
        }
        
        void attempt_connect(iterator_type iter)
        {
            if (iter == iterator_type())
            {
                _result.set_exception(exhausted_error());
            }
            else {
                _socket->open(iter->endpoint().protocol());
                _socket->set_option(asio::ip::tcp::no_delay(true));
                _socket->async_connect(iter->endpoint(),
                                       [this, self = this->shared_from_this(),
                                        iter]
                                       (auto const& ec)
                                       {
                                           this->handle_connect_attempt(ec, iter);
                                       });
            }
        }
        
        void handle_connect_attempt(boost::system::error_code const& ec,
                                    iterator_type iter)
        {
            if (ec) {
                _endpoints.emplace_back(iter->endpoint(), ec);
                boost::system::error_code sink;
                _socket->close(sink);
                attempt_connect(++iter);
            }
            else {
                _result.set_value();
            }
        }
        
        std::exception_ptr exhausted_error() const
        {
            return std::make_exception_ptr(std::runtime_error(connection_results_as_string()));
        }
        
        std::string connection_results_as_string() const
        {
            if (_endpoints.empty())
            {
                return { "no endpoints resolved" };
            }
            else {
                std::stringstream ss("unable to connect to the following endpoints: ");
                auto sep = "";
                for (auto const& pair : _endpoints)
                {
                    auto const& endpoint = pair.first;
                    auto const& reason = pair.second;
                    ss << sep << '(' << endpoint.address().to_string() << ':'
                    << endpoint.port() << "; " << reason.message() << ')';
                    sep = ", ";
                }
                return ss.str();
            }
        }
        
        
        
        
        query_type _query;
        resolver_type _resolver;
        socket_type* _socket;
        async_result_type _result;
        
        using attempt = std::pair<protocol_type::endpoint, system::error_code>;
        std::vector<attempt> _endpoints;
    };
    
    template<class Handler>
    void async_resolve_and_connect(asio::ip::tcp::socket& socket,
                                   asio::ip::tcp::resolver::query query,
                                   Handler&& handler)
    {
        auto p = std::make_shared<resolve_and_connect_op>(socket,
                                                          std::move(query));
        p->run([p, handler = std::move(handler)](std::future<void>& f) mutable
               {
                   handler(f);
               });
    }
}

