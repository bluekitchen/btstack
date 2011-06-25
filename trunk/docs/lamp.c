#include <stdio.h>

typedef enum {
    LAMP_OFF = 1,
    LAMP_ON
} state_t;

typedef enum {
    TOGGLE_ON = 1,
    TOGGLE_OFF
} event_t;

state_t state;

void statemachine(event_t ev){

    printf("State: %u, event: %u\n", state, ev);

    // @STATEMACHINE(lamp)    
    switch(state){
        case LAMP_OFF:
            switch(ev){
                case TOGGLE_ON:
                    // @ACTION(Turning lamp on)
                    printf("Turning lamp on\n");
                    state = LAMP_ON;
                    break;
                case TOGGLE_OFF:
                    // @ACTION(Ignoring toggle off)
                    printf("Ignoring toggle off\n");
                    break;
            }
            break;
        case LAMP_ON:
            switch(ev){
                case TOGGLE_ON:
                    // @ACTION(Ignoring toggle on)
                    printf("Ignoring toggle on\n");
                    break;
                case TOGGLE_OFF:
                    // @ACTION(Turning lamp off)
                    printf("Turning lamp off\n");
                    state = LAMP_OFF;
                    break;
            }
    }    
}

int main(void){
    state = LAMP_OFF;
    statemachine( TOGGLE_ON );
    statemachine( TOGGLE_ON );
    statemachine( TOGGLE_OFF );
}
