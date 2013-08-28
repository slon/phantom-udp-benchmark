#include <phantom/io_benchmark/method_stream/proto.H>

#include <endian.h>

#include <phantom/module.H>

#include <pd/base/config.H>
#include <pd/base/exception.H>

namespace phantom { namespace io_benchmark { namespace method_stream {

MODULE(io_benchmark_method_stream_proto_length_separated);

class proto_length_separated_t : public proto_t {
public:
    struct config_t {
        void check(in_t::ptr_t const &) const {}
    };

    inline proto_length_separated_t(
        string_t const &, config_t const &
    ) {}

    virtual bool reply_parse(
        in_t::ptr_t &ptr, in_segment_t const&,
        unsigned int&, stat_t&, logger_t::level_t&
    ) const {
        int32_t length = 0;
        for(int i = 0; i < 32; i += 8) {
            length |= (((unsigned char)(*ptr)) << i);
            ++ptr;
        }

        length = be32toh(length);

        ptr += length;

        return true;
    }

    virtual size_t maxi() const throw() { return 0; }
    virtual descr_t const *descr(size_t) const throw() { return NULL; }
};

namespace config_binding {
config_binding_sname(proto_length_separated_t);
config_binding_ctor(proto_t, proto_length_separated_t);
} // namespace config_binding

}}} // namespace phantom::io_benchmark::method_stream
