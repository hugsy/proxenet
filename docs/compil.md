## Compile proxenet

### Pre-requisites

`proxenet` requires a recent version of
[PolarSSL library](https://polarssl.org/source-code), on version 1.3 and
up. Support for version 1.2.x was definitively abandonned.

The choice for PolarSSL as main SSL development library came because of its easy
integration in multi-threaded environment, along with a simple (but yet
thoroughly documented) API.

Installing PolarSSL library is pretty straight-forward. Here with an example with version 1.3.9:
``` bash
~$ curl -s https://github.com/polarssl/polarssl/archive/polarssl-1.3.9.tar.gz | tar xfz -
~$ cd polarssl-1.3.9
~$ make && sudo make install
```

For most distro, a simple
```bash
$ apt-get install libpolarssl-dev   # for Debian-based Linux
or
$ yum install libpolarssl-devel     # for RedHat-based Linux
```
will be enough.


### Compilation
In order to build `proxenet` you will need to have
[CMake](http://www.cmake.org).

If you don't have it, just run
```bash
$ apt-get install cmake   # for Debian-based Linux
or
$ yum install cmake       # for RedHat-based Linux
```

Then, you can proceed with the compilation.

```bash
~$ git clone https://github.com/hugsy/proxenet.git
~$ cd proxenet && cmake . && make
```

`cmake` will generate the `Makefile` accordingly to your configuration and
libraries availableon your system.
If you want to explicity enable/disable scripting supports, use the option `-D`
when using `cmake`. For example, to disable C script support, simply type
```bash
~$ cmake -DUSE_C_PLUGIN=OFF . && make
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
If you do not want to download the repository, you will need to create manually an `autoload` subdirectory insite the `proxenet-plugins` to pass the check.
```bash
~/proxenet$ mkdir -p proxenet-plugins/autoload
```

Then, you will be able to start `proxenet`.
