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
Best way to set up a new `proxenet` environment is as this

```bash
$ git clone https://github.com/hugsy/proxenet.git
$ cd proxenet
$ make
```

``` Makefile ``` will attempt to find available librairies to enable plugins support (Python, Ruby, etc.).
You will need to have the correct libraries installed on your system to compile and link it properly (see Language Versions part). It relies on ``` pkg-config ``` for find flags and librairies used for compilation. Make sure your ``` PKG_CONFIG_PATH ``` is correctly setup.

Then, simply type ``` make ```.

To disable debug output (very verbose), type instead ``` make DEBUG=0```.
