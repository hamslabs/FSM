/**	
 ****************************************************************************************
 *  @file fsm.h
 *  @brief Implements a generic finite-state-machine.
 *
 *	Basic Operation:
 *
 *	This module implements an API to create and manage a finite-state-machine.
 *	The finite-state-machine (FSM) is created at run-time, and therefore
 *	allocates memory at run-time. All required memory is allocated during the
 *	execution of fsmCreateFsm().
 *
 *	The FSM implements the notions of states, transitions, actions, and conditions.
 *
 *	The storage of an object's current state identifier is stored external to this module,
 *	therefore leaving the management of the current state up to the application.
 *
 *	An object transitions to a given state upon the reception of an event if
 *	a Transition object exists in the FSM that matches the object's current state,
 *	the Transition event matches the current event and, if there is a transition
 *	Condition function, the transition Condition function completes successfully.
 *
 *	If an object is transitioning from one state to another, then the transition's action functions
 *	are called, in sequence. If all action functions return FSM_TRUE, then the current
 *	state's exit action function is called, if there is one. Finally, the new state's 
 *	entry action function is called.
 *
 *	If an action function returns FSM_FALSE, then no other action functions in the transition 
 *	are called. The FSM will first check to see if there is a 'Catch' transition specified
 *	for the state (created by a call to fsmCatchTransaction). If there is one, then the
 *	FSM will execute the action functions in the Catch transition, then call the current state's exit action
 *	function, and then call the new state's entry action function. If there is no 'Catch' transition,
 *	then the FSM will remain in the current state.
 *
 *	The fsmCatchTransition() provides the means of adding an error 'catch' transition handler. The catch
 *	transition is executed only if an action return FSM_FALSE during the execution of a transition's
 *	action functions.
 *
 *	The fsmCreateAnyState() provides the means to create transitions that can match a given event id
 *	independent of the current state. This can be used to process events that have the same actions,
 *	regardless of the current state.
 *
 *  Example (phone state machine): The following is a partial state-machine for driving a telephone.
 *   The STATE_DIALING state provides an example of using a Conditional function in a transition to
 *   specify multiple transitions for the same event/state that is based on the result of the
 *   Condition function. If no digits have been dialed, then conditionHaveDigits() will return FSM_FALSE,
 *   which means the Transition to STATE_ORIGINATING does not occur. Since the Condition function fails,
 *   the FSM continues to search for a Transition object match. The Transition to STATE_ERROR matches
 *   the event and current state, so it's transition occurs.
 *
 *
 *	if (NULL == (g_GwFsm = fsmCreateFsm()))
 *	    return FSM_FALSE;
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_IDLE, FSM_NULL_ENTRY, FSM_NULL_EXIT);
 *
 *	fsmTransition(l_hState,	EVENT_INBOUND_CALL, FSM_NO_CONDITION,   STATE_RINGING,  actionEnableRinger, FSM_END);
 *
 *	fsmTransition(l_hState,	EVENT_OFFHOOK,      FSM_NO_CONDITION,   STATE_DIALING, 	actionPlayDialtone, FSM_END);
 *
 *	fsmCatchTransition(l_hState, STATE_ERROR, actionHangup, FSM_END);
 *
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_RINGING, FSM_NULL_ENTRY, exitDisableRinger);
 *
 *	fsmTransition(l_hState, EVENT_OFFHOOK,      FSM_NO_CONDITION,   STATE_TALKING,  FSM_END);
 *
 *	fsmTransition(l_hState,	EVENT_CALL_IDLE,    FSM_NO_CONDITION,   STATE_IDLE,     FSM_END);
 *
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_DIALING, entryStartDialingTimer, FSM_NULL_EXIT);
 *
 *	fsmTransition(l_hState, EVENT_DIGIT,        conditionValidDigit, FSM_ST_SAME,  actionStopDialtone, FSM_END);
 *
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      conditionHaveDigits, STATE_ORIGINATING,  actionPlayRingback, actionOriginateCall, FSM_END);
 *
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      FSM_NO_CONDITION,    STATE_ERROR,        actionPlayErrorTone,FSM_END);
 *
 *	fsmTransition(l_hState, EVENT_DIGITDONE,	conditionDigitsDone, FSM_ST_SAME, actionSendDialingDone, FSM_END);
 *
 *
 *	l_hState = fsmCreateAnyState(l_hFsm);
 *	
 *	fsmTransition(l_hState, EVENT_ERROR_TONE, FSM_NO_CONDITION, STATE_ERROR,        actionIdleSystem,FSM_END);
 *
 *   ...
 *
 *
 *  The fsmCreateComplexState() is used to create complex/nested state-machines. If a state contains
 *  substates, then this API is used. This can be used to nest state-machines, and/or to re-use sub-state-machines
 *  in multiple places. The following example is an out-dialer, which uses a sub-state-machine for dialing. 
 *  This may be used if dialing is required by multiple places in the state-machine. When the state-machine enters the
 *  state STATE_ORIGINATE_CALL, the state-machine also enters the nested state STATE_DIALING_START. Any transitions that
 *  are specified within a parent state are automatically inherited by the sub-states. Therefore, if EVENT_ERROR_TONE occurs
 *  while the state machine is in STATE_DIALING_DELAY, then STATE_DIALING_DELAY is exitted (exit function called), STATE_ORIGINATE_CALL
 *  is exitted (exit function called), and the new state is STATE_IDLE. When the sub-state-machine completes, it sends
 *  an event EVENT_DIALING_DONE that is handled by the parent state, causing a state change after dialing is completed.
 *
 *  Any other state(s) of the state-machine may use the dialing sub-state machine by defining itself as a complex state,
 *  by using fsmCreateComplexState().
 *
 *	if (NULL == (g_GwFsm = fsmCreateFsm()))
 *	    return FSM_FALSE;
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_IDLE, FSM_NULL_ENTRY, FSM_NULL_EXIT);
 *	fsmTransition(l_hState,	APP_EVENT_ORIGINATE_CALL, FSM_NO_CONDITION,   STATE_ORIGINATE_CALL, actionPlayDialtone, FSM_END);
 *	fsmCatchTransition(l_hState, STATE_ERROR, actionHangup, FSM_END);
 *
 *
 *	l_hState = fsmCreateComplexState(g_GwFsm, STATE_ORIGINATE_CALL, STATE_DIALING_START, FSM_NULL_ENTRY, FSM_NULL_EXIT);
 *	fsmTransition(l_hState,	EVENT_ERROR_TONE,    FSM_NO_CONDITION,   STATE_IDLE,     actionHangup, FSM_END);
 *	fsmTransition(l_hState,	EVENT_DIALING_DONE,  FSM_NO_CONDITION,   STATE_TALK,     FSM_END);
 *
 *  // The next three states are a re-usable sub-state-machine for dialing.
 *  //
 *	l_hState = fsmCreateState(g_GwFsm, STATE_DIALING_START, entryStartDialingTimer, FSM_NULL_EXIT);
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      FSM_NO_CONDITION, STATE_DIALING_DIGIT,  actionPlayDigitTone, FSM_END);
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_DIALING_DIGIT, entryStartDigitTimer, exitPlaySilence);
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      FSM_NO_CONDITION, STATE_DIALING_DELAY,  FSM_END);
 *
 *	l_hState = fsmCreateState(g_GwFsm, STATE_DIALING_DELAY, entryStartInterDigitTimer, FSM_NULL_EXIT);
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      condMoreDigits,   STATE_DIALING_DIGIT,  actionPlayDigitTone, FSM_END);
 *	fsmTransition(l_hState,	EVENT_TIMEOUT,      FSM_NO_CONDITION, FSM_ST_SAME, actionSendDialingDoneEvent, FSM_END);
 *
 *     ...
 *
 *
 *
 ****************************************************************************************/
 
