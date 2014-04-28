#include <unistd.h>
#include <fcntl.h>

#include <random>
#include <string>
#include <fstream>

#include <pd/base/time.H>
#include <pd/base/config.H>
#include <pd/bq/bq_util.H>

#include <phantom/pd.H>
#include <phantom/io.H>
#include <phantom/module.H>
#include <phantom/io_stream/proto_http/handler.H>

namespace phantom { namespace io_stream { namespace proto_http {

class handler_simulator_t : public handler_t, public io_t {
public:
	struct config_t : public handler_t::config_t, public io_t::config_t {
		config_t() : mean_time(50 * interval::millisecond), stddev_time(5 * interval::millisecond), min_time(0), max_time(interval::minute) {}

		interval_t mean_time, stddev_time;
		interval_t min_time, max_time;
		string_t response_file;

		void check(in_t::ptr_t const &ptr) const {
			io_t::config_t::check(ptr);
			handler_t::config_t::check(ptr);
			if(!response_file)
				config::error(ptr, "response_file is required");
		}
	};

	handler_simulator_t(const string_t& name, const config_t& config) :
		handler_t(name, config),
		io_t(name, config),
		mean_time(config.mean_time),
		stddev_time(config.stddev_time),
		min_time(config.min_time),
		max_time(config.max_time),
		response_file(config.response_file) {}

	struct string_content_t : public http::local_reply_t::content_t {
		string_content_t(string_t content) : content(content) {}

		virtual http::code_t code() const throw() { return http::code_200; }
		virtual ssize_t size() const { return content.size(); }
		virtual void print_header(pd::out_t&, const pd::http::server_t&) const {};
		virtual bool print(pd::out_t& out) const {
			out(content);
			return true;
		}

		string_t content;
	};

	virtual void do_proc(request_t const &, reply_t &reply) const {
		double r = 0.0;
		{
			spinlock_guard_t guard(rng_lock);
			r = std::normal_distribution<double>()(rng);
		}

		interval_t sleep_time = mean_time + (r * 1000000 * std_time) / 1000000;
		if(sleep_time > max_time) sleep_time = max_time;
		if(sleep_time < min_time) sleep_time = min_time;
		bq_sleep(&sleep_time);

		reply.set(new string_content_t(response_content));
	}

	virtual void init() {
		std::string filename(response_file.ptr(), response_file.size());

		int fd = -1;
		if((fd = open(filename.c_str(), O_RDONLY)) == -1)
			throw exception_sys_t(log::error, errno, "open(%s): %m", filename.c_str());

		// 64 megs should be enought for anybody :), and ctor_t resizes buffer on overflow anyway
		string_t::ctor_t response_content_ctor(64 * 1024 * 1024);

		const int BUFFER_SIZE = 64 * 1024;
		char buff[BUFFER_SIZE];
		ssize_t len = 0;
		while((len = read(fd, buff, BUFFER_SIZE)) != 0) {
			if(len == -1) {
				close(fd);
				throw exception_sys_t(log::error, errno, "read(): %m");
			}

			response_content_ctor(str_t(buff, len));
		}

		close(fd);
		response_content = string_t(response_content_ctor);
	}

	virtual void run() const {}
	virtual void stat_print() const {}
	virtual void fini() {}

private:
	interval_t mean_time, std_time, min_time, max_time;
	string_t response_file;

	string_t response_content;

	mutable spinlock_t rng_lock;
	mutable std::default_random_engine rng;
};

MODULE(io_stream_proto_http_handler_simulator);

namespace handler_simulator_config {
config_binding_sname(handler_simulator_t);
config_binding_value(handler_simulator_t, response_file);
config_binding_value(handler_simulator_t, mean_time);
config_binding_value(handler_simulator_t, stddev_time);
config_binding_value(handler_simulator_t, min_time);
config_binding_value(handler_simulator_t, max_time);
config_binding_ctor(io_t, handler_simulator_t);
config_binding_parent(handler_simulator_t, io_t);
config_binding_parent(handler_simulator_t, handler_t);
} // namespace config

}}} // namespace phantom::io_stream::proto_http
