from UserList import UserList
from Queue import Queue
from threading import Thread, _Timer
from time import *

## these are special tokens for the FSM
_token_STATE_SAME = 'STATE_SAME'
_token_STATE_ANY = 'STATE_ANY'
_token_EVENT_CATCH = 'EVENT_CATCH'
## to exit an instance of FSMThread
_token_EVENT_EXIT = 'EVENT_EXIT'

debug = True

class Enumerate(object):
    def __init__(self, names):
        for number, name in enumerate(names):
            setattr(self, name, number)
            
            
class MethodList(UserList):
    """ Convert a list of strings that represent methods to a
        list of the actual methods for the class that object is
    """
    def __init__(self, object,  methodNameList):
        UserList.__init__(self)
        
        # if the description had None for a list, return an empty list
        if not methodNameList:
            return;
        for methodName in methodNameList:
            try:
                method = getattr(object, methodName)
            except AttributeError:
                raise ValueError, 'action "%s" not found' % methodName
            else:
                if not callable(method):
                    raise AttributeError, 'action "%s" not callable' % methodName
                self.append(method)
            
class FSMTimer(_Timer):
    def __init__(self, interval, function, args=[], kwargs={}):
#        Thread.__init__(self, target=self._CustomTimerThread, args=[function, args], kwargs=kwargs)
        _Timer.__init__(self, interval, function, args, kwargs)
        self.timeLeft = interval
        self.paused = False
        
    def run(self):
        while True:
            self.finished.wait(1)
            if not self.finished.isSet():
                if not self.paused:
                    self.timeLeft = self.timeLeft - 1
                    if self.timeLeft <= 0:
                        self.function(*self.args, **self.kwargs)
                        break
        self.finished.set()
        
        
    def Pause(self):
        self.paused = True
        
    def Resume(self):
        self.paused = False
        
    def TimeRemaining(self):
        return self.timeLeft

