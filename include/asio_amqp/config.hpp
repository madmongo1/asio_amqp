#pragma once

#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <boost/variant.hpp>
#include <boost/optional.hpp>

namespace asio_amqp {

	namespace asio = boost::asio;
	namespace system = boost::system;
    
	template<class...Ts> using variant = boost::variant<Ts...>;
    
    template<class T> using optional = boost::optional<T>;

}

