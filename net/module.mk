# net/module.mk: the network stack (lwIP, vendored in net/lwip) and its
# glue: network devices, the net thread, sockets.
lib-y := net
# lwIP includes <string.h>, <ctype.h>, ...: the kernel libc in include/hubble.
ccflags-y := -I$(ROOT_DIR)/net/port -I$(ROOT_DIR)/net/lwip/include -I$(ROOT_DIR)/include/hubble
