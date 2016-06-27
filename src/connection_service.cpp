#include <asio_amqp/connection_service.hpp>
#include <valuelib/debug/unwrap.hpp>
#include <iostream>

namespace asio_amqp {


    void connection_service::run()
    {
        while (!_service_dispatcher.stopped())
        {
            try {
                _service_dispatcher.run();
            }
            catch(...)
            {
                std::cerr << "fatal error: uncaught exception in asio_amqp::connection_service loop\n";
                std::cerr << value::debug::unwrap(std::current_exception());
            }
        }
    }

}