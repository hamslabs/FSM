/**	
 *************************************************************************
 *  @file fsm.c
 *	@brief Implements a generic finite-state-machine.
 *
 *	This module implements an API to create and manage a finite-state-machine.
 *	The finite-state-machine (FSM) is created at run-time, and therefore
 *	allocates memory at run-time. All required memory is allocated during the
 *	execution of fsmCreateFsm().
 *
 *
 *
 *************************************************************************/

/** include files **/
#include <stdlib.h>
#include <stdarg.h>                /*variable arguments support*/
#include <string.h>
#include "fsm.h"


#define FSM_CATCH_TRANSITION		(int)-1			/**< Event id used in creation of a 'catch' transition object. */

/**
 * Defines a list of FSM objects (transitions, states, actions).
 */
typedef struct fsm_list_tag
{
	struct fsm_list_tag *pnext;         /**< pointer to next object in list */
} FSM_LIST;

/**
 * Entry in list of condition functions.
 */
typedef struct fsm_condition_tag
{
	FSM_LIST			listObj;        /**< pointer to next object in list */

	FSM_FUNC_CONDITION	fpCondition;	/**< pointer to condition function */
} FSM_CONDITION;

/**
 * Entry in list of action functions.
 */
typedef struct fsm_action_tag
{
	FSM_LIST		listObj;            /**< pointer to next object in list */

	FSM_FUNC_ACTION	fpAction;           /**< pointer to action function */
} FSM_ACTION;

/**
 * Entry in list of transition objects.
 */
typedef struct fsm_transition_tag
{
	FSM_LIST		listObj;			/**< pointer to next object in list */

	int				iNewStateId;		/**< application state id of state to transition to if event matches and conditional is met */
    FSM_BOOL        bSubState;          /**< If FSM_TRUE, then the transition is to a sub-state machine, meaning that
                                                the new stated is nested within the current-state. */

	int				iEventId;			/**< application event id */
	FSM_LIST		listConditions;		/**< condition function. transition does not occur if condition returns FALSE */
	FSM_LIST		listActions;		/**< list of action functions */
} FSM_TRANSITION;


/**
 * FSM main control object.
 */
typedef struct fsm_cb_tag
{
	FSM_LIST	listStates;		/**< list of state objects */
	FSM_BOOL	bCreateError;	/**< FSM_TRUE if error occurred in transition or state creation */
} FSM_CB;

/**
 * Entry in list of state objects.
 */
typedef struct fsm_state_tag
{
	FSM_LIST			listObj;

	FSM_CB*				pFsm;			    /**< pointer to parent state machine */
	int					iStateId;		    /**< application state id */
	FSM_FUNC_ENTRY		fpEntryAction;	    /**< function called upon entry to state */
	FSM_LIST			listTransitions;    /**< list of state transitions */
	FSM_FUNC_EXIT		fpExitAction;	    /**< function called upon exit from state */

    FSM_BOOL            bComplex;           /**< FSM_TRUE if this is a complex state (contains nested substates) */
    int                 iInitialSubStateId; /**< If bComplex is TRUE, contains the initial substate of the complex state */
} FSM_STATE;

/**
 * Arguments passed internally during execution of state-machine.
 * Used to reduce function arguments and parameter passing overhead.
 */
typedef struct fsm_execute_args_tag
{
	FSM_CB*				pFsm;			    /**< pointer to parent state machine */
    FSM_OBJECT_STATE    *pObjectState;      /**< pointer to state-machine status */
	int 			    iEventId;           /**< the current event being processed by state-machine */
	FSM_APP_CONTEXT     AppContext;         /**< Application context */
	void			    *pAppMsg;           /**< Pointer to application message block */
} FSM_EXECUTE_ARGS;


/* private functions */

static void* 				_fsmCreateObject(size_t a_Size);
static FSM_LIST*			_fsmGetState(FSM_LIST *a_pList, int a_iStateId);
static void 				_fsmAddObjectToList(FSM_LIST *a_pList, FSM_LIST *a_pObject);
static FSM_LIST*			_fsmGetTransition(FSM_LIST *a_pList, int a_iEventId);
static FSM_LIST* 			_fsmGetNextObject(FSM_LIST *a_pList);
static FSM_EXECUTION_RESULT _fsmExecuteTransition(	FSM_EXECUTE_ARGS *a_pExecuteArgs, 	
													FSM_STATE *a_pCurrentState,
													FSM_TRANSITION  *a_pTransition);
static void             	_fsmDeleteObject(void* a_pObject);



		    
/* public functions */


/**	
 **********************************************************
 *  @brief Creates finite_state_machine.
 *	
 *********************************************************/
FSM_HANDLE		fsmCreateFsm()
{
FSM_CB	*l_pFsm = NULL;

	l_pFsm = (FSM_CB*)_fsmCreateObject(sizeof(FSM_CB));
	return (FSM_HANDLE)l_pFsm;
}


/**	
 **********************************************************
 * Initializes the object state with starting state.
 *	
 * @param   a_pstFsmObjectState - pointer to fsm state object
 * @param   a_iStartingStateId - id of initial state
 * @param   a_iPreviousStateId - id of initial 'previous' state
 *********************************************************/
void fsmSetStartState(FSM_OBJECT_STATE *a_pstFsmObjectState, int a_iStartingStateId, int a_iPreviousStateId)
{
    if (NULL != a_pstFsmObjectState)
    {
        a_pstFsmObjectState->iNestDepth = 0;
        a_pstFsmObjectState->iNestedStateIds[0] = a_iStartingStateId;
        a_pstFsmObjectState->iPreviousStateId = a_iPreviousStateId;
    }        
}

/**	
 **********************************************************
 *  Returns current state, given the fsm object.
 *	
 *  @param   a_pstFsmObjectState - pointer to fsm state object
 *	@return	Current state of the FSM.
 *********************************************************/
int fsmGetCurrentState(FSM_OBJECT_STATE *a_pstFsmObjectState)
{
    if (NULL != a_pstFsmObjectState)
        return a_pstFsmObjectState->iNestedStateIds[a_pstFsmObjectState->iNestDepth];
    else
        return FSM_ST_SAME;
}

/**	
 **********************************************************
 *  Returns current state from the top most nest depth, given the fsm object.
 *	
 *  @param   a_pstFsmObjectState - pointer to fsm state object
 *	@return	Current state of the FSM.
 *********************************************************/
int fsmGetCurrentTopLevelState(FSM_OBJECT_STATE *a_pstFsmObjectState)
{
    if (NULL != a_pstFsmObjectState)
        return a_pstFsmObjectState->iNestedStateIds[0];
    else
        return FSM_ST_SAME;
}

