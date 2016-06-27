#pragma once
#include <asio_amqp/config.hpp>


namespace asio_amqp {
    
    struct not_set_type {};
    constexpr auto not_set() { return not_set_type{}; }
    
    struct value_set_type {};
    constexpr auto value_set() { return value_set_type{}; }
    
    template<class Exception, class Variant>
    struct set_exception_visitor : boost::static_visitor<void>
    {
        template<class T>
        void operator()(const T&) {
            throw std::logic_error("value already set");
        }
        
        void operator()(not_set_type)
        {
            _v = std::move(_e);
        }
        
        
        Exception&& _e;
        Variant& _v;
    };
    
    template<class Exception, class Variant>
    auto make_set_exception_visitor(Exception&& e, Variant& v)
    {
        using ex_type = std::decay_t<Exception>;
        using v_type = std::decay_t<Variant>;
        return set_exception_visitor<ex_type, v_type>(std::forward<Exception>(e),
                                                      v);
    }
    
    template<class T>
    struct extract_value : boost::static_visitor<T&>
    {
        T& operator()(T& v) const {
            return v;
        }
        
        T& operator()(const std::exception_ptr& pe) const {
            std::rethrow_exception(pe);
        }
        
        T& operator()(const not_set_type&) const {
            throw std::logic_error("value not set");
        }
        
    };
    
    template<class T>
    struct future_value_store
    {
        template<class Exception>
        void set_exception(Exception&& e)
        {
            check_not_set() = std::make_exception_ptr(std::forward<Exception>(e));
        }
        
        void set_exception(std::exception_ptr pe)
        {
            check_not_set() = std::move(pe);
        }
        
        bool valid() const {
            return is_exception() or is_value();
        }
        
        bool is_exception() const {
            return boost::get<const std::exception_ptr>(std::addressof(_data));
        }
        
        bool is_value() const {
            return boost::get<const T>(std::addressof(_data));
        }
        
        
    protected:
        T& get_impl() {
            return boost::apply_visitor(extract_value<T>(), _data);
        }
        
        future_value_store() : _data { not_set() } {}
        
        void store_value_impl(T&& t)
        {
            check_not_set() = std::forward<T>(t);
        }
        
        
    private:
        using data_type = variant<not_set_type, std::exception_ptr, T>;
        data_type _data;
        
        data_type& check_not_set()
        {
            if (boost::get<not_set_type>(&_data))
            {
                return _data;
            }
            else if (boost::get<std::exception_ptr>(&_data))
            {
                throw std::logic_error("exception already set");
            }
            else
            {
                throw std::logic_error("value already set");
            }
        }
    };
    
    template<class T>
    struct future : future_value_store<T>
    {
        void set_value(T v)
        {
            this->store_value_impl(std::move(v));
        }
        
        T& get() {
            return this->get_impl();
        }
    };
    
    template<>
    struct future<void> : future_value_store<value_set_type>
    {
        void set_value(void)
        {
            this->store_value_impl(value_set());
        }
        
        void get() {
            this->get_impl();
        }
    };
    
    template<class T>
    struct future_callback_interface
    {
        virtual void trigger() = 0;
        virtual auto promise() -> future<T>& = 0;
    };
    
    template<class T, class Handler>
    struct future_callback
    : future_callback_interface<T>
    , std::enable_shared_from_this<future_callback<T, Handler>>
    {
        future_callback(asio::io_service& io_service, Handler handler)
        : _io_service(io_service)
        , _handler(handler)
        {}
        
        void trigger() override
        {
            assert(_future.valid());
            _io_service.post([this, self = this->shared_from_this()]()
                             {
                                 _handler(_future);
                             });
        }
        
        future<T>& promise() override {
            return _future;
        }
        
        asio::io_service& _io_service;
        Handler _handler;
        future<T> _future;
    };
    
    template<class T, class Handler>
    auto make_future_callback(asio::io_service& io_service, Handler&& handler)
    {
        using handler_type = std::decay_t<Handler>;
        using future_callback_type = future_callback<T, handler_type>;
        return std::make_shared<future_callback_type>(io_service,
                                                std::forward<Handler>(handler));
    }
    
    
    struct failable_handler {
        failable_handler() noexcept = default;
        failable_handler(const failable_handler&) noexcept = default;
        failable_handler& operator=(const failable_handler&) noexcept = default;
        failable_handler(failable_handler&&) noexcept = default;
        failable_handler& operator=(failable_handler&&) noexcept = default;
        virtual ~failable_handler() noexcept = default;
        
        virtual void operator()(std::exception_ptr pe) const
        {
            impl_fail(std::move(pe));
        }

        template<class E, std::enable_if_t<std::is_base_of<std::exception, std::decay_t<E>>::value>* = nullptr>
        void operator()(E&& e) const
        {
            impl_fail(std::make_exception_ptr(std::forward<E>(e)));
        }
        
    private:
        virtual void impl_fail(std::exception_ptr pe) const = 0;
    };
    
    template<class T>
    struct future_handler_base
    : failable_handler
    {
        using interface_type = future_callback_interface<T>;
        //        using callback_type = future_callback<T, Handler>;
        
        
        future_handler_base() = default;
        
        template<class Handler>
        future_handler_base(asio::io_service& dispatcher, Handler&& handler)
        : _pcallback(make_future_callback<T>(dispatcher,
                                             std::forward<Handler>(handler)))
        {}
    protected:
        interface_type& callback() const {
            assert(_pcallback.get());
            return *_pcallback;
        }
        
    private:
        void impl_fail(std::exception_ptr pe) const override
        {
            auto& cb = callback();
            cb.promise().set_exception(std::move(pe));
            cb.trigger();
        }
        
        std::shared_ptr<interface_type> _pcallback;
    };
    
    
    template<class T>
    struct future_handler
    : future_handler_base<T>
    {
        using future_handler_base<T>::future_handler_base;
        using future_handler_base<T>::operator();
        
        void operator()(T v) const
        {
            auto& cb = this->callback();
            cb.promise().set_value(std::move(v));
            cb.trigger();
        }
        
    };
    
    template<>
    struct future_handler<void>
    : future_handler_base<void>
    {
        using future_handler_base<void>::future_handler_base;
        using future_handler_base<void>::operator();
        
        void operator()() const
        {
            auto& cb = this->callback();
            cb.promise().set_value();
            cb.trigger();
        }
    };
    
    template<class T, class Handler>
    auto make_future_handler(asio::io_service& dispatcher, Handler&& handler)
    {
        using handler_type = std::decay_t<Handler>;
        return future_handler<T>(dispatcher,
                                 std::forward<Handler>(handler));
    }
    
    
    
    
}