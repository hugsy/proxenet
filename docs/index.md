# proxenet

## Why ?
The idea behind `proxenet` is to have a fully
[DIY](https://en.wikipedia.org/wiki/Do_it_yourself) web proxy for pentest. It is
a C-based proxy that allows you to interact with higher level languages for
modifying on-the-fly requests/responses sent by your Web browser.

Still under development, it offers the possibility to interact with the
following languages:

- C
- Python
- Lua
- Ruby
- Perl
-Tcl

And more to come


## The best of both world ?

Some people might miss the beautiful interface some other GUI-friendly proxies
provide. So be it! Plug `proxenet` as a relay behind your favorite `Burp`,
`Zap`, `Proxystrike`, `abrupt`, etc. and enjoy the show!


## Write-Your-Own-Plugins

If you ever had to do, you already know that writing extension for `Burp` is a
pain and other tools only provide plugins (when they do) in the language they
were written in.

So the simple but powerful idea behind `proxenet` is to allow pentesters to
**easily** interact with their HTTP requests/responses in their favorite
high-level language.


## Languages Versions

The current version of `proxenet` has been tested with:

- Python 2.7/3.3
- Ruby 1.8/1.9
- Perl 5.14
- Lua 5.2


## Want to help

Report crashes or improvement patches using the GitHub issues page of the
project.