/**	
 **********************************************************
 *  Returns previous state, given the fsm object.
 *	
 *  @param   a_pstFsmObjectState - pointer to fsm state object
 *	@return	Previous state of the FSM.
 *********************************************************/
int fsmGetPreviousState(FSM_OBJECT_STATE *a_pstFsmObjectState)
{
    if (NULL != a_pstFsmObjectState)
        return a_pstFsmObjectState->iPreviousStateId;
    return FSM_ST_SAME;
}


/**	
 **********************************************************
 *  Determines if the specified state id is ever entered by any
 *  transitions in the state machine. Used by fsmVerifyFsm().
 *  
 *  @param  a_pFsm      - pointer to state table
 *  @param  a_iStateId  - id of state to search for
 *
 *  @return FSM_TRUE if a transition exists that enters the state, else
 *          FSM_FALSE if the state is never entered.
 */
static FSM_BOOL _fsmIsStateEntered(FSM_CB *a_pFsm, int a_iStateId)
{
FSM_STATE	    *l_pState = NULL;
FSM_LIST 	    *l_pStateList = NULL;
FSM_LIST 	    *l_pTransitionList = NULL;
FSM_TRANSITION	*l_pTransition = NULL;

	if (NULL == a_pFsm)
		return FSM_FALSE;

    /* traverse all state objects and all transition objects to search
       for a transition that enters the specified state.
    */
	l_pStateList = &a_pFsm->listStates;
	while (NULL != (l_pStateList = _fsmGetNextObject(l_pStateList)))
	{
		l_pState = (FSM_STATE*)l_pStateList;		

        /* traverse transitions */
        l_pTransitionList = &l_pState->listTransitions;
    	while (NULL != (l_pTransitionList = _fsmGetNextObject(l_pTransitionList)))
    	{
    		l_pTransition = (FSM_TRANSITION*)l_pTransitionList;		
    		if (l_pTransition->iNewStateId == a_iStateId)
    			return FSM_TRUE;
    	}
    }

	return FSM_FALSE;
}

/**	
 **********************************************************
 *  Determines if the specified state id is ever exitted by any
 *  transitions in the state machine. Used by fsmVerifyFsm().
 *  
 *  @param  a_pFsm      - pointer to state table
 *  @param  a_iStateId  - id of state to search for
 *
 *  @return FSM_TRUE if a transition exists that exits the state, else
 *          FSM_FALSE if the state is never exitted.
 *********************************************************/
static FSM_BOOL _fsmIsStateExitted(FSM_CB *a_pFsm, int a_iStateId)
{
FSM_STATE	    *l_pState = NULL;
FSM_LIST 	    *l_pStateList = NULL;
FSM_LIST 	    *l_pTransitionList = NULL;
FSM_TRANSITION	*l_pTransition = NULL;

	if (NULL == a_pFsm)
		return FSM_FALSE;

    /* traverse all state objects and all transition objects to search
       for a transition that exits the specified state.
    */
	l_pStateList = &a_pFsm->listStates;
	while (NULL != (l_pStateList = _fsmGetNextObject(l_pStateList)))
	{
		l_pState = (FSM_STATE*)l_pStateList;		
        /* if the state object is for the specified state id, then
           search its transition objects.
        */
        if (l_pState->iStateId == a_iStateId)
        {
            l_pTransitionList = &l_pState->listTransitions;
        	while (NULL != (l_pTransitionList = _fsmGetNextObject(l_pTransitionList)))
        	{
        		l_pTransition = (FSM_TRANSITION*)l_pTransitionList;		
                /* if the transition specifies a new state id that is not
                   NO_CHANGE and also not the same as the existing state,
                   then this state is exitted, so return TRUE.
                */            
        		if ((l_pTransition->iNewStateId != a_iStateId) &&
                    (l_pTransition->iNewStateId != FSM_ST_SAME))
        			return FSM_TRUE;
        	}
        }
    }

	return FSM_FALSE;
}


/**	
 **********************************************************
 *  @brief Verifies the specified state-machine.
 *	
 *********************************************************/
FSM_BOOL	fsmVerifyFsm(FSM_HANDLE a_hFsm, FSM_FUNC_VERIFY_CB a_fpVerifyCb)
{
FSM_CB		    *l_pFsm = (FSM_HANDLE)a_hFsm;
FSM_BOOL        l_bRc = FSM_TRUE;
FSM_STATE	    *l_pState = NULL;
FSM_LIST 	    *l_pStateList = NULL;
FSM_LIST 	    *l_pTransitionList = NULL;
FSM_TRANSITION	*l_pTransition = NULL;

	if (NULL == l_pFsm)
		return FSM_FALSE;

    /* traverse the array of states to identify any states that are never entered.
       For each state in the state-list, search all transitions of all states to
       determine if the state is ever identified as a new state to a transition.
       If a transition is found that enters the state, then stop searching.
       If no transitions are found that enters the state, then mark as an error.
    */
	l_pStateList = &l_pFsm->listStates;
	while (NULL != (l_pStateList = _fsmGetNextObject(l_pStateList)))
	{
		l_pState = (FSM_STATE*)l_pStateList;		
		
		/* Ignore FSM_ST_ANY since it is not an application state-identifier */
		if (FSM_ST_ANY != l_pState->iStateId)
		{
			if (FSM_FALSE == _fsmIsStateEntered(l_pFsm, l_pState->iStateId))
			{
	            l_bRc = FSM_FALSE;
				if (a_fpVerifyCb)
	                (a_fpVerifyCb)(l_pState->iStateId, eFSM_VERIFY_ERROR_NOENTRY);
			}
		}
	}

    /* traverse the array of states and transitions to identify any states that 
       are never exitted. For each state in the state-list, search all transitions of 
       all states to determine if all states that are entered by a transition also
       have at least one exit transition. Therefore, the state id must have a state object
       and at least one transition that specifies a different new state.
    */
	l_pStateList = &l_pFsm->listStates;
	while (NULL != (l_pStateList = _fsmGetNextObject(l_pStateList)))
	{
		l_pState = (FSM_STATE*)l_pStateList;		

        l_pTransitionList = &l_pState->listTransitions;
    	while (NULL != (l_pTransitionList = _fsmGetNextObject(l_pTransitionList)))
        {
            l_pTransition = (FSM_TRANSITION*)l_pTransitionList;
			/* ignore state ids that are <= 0, since they are not
			   application state-identifiers.
			*/
            if (l_pTransition->iNewStateId >= 0) 
            {
                if (FSM_FALSE == _fsmIsStateExitted(l_pFsm, l_pTransition->iNewStateId))
                {
                    l_bRc = FSM_FALSE;
                    if (a_fpVerifyCb)
                        (a_fpVerifyCb)(l_pTransition->iNewStateId, eFSM_VERIFY_ERROR_NOEXIT);
                }
            }
        }
	}


	return l_bRc;
}

