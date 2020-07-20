//
// append 'u' suffix to decimal or hexadecimal constants
//

@r1@
// match decimal or hexadecimal constant without suffix 'u'
constant C =~ "^[(0x)0-9a-fA-F]+$";
@@
  C

@script:python p@
// define Cu := C + 'u'
C << r1.C;
Cu;
@@
coccinelle.Cu = coccinelle.C + 'u'

@@
// replace C with Cu
constant r1.C;
identifier p.Cu;
@@
- C
+ Cu

