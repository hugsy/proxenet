################################################################################
#
# proxenet Makefile
# 
#


VERSION         =       0.01
ARCH            =       $(shell uname)
DEBUG           =       1

# ifeq ($(shell uname -m), x86_64)
# LIB		= 	-L/usr/lib64
# else
# LIB		= 	-L/usr/lib
# endif

CC              =       gcc
BIN             =       proxenet
DEFINES         =       -DVERSION=$(VERSION)
# HARDEN		=	-Wl,-z,relro -pie -fstack-protector-all -fPIE
LDFLAGS         =       -lpthread -lgnutls
SRC		=	$(wildcard *.c)
OBJECTS         =       $(patsubst %.c, %.o, $(SRC))
INC             =       -I/usr/include
CFLAGS          =       -O2 -Wall $(HARDEN) $(DEFINES) $(INC) $(LIB)


# DEBUG
ifeq ($(DEBUG), 1)
DBGFLAGS        =       -ggdb -DDEBUG
CFLAGS          +=      $(DBGFLAGS)
endif


# PLUGINS 
WITH_C_PLUGIN		=	0
WITH_PYTHON_PLUGIN	=	0

ifeq ($(WITH_C_PLUGIN), 1)
DEFINES			+=	-D_C_PLUGIN #-rdynamic
LDFLAGS			+=	-ldl
endif

ifeq ($(WITH_PYTHON_PLUGIN), 1)
DEFINES			+=	-D_PYTHON_PLUGIN
LDFLAGS			+=	-lpython2.7
INC			+=	-I/usr/include/python2.7
endif



# Compile rules
.PHONY : all check-syntax clean keys tags purge

.c.o :
	$(CC) $(CFLAGS) -c -o $@ $<

all : $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

purge:
	rm -fr $(OBJECTS) ./core-$(BIN)-*

clean: purge
	rm -fr $(BIN)

keys:
	make -C keys all

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

valgrind:  $(BIN)
	valgrind --leak-check=full --show-reachable=yes ./$(BIN) $(ARGS
