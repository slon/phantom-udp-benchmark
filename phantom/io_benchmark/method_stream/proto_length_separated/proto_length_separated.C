#include <phantom/io_benchmark/method_stream/proto.H>

#include <endian.h>

#include <phantom/module.H>

#include <pd/base/config.H>
#include <pd/base/exception.H>

namespace phantom { namespace io_benchmark { namespace method_stream {

MODULE(io_benchmark_method_stream_proto_length_separated);

class proto_length_separated_t : public proto_t {
public:
	virtual void do_init() { }
	virtual void do_run() const { }
	virtual void do_stat_print() const { }
	virtual void do_fini() { }

	struct config_t {
		config_t() : big_endian(true) {}

		void check(in_t::ptr_t const &) const {}
		config::enum_t<bool> big_endian;
	};

	inline proto_length_separated_t(
		string_t const &, config_t const & config
	) : proto_t(), big_endian(config.big_endian) {}

	virtual size_t maxi() const throw() { return 0; }
	virtual descr_t const* descr(size_t) const throw() { return NULL; }

	virtual bool reply_parse(
		in_t::ptr_t &ptr, in_segment_t const&,
		unsigned int& res_code, stat_t &, logger_t::level_t&
	) const {
		int32_t length = 0;
		for(int i = 0; i < 32; i += 8) {
			length |= (((unsigned char)(*ptr)) << i);
			++ptr;
		}

		if(big_endian)
			length = be32toh(length);

		ptr += length;
		res_code = 200;

		return true;
	}

private:
	bool big_endian;
};

namespace config_binding {
config_binding_sname(proto_length_separated_t);
config_binding_ctor(proto_t, proto_length_separated_t);
config_binding_value(proto_length_separated_t, big_endian);
} // namespace config_binding

}}} // namespace phantom::io_benchmark::method_stream
