use avdtp_sink_test:
A2DP/SNK/AS/BV-01-I : (wait)

A2DP/SNK/CC/BV-01-I : (wait)
A2DP/SNK/CC/BV-02-I : (wait)
A2DP/SNK/CC/BV-03-I : (wait)
A2DP/SNK/CC/BV-04-I : (wait)
A2DP/SNK/CC/BV-05-I : (wait)
A2DP/SNK/CC/BV-06-I : (wait)
A2DP/SNK/CC/BV-07-I : (wait)
A2DP/SNK/CC/BV-08-I : (wait)

use pts/avdtp_sink_test:
A2DP/SNK/REL/BV-01-I : (wait)
A2DP/SNK/REL/BV-02-I : S

A2DP/SNK/SET/BV-01-I : (wait)
A2DP/SNK/SET/BV-02-I : d, g, s, o
A2DP/SNK/SET/BV-03-I : (wait)
A2DP/SNK/SET/BV-04-I : d, g, s, o, m
A2DP/SNK/SET/BV-05-I : C, C, C, C
A2DP/SNK/SET/BV-06-I : (d, g, s, o, m, C, c) x 4

A2DP/SNK/SUS/BV-01-I : (wait)
A2DP/SNK/SUS/BV-02-I : P

A2DP/SNK/SUS/BV-01-I : (D, (confirmation)) x 4

IOPT/CL/A2DP-SNK/SFC/BV-02-I : 
    rm /tmp/btstack_*
    ./avdtp_sink_test
    (yes), c
