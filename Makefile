default: all

include /usr/share/phantom/module.mk

# $(eval $(call MODULE,io_benchmark/method_datagram))
$(eval $(call MODULE,io_benchmark/method_stream/proto_length_separated))

include /usr/share/phantom/opts.mk

all: $(targets)

clean:; @rm -f $(targets) $(tmps) deps/*.d

.PHONY: default all clean

