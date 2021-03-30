// Replace sprintf with snprintf and assert trailing '\0'
@@
expression buffer, formatstring;
expression list args;
@@
- sprintf(buffer, formatstring, args);
+ snprintf(buffer, sizeof(buffer), formatstring, args);
+ buffer[sizeof(buffer)-1] = 0;
