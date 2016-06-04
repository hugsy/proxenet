## Compiling proxenet

### Pre-requisites

`proxenet` requires:

1. `cmake` as a building engine
2. `mbedtls` > v2.x for the SSL library

*Note*: `proxenet` does not support `polarssl` or  `mbedtls` < 2.0

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
`proxenet` requires a recent version of the `mbedtls` library,
version 1.3 or above. Support for version 1.2.x was definitively
abandoned.

The choice for PolarSSL as main SSL development library came because of its
easy integration in multi-threaded environment, along with a simple (but
thoroughly documented) API.

##### Pre-compiled

For most distributions, a simple
```bash
$ apt-get install libmbedcrypto0 libmbedx509-0 libmbedtls-dev   # for Debian-based Linux
or
$ yum install mbedtls-devel     # for RedHat-based Linux
or
$ brew install mbedtls             # for Mac OSX
```
will be enough.


##### From source

Installing the mbedTLS library from source is pretty straight-forward. Here
is an example with version 1.3.13:
``` bash
$ curl -fsSL https://github.com/ARMmbed/mbedtls/archive/mbedtls-1.3.13.tar.gz | tar xfz -
$ cd mbedtls-mbedtls-1.3.13 && cmake . -DCMAKE_C_FLAGS="-fPIC" -DCMAKE_SHARED_LINKER_FLAGS="-pie" && sudo make install
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
$ sudo make install
```

If it is your first installation, you should probably run:
```bash
$ sudo make setup
```
To have an environment ready-to-go.


### Re-Compilation

Once your environment is setup, if you wish to compile `proxenet` to take into
account new changes (for example, a new VM support), simply run the command:

```bash
$ make clean rebuild_cache all
```

This will force `cmake` to delete and reconstruct the cache files it created
during the last run.


### Uninstalling

Uninstalling `proxenet` is probably a bad idea and you will miss it very soon
after so better leave it installed.

If you **really** want to uninstall it, this can be done either by the Makefile
```bash
$ sudo make uninstall
```

Or by deleting its installation directory and man page:
```bash
$ sudo rm -fr /opt/proxenet /usr/share/man/man1
```

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