#ifndef _FSM_H_
#define _FSM_H_
 
typedef void*	FSM_APP_CONTEXT;	/**< FSM Application Context pointer, used as parameter to transition functions. */
typedef void*	FSM_HANDLE;			/**< Handle to a finite-state-machine */


// Generic Timeout event
#define EVENT_TIMEOUT (0xFFFFBEEF)

/**
 * Defines boolean returns used by FSM APIs.
 */
typedef enum 
{
	FSM_FALSE = 0,
	FSM_TRUE,
} FSM_BOOL;

/**
 * Defines errors that may be found during state-table verification.
 */
typedef enum
{
	eFSM_VERIFY_ERROR_NONE		= 0,		/**< No verification problems found. */
	eFSM_VERIFY_ERROR_NOENTRY,				/**< Specified state is never entered. Occurs if no transitions enter the specified state. */
	eFSM_VERIFY_ERROR_NOEXIT,				/**< Specified state cannot be exitted. Occurs if no transitions exit the specified state. */
} FSM_VERIFY_ERROR;

/**
 * Defines return values from fsmExecute().
 */
typedef enum
{
	eFSM_RESULT_NEW_STATE = 0,		        /**< Returned by fsmExecute() if object has transitioned to a new state. New state is found in the FSM_OBJECT_STATE object. */
	eFSM_RESULT_NO_CHANGE,				    /**< Returned by fsmExecute() if object has not transitioned to a new state. */
	eFSM_RESULT_NO_TRANSITION,				/**< Returned by fsmExecute() if no transition was found for the specified event and object's current state. */
    eFSM_RESULT_ACTION_FAILURE,             /**< Returned by fsmExecute() if an action failed in the transition and no catch-transition exists. */
    eFSM_RESULT_INTERNAL_FAILURE,           /**< Returned by fsmExecute() if a state-machine failure occurred (internal error) */
} FSM_EXECUTION_RESULT;

