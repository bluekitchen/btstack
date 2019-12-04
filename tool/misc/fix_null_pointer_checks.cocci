// Replace pointer null checks with boolen operations
// inverse version of https://github.com/neomutt/coccinelle/blob/master/null-check.cocci
// License: GPLv2 

@@
type T;
identifier I;
statement S1, S2;
expression E;
@@

T *I;

(
- if (!I)
+ if (I == NULL)
S1
|
- if (I)
+ if (I != NULL)
S1
|
- if (!I)
+ if (I == NULL)
S1 else S2
|
- if (I)
+ if (I != NULL)
S1 else S2
|
if (E) S1 else
- if (!I)
+ if (I == NULL)
S1 else S2
|
if (E) S1 else
- if (I)
+ if (I != NULL)
S1 else S2
)
