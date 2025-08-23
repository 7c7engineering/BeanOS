# Bean Context component

A component that provides a context for the bean application, managing the internal state and information needed elsewhere in other components.

## Usage

To use the bean_context component, include the header file in your code and include the component in your build system.

### including the component
Inside `CMakeLists.txt`, add the following line to include the bean_context component:

```cmake
set(priv_requires "bean_context" ...)
idf_component_register(SRCS ...
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
```

## TODO's
 - Add internal context struct pointer to share queue-pointers, eventbits, and other resources.