#define FSM_MAX_STATE_NEST_DEPTH    4       /**< Maximum depth of a state machine state nest */

/**
 * Used by application to maintain the object state. Passed to fsmExecute() to
 * run state-machine on the specified state.
 */
typedef struct
{
    int iNestDepth;                         /**< contains current state nest depth (0 if no nesting occurred, maximum of FSM_MAX_STATE_NEST_DEPTH) */
    int iNestedStateIds[FSM_MAX_STATE_NEST_DEPTH];
                                            /**< Contains state ids of super states and current state */
    int iPreviousStateId;                   /**< State ID of last state exitted */                                            
} FSM_OBJECT_STATE;

   
	/* returns 1 for true, 0 for false */
typedef FSM_BOOL	(*FSM_FUNC_ACTION)		(FSM_APP_CONTEXT AppContext, void* pAppMsg);	/**< Transition action function prototype. */
typedef FSM_BOOL	(*FSM_FUNC_CONDITION)	(FSM_APP_CONTEXT AppContext, void* pAppMsg);	/**< Transition condition function prototype. */
typedef void		(*FSM_FUNC_ENTRY)		(FSM_APP_CONTEXT AppContext);					/**< State entry action function prototype. */
typedef void		(*FSM_FUNC_EXIT)		(FSM_APP_CONTEXT AppContext);					/**< State exit action function prototype. */
typedef void		(*FSM_FUNC_VERIFY_CB)	(int iStateId, FSM_VERIFY_ERROR eFsmVerifyErr);	/**< State exit action function prototype. */


/**
 * The following are state ids that must not be used by applications:
 */
#define FSM_ST_SAME		    (int)-1			/**< Used as new state id in call to fsmTransition() to created a transition that does not change state. */
#define FSM_ST_PARENT       (int)-2         /**< Used by sub-state machines to exit the sub-state, but remain within the parent state */
#define FSM_ST_ANY			(int)-3			/**< Used (internally) as new state id creation of a 'any' state object. */



#define FSM_NULL_ENTRY			NULL			/**< Used as parameter to fsmCreateState() if state has no entry function. */
#define FSM_NULL_EXIT			NULL			/**< Used as parameter to fsmCreateState() if state has no exit function. */
#define FSM_END					NULL			/**< Used as last parameter to fsmTransition(). */
#define FSM_END_CONDITION		NULL			/**< Used as the terminating parameter to the condition function list. */
#define FSM_END_ACTION			NULL			/**< Used as the terminating parameter to the action function list. */
#define FSM_NO_CONDITION		NULL			/**< Used as parameter to fsmTransition(), if no transition condition. */

