#pragma once

#include <pd/base/config.H>
#include <pd/base/config_list.H>
#include <pd/base/netaddr.H>

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
    struct loggers_t {
        size_t size;
        logger_t **items;
        inline loggers_t() throw() : size(0), items(NULL) { }
        inline ~loggers_t() throw() { if(items) delete [] items; }

        void commit(
            in_segment_t const &request, in_segment_t &tag, result_t const &res
        ) const;

    } loggers;

    virtual bool test(stat_t &stat) const;
    virtual void init();
    virtual void stat(out_t &out, bool clear, bool hrr_flag) const;
    virtual void fini();

    virtual size_t maxi() const throw();
    virtual descr_t const *descr(size_t ind) const throw();

    virtual const netaddr_t& target_addr() = 0;

    source_t* source;
    int datagram_fd;

    int connect_fd();
public:
    struct config_t {
        config_binding_type_ref(source_t);
        config_binding_type_ref(logger_t);

        config::objptr_t<source_t> source;
        config::list_t<config::objptr_t<logger_t> > loggers;

        config_t();
        void check(const in_t::ptr_t& p) const;
    };

    method_datagram_t(const string_t&, const config_t& conf);
};

}} // namespace phantom::io_benchmark

#pragma GCC visibility pop
