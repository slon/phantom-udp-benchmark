#include "method_datagram.H"

#include <errno.h>
#include <unistd.h>

#include <pd/base/log.H>
#include <pd/base/exception.H>
#include <pd/base/netaddr_ipv4.H>
#include <pd/base/netaddr_ipv6.H>
#include <pd/bq/bq_util.H>

#include <phantom/module.H>

namespace phantom { namespace io_benchmark {

MODULE(io_benchmark_method_datagram);

struct method_datagram_t::loggers_t : sarray1_t<logger_t *> {
	inline loggers_t(
		config::list_t<config::objptr_t<logger_t>> const &list
	) :
		sarray1_t<logger_t *>(list) { }

	inline ~loggers_t() throw() { }

	inline void init(string_t const &name) const {
		for(size_t i = 0; i < size; ++i)
			items[i]->init(name);
	}

	inline void run(string_t const &name) const {
		for(size_t i = 0; i < size; ++i)
			items[i]->run(name);
	}

	inline void stat_print(string_t const &name) const {
		stat::ctx_t ctx(CSTR("loggers"), 1);
		for(size_t i = 0; i < size; ++i)
			items[i]->stat_print(name);
	}

	inline void fini(string_t const &name) const {
		for(size_t i = 0; i < size; ++i)
			items[i]->fini(name);
	}

	inline void commit(
		in_segment_t const &request, in_segment_t &tag, result_t const &res
	) const {
		for(size_t i = 0; i < size; ++i) {
			logger_t *logger = items[i];
			if(res.log_level >= logger->level)
				logger->commit(request, tag, res);
		}
	}
};

method_datagram_t::config_t::config_t() {}

void method_datagram_t::config_t::check(const in_t::ptr_t& ptr) const {
    if(!source) {
        config::error(ptr, "source is required");
    }
}

method_datagram_t::method_datagram_t(const string_t& name, const config_t& config)
    : method_t(name), source(config.source), loggers(new loggers_t(config.loggers)), datagram_fd(-1), timeout(config.timeout) {}

method_datagram_t::~method_datagram_t() {}

bool method_datagram_t::test(times_t& times) const {
    in_segment_t request, tag;
    if(!source->get_request(request, tag)) {
        return false;
    }

    result_t res;

    res.size_out = request.size();
    res.time_conn = timeval::current();

    interval_t _timeout = timeout;

    str_t req = in_t::ptr_t(request).__chunk();
    req.truncate(request.size());

    ssize_t err = bq_write(datagram_fd, req.ptr(), req.size(), &_timeout);

    // HACK around epoll interface
    // Since we can't add one fd to epoll twice, we are dup()-ing it
    // This event should be very rare
    if(err < 0 && errno == EAGAIN) {
        int fd_dup = ::dup(datagram_fd);
        if(fd_dup < 0) {
            res.err = errno;
            res.log_level = logger_t::network_error;
        } else {
            err = bq_write(datagram_fd, req.ptr(), req.size(), &_timeout);
            res.err = errno;
        }

        // no errors before close
        if(fd_dup >= 0 && ::close(fd_dup) < 0 && res.err == 0) {
            res.err = errno;
            res.log_level = logger_t::network_error;
        }
    }

    if(err < 0 && errno != ECANCELED) {
        res.err = errno;
        res.log_level = logger_t::network_error;
    }

    res.time_send = res.time_recv = res.time_end = timeval::current();
    res.interval_event = res.time_end - res.time_start;

    interval_t interval_real = res.time_end - res.time_start;
    times.inc(interval_real);

    loggers->commit(request, tag, res);

    return true;
}

void method_datagram_t::do_init() {
    source->init(name);
    loggers->init(name);

    datagram_fd = connect_fd();
}

void method_datagram_t::do_run() const {
    source->run(name);
    loggers->run(name);
}

void method_datagram_t::do_stat_print() const {
    source->stat_print(name);
    loggers->stat_print(name);
}

int method_datagram_t::connect_fd() {
    const netaddr_t& addr = target_addr();

    int fd = socket(addr.sa->sa_family, SOCK_DGRAM, 0);
    if(fd < 0) {
        throw exception_sys_t(log::error, errno, "socket: %m");
    }

    bq_fd_setup(fd);

    if(bq_connect(fd, addr.sa, addr.sa_len, NULL) < 0) {
        throw exception_sys_t(log::error, errno, "connect: %m");
    }

    return fd;
}

void method_datagram_t::do_fini() {
    source->fini(name);
    loggers->fini(name);

    if(::close(datagram_fd) < 0) {
        log_error("close: %m");
    }

    datagram_fd = -1;
}

class method_datagram_ipv4_t : public method_datagram_t {
public:
    struct config_t : public method_datagram_t::config_t {
        address_ipv4_t address;
        uint16_t port;

        config_t() : port(0) {}

        void check(const in_t::ptr_t& ptr) const {
            if(!address) {
                config::error(ptr, "address is required");
            }

            if(!port) {
                config::error(ptr, "port is required");
            }

            method_datagram_t::config_t::check(ptr);
        }
    };

    method_datagram_ipv4_t(const string_t& name, const config_t& config)
        : method_datagram_t(name, config),
          addr(config.address, config.port) {}

    virtual const netaddr_t& target_addr() {
        return addr;
    }

private:
    netaddr_ipv4_t addr;
};

class method_datagram_ipv6_t : public method_datagram_t {
public:
    struct config_t : public method_datagram_t::config_t {
        address_ipv6_t address;
        uint16_t port;

        config_t() : port(0) {}

        void check(const in_t::ptr_t& ptr) const {
            if(!address) {
                config::error(ptr, "address is required");
            }

            if(!port) {
                config::error(ptr, "port is required");
            }

            method_datagram_t::config_t::check(ptr);
        }
    };

    method_datagram_ipv6_t(const string_t& name, const config_t& config)
        : method_datagram_t(name, config),
          addr(config.address, config.port) {}

    virtual const netaddr_t& target_addr() {
        return addr;
    }

private:
    netaddr_ipv6_t addr;
};

namespace method_datagram {
config_binding_sname(method_datagram_t);
config_binding_type(method_datagram_t, source_t);
config_binding_value(method_datagram_t, source);
config_binding_type(method_datagram_t, logger_t);
config_binding_value(method_datagram_t, loggers);
config_binding_value(method_datagram_t, timeout);
config_binding_cast(method_datagram_t, method_t);
} // namespace method_datagram

namespace method_datagram_ipv4 {
config_binding_sname(method_datagram_ipv4_t);
config_binding_value(method_datagram_ipv4_t, address);
config_binding_value(method_datagram_ipv4_t, port);
config_binding_parent(method_datagram_ipv4_t, method_datagram_t);
config_binding_ctor(method_t, method_datagram_ipv4_t);
} // namespace method_datagram_ipv4

namespace method_datagram_ipv6 {
config_binding_sname(method_datagram_ipv6_t);
config_binding_value(method_datagram_ipv6_t, address);
config_binding_value(method_datagram_ipv6_t, port);
config_binding_parent(method_datagram_ipv6_t, method_datagram_t);
config_binding_ctor(method_t, method_datagram_ipv6_t);
} // namespace method_datagram_ipv6

}} // namespace phantom::io_benchmark
