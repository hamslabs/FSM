from fsm import FSM, FSMThreaded
import inspect
import timeit


        # global functions for debug info functions
def whoami():
    t = inspect.currentframe().f_back.f_code.co_name
    return t

def whosMyCaller():
    t = inspect.currentframe().f_back.f_back.f_code.co_name
    return t

        
FSM1 = \
[
'STATE_1',
[
 ['STATE_ANY', ['EntryAction0', 'EntryAction1'], ['ExitAction1', 'ExitAction0'],
  [ 
   ['EVENT4', ['condTrue', 'condTrue'], 'STATE_2' , ['actionTrue', 'actionTrue']],
  ]
 ],
 ['STATE_1', ['EntryAction0', 'EntryAction1'], ['ExitAction1', 'ExitAction0'],
  [ 
   ['EVENT1', ['condFalse', 'condFalse'], 'STATE_2' , ['actionTrue', 'actionTrue']],
   ['EVENT1', ['condFalse', 'condTrue'], 'STATE_2' , ['actionTrue', 'actionTrue']],
   ['EVENT1', ['condTrue', 'condTrue'], 'STATE_2' , ['actionTrue', 'actionTrue']],
   ['EVENT2', ['condTrue', 'condTrue'], 'STATE_2' , ['actionTrue', 'actionFalse']],
   ['EVENT3', ['condFalse'], 'STATE_2', ['actionTrue']],
   ['EVENT4', ['condFalse'], 'STATE_2', ['actionTrue']],
   
   ['EVENT_CATCH', None, 'STATE_2' , ['actionTrue', 'actionFalse']],
  ]
 ],
 ['STATE_2', ['EntryAction1'], ['ExitAction0', 'ExitAction1'],
  [ 
   ['EVENT3', ['condTrue', 'condTrue'], 'STATE_1' , ['actionTrue', 'actionTrue']],
   ['EVENT4', None, 'STATE_SAME' , ['actionTrue', 'actionTrue']],
   ['EVENT5', [], 'STATE_1' , ['actionFalse', 'actionTrue']],
  ]
 ]
]
]
        
class myStateMachine(FSMThreaded):
    
    def __init__(self, threaded = False):
        if threaded:
            FSMThreaded.__init__(self, FSM1) 
            self.eventRoutine = getattr(self, 'SendEvent')
        else:
            FSM.__init__(self, FSM1) 
            self.eventRoutine = getattr(self, 'OnEvent')
    
    def condTrue(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def condFalse(self):
        retVal = False
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def actionTrue(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def actionFalse(self):
        retVal = False
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def EntryAction0(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def EntryAction1(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def ExitAction0(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def ExitAction1(self):
        retVal = True
        if __debug__: self.dbgPrint('"%s" returning %s - called from "%s"' % (whoami(), retVal, whosMyCaller()))
        return retVal
    
    def Test(self):
        self.eventRoutine('EVENT4')
        self.eventRoutine('EVENT1')
        self.eventRoutine('EVENT5')
        self.eventRoutine('EVENT2')
        self.eventRoutine('EVENT4')
        self.eventRoutine('EVENT3')
        self.eventRoutine('EVENT5')
        self.eventRoutine('EVENT_EXIT')
        
    def Test1(self):
        for i in xrange(100):
            self.eventRoutine('EVENT1')
            self.eventRoutine('EVENT3')
            self.eventRoutine('EVENT1')
            self.eventRoutine('EVENT3')
            self.eventRoutine('EVENT1')
            self.eventRoutine('EVENT3')
            self.eventRoutine('EVENT1')
            self.eventRoutine('EVENT3')
            self.eventRoutine('EVENT1')
            self.eventRoutine('EVENT3')
        self.eventRoutine('EVENT_EXIT')
    
    
global a
if __name__ == "__main__":    
    setup = """
from __main__ import myStateMachine
from __main__ import FSM1
b = myStateMachine(True)
b.Start()
"""
    stmt = """
b.Test1()
"""
    try:
        a = myStateMachine(False)
    except ValueError, s:
        print 'ValueError %s' % s
    except AttributeError, s:
        print 'AttributeError %s' % s
    else:
        MyTimer = timeit.Timer(stmt, setup)
        iters = 10000
        myTime = MyTimer.timeit(iters)
        trans = 10 * iters
        print "%d transitions in %f seconds %f sec/trans %f trans/sec" % (trans, myTime, myTime/trans, trans/myTime)
        
        print a.currentStateName
    z = 1