#ifdef __cplusplus
extern "C" {
#endif

/**	
 **********************************************************
 *  @brief Creates finite_state_machine.
 *	
 *	This function creates a finite state machine object.
 *
 *	@return	NULL if failure, else non-NULL pointer to FSM.
 *********************************************************/
FSM_HANDLE	fsmCreateFsm();

/**	
 **********************************************************
 *  @brief Initializes the object state with starting state.
 *	
 *********************************************************/
void fsmSetStartState(FSM_OBJECT_STATE *pstFsmObjectState, int iStartingStateId, int iPreviousStateId);

/**	
 **********************************************************
 *  @brief Returns current state, given the fsm object.
 *	
 *	@return	Current state of the FSM.
 *********************************************************/
int fsmGetCurrentState(FSM_OBJECT_STATE *pstFsmObjectState);

/**	
 **********************************************************
 *  @brief Returns current state from the top most nest depth, given the fsm object.
 *	
 *	@return	Current state of the FSM.
 *********************************************************/
int fsmGetCurrentTopLevelState(FSM_OBJECT_STATE *pstFsmObjectState);

/**	
 **********************************************************
 *  @brief Returns previous state, given the fsm object.
 *	
 *	@return	Previous state of the FSM.
 *********************************************************/
int fsmGetPreviousState(FSM_OBJECT_STATE *pstFsmObjectState);

/**
 **********************************************************
 *  @brief Verifies the specified state-machine.
 *	
 *	This function traverses the state-machine to search for any states
 *	that either are never exitted or never entered.
 *
 *	@param  hFsm            - handle to created FSM
 *  @param  fpVerifyCb		- Specifies pointer to callback function that will
 *								be called during verification for any errors that
 *								are found. If NULL, no verification occurs without
 *								callback.
 *
 *	@return	TRUE if verification passed, else FALSE if there was an error.
 *********************************************************/
FSM_BOOL	fsmVerifyFsm(FSM_HANDLE hFsm, FSM_FUNC_VERIFY_CB fpVerifyCb);

/**	@brief Determines if an error has occurred during FSM 
 *         creation.
 *	
 *  This function can be used at the end of FSM creation to
 *  determine if any errors occurred during the creation of the
 *  FSM.
 *
 *	@return	FSM_TRUE if a failure occurred during
 *          state or transition creation, else returns FSM_FALSE.
 *********************************************************/
FSM_BOOL	fsmHasCreateErrorOccurred(FSM_HANDLE hFsm);

/**	
 **********************************************************
 *  @brief Destroys finite_state_machine.
 *	
 *  This function deallocates all memory allocated during
 *  the creation of the FSM. The hFsm is invalid after 
 *  calling this function.
 *
 *  @param  hFsm    - handle to created FSM.
 *	@return	None
 *********************************************************/
void		fsmDestroyFsm(FSM_HANDLE hFsm);

/**	
 **********************************************************
 *  @brief Creates and adds a state to the state machine.
 *	
 *  This function creates a state on the specified FSM. It allocates
 *  memory for the state object.
 *
 *	@param  a_hFsm          - handle to created FSM
 *	@param  a_iStateId      - application's state identifier (can be any integer >= 0)
 *	@param  fpEntryAction	- FSM_FUNC_ENTRY state entry action that will be called on entry 
 *                            to the sate. FSM_NULL_ENTRY if no state entry function.
 *	@param  fpExitAction	- FSM_FUNC_EXIT state exit action that will be called on exit 
 *                            from the sate. FSM_NULL_ENTRY if no state exit function.
 *
 *	@return	Non-NULL handle of created state, else NULL if failure.
 *********************************************************/
FSM_HANDLE 	fsmCreateState(FSM_HANDLE a_hFsm, int a_iStateId, /* FSM_FUNC_ENTRY, FSM_FUNC_EXIT */ ...);

/**	
 **********************************************************
 *  @brief Creates and adds a complex/nested state to the state machine.
 *	
 *  This function creates a state on the specified FSM. It allocates
 *  memory for the state object. The state created is a complex state,
 *  which means that the state is a nested state (comprised of sub-states).
 *  The sub-state specified by a_iInitialSubStateId is the sub-state that
 *  is entered upon entering this complex state. Any transitions defined
 *  within the complex state are inherited by all sub-states nested within
 *  the complex state. When a complex state is entered, its entry function is called
 *  followed by its sub-state entry function. When a complex stated is exitted,
 *  the nested state exit functions are called from the bottom states up to
 *  the complex state on which the transition occurred.
 *
 *	@param  a_hFsm            - handle to created FSM
 *	@param  a_iStateId        - application's state identifier (can be any integer >= 0)
 *	@param  a_iInitialSubStateId - application's state identifier for the initial sub-state of this state (can be any integer >= 0).
 *                                  The initial sub-state is entered upon entry to this complex state.
 *                                  If set to FSM_ST_SAME, then there is no initial sub-state. Sub-States can be
 *                                  entered via a transition using fsmTransitionSub.
 *	@param  fpEntryAction	- FSM_FUNC_ENTRY state entry action that will be called on entry 
 *                            to the sate. FSM_NULL_ENTRY if no state entry function.
 *	@param  fpExitAction	- FSM_FUNC_EXIT state exit action that will be called on exit 
 *                            from the sate. FSM_NULL_ENTRY if no state exit function.
 *
 *	@return	Non-NULL handle of created state, else NULL if failure.
 *********************************************************/
FSM_HANDLE 	fsmCreateComplexState(FSM_HANDLE a_hFsm, int a_iStateId, int a_iInitialSubStateId, /* FSM_FUNC_ENTRY, FSM_FUNC_EXIT */ ...  );

/**	
 **********************************************************
 *  @brief Creates and adds a pseudo-state that contains transitions that can occur regardless of current state.
 *	
 *  This function creates a pseudo-state that can contain transitions that will execute on an
 *  event regardless of current state. This can be used as a catch all for error events
 *  that may cause the same actions to execute regardless of current state.
 *
 *	@param  hFsm            - handle to created FSM
 *
 *	@return	Non-NULL handle of created state, else NULL if failure.
 *********************************************************/
FSM_HANDLE 	fsmCreateAnyState(FSM_HANDLE hFsm);

/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created.
 *
 *      Creates a Transition object on the specified created state.
 *		If the Transition has a Condition function, then the
 *      Condition function must return FSM_TRUE in order for the Transition
 *      to occur. If the Condition function returns FSM_FALSE, then 
 *		the state-machine will then search for the next-matching Transition.
 *		If there are no other matching transitions, then no transition occurs.
 *		If there is another matching transition, then its Condition is executed and
 *		checked.
 *		This allows the creation of 'alternative transitions' so that if a transition
 *		fails, alternative transitions can be created to handle the event.
 *
 *      If a Transition matches, and its Condition function returns FSM_TRUE,
 *      then the current state's exit action function is called, then the
 *      Transition action functions are called, and finally the new state's
 *      entry action function is called.
 *
 *	@param  hState          - handle to created State object
 * 	@param  iEventId		- Id of the application event
 *	@param  fpCondition     - pointer to Condition function. FSM_NO_CONDITION if there is no
 *                            Conditional.
 *	@param  iNewStateId		- Id of the application state to transition to
 *	@param  fpActionFuncs   - function pointers to action functions of type FSM_FUNC_ACTION. Last entry
 *                            MUST be FSM_END.
 *
 *  @return FSM_FALSE if failed to create transition, else FSM_TRUE.
 *********************************************************/
FSM_BOOL fsmTransition(FSM_HANDLE hState, int iEventId, void* fpCondition, int iNewStateId, /* FSM_FUNC_ACTION */ ...);

/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created allowing
 *		   for multiple condition functions.
 *
 *		This function works just like "fsmTransition" , except that it
 *		allows for multiple conditions. Each condition function in the list
 *		must return TRUE in order for the next condition function to be
 *		called. If all condition functions return TRUE, the Transition
 *		will occur.
 *
 *	@param  hState          - handle to created State object
 * 	@param  iEventId		- Id of the application event
 *	@param  ... (var args)  - A terminated list of conditional function     - followed by
 *                            Id of the application state to transition to  - followed by
 *							  A terminated list of action functions.
 *
 *  @return FSM_FALSE if failed to create transition, else FSM_TRUE.
 *********************************************************/
FSM_BOOL fsmTransitionEx(FSM_HANDLE hState, int iEventId,  /* FSM_FUNC_CONDITION... int iNewStateId  FSM_FUNC_ACTION... */ ...);

/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created allowing
 *		   for multiple condition functions. The transition is to a sub-state machine,
 *         to allow nested state-machines. If this transition is called, then the current state
 *         becomes the parent state to the newly entered state. Events and transitions 
 *         handled by the parent state are inherited by the sub-state.
 *
 *         The Sub-State machine is exitted by transitioning to FSM_ST_PARENT.
 *
 *	@param  hState          - handle to created State object
 * 	@param  iEventId		- Id of the application event
 *	@param  fpCondition     - pointer to Condition function. FSM_NO_CONDITION if there is no
 *                            Conditional.
 *	@param  iNewStateId		- Id of the application state to transition to
 *	@param  fpActionFuncs   - function pointers to action functions of type FSM_FUNC_ACTION. Last entry
 *                            MUST be FSM_END.
 *
 *  @return FSM_FALSE if failed to create transition, else FSM_TRUE.
 *********************************************************/
FSM_BOOL fsmTransitionSub(FSM_HANDLE hState, int iEventId, void* fpCondition, int iNewStateId, /* FSM_FUNC_ACTION */ ...);

/**	
 **********************************************************
 *  @brief Creates an error 'catch' transition to a state that has already been created.
 *
 *      Creates an error 'catch' Transition object on the specified created state.
 *		The 'catch' Transition is invoked by the FSM if any action functions return FSM_FALSE
 *		on a Transition on the specified state. The 'catch' Transition is a means of handling
 *		errors that may occur in the execution of action functions in a state.
 *		The FSM ignores the return values of action functions in the 'catch' Transition.
 *
 *		Each state object may only have one (1) 'catch' transition.
 *
 *	@param  hState          - handle to created State object
 *	@param  iNewStateId		- Id of the application state to transition to
 *	@param  fpActionFuncs   - function pointers to action functions of type FSM_FUNC_ACTION. Last entry
 *                            MUST be FSM_END.
 *
 *  @return FSM_FALSE if failed to create transition, else FSM_TRUE.
 *********************************************************/
FSM_BOOL fsmCatchTransition(FSM_HANDLE hState, int iNewStateId, /* FSM_FUNC_ACTION */ ...);

/**	
 **********************************************************
 *  @brief Called by application upon reception of event to run the
 *          state-machine.
 *
 *  This function is called by the application to process an event on an
 *  object's current state. The FSM is executed. The FSM attempts to find
 *  a matching Transition object given the current state and event.
 *  A Transition object is considered a 'match' if its state id matches iCurrentStateId, its
 *  event id matches iEventId, and it either has no Conditional function or its Conditional function
 *  returns FSM_TRUE.
 *
 *	@param  hFsm            - handle to created FSM
 *	@param  pObjectState	- contains current and last state of object. updated by FSM if transition occurs
 *	@param  iEventId		- event id
 *	@param  AppContext		- application context passed to state entry/exit actions and transition actions
 *	@param  pAppMsg			- application message passed to transition actions
 *
 *  @return Returns eFSM_RESULT_NEW_STATE if transition match found, and transition specifies a new state.
 *		    Returns eFSM_RESULT_NO_CHANGE if transition match found, but transition specifies no state change. 
 *		    Returns eFSM_RESULT_NO_TRANSITION if no transition match found.
 *		    Returns eFSM_RESULT_ACTION_FAILURE if transition match found, but an action function returns FSM_FALSE and
 *							there is no 'catch' transition to handle the error.
 *
 *********************************************************/
FSM_EXECUTION_RESULT fsmExecute(FSM_HANDLE hFsm, FSM_OBJECT_STATE* pObjectState, int iEventId, FSM_APP_CONTEXT AppContext, void* pAppMsg);

#ifdef __cplusplus 
}
#endif

#endif /* _FSM_H_ */

/* EOF */
