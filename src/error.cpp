#include <asio_amqp/error.hpp>

namespace asio_amqp
{
	auto logic_error_category() -> const system::error_category&
	{
        static const struct category : system::error_category
        {
            const char * name() const noexcept override {
                return "asio_amqp::logic_error";
            }

            std::string message( int ev ) const override {
                switch (static_cast<logic_error_code>(ev))
                {
                    case logic_error_code::wrong_state_for_connect: return "wrong state for connect";
                    case logic_error_code::zombie: return "operation on zombie object";
                }
                return "utter balls up";
            }
            
            system::error_condition  default_error_condition( int ev ) const noexcept override
            {
                return system::error_condition(ev, *this);
            }

        } _ {};
        return _;
	}

	auto runtime_error_category() -> const system::error_category&
	{
        static const struct category : system::error_category
        {
        const char * name() const noexcept override {
            return "asio_amqp::logic_error";
        }
        
        std::string message( int ev ) const override {
            switch (static_cast<logic_error_code>(ev))
            {
                case logic_error_code::wrong_state_for_connect: return "wrong state for connect request";
                case logic_error_code::zombie: return "operation on zombie object";
            }
            return "utter balls up";
        }
        
        system::error_condition  default_error_condition( int ev ) const noexcept override
        {
            return system::error_condition(ev, *this);
        }
        
    } _ {};
    return _;
	}
    
    system::error_code make_error_code( logic_error_code e ) noexcept
    {
        return system::error_code(static_cast<int>(e), logic_error_category());
    }
    
    system::error_code make_error_code( runtime_error_code e ) noexcept
    {
        return system::error_code(static_cast<int>(e), runtime_error_category());
    }
    
    system::error_condition make_error_condition( logic_error_code e ) noexcept
    {
        return system::error_condition(static_cast<int>(e), logic_error_category());
    }
}

