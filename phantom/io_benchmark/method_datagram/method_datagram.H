#pragma once

#include <memory>

#include <pd/base/config.H>
#include <pd/base/config_list.H>
#include <pd/base/netaddr.H>
#include <pd/base/thr.H>

#include <phantom/pd.H>
#include <phantom/module.H>
#include <phantom/io_benchmark/method.H>
#include <phantom/io_benchmark/method_stream/source.H>
#include <phantom/io_benchmark/method_stream/logger.H>

#pragma GCC visibility push(default)

namespace phantom { namespace io_benchmark {

class method_datagram_t : public method_t {
public:
    typedef method_stream::source_t source_t;
    typedef method_stream::logger_t logger_t;
    typedef method_stream::result_t result_t;

private:
    class loggers_t;

    virtual bool test(times_t &times) const;
    virtual void do_init();
    virtual void do_run() const;
    virtual void do_stat_print() const;
    virtual void do_fini();

    virtual const netaddr_t& target_addr() = 0;

    source_t* source;
    std::unique_ptr<loggers_t> loggers;

    int datagram_fd;
    const interval_t timeout;

    int connect_fd();
public:
    struct config_t {
        config_binding_type_ref(source_t);
        config_binding_type_ref(logger_t);

        config::objptr_t<source_t> source;
        config::list_t<config::objptr_t<logger_t>> loggers;
        interval_t timeout;

        config_t();
        void check(const in_t::ptr_t& p) const;
    };

    method_datagram_t(const string_t&, const config_t& conf);
    ~method_datagram_t();
};

}} // namespace phantom::io_benchmark

#pragma GCC visibility pop