class FSM(object):
    def __init__(self, FSMDescription):
        self.ParseFSMDescription(FSMDescription)
        self.currentStateName = self.initialStateName
        self.dbgLabel = 'FSM'
        
    def dbgPrint(self, string):
        print "%s - %s" % (self.dbgLabel, string)
    
    def GetCurrentState(self):
        return self.currentStateName

    def ParseFSMDescription(self, description):
        """ Parse the state machine description
            and return a list that the FSM will use for processing.
        """
        
        eFSM =  Enumerate(('initialState', 'stateList'))
        eState =  Enumerate(('name', 'entryActions', 'exitActions', 'transitions'))
        self.stateDict = {}
        
        # scan through list and get all state names and create an empty
        # dictionary entry for them, this allows 'nextState' to be verified
        # during the parsing of transitions (forward references are checked'
        # also check for duplicate stateNames
        for state in description[eFSM.stateList]:
            stateName = state[eState.name]
            if stateName in self.stateDict:
                raise ValueError, 'Duplicate State Name "%s"' % stateName
            
            # add the name to the dictionary
            self.stateDict[stateName] = None
            
        # validate initialstate
        self.initialStateName = description[eFSM.initialState]
        if self.initialStateName not in self.stateDict:
            raise ValueError, 'Initial State "%s" not Found' % self.initialStateName
        
            
        # parse the list of states and hand on to the generated
        # parsed description that the FSM will actually act on
        self.stateDict = dict([self.parseState(state) for state in description[eFSM.stateList]])
        
          
            
    def parseState(self, state):
        """ Parse the sequence that defines the state and return a sequence that
            FSM will use for processing
            the format of the state list is
            [ 'StateName', [ 'EntryAction', ...], ['ExitAction', ...], [Transitions, ...] ]
        """
        eState =  Enumerate(('name', 'entryActions', 'exitActions', 'transitions'))
        
        # Get the name
        stateName = state[eState.name]
        # convert to method list for entry actions
        entryActions = MethodList(self, state[eState.entryActions])
            
        # convert to method list for exit actions
        exitActions = MethodList(self, state[eState.exitActions])
            
        transitions = state[eState.transitions]
        transitionDict = {}
        for transition in transitions:
            (k,v) = self.parseTransition(transition)
            if k in transitionDict:
                # if the event already exist append the transition to the list
                transitionDict[k].append(v)
            else:
                # otherwise just create a new list entry with the transition
                transitionDict[k] = [v]
                    
        
        key = stateName
        value = [stateName, entryActions, exitActions, transitionDict]
        return (key, value)
            
            
    def parseTransition(self, transition):
        """ Parse a transition list and return a list used for FSM processing
            the format of the list is:
            [ 'eventName', ['condition0', ...], 'nextState', ['transtionAction', ...]]
        """
        eTrans =  Enumerate(('name', 'conditions', 'nextState', 'transitionActions'))
        eventName = transition[eTrans.name]
            
        conditionActions = MethodList(self,transition[eTrans.conditions])
                
        nextStateName = transition[eTrans.nextState]
        if (nextStateName not in self.stateDict) and (nextStateName != _token_STATE_SAME):
            raise ValueError, 'Target Next State "%s" not Found' % nextStateName
                
        transitionActions = MethodList(self, transition[eTrans.transitionActions])
        
        key = eventName
        value = [conditionActions, nextStateName, transitionActions]
        return (key, value)
        
        
        
    def performActionList(self, actionList):
        """ perform a list of actions without regard to their return value
            this is used for entry and exit actions
        """
        if actionList:
            [action() for action in actionList]
        
    def performConditionalActionList(self, actionList):
        """ perform a list of actions. Checks the return value of each action and
            terminates if an action returns False
            this is used for condition and transition actions
        """
        if actionList:
            for action in actionList:
                if not action():
                    return False
        return True
        
    def Start(self):
        eState =  Enumerate(('name', 'entryActions', 'exitActions', 'transitions'))
        
        state = self.stateDict[self.currentStateName]
        if __debug__: self.dbgPrint('Starting FSM in state "%s"' % state[eState.name])
        self.performActionList(state[eState.entryActions])
        
    def OnEvent(self, event):
        """ Process an event coming to the state Machine
        """
        eState =  Enumerate(('name', 'entryActions', 'exitActions', 'transitions'))
        eTrans =  Enumerate(('conditions', 'nextState', 'transitionActions'))
        
        state = self.stateDict[self.currentStateName]
        if __debug__: self.dbgPrint('Event "%s" seen in state "%s"' % (event, state[eState.name]))
        
        transitionDict = state[eState.transitions]
        if event not in transitionDict:
            if __debug__: self.dbgPrint('Event "%s" not handled in state "%s"' % (event, state[eState.name]))
            transitionList = []
        else:
            transitionList = transitionDict[event]
        
        # get the transitions for STATE_ANY and put them at the end of the list
        # does STATE_ANY exist
        if  _token_STATE_ANY in self.stateDict:
            # Get the state list
            anyState = self.stateDict[_token_STATE_ANY]
            # get the transaction dict
            anyTransitionDict = anyState[eState.transitions]
            # is the event handled
            if event in anyTransitionDict:
                # add the list of transactions that handle "event"
                transitionList.extend(anyTransitionDict[event])
        
        # if there are no transition in this state or STATE_ANY then return
        if not transitionList:
            if __debug__: self.dbgPrint('Event "%s" not handled in state "%s or STATE_ANY"' % (event, state[eState.name]))
            return False
        
        for transition in transitionList:
            if self.performConditionalActionList(transition[eTrans.conditions]):
                if __debug__: self.dbgPrint('Event "%s" Handled in state "%s"' % (event, state[eState.name]))
                if not self.performConditionalActionList(transition[eTrans.transitionActions]):
                    ## do the catch actions
                    if __debug__: self.dbgPrint('Event "%s" in state "%s" - Transition Actions returned False' % (event, state[eState.name]))
                    if _token_EVENT_CATCH in transitionDict:
                        # make the transition the catch transition
                        # perform the actions associated with the catch transition
                        # and fall through to the state transition logic below
                        if __debug__: self.dbgPrint('Performing "CATCH" transition')
                        transition = transitionDict[_token_EVENT_CATCH][0]
                        self.performActionList(transition[eTrans.transitionActions])
                    else:
                        ## stay in the same state and continue processing
                        if __debug__: self.dbgPrint('No "CATCH" transition - remaining in state "%s"' % self.currentStateName)
                        return False
                
                # get next state
                nextStateName = transition[eTrans.nextState]
                
                # only perform exit and entry actions if state explicitly is changed
                # even if is is to the same state but using the state name
                if nextStateName != _token_STATE_SAME:
                    self.currentStateName = transition[eTrans.nextState]
                    if __debug__: self.dbgPrint('transitioning to state "%s"' % self.currentStateName)
                    self.performActionList(state[eState.exitActions])
                    newstate = self.stateDict[self.currentStateName]
                    self.performActionList(newstate[eState.entryActions])
                    return True
                else:
                    if __debug__: self.dbgPrint('remaining in state "%s"' % self.currentStateName)
                    return True
            else:
                if __debug__: self.dbgPrint('Event "%s" in state "%s - Conditions not met"' % (event, state[eState.name]))
                
        if __debug__: self.dbgPrint('Event "%s" - no conditions met in state "%s"' % (event, state[eState.name]))
        return False
                
class FSMThreaded(FSM):        
    def __init__(self, FSMDescription):
        FSM.__init__(self, FSMDescription)
        self.dbgLabel = 'FSMThreaded'
        self.running = False
        self.myThread = Thread(target=self.threadMethod)
        self.myQueue = Queue(100)
        self.paused = False
    
    def threadMethod(self):
        self.threadDone = False
        while not self.threadDone:
            event = self.myQueue.get(True)
            if event == _token_EVENT_EXIT:
                self.running = False
                return
            self.OnEvent(event)
            
        
    def Start(self):
        if self.running:
            raise RuntimeError
        else:
            self.running = True
        self.myThread.start()
        FSM.Start(self)
        
    def isAlive(self):
        return self.myThread.isAlive()
    
    def Pause(self):
        self.paused = True
        
    def Resume(self):
        self.paused = False
        
    def SendEvent(self, event):
        if not self.paused and self.running:
            self.myQueue.put(event)
        
    def SendExitEvent(self):
        self.SendEvent(_token_EVENT_EXIT)
        return True
        

        
        
            
