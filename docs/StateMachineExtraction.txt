
Project title: "Annotation-based state machine definition for doubly nested switch-case implementations"

- goals: testability, documentation
- assumptions: fixed code stucture
- input: some code (.c)
- output: graphviz state machine ...
- documentation: BTstack Wiki page
- implementation: Python
- problems: 
  - implicit event like "can send packet now", =? deal with guards?
  - "inverse handler" for "global" events

Example:
{
    // @STATEMACHINE(multiplexer)
    switch (multiplexer->state) {       // detect state variable, count {
        case W4_MULTIPLEXER:            // implicit state
            switch (event) {            // events start here
                case  L2CAP_OPEN:       // implicit event
                    // @ACTION(action description)
                    multiplexer->state = OTHER_STATE;
                    break;              // break || return -> end case block
            }
            break;
        case OTHER_STATE:
            break;
        }
    }
}
    
