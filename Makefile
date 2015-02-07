################################################################################
#
# proxenet Makefile
#
#

PROGNAME			=	\"proxenet\"
AUTHOR				= 	\"hugsy\"
LICENSE				=	\"GPLv2\"
VERSION_MAJOR			= 	0
VERSION_MINOR			= 	3
VERSION_REL			=	git
VERSION      		   	=       \"$(VERSION_MAJOR).$(VERSION_MINOR)-$(VERSION_REL)\"
ARCH            		=       $(shell uname)
DEBUG		           	=       0
DEBUG_SSL			=       0

CC				=       cc
BIN	  			=       proxenet
DEFINES				=       -DPROGNAME=$(PROGNAME) -DVERSION=$(VERSION) -DAUTHOR=$(AUTHOR) -DLICENSE=$(LICENSE)
CHARDEN				=	-fstack-protector-all
LHARDEN				=	-Wl,-z,relro
LDFLAGS				=       -lpthread $(LHARDEN)
SRC				=	$(wildcard *.c)
OBJECTS         		=       $(patsubst %.c, %.o, $(SRC))
INC				=      	-I/usr/include -pthread
CFLAGS				=	-Wall $(DEFINES) $(INC) -pthread $(CHARDEN)
LIB				= 	-L/lib


# if DEBUG
ifeq ($(DEBUG), 1)
DBGFLAGS		=       -ggdb -DDEBUG
CFLAGS			+=      $(DBGFLAGS)

ifeq ($(CC), clang)
CFLAGS			+=      -fsanitize=address -fno-omit-frame-pointer -O1
endif

ifeq ($(DEBUG_SSL), 1)
CFLAGS			+=	-DDEBUG_SSL
endif

else

# if not DEBUG
CFLAGS			+=      -O3
endif


# PLUGINS
#
ifeq ($(FORCE_PYTHON3), 1)
PYTHON_VERSION		=	python-3.3
PYTHON_MAJOR		=	3
else
PYTHON_VERSION		=	python-2.7
PYTHON_MAJOR		=	2
endif

ifeq ($(FORCE_RUBY18), 1)
RUBY_VERSION		=	ruby-1.8
RUBY_MINOR		=	8
else # ruby1.9 API is compatible with ruby2
RUBY_VERSION		=	ruby-1.9
RUBY_MINOR		=	9
endif

LUA_VERSION		=	lua5.2
PERL_VERSION		=	perl5.18.2

# TEST
TEST_ARGS		= 	-4 -vvvv -t 10 -b 0.0.0.0 -p 8000


# Compile rules
.PHONY : all install uninstall check-syntax clean keys tags purge test check-required check-plugins check-python check-lua check-ruby check-perl check-tcl leaks

.c.o :
	@echo "[+] CC $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

all :  check-required check-plugins $(BIN)

$(BIN): $(OBJECTS)
	@echo "[+] LINK with $(LDFLAGS)"
	@$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LIB) $(LDFLAGS)

clean:
	@echo "[+] RM objects"
	@rm -fr $(OBJECTS) ./core-$(BIN)-*

purge: clean
	@echo "[+] RM $(BIN)"
	@rm -fr $(BIN)

keys:
	@make -C keys keys

install: $(BIN)
	install -s -m 755 -o root -g root -- ./$(BIN) /usr/local/bin/
	gzip -c ./man/$(BIN).1 >> ./man/$(BIN).1.gz
	install -m 644 -o root -- ./man/$(BIN).1.gz /usr/share/man/man8/

uninstall: clean
	rm -fr /usr/local/bin/$(BIN) /usr/share/man/man1/$(BIN).1.gz

check-required: check-polarssl check-dl

check-plugins: check-python check-lua check-ruby check-perl check-tcl

check-polarssl:
	@echo -n "[+] Looking for required 'polarssl' library ... "
	@echo "int main(int a,char** b){ return 0; }">_a.c;$(CC) _a.c -lpolarssl || (echo "not found"; rm -fr _a.c && exit 1)
	@rm -fr _a.c a.out
	@echo "found"
	$(eval LDFLAGS += -lpolarssl)

