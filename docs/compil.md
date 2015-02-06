## Compile proxenet

Best way to set up a new `proxenet` environment is as this

```
$ git clone https://github.com/hugsy/proxenet.git
$ cd proxenet
$ make
```

``` Makefile ``` will attempt to find available librairies to enable plugins support (Python, Ruby, etc.).
You will need to have the correct libraries installed on your system to compile and link it properly (see Language Versions part). It relies on ``` pkg-config ``` for find flags and librairies used for compilation. Make sure your ``` PKG_CONFIG_PATH ``` is correctly setup.

Then, simply type ``` make ```.

To disable debug output (very verbose), type instead ``` make DEBUG=0```.
