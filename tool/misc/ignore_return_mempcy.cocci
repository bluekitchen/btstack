// Ignore memcpy return
@@
expression E1, E2, E3;
@@
- memcpy(E1, E2, E3)
+ (void) memcpy(E1, E2, E3)
