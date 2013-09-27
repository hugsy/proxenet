# Proxenet

The idea behind Proxenet is to have a fully DIY web proxy for pentest. It is a
C-based proxy that allows you to interact with higher level languages for
modifying on-the-fly requests/responses sent by your Web browser.

It is still at a very early stage of development (but very active though), and
supports plugins in the following languages :
- C
- Python
- Lua
- Ruby (-ish)
- Perl (-ish)

(Maybe more to come)

Check the project Wiki for all details!


## Current status
[![Continuous Integration status](https://secure.travis-ci.org/hugsy/proxenet.png)](https://travis-ci.org/hugsy/proxenet)


### The best of both world ?

Some people might miss the beautiful interface some other GUI-friendly proxies
provide. So be it! Plug `proxenet` as a relay behind your favorite `Burp`,
`Zap`, `Proxystrike`, `abrupt`, etc. and enjoy the show!


## Write-Your-Own-Plugins

It is a fact that writing extension for `Burps` is a pain, and other tools only
provide plugins (when they do) in the language they were written in.
So the basic core idea behind `proxenet` is to allow pentesters to **easily**
interact with their HTTP requests/responses in their favorite high-level
language. 


## Languages Versions

Implemented with C API in :
- Python 2.7
- Ruby 1.8 (Ruby 1.9 and 2.0 yet to come)
- Perl 5.16
- Lua 5.2


## What's next?

There are heaps and heaps of bugs, crashes, etc., so I am working on fixing that to
provide more stability (please tell me your bugs/patches!)
Many many many other features to come as well through plugins!

Want to get more? Fork it, patch it, push it!
