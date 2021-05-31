Tool: avdtp_sink_test

A2DP/SNK/AS/BV-01-I : (wait)
A2DP/SNK/AS/BV-02-I : (wait)

A2DP/SNK/CC/BV-01-I : (confirm)
A2DP/SNK/CC/BV-02-I : (confirm)
A2DP/SNK/CC/BV-03-I : (confirm)
A2DP/SNK/CC/BV-04-I : (confirm)
A2DP/SNK/CC/BV-05-I : (confirm)
A2DP/SNK/CC/BV-06-I : (confirm)
A2DP/SNK/CC/BV-07-I : (confirm)
A2DP/SNK/CC/BV-08-I : (confirm)

A2DP/SNK/REL/BV-01-I : (wait)
A2DP/SNK/REL/BV-02-I : S

A2DP/SNK/SDP/BV-02-I: (confirm)

A2DP/SNK/SET/BV-01-I : (wait)
A2DP/SNK/SET/BV-02-I : g, s, o
A2DP/SNK/SET/BV-03-I : (wait)
A2DP/SNK/SET/BV-04-I : g, s, o, m
A2DP/SNK/SET/BV-05-I : (C, (OK)) x 3
A2DP/SNK/SET/BV-06-I : (g, s, o, m, C, c) x 3

A2DP/SNK/SUS/BV-01-I : (wait)
A2DP/SNK/SUS/BV-02-I : P

A2DP/SNK/SYN/BV-01-I : (D, (confirm)) x 4

IOPT/CL/A2DP-SNK/SFC/BV-02-I : 
    rm /tmp/btstack_*
    ./avdtp_sink_test
    (yes), c
