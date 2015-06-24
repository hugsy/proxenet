# Rust plugin

Recent experiments have shown that it is possible to bind any language that
compile to native code to `proxenet`, as long as it exports some specific
symbols. This relies on one of most powerful advantages of `proxenet`, that is
being 100% written in `C`.

This page will detail how to write `proxenet` plugins in the [`Rust`](http://rust-lang.org/).
`Rust` will use its compiler (`rustc`) to generate native code that respects its
paradigms (multi-threadable, memory and type safe). Since `rust` allows to
generate shared objects, and explicitly exposing some exported symbols, binding
it on `proxenet` did not require any change to the `C` interface. The challenge
consisted in exporting the structure from the `C` level (and potentially unsafe)
to `rust` structure.

As a result, you can now use the following skeleton for writing your `rust`
plugin for `proxenet`.

Enjoy!


## Plugin skeleton


```rust
//
// Dummy Rust plugin for proxenet
// by @_hugsy_
//
// Compile with:
// $ rustc -o MyPlugin.so Myplugin.rs
//

#![crate_type = "dylib"]

use std::mem;


//
// proxenet request hook for Rust
// This function is type and memory safe.
//
fn rust_request_hook(request_id: u32, request: Vec<u8>, uri: String) -> Vec<u8>
{
    // Play your funky Rust here white boy!
    return request;
}


//
// proxenet response hook for Rust
// This function is type and memory safe.
//
fn rust_response_hook(response_id: u32, response: Vec<u8>, uri: String) -> Vec<u8>
{
    return response;
}


//
// Poor man strlen() compat function for Rust
//
fn rust_strlen(src: *const u8) -> usize
{
    let mut i = 0;
    let mut do_loop = true;

    while do_loop {
        let c = unsafe { *src.offset(i) as char };
        if c == '\x00' { do_loop = false; }
        else { i += 1; }
        }

    return i as usize;
}


//
// This function is used to transform and dispatch C compatible data type (unsafe) to
// Rust valid types, dispatch to the right function ({request,response}_hook).
//
fn generic_hook(rid: u32, buf: *mut u8, uri: *mut u8, buflen: *mut usize, is_request: bool) -> *mut u8
{
    unsafe {
        let rust_buf = Vec::from_raw_parts(buf, *buflen, *buflen);
        let rust_urilen = rust_strlen( uri );
        let rust_uri = String::from_raw_parts(uri, rust_urilen, rust_urilen);
        let ret;

        if is_request {
            ret = rust_request_hook (rid, rust_buf, rust_uri);
        } else {
            ret = rust_response_hook(rid, rust_buf, rust_uri);
        }

        *buflen = ret.len();
        return mem::transmute(&ret);
    }
}


#[no_mangle]
pub extern fn proxenet_request_hook(request_id: u32, request: *mut u8, uri: *mut u8, request_len: *mut usize) -> *mut u8
{
    return generic_hook(request_id, request, uri, request_len, true);
}


#[no_mangle]
pub extern fn proxenet_response_hook(response_id: u32, response: *mut u8, uri: *mut u8, response_len: *mut usize) -> *mut u8
{
    return generic_hook(response_id, response, uri, response_len, false);
}

```

>
> _Note_: not being a professional `rust` developper, maybe my approach is sub
> optimal. If so, shoot me an email with your improvement :)
>

The `C` plugin interface of `proxenet` must be used to load `rust` compiled
plugins. So all the plugins **must** be compiled first, and the shared library
**must** be suffixed with `.so`.

```bash
$ rustc  -o MyPlugin.so MyPlugin.rs
```

Move your compiled plugin to `proxenet` plugin directory, and fire it up.
