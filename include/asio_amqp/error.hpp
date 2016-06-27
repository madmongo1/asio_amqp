#pragma once
#include <asio_amqp/config.hpp>

namespace asio_amqp {
    
    
    enum class logic_error_code
    {
        wrong_state_for_connect,
        zombie
    };
    
    enum class runtime_error_code
    {
        authentication
    };
    
    system::error_code make_error_code( logic_error_code e ) noexcept;
    system::error_condition make_error_condition( logic_error_code e ) noexcept;
    
    system::error_code make_error_code( runtime_error_code e ) noexcept;
    system::error_condition make_error_condition( runtime_error_code e ) noexcept;
    
    const system::error_category& logic_error_category();
    const system::error_category& runtime_error_category();
    
    struct logic_system_error : system::system_error
    {
        using system::system_error::system_error;
    };
    
    struct runtime_system_error : system::system_error
    {
        using system::system_error::system_error;
    };
}
namespace boost { namespace system {
    
    template<> struct is_error_code_enum<::asio_amqp::runtime_error_code>
    { static const bool value = true; };
    
    template<> struct is_error_code_enum<::asio_amqp::logic_error_code>
    { static const bool value = true; };
    
    template<> struct is_error_condition_enum<::asio_amqp::runtime_error_code>
    { static const bool value = true; };
    
    template<> struct is_error_condition_enum<::asio_amqp::logic_error_code>
    { static const bool value = true; };
}}

namespace asio_amqp {
    
    inline
    void throw_system_error(system::error_code ec)
    {
        if (ec)
        {
            if (ec.category() == runtime_error_category())
            {
                throw runtime_system_error(ec, ec.message());
            }
            if (ec.category() == logic_error_category()) {
                throw logic_system_error(ec, ec.message());
            }
            throw system::system_error(ec, ec.message());
        }
    }
    
    inline
    system::error_code& assign_error(system::error_code& ec, system::error_code err)
    {
        if (std::addressof(ec) == std::addressof(system::throws)) {
            if (err) {
                throw_system_error(err);
            }
        }
        else {
            if (err) {
                ec = err;
            }
            else {
                ec.clear();
            }
        }
        return ec;
    }
    
    struct resolve_failure : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    
    
}

