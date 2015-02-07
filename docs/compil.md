## Compile proxenet

### Pre-requisites
The only requirement for compiling successfully `proxenet` is to install a recent version of [PolarSSL library](https://polarssl.org/source-code). `proxenet` relies on PolarSSL API from version 1.3 and up. Support for version 1.2.x was abandonned.

The choice for PolarSSL as main SSL development library came because of its easy integration in multi-threaded environment, along with a simple (but yet thoroughly documented) API.

Installing PolarSSL library is pretty straight-forward. Here with an example with version 1.3.9:
``` bash
$ curl -s https://github.com/polarssl/polarssl/archive/polarssl-1.3.9.tar.gz | tar xfz -
$ cd polarssl-1.3.9
$ make && sudo make install
```

### Compilation
In order to build `proxenet` you will need to have [CMake](http://www.cmake.org)

```bash
$ git clone https://github.com/hugsy/proxenet.git
$ cd proxenet && cmake . && make
```

`cmake` will generate the `Makefile` accordingly to your configuration and libraries availableon your system.
If you want to explicity enable/disable scripting supports, use the option `-D` when using `cmake`. For example, to disable C script support, simply type
```bash
$ cmake -DUSE_C_PLUGIN=OFF . && make 
```
