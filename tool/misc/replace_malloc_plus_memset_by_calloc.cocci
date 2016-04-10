@@
expression size, src;
@@
- src = malloc(size)
+ src = calloc(size)
...
- memset(src, 0, size);

@@
expression size;
@@
- calloc(size)
+ calloc(size, 1)