/**
 **********************************************************
 *  @brief Determines if an error has occurred during FSM 
 *         creation.
 *********************************************************/
FSM_BOOL	fsmHasCreateErrorOccurred(FSM_HANDLE a_hFsm)
{
FSM_CB		*l_pFsm = (FSM_HANDLE)a_hFsm;

	if (NULL == l_pFsm)
		return FSM_TRUE;

	return l_pFsm->bCreateError;
}

/**	
 **********************************************************
 *  @brief Destroys finite_state_machine.
 *
 *********************************************************/
void		fsmDestroyFsm(FSM_HANDLE a_hFsm)
{
FSM_CB		    *l_pFsm = (FSM_HANDLE)a_hFsm;
FSM_STATE	    *l_pState = NULL;
FSM_LIST 	    *l_pStateList = NULL;
FSM_LIST 	    *l_pNextState = NULL;
FSM_LIST 	    *l_pTransitionList = NULL;
FSM_LIST 	    *l_pNextTransition = NULL;
FSM_TRANSITION	*l_pTransition = NULL;
FSM_LIST 	    *l_pConditionList = NULL;
FSM_LIST 	    *l_pNextCondition = NULL;
FSM_CONDITION   *l_pCondition = NULL;
FSM_LIST 	    *l_pActionList = NULL;
FSM_LIST 	    *l_pNextAction = NULL;
FSM_ACTION      *l_pAction = NULL;

	if (NULL == l_pFsm)
		return;

	l_pStateList = _fsmGetNextObject(&l_pFsm->listStates);
	while (NULL != l_pStateList)
	{
        l_pNextState = _fsmGetNextObject(l_pStateList);

		l_pState = (FSM_STATE*)l_pStateList;		
        if (l_pState)
        {
            l_pTransitionList = _fsmGetNextObject(&l_pState->listTransitions);
            while (NULL != l_pTransitionList)
            {
                l_pNextTransition = _fsmGetNextObject(l_pTransitionList);

                l_pTransition = (FSM_TRANSITION*)l_pTransitionList;
                if (NULL != l_pTransition)
                {
                    l_pConditionList = _fsmGetNextObject(&l_pTransition->listConditions);
                    while (NULL != l_pConditionList)
                    {
                        l_pNextCondition = _fsmGetNextObject(l_pConditionList);
                        l_pCondition = (FSM_CONDITION*)l_pConditionList;
                        _fsmDeleteObject((void*)l_pCondition);
                        l_pConditionList = l_pNextCondition;                       
                    }

                    l_pActionList = _fsmGetNextObject(&l_pTransition->listActions);
                    while (NULL != l_pActionList)
                    {
                        l_pNextAction = _fsmGetNextObject(l_pActionList);
                        l_pAction = (FSM_ACTION*)l_pActionList;
                        _fsmDeleteObject((void*)l_pAction);
                        l_pActionList = l_pNextAction;                       
                    }
                
                    _fsmDeleteObject((void*)l_pTransition);
                    l_pTransitionList = l_pNextTransition;
                }
            }

            _fsmDeleteObject((void*)l_pState);
            l_pStateList = l_pNextState;
        }
    }
    _fsmDeleteObject((void*)l_pFsm);
}

/**	
 **********************************************************
 *  @brief Creates and adds a state to the state machine.
 *	
 *********************************************************/
FSM_HANDLE 	fsmCreateState(FSM_HANDLE a_hFsm, int a_iStateId, /* FSM_FUNC_ENTRY, FSM_FUNC_EXIT */ ...)
{
FSM_CB		*l_pFsm = (FSM_HANDLE)a_hFsm;
FSM_STATE	*l_pState = NULL;
va_list		l_args;			/* list of variable arguments */

	if (NULL == l_pFsm)
		return NULL;

	/* check state id to make sure that it isn't a reserved id */
	if (0 > a_iStateId)
	{
		l_pFsm->bCreateError = FSM_TRUE;
		return NULL;
	}

	/* make sure state doesn't already exist. If it does, then fail,
	   since the application may be trying to set different entry/exit
	   functions for the same state
	*/
	l_pState = (FSM_STATE*)_fsmGetState(&l_pFsm->listStates, a_iStateId);
	if (NULL != l_pState)
	{
		l_pFsm->bCreateError = FSM_TRUE;
		return NULL;
	}

	/* create state object */
	l_pState = (FSM_STATE*)_fsmCreateObject(sizeof(FSM_STATE));
	if (NULL == l_pState)
	{
		l_pFsm->bCreateError = FSM_TRUE;
		return NULL;
	}

	l_pState->iStateId = a_iStateId;
	l_pState->pFsm = l_pFsm;
    l_pState->bComplex = FSM_FALSE; /* simple/non-nested state */
    l_pState->iInitialSubStateId = FSM_ST_SAME; /* undefined initial substate id */

	/* create action objects for variable list */
	va_start(l_args, a_iStateId);	/* extract variable arguments */
		l_pState->fpEntryAction = (FSM_FUNC_ENTRY)va_arg(l_args, FSM_FUNC_ENTRY);
		l_pState->fpExitAction = (FSM_FUNC_EXIT)va_arg(l_args, FSM_FUNC_EXIT);
	va_end(l_args);			/* done with arguments */

	/* add state to list */
	_fsmAddObjectToList(&l_pFsm->listStates, (FSM_LIST*)l_pState);

	return (FSM_HANDLE)l_pState;
}

/**	
 **********************************************************
 *  @brief Creates and adds a complex/nested state to the state machine.
 *	
 *********************************************************/
