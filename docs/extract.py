#!/usr/bin/env python

import re

class Transition(object):
    def __init__ (self, fromState, toState, event, guard, action):
        self.fromState   = fromState
        self.toState     = toState
        self.event  = event
        self.guard  = guard
        self.action = action

statemachineName = ""
stateVariable = ""
numBraces = 0
fromState = ""
toState = ""
onEvent = ""
action = ""

def parseLine(line):
    global statemachineName
    global numBraces
    global stateVariable
    global fromState
    global toState
    global onEvent
    global action
    
    # looks for annotations
    ann = re.compile("(@\w*)\s*(\((.*)\))?")
    anns = ann.findall(line)
    if anns:
        if "@STATEMACHINE" in anns[0]:
            # print "statemachine, name:", anns[0][2]
            statemachineName = anns[0][2]
            numBraces = 0
            stateVariable = ""
        if "@ACTION" in anns[0]:
            action = anns[0][2]
                        
    # look for switch
    switch = re.compile("(switch)\s*\(\s*(.*)\s*\)")
    switches = switch.findall(line)
    if (switches):
        if numBraces == 0:
            stateVariable = switches[0][1]
            # print "switch on state opened", numBraces, stateVariable
        # if numBraces == 1:
            # print "switch on event opened", numBraces
        
    # count curly braces when inside statemachine
    if statemachineName:
        for c in line:
            before = numBraces
            if '{' in c:
                numBraces = numBraces + 1
            if '}' in c:
                numBraces = numBraces - 1            
            
    # look for case
    case = re.compile("(case)\s*(.*):")
    cases = case.findall(line)
    if cases:
        if numBraces == 1:
            # print "from state: " + cases[0][1]
            fromState = cases[0][1]
        if numBraces == 2:
            # print "on event: " + cases[0][1]
            onEvent = cases[0][1]
            toState = ""
            action = ""
            
    # look for break or return
    brk = re.compile("(break)")
    breaks = brk.findall(line)
    rtrn = re.compile("(return)")
    returns = rtrn.findall(line)
    if numBraces > 1:
        if breaks or returns:
            if not toState:
                toState = fromState
            # print "to state:", toState
            print fromState + "->" + toState + " [label=\"" + onEvent + "/" + action + "\"];"
    
    # look for state transition
    if stateVariable:
        stateTransition = re.compile( stateVariable + "\s*=\s*(.*);")
        transition = stateTransition.findall(line)
        if transition:
            toState = transition[0]
    

def parseFile(filename):
    f = open (filename, "r")
    for line in f:
        parseLine(line)
    f.close()

print "digraph fsm { "
print "size = \"8.5\""
parseFile("/Projects/iPhone/btstack/src/l2cap.c")
print "}"

