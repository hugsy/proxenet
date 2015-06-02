# Plugins

This section will detail examples and skeletons for writing `proxenet` plugins
in your favorite languages.

## API and languages

Each page will provide:

1. A raw skeleton that you can copy for your new plugins;
2. A plugin example: adding a new header in your requests.


Equipped with those information, you will not have any problem getting along and
starting making powerful plugins!

The [plugin page](../plugin) explains the general structure for writing
`proxenet`. Please refer to it for generic information on the plugin behavior,
or jump directly to your favorite language page.


   - [Python](python.md)
   - [Ruby](ruby.md)
   - [Java](java.md)
   - [Perl](perl.md)
   - [C](c.md)
   - [Tcl](tcl.md)
   - [Lua](lua.md)

## Autoload

To be loaded properly, plugins should be located in the plugins directory. The
`autoload` directory should **only** contain symbolic links to files in the
regular plugins directory.

To auto-load a plugin,

```bash
$ cp MyPlugin.ext /path/to/proxenet/proxenet-plugins/
$ ln -sf /path/to/proxenet/proxenet-plugins/MyPlugin.ext /path/to/proxenet/proxenet-plugins/autoload/MyPlugin.ext
```