FSM_HANDLE 	fsmCreateComplexState(FSM_HANDLE a_hFsm, int a_iStateId, int a_iInitialSubStateId, /* FSM_FUNC_ENTRY, FSM_FUNC_EXIT */ ...  )
{
FSM_STATE	*l_pState = NULL;
va_list		l_args;			/* list of variable arguments */
FSM_FUNC_ENTRY  l_pfnEntryAction = NULL;
FSM_FUNC_EXIT   l_pfnExitAction = NULL;

    /* get the entry and exit actions so that they can be passed along */
	va_start(l_args, a_iInitialSubStateId);	/* extract variable arguments */
		l_pfnEntryAction = (FSM_FUNC_ENTRY)va_arg(l_args, FSM_FUNC_ENTRY);
		l_pfnExitAction = (FSM_FUNC_EXIT)va_arg(l_args, FSM_FUNC_EXIT);
	va_end(l_args);			/* done with arguments */

    /* create simple state object */
    l_pState = (FSM_STATE*)fsmCreateState(a_hFsm, a_iStateId, l_pfnEntryAction, l_pfnExitAction);
    if (NULL == l_pState)
	{
        /* error flag already set by fsmCreateState */
        return NULL;
	}

    /* tag the state as a complex state and store the default initial substate */
    l_pState->bComplex = FSM_TRUE;
    l_pState->iInitialSubStateId = a_iInitialSubStateId;

	return (FSM_HANDLE)l_pState;
}


	
/**	
 **********************************************************
 *  @brief Creates and adds a pseudo-state that contains transitions that can occur regardless of current state.
 *	
 *********************************************************/
FSM_HANDLE 	fsmCreateAnyState(FSM_HANDLE a_hFsm)
{
FSM_CB		*l_pFsm = (FSM_HANDLE)a_hFsm;
FSM_STATE	*l_pState = NULL;

	if (NULL == l_pFsm)
		return NULL;

	/* If any-state already created, then return its handle so that the
	   application can add more transitions to it.
	*/
	l_pState = (FSM_STATE*)_fsmGetState(&l_pFsm->listStates, FSM_ST_ANY);
	if (NULL != l_pState)
	{
		return (FSM_HANDLE)l_pState;
	}

	/* create state object */
	l_pState = (FSM_STATE*)_fsmCreateObject(sizeof(FSM_STATE));
	if (NULL == l_pState)
	{
		l_pFsm->bCreateError = FSM_TRUE;
		return NULL;
	}

	l_pState->iStateId = FSM_ST_ANY;
	l_pState->pFsm = l_pFsm;
	l_pState->fpEntryAction = FSM_NULL_ENTRY;
	l_pState->fpExitAction = FSM_NULL_EXIT;

	/* add state to list */
	_fsmAddObjectToList(&l_pFsm->listStates, (FSM_LIST*)l_pState);

	return (FSM_HANDLE)l_pState;
}


/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created.
 *
 *********************************************************/
FSM_BOOL	fsmTransition
(
	FSM_HANDLE			a_hState, 
	int 				a_iEventId, 
	void*				a_fpCondition,
	int 				a_iNewStateId, 
	...					/* FSM_FUNC_ACTION */
)
{
FSM_STATE		*l_pState = (FSM_HANDLE)a_hState;
FSM_TRANSITION	*l_pTransition = NULL;
FSM_CONDITION	*l_pCondition = NULL;
FSM_ACTION		*l_pAction = NULL;
FSM_FUNC_ACTION l_fpAction = NULL;
va_list 		l_args;			/* list of variable arguments */

	if (NULL == l_pState)
		return FSM_FALSE;
		
	if (	(FSM_CATCH_TRANSITION == a_iEventId) 
		||	(FSM_ST_ANY == a_iNewStateId))
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* create a transition object */
	l_pTransition = (FSM_TRANSITION*)_fsmCreateObject(sizeof(FSM_TRANSITION));
	if (NULL == l_pTransition)
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* initialize and add transition to the state */
	l_pTransition->iNewStateId = a_iNewStateId;
    l_pTransition->bSubState = FSM_FALSE;
	l_pTransition->iEventId = a_iEventId;
	_fsmAddObjectToList(&l_pState->listTransitions, (FSM_LIST*)l_pTransition);

	/* Add condition list - maximum of one function only */
	if (a_fpCondition != NULL)
	{
		l_pCondition = (FSM_CONDITION*)_fsmCreateObject(sizeof(FSM_CONDITION));
		l_pCondition->fpCondition = a_fpCondition;
		_fsmAddObjectToList(&l_pTransition->listConditions, (FSM_LIST*)l_pCondition);
	}
	
	/* create action objects for variable list */
	va_start(l_args, a_iNewStateId);	/* extract variable arguments */

	do
	{
		l_fpAction = (FSM_FUNC_ACTION)va_arg(l_args, FSM_FUNC_ACTION);
		if (FSM_END_ACTION != l_fpAction)
		{
			l_pAction = (FSM_ACTION*)_fsmCreateObject(sizeof(FSM_ACTION));
			if (NULL == l_pAction)
			{
				l_pState->pFsm->bCreateError = FSM_TRUE;
				return FSM_FALSE;
			}
			l_pAction->fpAction = l_fpAction;
			_fsmAddObjectToList(&l_pTransition->listActions, (FSM_LIST*)l_pAction);
		}
	}
	while (l_fpAction != FSM_END_ACTION);
	va_end(l_args);			/* done with arguments */

	return FSM_TRUE;
}

/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created allowing
 *		   for multiple condition functions. The transition is to a sub-state machine,
 *         to allow nested state-machines. If this transition is called, then the current state
 *         becomes the parent state to the newly entered state. Events and transitions 
 *         handled by the parent state are inherited by the sub-state.
 *
 *         The Sub-State machine is exitted by transitioning to FSM_ST_PARENT.
 *********************************************************/
