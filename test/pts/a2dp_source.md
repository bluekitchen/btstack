Tool: avdtp_source_test

A2DP/SRC/AS/BV-01-I : d, g, s, o, m, (confirm)
A2DP/SRC/AS/BV-02-I : d, g, s, o, m
A2DP/SRC/AS/BV-03-I : d, g, s, o, m, d, g, s, o, m

A2DP/SRC/CC/BV-09-I : (wait)
A2DP/SRC/CC/BV-10-I : (wait)

A2DP/SRC/REL/BV-01-I : g, s, o, m, S
A2DP/SRC/REL/BV-02-I : g, s, o, m

A2DP/SRC/SDP/BV-01 : (confirm)

A2DP/SRC/SET/BV-01-I : g, s, o
A2DP/SRC/SET/BV-02-I : (wait)
A2DP/SRC/SET/BV-03-I : g, s, o, m
A2DP/SRC/SET/BV-04-I : (wait)
A2DP/SRC/SET/BV-05-I : (g, s, o, m, C, c) x 4
A2DP/SRC/SET/BV-06-I : (C, (ok)) x 3

A2DP/SRC/SUS/BV-01-I : g, s, o, m, P
A2DP/SRC/SUS/BV-02-I : g, s, o, m

IOPT/CL/A2DP-SRC/SFC/BV-01-I : 
    rm /tmp/btstack_*
    ./avdtp_source_test
    (yes), c
