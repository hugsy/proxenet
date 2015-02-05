## proxenet plugins

### Format
This directory contains examples of plugins compatible that will work with `proxenet`. 

To be executed, a script must:
- be in a language supported by proxenet (C, Python, ...);
- have a name using this structure: 
    <priority><name>.<ext>
    - `priority` is an integer between 1 and 9 (1: high priority, 9: low priority)
    - `name` is the script name
    - `ext` is the extension as supported by `proxenet` (like `.py`, `.rb`, etc.)
  For example, a valid script name would be
    * 900MyScript.py -> prio:9, name:00MyScript, ext:py

Priority corresponds to the place the plugin will be inserted in the plugin
list. proxenet scheduler won't make any difference between plugins running with
a same level of priority.

### Loading
#### Autoload
When executed, `proxenet` will inspect the default directory and look for a sub-directory called `autoload`. 
Each script or symbolic link inside this sub-directory will be executed at loading.

#### Manual
Using the client (`control-client.py`) it is possible to add a new plugin at runtime. 
In the following example, we will load a new plugin, called `8LogReqRes.py`:
```bash
root@kali:~/proxenet# ./control-client.py
[*] 2015/02/04 22:12:03: Connected
Welcome on proxenet control interface
Type `help` to list available commands
>>>  plugin list
Plugins list:
|_ priority=8   id=1   type=Python2   [0x0] name=FilterEncodingHeader (ACTIVE)
>>>  plugin load 
Plugin '8LogReqRes.py' added successfully
>>>  plugin list
Plugins list:
|_ priority=8   id=1   type=Python2   [0x0] name=FilterEncodingHeader (ACTIVE)
|_ priority=9   id=2   type=Python2   [0x0] name=LogReqRes            (ACTIVE)
```
