#pragma once
#include <cassert>
#include <algorithm>
#include <vector>
#include <array>
#include <cstdlib>
#include <iomanip>

namespace asio_amqp { namespace detail {
    
    namespace detail {
        struct chars_dumper
        {
            chars_dumper(const char* first, const char* last)
            : _begin(first), _end(last)
            {}
            
            std::ostream& operator()(std::ostream& os) const
            {
                os << '"';
                for(auto first = _begin ; first != _end ; ++first) {
                    
                    auto c = *first;
                    if (c == '\\') {
                        os << "\\\\";
                    }
                    else if (std::isprint(c)) {
                        os.put(c);
                    }
                    else {
                        os << "\\x" << std::setw(2) << std::hex << std::setfill('0') << (int(c) & 0xff);
                    }
                }
                os << '"';
                return os;
            }
            
            const char* _begin, *_end;
        };
        
        inline
        std::ostream& operator<<(std::ostream& os, chars_dumper cd)
        {
            return cd(os);
        }
    };
    
    inline
    auto char_dump(const char* first, const char* last)
    {
        return detail::chars_dumper(first, last);
    }
    
	struct receiver
	{
        template<class Socket, class Handler>
        void async_read(Socket& s, Handler&& handler)
        {
            assert(not busy());
            normalise();
            _receiving = true;
            s.async_read_some(asio::buffer(_read_area),
                              [this, handler = std::move(handler)]
                              (auto const& ec, auto bytes)
            {
                _receiving = false;
                auto first = _read_area.data();
                auto last = first + bytes;
                _buffer.insert(std::end(_buffer), first, last);
                handler(ec, _buffer.size() - _getp);
            });
        }
        
        bool busy() const {
            return _receiving;
        }
        
        auto data()
        {
            return asio::mutable_buffer(_buffer.data() + _getp , _buffer.size() - _getp);
        }
        
        void consume(std::size_t bytes)
        {
            BOOST_LOG_TRIVIAL(trace) << "consuming: " << char_dump(&_buffer[_getp], &_buffer[_getp + bytes]);
            _getp += bytes;
            assert(_getp <= _buffer.size());
        }
        
        void normalise()
        {
            if (_getp) {
                auto new_size = _buffer.size() - _getp;
                auto dfirst = _buffer.data();
                auto sfirst = dfirst + _getp;
                auto slast = sfirst + new_size;
                std::copy(sfirst, slast, dfirst);
                _buffer.resize(new_size);
                _getp = 0;
            }
        }

        std::array<char, 65536> _read_area;
        std::vector<char> _buffer;
        std::size_t _getp = 0;
        bool _receiving = false;
	};
}}