FSM_BOOL	fsmTransitionSub
(
	FSM_HANDLE			a_hState, 
	int 				a_iEventId, 
	void*				a_fpCondition,
	int 				a_iNewStateId, 
	...					/* FSM_FUNC_ACTION */
)
{
FSM_STATE		*l_pState = (FSM_HANDLE)a_hState;
FSM_TRANSITION	*l_pTransition = NULL;
FSM_CONDITION	*l_pCondition = NULL;
FSM_ACTION		*l_pAction = NULL;
FSM_FUNC_ACTION l_fpAction = NULL;
va_list 		l_args;			/* list of variable arguments */

	if (NULL == l_pState)
		return FSM_FALSE;
		
	if (	(FSM_CATCH_TRANSITION == a_iEventId) 
		||	(FSM_ST_ANY == a_iNewStateId))
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* create a transition object */
	l_pTransition = (FSM_TRANSITION*)_fsmCreateObject(sizeof(FSM_TRANSITION));
	if (NULL == l_pTransition)
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* initialize and add transition to the state */
	l_pTransition->iNewStateId = a_iNewStateId;
    l_pTransition->bSubState = FSM_TRUE;
	l_pTransition->iEventId = a_iEventId;
	_fsmAddObjectToList(&l_pState->listTransitions, (FSM_LIST*)l_pTransition);

	/* Add condition list - maximum of one function only */
	if (a_fpCondition != NULL)
	{
		l_pCondition = (FSM_CONDITION*)_fsmCreateObject(sizeof(FSM_CONDITION));
		l_pCondition->fpCondition = a_fpCondition;
		_fsmAddObjectToList(&l_pTransition->listConditions, (FSM_LIST*)l_pCondition);
	}
	
	/* create action objects for variable list */
	va_start(l_args, a_iNewStateId);	/* extract variable arguments */

	do
	{
		l_fpAction = (FSM_FUNC_ACTION)va_arg(l_args, FSM_FUNC_ACTION);
		if (FSM_END_ACTION != l_fpAction)
		{
			l_pAction = (FSM_ACTION*)_fsmCreateObject(sizeof(FSM_ACTION));
			if (NULL == l_pAction)
			{
				l_pState->pFsm->bCreateError = FSM_TRUE;
				return FSM_FALSE;
			}
			l_pAction->fpAction = l_fpAction;
			_fsmAddObjectToList(&l_pTransition->listActions, (FSM_LIST*)l_pAction);
		}
	}
	while (l_fpAction != FSM_END_ACTION);
	va_end(l_args);			/* done with arguments */

	return FSM_TRUE;
}


/**	
 **********************************************************
 *  @brief Creates transition to a state that has already been created.
 *			This version allows multiple conditions that must all be
 *			true to pass.
 *
 *********************************************************/
FSM_BOOL	fsmTransitionEx
(
	FSM_HANDLE			a_hState, 
	int 				a_iEventId, 
	...					/* FSM_FUNC_CONDITION... int iNewStateId  FSM_FUNC_ACTION... */
)
{
FSM_STATE			*l_pState = (FSM_HANDLE)a_hState;
FSM_TRANSITION		*l_pTransition = NULL;
FSM_ACTION			*l_pAction = NULL;
FSM_CONDITION		*l_pCondition = NULL;
FSM_FUNC_ACTION 	l_fpAction = NULL;
FSM_FUNC_CONDITION	l_fpCondition = NULL;
va_list 			l_args;			/* list of variable arguments */

	/* Error Checking */
	if (NULL == l_pState)
		return FSM_FALSE;
		
	if (FSM_CATCH_TRANSITION == a_iEventId) 
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* create a transition object */
	l_pTransition = (FSM_TRANSITION*)_fsmCreateObject(sizeof(FSM_TRANSITION));
	if (NULL == l_pTransition)
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* initialize and add transition to the state */
	l_pTransition->iEventId = a_iEventId;
	_fsmAddObjectToList(&l_pState->listTransitions, (FSM_LIST*)l_pTransition);

	/* parse variable list for conditions, new state ID, and actions */
	va_start(l_args, a_iEventId);	/* extract variable arguments */

	/* There can be many conditions. The list is terminated will NULL */
	do
	{
		l_fpCondition = (FSM_FUNC_CONDITION)va_arg(l_args, FSM_FUNC_CONDITION);
		if (FSM_END_CONDITION != l_fpCondition)
		{
			l_pCondition = (FSM_CONDITION*)_fsmCreateObject(sizeof(FSM_CONDITION));
			if (NULL == l_fpCondition)
			{
				l_pState->pFsm->bCreateError = FSM_TRUE;
				return FSM_FALSE;
			}
			l_pCondition->fpCondition = l_fpCondition;
			_fsmAddObjectToList(&l_pTransition->listConditions, (FSM_LIST*)l_pCondition);
		}
	}
	while (l_fpCondition != FSM_END_CONDITION);

	/* The next item (after the conditions) is the 'New State ID' */
	l_pTransition->iNewStateId = (int)va_arg(l_args, int);
    l_pTransition->bSubState = FSM_FALSE;
	if (FSM_ST_ANY == l_pTransition->iNewStateId)
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* There can be many actions. The list is terminated will NULL */
	do
	{
		l_fpAction = (FSM_FUNC_ACTION)va_arg(l_args, FSM_FUNC_ACTION);
		if (FSM_END_ACTION != l_fpAction)
		{
			l_pAction = (FSM_ACTION*)_fsmCreateObject(sizeof(FSM_ACTION));
			if (NULL == l_pAction)
			{
				l_pState->pFsm->bCreateError = FSM_TRUE;
				return FSM_FALSE;
			}
			l_pAction->fpAction = l_fpAction;
			_fsmAddObjectToList(&l_pTransition->listActions, (FSM_LIST*)l_pAction);
		}
	}
	while (l_fpAction != FSM_END_ACTION);

	va_end(l_args);			/* done with arguments */

	return FSM_TRUE;
}


/**	
 **********************************************************
 *  @brief Creates an error 'catch' transition to a state that has already been created.
 *
 *********************************************************/
FSM_BOOL fsmCatchTransition
(
	FSM_HANDLE	a_hState, 
	int         a_iNewStateId, 
	/* FSM_FUNC_ACTION */ ...
)
{
FSM_STATE		*l_pState = (FSM_HANDLE)a_hState;
FSM_TRANSITION	*l_pTransition = NULL;
FSM_ACTION		*l_pAction = NULL;
FSM_FUNC_ACTION l_fpAction = NULL;
va_list 		l_args;			/* list of variable arguments */

	if (NULL == l_pState)
		return FSM_FALSE;
		
    /* first, see if there already is a 'catch' transition */
    if (NULL != _fsmGetTransition(&l_pState->listTransitions, FSM_CATCH_TRANSITION))
        return FSM_FALSE;

	/* create a transition object */
	l_pTransition = (FSM_TRANSITION*)_fsmCreateObject(sizeof(FSM_TRANSITION));
	if (NULL == l_pTransition)
	{
		l_pState->pFsm->bCreateError = FSM_TRUE;
		return FSM_FALSE;
	}

	/* initialize and add transition to the state */
	l_pTransition->iNewStateId = a_iNewStateId;
    l_pTransition->bSubState = FSM_FALSE;
	l_pTransition->iEventId = FSM_CATCH_TRANSITION;
	_fsmAddObjectToList(&l_pState->listTransitions, (FSM_LIST*)l_pTransition);

	/* Nothing to add to condition list - Condition will always be NULL */

	/* create action objects for variable list */
	va_start(l_args, a_iNewStateId);	/* extract variable arguments */

	do
	{
		l_fpAction = (FSM_FUNC_ACTION)va_arg(l_args, FSM_FUNC_ACTION);
		if (NULL != l_fpAction)
		{
			l_pAction = (FSM_ACTION*)_fsmCreateObject(sizeof(FSM_ACTION));
			if (NULL == l_pAction)
			{
				l_pState->pFsm->bCreateError = FSM_TRUE;
				return FSM_FALSE;
			}
			l_pAction->fpAction = l_fpAction;
			_fsmAddObjectToList(&l_pTransition->listActions, (FSM_LIST*)l_pAction);
		}
	}
	while (l_fpAction != NULL);
	va_end(l_args);			/* done with arguments */

	return FSM_TRUE;
}


