#pragma once
#include <asio_amqp/config.hpp>
#include <deque>
#include <vector>
#include <cstdint>
#include <boost/log/trivial.hpp>

namespace asio_amqp { namespace detail {
    
    
    template<class StreamType>
    struct sender
    {
        sender(StreamType& stream) : _stream(stream) {}
        
        template<class Iter>
        void queue_for_send(Iter first, Iter last)
        {
            if (first != last) {
                _send_buffers.emplace_back(first, last);
                check_send();
            }
        }
        
    private:
        void check_send()
        {
            if (_send_in_progress or _send_buffers.empty()) { return; }
            _sending_buffers.clear();
            std::swap(_sending_buffers, _send_buffers);
            build_asio_buffers();
            asio::async_write(_stream,
                              _asio_buffers,
                              [this] (const system::error_code& ec,
                                      std::size_t sent)
                              {
                                  _send_in_progress = false;
                                  if (ec) {
                                      BOOST_LOG_TRIVIAL(info) << "asio_amqp::send failure: " << ec.message();
                                      // somehow send this error up the chain
                                  }
                                  else {
                                      check_send();
                                  }
                              });
            
        }
        
        void build_asio_buffers()
        {
            _asio_buffers.clear();
            _asio_buffers.reserve(_sending_buffers.size());
            std::transform(std::begin(_sending_buffers), std::end(_sending_buffers),
                           std::back_inserter(_asio_buffers),
                           [](const auto& buf) {
                               return asio::buffer(buf);
                           });
        }
        
        
        
        StreamType& _stream;
        std::vector<std::vector<char>> _send_buffers;
        std::vector<std::vector<char>> _sending_buffers;
        std::vector<asio::const_buffers_1> _asio_buffers;
        bool _send_in_progress = false;
    };
}}
