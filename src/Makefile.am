AM_CFLAGS		= -O2 -Wall -pthread
AM_CFLAGS		+= $(PYTHON_CFLAGS) $(LUA_CFLAGS) $(C_CFLAGS) $(PERL_CFLAGS)  $(RUBY_CFLAGS) 

AM_LDFLAGS		= -Wl,-z,relro -fstack-protector-all
AM_LDFLAGS		+= $(PYTHON_LFLAGS) $(LUA_LFLAGS) $(C_LFLAGS) $(PERL_LFLAGS)  $(RUBY_LFLAGS) 

sbin_PROGRAMS		= proxenet
proxenet_SOURCES	= main.c main.h \
	ssl.c ssl.h \
        socket.c socket.h\
        http.c http.h \
	core.c core.h\
	utils.c utils.h \
	control-server.c control-server.h \
	plugin.c plugin.h \
	plugin-python.c plugin-python.h \
	plugin-c.c plugin-c.c \
	plugin-lua.c plugin-lua.h \
	plugin-perl.c plugin-perl.h \
	plugin-ruby.c plugin-ruby.h