check-dl:
	@echo -n "[+] Looking for required 'dl' library ... "
	@echo "int main(int a,char** b){ return 0; }">_a.c; $(CC) _a.c -ldl || (echo "not found"; rm -fr _a.c && exit 1)
	@rm -fr _a.c a.out
	@echo "found"
	$(eval DEFINES += -D_C_PLUGIN )
	$(eval LDFLAGS += -ldl )

check-python:
ifeq ($(NO_PYTHON), 1)
	@echo "[-] Explicitly disabling Python support"
else
	@echo -n "[+] Looking for '$(PYTHON_VERSION)' ... "
ifeq ($(strip $(shell pkg-config --cflags --libs $(PYTHON_VERSION) >/dev/null 2>&1 && echo ok)), ok)
	@echo "found"
	$(eval DEFINES += -D_PYTHON_PLUGIN -D_PYTHON_MAJOR_=$(PYTHON_MAJOR) )
	$(eval LDFLAGS += $(shell pkg-config --libs $(PYTHON_VERSION)) )
	$(eval INC += $(shell pkg-config --cflags $(PYTHON_VERSION)) )
else
	@echo "Python libs not found"
endif
endif

check-lua:
ifeq ($(NO_LUA), 1)
	@echo "[-] Explicitly disabling Lua support"
else
	@echo -n "[+] Looking for '$(LUA_VERSION)' ... "
ifeq ($(strip $(shell pkg-config --cflags --libs $(LUA_VERSION) >/dev/null 2>&1 && echo ok)), ok)
	@echo "found"
	$(eval DEFINES += -D_LUA_PLUGIN )
	$(eval LDFLAGS += $(shell pkg-config --libs $(LUA_VERSION)) )
	$(eval INC += $(shell pkg-config --cflags $(LUA_VERSION)) )
else
	@echo "Lua libs not found"
endif
endif

check-ruby:
ifeq ($(NO_RUBY), 1)
	@echo "[-] Explicitly disabling Ruby support"
else
	@echo -n "[+] Looking for '$(RUBY_VERSION)' ... "
ifeq ($(strip $(shell pkg-config --cflags --libs $(RUBY_VERSION) > /dev/null 2>&1  && echo ok)), ok)
	@echo "found"
	$(eval DEFINES += -D_RUBY_PLUGIN -D_RUBY_MINOR_=$(RUBY_MINOR))
	$(eval LDFLAGS += $(shell pkg-config --libs $(RUBY_VERSION)) )
	$(eval INC += $(shell pkg-config --cflags $(RUBY_VERSION)) )
else
	@echo "not found"
endif
endif

check-perl:
ifeq ($(NO_PERL), 1)
	@echo "[-] Explicitly disabling Perl support"
else
	@echo -n "[+] Looking for '$(PERL_VERSION)' ... "
ifeq ($(strip $(shell pkg-config --cflags --libs $(PERL_VERSION) >/dev/null 2>&1 && echo ok)), ok)
	@echo "found"
	$(eval DEFINES += -D_PERL_PLUGIN)
	$(eval LDFLAGS += $(shell pkg-config --libs $(PERL_VERSION)) )
	$(eval INC += $(shell pkg-config --cflags $(PERL_VERSION)) )
else
	@echo "not found"
endif
endif

check-tcl:
ifeq ($(NO_TCL), 1)
	@echo "[-] Explicitly disabling TCL support"
else
	@echo -n "[+] Looking for required 'tcl' library ... "
	@echo "int main(int a,char** b){return 0;}">_a.c; $(CC) _a.c -ltcl || (echo "not found"; rm -fr _a.c && exit 1)
	@rm -fr _a.c a.out
	@echo "found"
	$(eval DEFINES += -D_TCL_PLUGIN -I/usr/include/tcl8.6/)
	$(eval LDFLAGS += -ltcl )
endif

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
test: clean all
	./$(BIN) $(TEST_ARGS)

leaks: all
	valgrind -v --track-origins=yes --leak-check=full --show-reachable=yes ./$(BIN) $(TEST_ARGS)