/**	
 **********************************************************
 *  Executes the specified transition.
 *  
 *  @param  a_pExecuteArgs	- arguments for the current state-machine execution.
 *  @param  a_pCurrentState	- pointer to current-state entry of state-machine
 *  @param  a_pTransition   - pointer to the transition object to execute
 *
 *  @return Result of the execution.
 *********************************************************/
static FSM_EXECUTION_RESULT _fsmExecuteTransition
(
    FSM_EXECUTE_ARGS    *a_pExecuteArgs,
	FSM_STATE			*a_pCurrentState,
    FSM_TRANSITION      *a_pTransition
)
{
FSM_LIST		*l_pActionList = NULL;
FSM_BOOL		l_bActionsPass = FSM_TRUE;		/* FSM_TRUE while action functions return FSM_TRUE */
FSM_ACTION		*l_pAction = NULL;
FSM_EXECUTION_RESULT l_eResult = eFSM_RESULT_NO_CHANGE;

    if (    (NULL == a_pCurrentState) 
         || (NULL == a_pTransition) 
         || (NULL == a_pExecuteArgs)
         )
        return FSM_FALSE;

	/* execute transition actions. If an action returns FSM_FALSE, then stop
	   executing the actions.
	*/
	l_pActionList = &a_pTransition->listActions;
	while ( (FSM_TRUE == l_bActionsPass) &&
	        (NULL != (l_pActionList = _fsmGetNextObject(l_pActionList))))
	{
		l_pAction = (FSM_ACTION*)l_pActionList;
		if (l_pAction->fpAction)
		{
			/* if the action has a failure, then do not continue processing
			   the action list of this transaction.
			*/
			if (FSM_FALSE == (l_pAction->fpAction)(a_pExecuteArgs->AppContext, a_pExecuteArgs->pAppMsg))
			{
				/* If this is NOT an error 'catch' transition, then
				   set transition flag to false, since a failure occurred. 
                   If this is a 'catch' transition, then call all action functions,
                   even if one fails.
				*/
                if (FSM_CATCH_TRANSITION != a_pTransition->iEventId)
    				l_bActionsPass = FSM_FALSE;
			}
		}
	}
		  
	/* if a failure occurred in one of the actions, then do not transition.
	   Continue searching for a machine 'alternative' transition.
	   If no failure occurred, then transition the state.
	*/
	if (FSM_TRUE != l_bActionsPass)
    {
        l_eResult = eFSM_RESULT_ACTION_FAILURE;
    }
    else
	{
        if (FSM_ST_SAME == a_pTransition->iNewStateId)
            l_eResult = eFSM_RESULT_NO_CHANGE; /* Don't change state, remain in current state. */
        else
            l_eResult = eFSM_RESULT_NEW_STATE; /* Change to new state */
    } /* bActionsPass */
 
    return l_eResult;   
}




/**	
 **********************************************************
 *  Executes the specified state object. If a transition is found that matches the
 *  event id, then the state-machine attempts to execute the transition.
 *  
 *  @param  a_pExecuteArgs	- arguments for the current state-machine execution.
 *  @param  a_pstCurrentState		- pointer to current state object
 *  @param  a_piNewStateId		- if there is a state change, then id of new state
 *                                    is returned here.
 *  @param  a_pbIsSubState      - returns TRUE if the new state is a sub-state of the
 *                                    current state, FALSE if the new state
 *                                    is a transition out of the current state.
 *
 *  @return Result of the state execution. 
 *********************************************************/
static FSM_EXECUTION_RESULT _fsmExecuteState
(
    FSM_EXECUTE_ARGS    *a_pExecuteArgs,
	FSM_STATE		    *a_pstCurrentState,
    int                 *a_piNewStateId,
    FSM_BOOL            *a_pbIsSubState
)
{
FSM_LIST		*l_pTransitionList = NULL;
FSM_TRANSITION	*l_pTransition = NULL;
FSM_BOOL		l_bTransitionPass = FSM_FALSE;		/* FSM_TRUE if a transition's conditional function passes */
FSM_EXECUTION_RESULT l_eResult = eFSM_RESULT_NO_TRANSITION;

FSM_LIST		*l_pConditionList = NULL;
FSM_BOOL		l_bConditionsPass = FSM_TRUE;		/* FSM_TRUE while condition functions return FSM_TRUE */
FSM_CONDITION	*l_pCondition = NULL;

	if (
            (NULL == a_pstCurrentState)
        ||  (NULL == a_pExecuteArgs)
        ||  (NULL == a_piNewStateId)
        ||  (NULL == a_pbIsSubState))
		return l_eResult;

	/* Loop through state transitions to find match, given the event id.
       If a transition matches, then execute its condition(s).
            If its condition(s) pass, then continue to execute the transition's actions.
            otherwise, keep searching for matching transition.   
       If a matching transition's action function returns FSM_FALSE, then execute the state's
            'catch' transition, if there is one.
    */
	l_pTransitionList = &a_pstCurrentState->listTransitions;
	while (	(FSM_FALSE == l_bTransitionPass) &&
			( NULL != (l_pTransitionList = _fsmGetTransition(l_pTransitionList, a_pExecuteArgs->iEventId))))
	{
		/* get pointer to the transition object */
		l_pTransition = (FSM_TRANSITION*)l_pTransitionList;

		/* execute transition conditions. If a condition is false, then stop
		   executing the conditions and continue to the next transition.
		*/
		l_bConditionsPass = FSM_TRUE;
		l_pConditionList = &l_pTransition->listConditions;
		while ( (FSM_TRUE == l_bConditionsPass) &&
		        (NULL != (l_pConditionList = _fsmGetNextObject(l_pConditionList))))
		{
			l_pCondition = (FSM_CONDITION*)l_pConditionList;
			if (l_pCondition->fpCondition)
			{
				/* if the action has a failure, then do not continue processing
				   the action list of this transaction.
				*/
				if (FSM_FALSE == (l_pCondition->fpCondition)(a_pExecuteArgs->AppContext, a_pExecuteArgs->pAppMsg))
				{
					l_bConditionsPass = FSM_FALSE;
				}
			}
		}
		if (FSM_FALSE == l_bConditionsPass)
			continue;

		/* change return value since a transition has matched.
		*/
		l_eResult = eFSM_RESULT_NO_CHANGE;

		/* if we got here, then we have a transition match, so don't look for any more matches */
		l_bTransitionPass = FSM_TRUE;	

        /* execute the matching transition. 
           If the transition execution fails, then
                if there is a 'catch' transition for the current state, then
                    execute the 'catch' transition
                else
                    do not change state
        */  
        l_eResult = _fsmExecuteTransition(  a_pExecuteArgs, a_pstCurrentState, l_pTransition);             
        if (eFSM_RESULT_ACTION_FAILURE == l_eResult)
        {
            l_pTransition = (FSM_TRANSITION*)_fsmGetTransition(&a_pstCurrentState->listTransitions, FSM_CATCH_TRANSITION);    
            if (NULL != l_pTransition)
            {
                l_eResult = _fsmExecuteTransition(a_pExecuteArgs, a_pstCurrentState, l_pTransition);
            }
        }
	} /* while searching transitions */

    /* if there is going to be a state change, return pointer
       to new state's object.
    */
    if (eFSM_RESULT_NEW_STATE == l_eResult)
    {
        *a_piNewStateId = l_pTransition->iNewStateId;
        *a_pbIsSubState = l_pTransition->bSubState;
    }

	return l_eResult;
}


