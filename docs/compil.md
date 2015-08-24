## Compiling proxenet

### Pre-requisites

`proxenet` requires:

1. `cmake` as a building engine
2. `polarssl` (now known as mbed TLS) for the SSL library


#### CMake

##### Linux

Most distributions will provide `cmake` with their main packaging system.

If you don't have it, just run
```bash
$ apt-get install cmake   # for Debian-based Linux
or
$ yum install cmake       # for RedHat-based Linux
```

##### Mac OSX

The best and easiest way to install `cmake` on OSX is through `brew`.
```bash
$ brew install cmake
```

##### FreeBSD

From FreeBSD 10 and up, `cmake` is provided through `pkg`
```bash
$ pkg install cmake
```

#### PolarSSL
`proxenet` requires a recent version of the PolarSSL library (now called mbed
TLS), version 1.3 or above. Support for version 1.2.x was definitively
abandoned.

The choice for PolarSSL as main SSL development library came because of its
easy integration in multi-threaded environment, along with a simple (but
thoroughly documented) API.

##### Pre-compiled

For most distributions, a simple
```bash
$ apt-get install libpolarssl-dev   # for Debian-based Linux
or
$ yum install libpolarssl-devel     # for RedHat-based Linux
or
$ brew install polarssl             # for Mac OSX
```
will be enough.

*Note*: FreeBSD provides by default an old version of PolarSSL (1.2
branch). This branch is not supported anymore, so please install from source
as explained below.


##### From source

Installing the PolarSSL / mbed TLS library from source is pretty straight-forward. Here
is an example with version 1.3.9:
``` bash
$ curl -fsSL https://github.com/ARMmbed/mbedtls/archive/polarssl-1.3.9.tar.gz | tar xfz -
$ cd mbedtls-polarssl-1.3.9 && cmake . -DCMAKE_C_FLAGS="-fPIC" -DCMAKE_SHARED_LINKER_FLAGS="-pie" && sudo make install
```

Later versions of mbed TLS can be obtained from the
[GitHub releases page](https://github.com/ARMmbed/mbedtls/releases).

#### VM support

By default, `cmake` will try to find all the libraries already installed on the
system to enable specific plugin. Without any additional libraries installed,
the `C` plugin will be the minimum available.

If you wish to compile `proxenet` with all the VM it currently supports, try the
following command:


##### For Red-Hat based distributions


```bash
$ sudo yum install \
  ruby-devel \                    # for compiling with Ruby plugin support
  lua-devel \                     # for compiling with Lua plugin support
  java-1.8.0-openjdk-devel \      # for compiling with Java/OpenJDK plugin support
  python-devel \                  # for compiling with Python plugin support
  perl-devel                      # for compiling with Perl plugin support
```

##### For Debian based distributions


```bash
$ sudo apt-get install \
  ruby-dev \                      # for compiling with Ruby plugin support
  liblua5.2-dev \                 # for compiling with Lua plugin support
  java-1.8.0-openjdk-dev \        # for compiling with Java/OpenJDK plugin support
  python-dev \                    # for compiling with Python2 plugin support
  python3-dev \                   # for compiling with Python3 plugin support
  libperl-dev                     # for compiling with Perl plugin support
```


### Compilation
In order to build `proxenet` make sure you have [CMake](http://www.cmake.org)
version 3.0+.

You can proceed with the compilation like this:

```bash
$ git clone https://github.com/hugsy/proxenet.git
$ cd proxenet && cmake . && make
```

`cmake` will generate the `Makefile` for your configuration and the libraries
available on your system, accordingly.
If you want to explicitly enable/disable scripting supports, use the option
`-D` when using `cmake`. For example:
```bash
$ cmake . -DUSE_C_PLUGIN=OFF && make   # to disable C script support
or
$ cmake . -DUSE_PYTHON_PLUGIN=OFF && make   # to disable Python script support
```

If you want to install it, the following command will install `proxenet` (by
default in `/opt/proxenet`) and setup the environment too.
```bash
# make install
# make setup
```

### Re-Compilation

Once your environment is setup, if you wish to compile `proxenet` to take into
account new changes (for example, a new VM support), simply run the command:

```bash
$ make clean rebuild_cache all
```

This will force `cmake` to delete and reconstruct the cache files it created
during the last run.


### Setup the environment

To spawn `proxenet` the first time, you will need to :

1. generate your SSL keys (see "SSL Keys" section in the configuration section)
2. have a valid plugin directory tree. The best way to achieve this is by cloning the `proxenet-plugins` repository.
```bash
~/proxenet$ git submodule init
Submodule 'proxenet-plugins' (https://github.com/hugsy/proxenet-plugins.git) registered for path 'proxenet-plugins'
~/proxenet$ git submodule update
Cloning into 'proxenet-plugins'...
remote: Counting objects: 32, done.
remote: Compressing objects: 100% (27/27), done.
remote: Total 32 (delta 12), reused 15 (delta 4)
Unpacking objects: 100% (32/32), done.
Checking connectivity... done.
Submodule path 'proxenet-plugins': checked out 'b7fa32a72d7e938d891ac393f30b497d6ceaf37d'
```
This will populate the `proxenet-plugins` directory with the right setup tree and add some useful plugins.
If you do not wish to download the repository, you will need to create  an `autoload` subdirectory manually, inside the `proxenet-plugins` to pass the check.
```bash
~/proxenet$ mkdir -p proxenet-plugins/autoload
```

Congratz! Your installation of `proxenet` is ready to go. Let the fun begin...
