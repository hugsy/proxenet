################################################################################
#
# proxenet Makefile
# 
#


VERSION         =       0.01
ARCH            =       $(shell uname)
DEBUG           =       1
DEBUG_SSL	=       0

CC              =       cc
BIN             =       proxenet
DEFINES         =       -DVERSION=$(VERSION)
HARDEN		=	-Wl,-z,relro -fstack-protector-all
LDFLAGS         =       $(HARDEN) -lpthread 
SRC		=	$(wildcard *.c)
OBJECTS         =       $(patsubst %.c, %.o, $(SRC))
INC             =       -I/usr/include -pthread
CFLAGS          =       -O2 -Wall $(DEFINES) $(INC) -pthread
LIB		= 	-L/lib


# SSL 
INC		+=	-Ipolarssl/include
LIB		+= 	-Lpolarssl/library
LDFLAGS		+=	-lpolarssl


# DEBUG
ifeq ($(DEBUG), 1)
DBGFLAGS        =       -ggdb -DDEBUG
CFLAGS          +=      $(DBGFLAGS)

ifeq ($(DEBUG_SSL), 1)
CFLAGS		+=	-DDEBUG_SSL
endif

endif


# PLUGINS
# Modify path to match your config
#
WITH_C_PLUGIN		=	0
WITH_PYTHON_PLUGIN	=	0
_USE_PYTHON2_		= 	0
_USE_PYTHON3_		= 	1

WITH_RUBY_PLUGIN	=	0
_USE_RUBY18_		=	1
_USE_RUBY19_		=	0

WITH_LUA_PLUGIN		=	0
WITH_PERL_PLUGIN	=	0

ifeq ($(WITH_C_PLUGIN), 1)
DEFINES			+=	-D_C_PLUGIN 
LDFLAGS			+=	-ldl
endif

ifeq ($(WITH_PYTHON_PLUGIN), 1)

# Check Python version (2 or 3)
ifeq ($(_USE_PYTHON2_), 1)
	DEFINES		+=	-D_PYTHON_PLUGIN -D_PYTHON2_
	LDFLAGS		+=	-lpython2.7
	INC		+=	-I/usr/include/python2.7
else ifeq ($(_USE_PYTHON3_), 1)
	DEFINES		+=	-D_PYTHON_PLUGIN -D_PYTHON3_
	LDFLAGS		+=	-lpython3.3m
	INC		+=	-I/usr/include/python3.3m
endif

endif

ifeq ($(WITH_PERL_PLUGIN), 1)
DEFINES			+=	-D_PERL_PLUGIN
INC			+=	-I/usr/lib/perl5/core_perl/CORE/
INC			+=	-I/usr/lib/perl5/CORE/
LIB			+=	-L/usr/lib/perl5/core_perl/CORE/
LIB			+=	-L/usr/lib/perl5/CORE/
LDFLAGS			+=	-lperl
endif

# Check Ruby version (1.8 or 1.9 - 1.9 api is compatible with Ruby2)
ifeq ($(WITH_RUBY_PLUGIN), 1)

ifeq ($(_USE_RUBY18_), 1)
	DEFINES		+=	-D_RUBY_PLUGIN -D_RUBY18_
	INC		+=	-I/home/hugsy/.rvm/rubies/ruby-1.8.7-p370/lib/ruby/1.8/x86_64-linux/
	LIB		+=	-L/home/hugsy/.rvm/rubies/ruby-1.8.7-p370/lib
	LDFLAGS		+=	-lruby
else ifeq ($(_USE_RUBY19_), 1)
	DEFINES		+=	-D_RUBY_PLUGIN -D_RUBY19_
	INC		+=	-I/home/hugsy/.rvm/rubies/ruby-1.9.3-p194/include/ruby-1.9.1
	INC		+=	-I/home/hugsy/.rvm/rubies/ruby-1.9.3-p194/include/ruby-1.9.1/x86_64-linux
	LIB		+=	-L/home/hugsy/.rvm/rubies/ruby-1.9.3-p194/lib
	LDFLAGS		+=	-lruby
endif

endif

ifeq ($(WITH_LUA_PLUGIN), 1)
DEFINES			+=	-D_LUA_PLUGIN
LDFLAGS			+=	-llua
endif


# TEST
TEST_ARGS		= 	-4 -vvvv -t 10 -b 0.0.0.0 -p 8000 


# Compile rules
.PHONY : all check-syntax clean keys tags purge ssl ssl-clean test 

.c.o :
	@echo "CC $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

all : $(BIN)

$(BIN): $(OBJECTS) 
	@echo "LINK with $(LDFLAGS)"
	@$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LIB) $(LDFLAGS)

purge:
	@echo "RM objects"
	@rm -fr $(OBJECTS) ./core-$(BIN)-*

clean: purge
	@echo "RM $(BIN)"
	@rm -fr $(BIN)

keys:
	@make -C keys keys

# use this rule if you want to embed polarssl
ssl:	polarssl/library/libpolarssl.a

ssl-git:
	@echo "Downloading PolarSSL library"
	@git clone https://github.com/polarssl/polarssl.git

polarssl/library/libpolarssl.a:
	@if [ $(DEBUG_SSL) -eq 1 ]; then \
		echo "Building PolarSSL library with DEBUG syms";\
		make -C polarssl lib DEBUG=1; \
	else \
		echo "Building PolarSSL library";\
		make -C polarssl lib; \
	fi

ssl-clean: clean
	@make -C polarssl clean

# Packaging
snapshot: clean
	git add . && \
	git ci -m "$(shell date): Generating snapshot release" && \
        git archive --format=tar --prefix=$(BIN)-$(VERSION)/ HEAD \
	|gzip > /tmp/$(PROGNAME)-latest.tgz

stable: clean
	git add . && \
	git ci -m "$(shell date): Generating stable release" && \
	git archive --format=tar --prefix=$(BIN)-$(VERSION)/ master \
	|gzip > /tmp/${PROGNAME}-${PROGVERS}.tgz

# Tests
check-syntax:
	$(CC) $(CFLAGS) -fsyntax-only $(CHK_SOURCES)

test: clean $(BIN)
	./$(BIN) $(TEST_ARGS)

valgrind: $(BIN)
	valgrind -v --track-origins=yes --leak-check=full --show-reachable=yes ./$(BIN) $(TEST_ARGS)