/**	
 **********************************************************
 *  @brief Called by application upon reception of event to run the
 *          state-machine.
 *********************************************************/
FSM_EXECUTION_RESULT fsmExecute
(
	FSM_HANDLE		    a_hFsm, 
    FSM_OBJECT_STATE    *a_pObjectState,
	int 			    a_iEventId, 
	FSM_APP_CONTEXT     a_AppContext, 
	void* 			    a_pAppMsg
)
{
FSM_CB			*l_pFsm = (FSM_CB*)a_hFsm;
FSM_EXECUTION_RESULT    l_eResult = eFSM_RESULT_NO_TRANSITION;
int             l_iNestDepthCnt = 0;
FSM_EXECUTE_ARGS    l_stExecuteArgs;    /**< arguments passed to other functions, internally */
FSM_STATE       *l_pstCurrentState = NULL;  /**< contains pointer to current state object */
int             l_iNewStateId = -1;         /**< will contain new state's id, if there is a state change */
int             l_iStateChangeDepth = 0 ;   /**< nested state depth at which a state-change occurred (default is top level) */
FSM_BOOL        l_bNewStateIsSubState = FSM_FALSE; /**< Set to FSM_TRUE if the new state is a sub-state of the current state,
                                                       FSM_FALSE if the new state is a transition out of the current state. */
	if (NULL == l_pFsm)
		return eFSM_RESULT_NO_TRANSITION;
			
    /* make sure that the event id is valid */
    if (FSM_CATCH_TRANSITION == a_iEventId)
        return eFSM_RESULT_NO_TRANSITION;

    /* store the arguments, locally, to be passed to sub-routines */
    l_stExecuteArgs.pFsm = l_pFsm; 
    l_stExecuteArgs.pObjectState = a_pObjectState;
	l_stExecuteArgs.iEventId = a_iEventId;
	l_stExecuteArgs.AppContext = a_AppContext;
	l_stExecuteArgs.pAppMsg = a_pAppMsg;

    /* Look for a matching transition on this event within any currently-nested state, 
       starting with top-most super state. 
    */
    for (l_iNestDepthCnt=0; l_iNestDepthCnt <= a_pObjectState->iNestDepth; l_iNestDepthCnt++)
    {
        l_pstCurrentState = (FSM_STATE*)_fsmGetState(&l_pFsm->listStates, a_pObjectState->iNestedStateIds[l_iNestDepthCnt]);
        if (NULL == l_pstCurrentState)
    		return eFSM_RESULT_NO_TRANSITION;
       
	    /* Execute this state. If a matching transition was found, then break the loop,
           otherwise, keep looking for a match within the nested states. 
	    */
	    l_eResult = _fsmExecuteState(&l_stExecuteArgs, l_pstCurrentState, &l_iNewStateId, &l_bNewStateIsSubState);
        if (eFSM_RESULT_NO_TRANSITION != l_eResult)
        {
            /* if a matching transition was found, then break the for loop
               and store the nest depth at which the match was found, so that
               the corresponding state can be exitted.
            */
            l_iStateChangeDepth = l_iNestDepthCnt;
            break;
        }
    }

    /*	if no transition match found, then see if there is an 'any-state' object that has a transition that
	    matches. 
    */
    if (eFSM_RESULT_NO_TRANSITION == l_eResult)
	{
    FSM_STATE *l_pAnyState = NULL;

	    /* see if there is an any-state transition that matches */ 
		l_pAnyState = (FSM_STATE*)_fsmGetState(&l_pFsm->listStates, FSM_ST_ANY);
		if (NULL != l_pAnyState)
		{
		    l_eResult = _fsmExecuteState(&l_stExecuteArgs, l_pAnyState, &l_iNewStateId, &l_bNewStateIsSubState);
        }
    }

    /* If a state-change needs to occur, then do it here.

        1) Execute all nested state exit functions.
        2) Change state.
        3) Execute all nested state entry functions.
    */
    if (eFSM_RESULT_NEW_STATE == l_eResult)
    {
        /** STATE EXIT **/

        /* save current state into previous state, for reference */
        a_pObjectState->iPreviousStateId = a_pObjectState->iNestedStateIds[a_pObjectState->iNestDepth];

        /* If the new state is a sub-state of the current state, then do not call
           any exit routines, just increment the state nest depth, 
           since the current state is not being exitted.
           If the new state is not a sub-state of the current state, then
           call all state exit routines from the lowest depth of
           nested states up to the depth at which the state-change occurred.
        */
        if (l_bNewStateIsSubState)
        {
            /* Increment nest-depth since the new state is a sub-state of the
               current state.
            */
            if (a_pObjectState->iNestDepth >= FSM_MAX_STATE_NEST_DEPTH)
               return eFSM_RESULT_INTERNAL_FAILURE;
            else            
                a_pObjectState->iNestDepth++;
        }
        else
        {
            for (l_iNestDepthCnt = a_pObjectState->iNestDepth; l_iNestDepthCnt >= l_iStateChangeDepth; l_iNestDepthCnt--)
            {
            FSM_STATE *l_pstExittingState = NULL;
            
                l_pstExittingState = (FSM_STATE*)_fsmGetState(   &l_pFsm->listStates, 
                                                                 a_pObjectState->iNestedStateIds[l_iNestDepthCnt]);
                if (NULL != l_pstExittingState)
                {
                    /* execute state exit action */ 
                    if (NULL != l_pstExittingState->fpExitAction)
    	                (l_pstExittingState->fpExitAction)(a_AppContext);
                }
            }

            /* If the new state is the parent of a sub-state, then decrement the
               nest-depth, since no longer in the sub-state.
               If the new state is not the parent of a sub-state, then the
               new nest-depth is that at which the top-most exitting state was at.
            */
            if (FSM_ST_PARENT == l_iNewStateId)
            {
                if (a_pObjectState->iNestDepth > 0)
                    a_pObjectState->iNestDepth--;
            }
            else
            {
                a_pObjectState->iNestDepth = l_iStateChangeDepth;
            }                
        }
        
        /** STATE ENTRY **/

        /* If the new state is not the parent state, then enter the new state. 
           Check if the new state is complex/nested. If it
           is, then continue to enter the state's initial sub-state and store
           nested states in the nested state array.
        */
        if (FSM_ST_PARENT != l_iNewStateId)
        {
            FSM_BOOL l_bComplexState = FSM_FALSE;   /**< Set to FSM_TRUE if the new state is a complex/nested state */

            do
            {
            FSM_STATE   *l_pNewState = NULL;

                l_bComplexState = FSM_FALSE;

                /* get pointer to new state and execute state entry action */
                l_pNewState = (FSM_STATE*)_fsmGetState(&l_pFsm->listStates, l_iNewStateId);			
                /* store new super state in the nest */
                a_pObjectState->iNestedStateIds[a_pObjectState->iNestDepth] = l_iNewStateId;
                /* process new state object */
                if (NULL != l_pNewState)
                {
              	    if (NULL != l_pNewState->fpEntryAction)
       		            (l_pNewState->fpEntryAction)(a_AppContext);

                    /* if new state is a complex state (and there is a defined initial state), then store
                        the nested complex state in the object state and 
                        enter the sub-state by callling this function recursively.
                        This provides support for multi-level nests.
                    */
                    if (l_pNewState->bComplex && (FSM_ST_SAME != l_pNewState->iInitialSubStateId))
                    {
                        /* check number of state nests, if already at maximum depth, then error.
                        */
                        if (a_pObjectState->iNestDepth >= (FSM_MAX_STATE_NEST_DEPTH-1))
                        {
                            return eFSM_RESULT_INTERNAL_FAILURE;
                        }
                        else
                        {
                            l_bComplexState = FSM_TRUE;
                            a_pObjectState->iNestDepth++;
                            l_iNewStateId = l_pNewState->iInitialSubStateId;
                        }
                    }
                } 
            } while (l_bComplexState);
        }            
    } /* new state */

	return l_eResult;
}















