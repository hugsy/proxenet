# Java plugin

This page will explain how to write a java plugin for `proxenet`.


## Plugin skeleton

```java

public class MyPlugin
{

    public static String AUTHOR = "";
    public static String PLUGIN_NAME = "";


    public static byte[] proxenet_request_hook(int request_id, byte[] request, String uri){
        return request;
    }

    public static byte[] proxenet_response_hook(int response_id, byte[] response, String uri){
        return response;
    }

    public static void main(String[] args){
        return;
    }
}
```

`proxenet` cannot compile by itself to Java bytecode. So you need to generate
the `.class` file, like this:
```bash
$ javac MyPlugin.java
```

And copy the new `MyPlugin.class` to the *proxenet-plugins* directory.


## Example

```java
public class AddHeader
{

    public static String AUTHOR = "hugsy";
    public static String PLUGIN_NAME = "AddHeader";


    public static byte[] proxenet_request_hook(int request_id, byte[] request, String uri){
        String myReq = new String( request );
        String header = "X-Powered-By: java-proxenet";
        myReq = myReq.replace("\r\n\r\n", "\r\n"+header+"\r\n\r\n");
        return request;
    }

    public static byte[] proxenet_response_hook(int response_id, byte[] response, String uri){
        return response;
    }

    public static void main(String[] args){
        return;
    }
}
```
