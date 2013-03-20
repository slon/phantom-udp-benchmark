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

method_datagram_t::config_t::config_t() {}

void method_datagram_t::config_t::check(const in_t::ptr_t& ptr) const {
    if(!source) {
        config::error(ptr, "source is required");
    }
}

void method_datagram_t::loggers_t::commit(const in_segment_t& request,
                                          in_segment_t& tag,
                                          const result_t& res) const {
    for(size_t i = 0; i < size; ++i) {
        logger_t *logger = items[i];
        if(res.log_level >= logger->level)
            logger->commit(request, tag, res);
    }
}

method_datagram_t::method_datagram_t(const string_t&, const config_t& config)
    : source(config.source), datagram_fd(-1) {

    for(typeof(config.loggers.ptr()) lptr = config.loggers; lptr; ++lptr)
        ++loggers.size;

    loggers.items = new logger_t *[loggers.size];

    size_t i = 0;
    for(typeof(config.loggers.ptr()) lptr = config.loggers; lptr; ++lptr)
        loggers.items[i++] = lptr.val();
}

bool method_datagram_t::test(stat_t& stat) const {
    in_segment_t request, tag;
    if(!source->get_request(request, tag)) {
        return false;
    }

    stat_t::tcount_guard_t tcount_guard(stat);
    result_t res;

    res.size_out = request.size();
    res.time_conn = timeval_current();

    interval_t timeout = interval_second;

    str_t req = in_t::ptr_t(request).__chunk();
    req.truncate(request.size());

    ssize_t err = bq_write(datagram_fd, req.ptr(), req.size(), &timeout);
    if(err < 0 && errno != ECANCELED) {
        res.err = errno;
        res.log_level = logger_t::network_error;
    }

    res.time_send = res.time_recv = res.time_end = timeval_current();
    res.interval_event = res.time_end - res.time_start;
    loggers.commit(request, tag, res);

    return true;
}

void method_datagram_t::init() {
    source->init();

    datagram_fd = connect_fd();
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

void method_datagram_t::stat(out_t& out, bool clear, bool hrr_flag) const {
    source->stat(out, clear, hrr_flag);
}

void method_datagram_t::fini() {
    source->fini();

    if(::close(datagram_fd) < 0) {
        log_error("close: %m");
    }

    datagram_fd = -1;
}

size_t method_datagram_t::maxi() const throw() {
    return 1;
}

class network_descr_t : public descr_t {
    static size_t const max_errno = 140;

    virtual size_t value_max() const { return max_errno; }

    virtual void print_header(out_t &out) const {
        out(CSTR("network"));
    }

    virtual void print_value(out_t &out, size_t value) const {
        if(value < max_errno) {
            char buf[128];
            char *res = strerror_r(value, buf, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';
            out(str_t(res, strlen(res)));
        }
        else
            out(CSTR("Unknown error"));
    }
public:
    inline network_descr_t() throw() : descr_t() { }
    inline ~network_descr_t() throw() { }
};

static network_descr_t const network_descr;

descr_t const* method_datagram_t::descr(size_t /*ind*/) const throw() {
    return &network_descr;
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
} // namespace method_datagram

namespace method_datagram_ipv4 {
config_binding_sname(method_datagram_ipv4_t);
config_binding_value(method_datagram_ipv4_t, address);
config_binding_value(method_datagram_ipv4_t, port);
config_binding_parent(method_datagram_ipv4_t, method_datagram_t, 1);
config_binding_ctor(method_t, method_datagram_ipv4_t);
} // namespace method_datagram_ipv4

namespace method_datagram_ipv6 {
config_binding_sname(method_datagram_ipv6_t);
config_binding_value(method_datagram_ipv6_t, address);
config_binding_value(method_datagram_ipv6_t, port);
config_binding_parent(method_datagram_ipv6_t, method_datagram_t, 1);
config_binding_ctor(method_t, method_datagram_ipv6_t);
} // namespace method_datagram_ipv6

}} // namespace phantom::io_benchmark