/* private functions */







/**	
 **********************************************************
 *  Allocates memory for an object.
 *  
 *  @param  a_Size  - number of bytes to allocate
 *  @return NULL if failed to allocate, else non-NULL pointer to object.
 *********************************************************/
void* _fsmCreateObject(size_t a_Size)
{															
void* l_pObject = NULL;

	l_pObject = malloc(a_Size);
	if (NULL != l_pObject)
		memset(l_pObject, 0, a_Size);

	return l_pObject;
}

/**	
 **********************************************************
 *  Deallocates memory for an object.
 *  
 *********************************************************/
void _fsmDeleteObject(void* a_pObject)
{															
    if (a_pObject)
        free(a_pObject);
}

/**	
 **********************************************************
 *  Searches list for a state object the matches the state identifier.
 *  
 *  @param  a_pList     - pointer to list of state objects
 *  @param  a_iStateId  - state identifier to match
 *  @return NULL if failed to find match, else non-NULL state object.
 *********************************************************/
FSM_LIST*	_fsmGetState(FSM_LIST *a_pList, int a_iStateId)
{
FSM_STATE* l_pState = NULL;

	if (NULL == a_pList)
		return NULL;

	/* traverse list of states to find a match */
	while (NULL != (a_pList = _fsmGetNextObject(a_pList)))
	{
		l_pState = (FSM_STATE*)a_pList;		
		if (l_pState->iStateId == a_iStateId)
			return (FSM_LIST*)l_pState;
	}

	return (FSM_LIST*)NULL;
}

/**	
 **********************************************************
 *  Searches list for a transition object the matches the event identifier.
 *  
 *  @param  a_pList     - pointer to list of state objects
 *  @param  a_iEventId  - event identifier to match
 *  @return NULL if failed to find match, else non-NULL transition object.
 *********************************************************/
FSM_LIST*	_fsmGetTransition(FSM_LIST *a_pList, int a_iEventId)
{
FSM_TRANSITION	*l_pTransition = NULL;

	if (NULL == a_pList)
		return NULL;

	/* traverse list of transitions to find a match */
	while (NULL != (a_pList = _fsmGetNextObject(a_pList)))
	{
		l_pTransition = (FSM_TRANSITION*)a_pList;		
		if (l_pTransition->iEventId == a_iEventId)
			return (FSM_LIST*)l_pTransition;
	}

	return (FSM_LIST*)NULL;
}


/**	
 **********************************************************
 *  Retrieves next object in the specified list.
 *  
 *  @param  a_pList  - pointer to next object in the list
 *  @return NULL if end of list, else non-NULL object.
 *********************************************************/
FSM_LIST* _fsmGetNextObject(FSM_LIST *a_pList)
{
	if (NULL == a_pList)
		return NULL;

	if (NULL != a_pList->pnext)
		return a_pList->pnext;		
	else
		return NULL;
}

/**	
 **********************************************************
 *  Adds specified object to the tail of the list.
 *  
 *  @param  a_pList     - pointer to list
 *  @param  a_pObject   - pointer to object to add to list
 *
 *  @return NULL if end of list, else non-NULL object.
 *********************************************************/
void _fsmAddObjectToList(FSM_LIST *a_pList, FSM_LIST *a_pObject)
{
	if ((NULL == a_pList) || (NULL == a_pObject))
		return;

	while (NULL != a_pList->pnext)
		a_pList = a_pList->pnext;
	a_pList->pnext = a_pObject;
}


/* EOF */
