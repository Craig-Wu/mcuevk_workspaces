/*
* This source file is part of the EtherCAT Slave Stack Code licensed by Beckhoff Automation GmbH & Co KG, 33415 Verl, Germany.
* The corresponding license agreement applies. This hint shall not be removed.
* https://www.beckhoff.com/media/downloads/slave-stack-code/ethercat_ssc_license.pdf
*/

/**
\addtogroup CiA402appl CiA402 Sample Application
@{
*/

/**
\file cia402appl.c
\author EthercatSSC@beckhoff.com
\brief Implementation
This file contains all ciA402 specific functions

\version 5.13

<br>Changes to version V5.12:<br>
V5.13 CIA402 2: write profile info to 0xF010.x bit15-0 (was bit16-31 before)<br>
V5.13 CIA402 3: change define "CIA402_DEVICE" to "CiA402_SAMPLE_APPLICATION"<br>
<br>Changes to version V5.11:<br>
V5.12 COE3: update entry access right handling<br>
<br>Changes to version V5.10:<br>
V5.11 ECAT11: create application interface function pointer, add eeprom emulation interface functions<br>
<br>Changes to version V5.01:<br>
V5.10 CIA402 1: Update complete access handling for 0xF030<br>
V5.10 ECAT6: Add "USE_DEFAULT_MAIN" to enable or disable the main function<br>
<br>Changes to version V5.0:<br>
V5.01 ESC2: Add missed value swapping<br>
<br>Changes to version V4.40:<br>
V5.0 CIA402 1: Syntax bugfix in dummy motion controller<br>
V5.0 CIA402 2: Handle 0xF030/0xF050 in correlation do PDO assign/mapping objects<br>
V5.0 CIA402 3: Trigger dummy motion controller if valid mode of operation is set.<br>
V5.0 CIA402 4: Change Axes structure handling and resources allocation.<br>
V5.0 ECAT2: Create generic application interface functions. Documentation in Application Note ET9300.<br>
<br>Changes to version V4.30:<br>
V4.40 CoE 6: add AL Status code to Init functions<br>
V4.40 CIA402 2: set motion control trigger depending on "Synchronisation", "mode of operation" and "cycle time"<br>
V4.40 CIA402 1: change behaviour and name of bit12 of the status word (0x6041) (WG CIA402 24.02.2010)<br>
V4.30 : create file (state machine; handling state transition options; input feedback)
*/

/*-----------------------------------------------------------------------------------------
------
------    Includes
------
-----------------------------------------------------------------------------------------*/
#include "ecat_def.h"

/*ECATCHANGE_START(V5.13) CIA402 3*/
/*ECATCHANGE_END(V5.13) CIA402 3*/

#include "applInterface.h"


#include "coeappl.h"

#define _CiA402_
#include "cia402appl.h"
#undef _CiA402_

#include "renesashw.h"

// BOARD_RZT2_RSK
#include "bsp_api.h"
#include "board_cfg.h"

#if defined(BOARD_RZT2M_RSK) | defined(BOARD_RZN2L_RSK) | defined(BOARD_RZT2L_RSK)
UINT16 APPL_GetDipSw(void);
#endif

/*--------------------------------------------------------------------------------------
------
------    local types and defines
------
--------------------------------------------------------------------------------------*/
#if defined(BOARD_RZT2M_RSK) | defined(BOARD_RZN2L_RSK) | defined(BOARD_RZT2L_RSK)
/* Information on how many SWs and what pins they are on. */
typedef struct st_bsp_dip_sws
{
    UINT16         sw_count;        ///< The number of DIP SWs on this board
    UINT16 const * p_sws;           ///< Pointer to an array of IOPORT pins for controlling DIP SWs
} bsp_dip_sws_t;

/* Available user-controllable DIP SWs on this board. 
 * These enums can be can be used to index into the array of DIP SW pins
 * found in the appl_dip_sws_t structure. */
typedef enum e_bsp_dipsw
{
    BSP_DIPSW_0 = 0,
    BSP_DIPSW_1 = 1,
    BSP_DIPSW_2 = 2,
    BSP_DIPSW_3 = 3,
    BSP_DIPSW_4 = 4,
    BSP_DIPSW_5 = 5,
    BSP_DIPSW_6 = 6,
    BSP_DIPSW_7 = 7,
} bsp_dipsw_t;
#endif
/*-----------------------------------------------------------------------------------------
------
------    local variables and constants
------
-----------------------------------------------------------------------------------------*/
TCiA402Axis       LocalAxes[MAX_AXES];

#if defined(BOARD_RZT2M_RSK)
/** Array of DIP SW IOPORT pins. */
static const UINT16 g_bsp_prv_dip_sws[] =
{
	(UINT16) BSP_IO_PORT_11_PIN_0, // SW3-1
	(UINT16) BSP_IO_PORT_11_PIN_3, // SW3-2
	(UINT16) BSP_IO_PORT_11_PIN_4, // SW3-3
	(UINT16) BSP_IO_PORT_11_PIN_6, // SW3-4
	(UINT16) BSP_IO_PORT_10_PIN_6, // SW3-5
	(UINT16) BSP_IO_PORT_13_PIN_2, // SW3-6
	(UINT16) BSP_IO_PORT_13_PIN_7, // SW3-7
	(UINT16) BSP_IO_PORT_14_PIN_1, // SW3-8
};

/** Structure with DIP SW information for this board. */
const bsp_dip_sws_t g_bsp_dip_sws =
{
    .sw_count = (UINT16) ((sizeof(g_bsp_prv_dip_sws) / sizeof(g_bsp_prv_dip_sws[0]))),
    .p_sws    = &g_bsp_prv_dip_sws[0]
};
#endif

#if defined(BOARD_RZN2L_RSK)		 
/** Array of DIP SW IOPORT pins. */ 		 
static const uint16_t g_bsp_prv_dip_sws[] =		 
{		 
    (uint16_t) BSP_IO_PORT_13_PIN_6, // SW3-1		 
    (uint16_t) BSP_IO_PORT_13_PIN_5, // SW3-2		 
    (uint16_t) BSP_IO_PORT_14_PIN_0, // SW3-3		 
    (uint16_t) BSP_IO_PORT_13_PIN_7, // SW3-4		 
};		 
 		 
/** Structure with DIP SW information for this board. */ 		 
const bsp_dip_sws_t g_bsp_dip_sws =		 
{		 
    .sw_count = (UINT16) ((sizeof(g_bsp_prv_dip_sws) / sizeof(g_bsp_prv_dip_sws[0]))),		 
    .p_sws    = &g_bsp_prv_dip_sws[0]		 
};		 
#endif

#if defined(BOARD_RZT2L_RSK)
/** Array of DIP SW IOPORT pins. */
static const uint16_t g_bsp_prv_dip_sws[] =
{
     (uint16_t) BSP_IO_PORT_01_PIN_6, // SW3-1
     (uint16_t) BSP_IO_PORT_04_PIN_1, // SW3-2
     (uint16_t) BSP_IO_PORT_14_PIN_1, // SW3-3
     (uint16_t) BSP_IO_PORT_14_PIN_3, // SW3-4
};

/** Structure with DIP SW information for this board. */
const bsp_dip_sws_t g_bsp_dip_sws =
{
    .sw_count = (UINT16) ((sizeof(g_bsp_prv_dip_sws) / sizeof(g_bsp_prv_dip_sws[0]))),
    .p_sws    = &g_bsp_prv_dip_sws[0]
};
#endif
/*-----------------------------------------------------------------------------------------
------
------    application specific functions
------
-----------------------------------------------------------------------------------------*/

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    0               Init CiA402 device successful
            ALSTATUSCODE_XX Init CiA402 device failed

 \brief    This function initializes the Axes structures
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 CiA402_Init(void)
{
    UINT16 result = 0;
    UINT16 AxisCnt = 0;
    UINT16 j = 0;
    UINT32 ObjectOffset = 0x800;
    UINT8 PDOOffset = 0x10;

    for(AxisCnt = 0; AxisCnt < MAX_AXES ; AxisCnt++)
    {
        /*Reset Axis buffer*/
        HMEMSET(&LocalAxes[AxisCnt],0,SIZEOF(TCiA402Axis));

        LocalAxes[AxisCnt].bAxisIsActive = FALSE;
        LocalAxes[AxisCnt].bBrakeApplied = TRUE;
        LocalAxes[AxisCnt].bLowLevelPowerApplied = TRUE;
        LocalAxes[AxisCnt].bHighLevelPowerApplied = FALSE;
        LocalAxes[AxisCnt].bAxisFunctionEnabled = FALSE;
        LocalAxes[AxisCnt].bConfigurationAllowed = TRUE;
        LocalAxes[AxisCnt].u16AxisNum = AxisCnt+1;
        LocalAxes[AxisCnt].i16State = STATE_NOT_READY_TO_SWITCH_ON;
        LocalAxes[AxisCnt].u16PendingOptionCode = 0x00;

        LocalAxes[AxisCnt].fCurPosition = 0;
        LocalAxes[AxisCnt].u32CycleTime = 0;


        /***********************************
            init objects
        *************************************/

        /*set default values*/
        HMEMCPY(&LocalAxes[AxisCnt].Objects,&DefCiA402ObjectValues,CIA402_OBJECTS_SIZE);

        /***set Object offset to PDO entries***/

        /*csv/csp RxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sRxPDOMap0.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sRxPDOMap0.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }

        /*csp RxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sRxPDOMap1.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sRxPDOMap1.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }

        /*csv RxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sRxPDOMap2.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sRxPDOMap2.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }


        /*csv/csp TxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sTxPDOMap0.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sTxPDOMap0.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }

        /*csp TxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sTxPDOMap1.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sTxPDOMap1.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }

        /*csv TxPDO*/
        for (j = 0; j < LocalAxes[AxisCnt].Objects.sTxPDOMap2.u16SubIndex0; j++)
        {
            LocalAxes[AxisCnt].Objects.sTxPDOMap2.aEntries[j] += AxisCnt* (ObjectOffset << 16);
        }



        /***********************************
            init objects dictionary entries
        *************************************/
        LocalAxes[AxisCnt].ObjDic = (TOBJECT *) ALLOCMEM(SIZEOF(DefCiA402AxisObjDic));
        HMEMCPY(LocalAxes[AxisCnt].ObjDic,&DefCiA402AxisObjDic,SIZEOF(DefCiA402AxisObjDic));
        {
        TOBJECT OBJMEM *pDiCEntry = LocalAxes[AxisCnt].ObjDic;

        /*adapt Object index and assign Var pointer*/
        while(pDiCEntry->Index != 0xFFFF)
        {
            BOOL bObjectFound = TRUE;

            switch(pDiCEntry->Index)
            {
            case 0x1600:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sRxPDOMap0;
                break;
            case 0x1601:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sRxPDOMap1;
                break;
            case 0x1602:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sRxPDOMap2;
                break;
            case 0x1A00:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sTxPDOMap0;
                break;
            case 0x1A01:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sTxPDOMap1;
                break;
            case 0x1A02:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.sTxPDOMap2;
                break;
            case 0x603F:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objErrorCode;
                break;
            case 0x6040:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objControlWord;
                break;
            case 0x6041:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objStatusWord;
                break;
            case 0x605A:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objQuickStopOptionCode;
                break;
            case 0x605B:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objShutdownOptionCode;
                break;
            case 0x605C:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objDisableOperationOptionCode;
                break;
            case 0x605D:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objHaltOptionCode;
                break;
            case 0x605E:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objFaultReactionCode;
                break;
            case 0x6060:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objModesOfOperation;
                break;
            case 0x6061:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objModesOfOperationDisplay;
                break;
            case 0x6062:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionDemandValue;
                break;
            case 0x6063:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionActualInternalValue;
                break;
            case 0x6064:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionActualValue;
                break;
            case 0x6065:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objFollowingErrorWindow;
                break;
            case 0x6066:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objFollowingErrorTimeOut;
                break;
            case 0x6067:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionWindow;
                break;
            case 0x606C:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objVelocityActualValue;
                break;
            case 0x6072:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objMaxTorque;
                break;
            case 0x6077:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTorqueActualValue;
                break;
            case 0x607A:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTargetPosition;
                break;
            case 0x607B:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionRangeLimit;
                break;
            case 0x607C:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objHomeOffset;
                break;
            case 0x607D:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objSoftwarePositionLimit;
                break;
            case 0x6085:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objQuickStopDeclaration;
                break;
            case 0x6091:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objGearRatio;
                break;
            case 0x6099:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objHomingSpeeds;
                break;
            case 0x60B0:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositionOffset;
                break;
            case 0x60B1:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objVelocityOffset;
                break;
            case 0x60B2:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTorqueOffset;
                break;
            case 0x60B8:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTouchProbeFunction;
                break;
            case 0x60B9:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTouchProbeStatus;
                break;
            case 0x60BA:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTouchProbePos1PosValue;
                break;
            case 0x60BB:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.ObjTouchProbePos1NegValue;
                break;
            case 0x60C2:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objInterpolationTimePeriod;
                break;
            case 0x60D0:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTouchProbeSource;
                break;
            case 0x60E0:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objPositiveTorqueLimitValue;
                break;
            case 0x60E1:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objNegativeTorqueLimitValue;
                break;
            case 0x60F4:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objFollowingErrorActualValue;
                break;
            case 0x60FD:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objDigitalInputs;
                break;
            case 0x60FF:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objTargetVelocity;
                break;
            case 0x6402:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objMotorType;
                break;
            case 0x6502:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objSupportedDriveModes;
                break;
            case 0x60FE:
                pDiCEntry->pVarPtr = &LocalAxes[AxisCnt].Objects.objDigitalOutputs;
                break;
            default :
                bObjectFound = FALSE;
            break;
            }//switch(pDiCEntry->Index)

            /*increment object index*/
            if (pDiCEntry->Index >= 0x1400 && pDiCEntry->Index <= 0x1BFF)    //PDO region
            {
                pDiCEntry->Index += AxisCnt* PDOOffset;
            }
            else
            {
                pDiCEntry->Index += AxisCnt* (UINT16)ObjectOffset;
            }

            pDiCEntry++;
        }//while(pDiCEntry->Index != 0xFFFF)

        }
    }// for "all active axes"

    return result;
}
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    CiA402_DeallocateAxis
 \brief    Remove all allocated axes resources
*////////////////////////////////////////////////////////////////////////////////////////
void CiA402_DeallocateAxis(void)
{
    UINT8 cnt = 0;

    for(cnt = 0 ; cnt < MAX_AXES ; cnt++)
    {
    /*Remove object dictionary entries*/
    if(LocalAxes[cnt].ObjDic != NULL)
    {
        TOBJECT OBJMEM *pEntry = LocalAxes[cnt].ObjDic;

        while(pEntry->Index != 0xFFFF)
        {
            COE_RemoveDicEntry(pEntry->Index);

            pEntry++;
        }

        FREEMEM(LocalAxes[cnt].ObjDic);
    }

    nPdOutputSize = 0;
    nPdInputSize = 0;

    }

}
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    CiA402-Statemachine
        This function handles the state machine for devices using the CiA402 profile.
        called cyclic from MainLoop()
        All described transition numbers are referring to the document
        "ETG Implementation Guideline for the CiA402 Axis Profile" located on the EtherCAT.org download section

*////////////////////////////////////////////////////////////////////////////////////////
void CiA402_StateMachine(void)
{
    TCiA402Axis *pCiA402Axis;
    UINT16 StatusWord = 0;
    UINT16 ControlWord6040 = 0;
    UINT16 counter = 0;

    for(counter = 0; counter < MAX_AXES;counter++)
    {
        if(!LocalAxes[counter].bAxisIsActive)
        {
            continue;
        }

        pCiA402Axis = &LocalAxes[counter];
        StatusWord = pCiA402Axis->Objects.objStatusWord;
        ControlWord6040 = pCiA402Axis->Objects.objControlWord;

        /*clear statusword state and controlword processed complete bits*/
        StatusWord &= ~(STATUSWORD_STATE_MASK | STATUSWORD_REMOTE);

        /*skip state state transition if the previous transition is pending*/
        if(pCiA402Axis->u16PendingOptionCode!= 0x0)
        {
            return;
        }
        /*skip transition 1 and 2*/
        if(pCiA402Axis->i16State < STATE_READY_TO_SWITCH_ON && nAlStatus == STATE_OP){
            if(pCiA402Axis->i16State == STATE_NOT_READY_TO_SWITCH_ON){
                CiA402_StateTransition1(pCiA402Axis);
				CiA402_StateTransition2(pCiA402Axis);
            }
            if(pCiA402Axis->i16State == STATE_SWITCH_ON_DISABLED){
                CiA402_StateTransition2(pCiA402Axis);
            }

            pCiA402Axis->i16State = STATE_READY_TO_SWITCH_ON;
        }

        switch(pCiA402Axis->i16State)
        {
        case STATE_NOT_READY_TO_SWITCH_ON:
            StatusWord |= (STATUSWORD_STATE_NOTREADYTOSWITCHON);
            if(nAlStatus == STATE_OP)
            {
                if(!CiA402_StateTransition1(pCiA402Axis)){
                // Automatic transition -> Communication shall be activated
                    pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED; // Transition 1
                }
            }
            else
            {
                /*
                CiA402 statemachine shall stay in "STATE_NOT_READY_TO_SWITCH_ON" if EtherCAT state is not OP.
                */
                pCiA402Axis->i16State = STATE_NOT_READY_TO_SWITCH_ON; // stay in current state
            }

            break;
        case STATE_SWITCH_ON_DISABLED:
            StatusWord |= (STATUSWORD_STATE_SWITCHEDONDISABLED);
            if ((ControlWord6040 & CONTROLWORD_COMMAND_SHUTDOWN_MASK) == CONTROLWORD_COMMAND_SHUTDOWN)
            {
                pCiA402Axis->i16State = STATE_READY_TO_SWITCH_ON; // Transition 2
            }
            break;
        case STATE_READY_TO_SWITCH_ON:
            StatusWord |= (STATUSWORD_STATE_READYTOSWITCHON);
            if (((ControlWord6040 & CONTROLWORD_COMMAND_QUICKSTOP_MASK) == CONTROLWORD_COMMAND_QUICKSTOP)
                || ((ControlWord6040 & CONTROLWORD_COMMAND_DISABLEVOLTAGE_MASK) == CONTROLWORD_COMMAND_DISABLEVOLTAGE))
            {
                if(!CiA402_StateTransition7(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED; // Transition 7
                }
            } else
                if (((ControlWord6040 & CONTROLWORD_COMMAND_SWITCHON_MASK) == CONTROLWORD_COMMAND_SWITCHON) ||
                    ((ControlWord6040 & CONTROLWORD_COMMAND_SWITCHON_ENABLEOPERATION_MASK) == CONTROLWORD_COMMAND_SWITCHON_ENABLEOPERATION))
                {
                    if(!CiA402_StateTransition3(pCiA402Axis)){
                        pCiA402Axis->i16State = STATE_SWITCHED_ON;           // Transition 3
                    }
                }
                break;
        case STATE_SWITCHED_ON:
            StatusWord |= (STATUSWORD_STATE_SWITCHEDON);

            if ((ControlWord6040 & CONTROLWORD_COMMAND_SHUTDOWN_MASK) == CONTROLWORD_COMMAND_SHUTDOWN)
            {
                if(!CiA402_StateTransition6(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_READY_TO_SWITCH_ON; // Transition 6
                }
            } else
                if (((ControlWord6040 & CONTROLWORD_COMMAND_QUICKSTOP_MASK) == CONTROLWORD_COMMAND_QUICKSTOP
                    || (ControlWord6040 & CONTROLWORD_COMMAND_DISABLEVOLTAGE_MASK) == CONTROLWORD_COMMAND_DISABLEVOLTAGE))
                {
                    if(!CiA402_StateTransition10(pCiA402Axis)){
                        pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED; // Transition 10
                    }
                } else
                    if ((ControlWord6040 & CONTROLWORD_COMMAND_ENABLEOPERATION_MASK) == CONTROLWORD_COMMAND_ENABLEOPERATION)
                    {
                        if(!CiA402_StateTransition4(pCiA402Axis)){	
                            pCiA402Axis->i16State = STATE_OPERATION_ENABLED;  // Transition 4
                        //The Axis function shall be enabled and all internal set-points cleared.
                        }
                    }
                    break;
        case STATE_OPERATION_ENABLED:
            StatusWord |= (STATUSWORD_STATE_OPERATIONENABLED);

            if ((ControlWord6040 & CONTROLWORD_COMMAND_DISABLEOPERATION_MASK) == CONTROLWORD_COMMAND_DISABLEOPERATION)
            {
                if(pCiA402Axis->Objects.objDisableOperationOptionCode!= DISABLE_DRIVE)
                {
                    /*disable operation pending*/
                    pCiA402Axis->u16PendingOptionCode = 0x605C; //STATE_TRANSITION (STATE_OPERATION_ENABLED,STATE_SWITCHED_ON);
                    return;
                }else{
                    if(!CiA402_StateTransition5(pCiA402Axis)){
                        pCiA402Axis->i16State = STATE_SWITCHED_ON;        // Transition 5
                    }
                }
            } else
                if ((ControlWord6040 & CONTROLWORD_COMMAND_QUICKSTOP_MASK) == CONTROLWORD_COMMAND_QUICKSTOP)
                {
                    if(!CiA402_StateTransition11(pCiA402Axis)){
                        pCiA402Axis->i16State = STATE_QUICK_STOP_ACTIVE;  // Transition 11
                    }
                } else
                    if ((ControlWord6040 & CONTROLWORD_COMMAND_SHUTDOWN_MASK) == CONTROLWORD_COMMAND_SHUTDOWN)
                    {
                        if(pCiA402Axis->Objects.objShutdownOptionCode != DISABLE_DRIVE)
                        {
                            /*shutdown operation required*/
                            pCiA402Axis->u16PendingOptionCode = 0x605B; //STATE_TRANSITION (STATE_OPERATION_ENABLED,STATE_READY_TO_SWITCH_ON);
                            return;
                        }else{
                            if(!CiA402_StateTransition8(pCiA402Axis)){
                                pCiA402Axis->i16State = STATE_READY_TO_SWITCH_ON; // Transition 8
                            }
                        }
                    } else
                        if ((ControlWord6040 & CONTROLWORD_COMMAND_DISABLEVOLTAGE_MASK) == CONTROLWORD_COMMAND_DISABLEVOLTAGE)
                        {
                            if(!CiA402_StateTransition9(pCiA402Axis)){
                                pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED; // Transition 9
                            }
                        }
                        break;
        case STATE_QUICK_STOP_ACTIVE:
            StatusWord |= STATUSWORD_STATE_QUICKSTOPACTIVE;

            if((pCiA402Axis->Objects.objQuickStopOptionCode != DISABLE_DRIVE) &&
                ((pCiA402Axis->Objects.objStatusWord & STATUSWORD_STATE_MASK)!= STATUSWORD_STATE_QUICKSTOPACTIVE))
            {
                /*Only execute quick stop action in state transition 11*/
                pCiA402Axis->u16PendingOptionCode = 0x605A;//STATE_TRANSITION (STATE_OPERATION_ENABLED,STATE_QUICK_STOP_ACTIVE);
                return;
            }

            if ((ControlWord6040 & CONTROLWORD_COMMAND_DISABLEVOLTAGE_MASK) == CONTROLWORD_COMMAND_DISABLEVOLTAGE)
            {
                if(!CiA402_StateTransition12(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED; // Transition 12
                }
            }

            /*NOTE: it is recommend to support transition 16 */
            if ((ControlWord6040 & CONTROLWORD_COMMAND_ENABLEOPERATION_MASK) == CONTROLWORD_COMMAND_ENABLEOPERATION)
            {
                if(!CiA402_StateTransition16(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_OPERATION_ENABLED;  // Transition 16
                    //The Axis function shall be enabled and all internal set-points cleared.
                }
            }
            break;
        case STATE_FAULT_REACTION_ACTIVE:
            StatusWord |= (STATUSWORD_STATE_FAULTREACTIONACTIVE);
            if(pCiA402Axis->Objects.objFaultReactionCode!= DISABLE_DRIVE)
            {
                /*fault reaction pending*/
                pCiA402Axis->u16PendingOptionCode = 0x605E;
                return;
            }else
                if(!CiA402_StateTransition14(pCiA402Axis)){
                // Automatic transition
                pCiA402Axis->i16State = STATE_FAULT;// Transition 14
            }
            break;
        case STATE_FAULT:
            StatusWord |= (STATUSWORD_STATE_FAULT);

            if ((ControlWord6040 & CONTROLWORD_COMMAND_FAULTRESET_MASK) == CONTROLWORD_COMMAND_FAULTRESET)
            {
                if(!CiA402_StateTransition15(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED;// Transition 15
                }
            }
            break;

        default:    //the sate variable is set to in invalid value => rest Axis
            StatusWord = (STATUSWORD_STATE_NOTREADYTOSWITCHON);
            pCiA402Axis->i16State = STATE_NOT_READY_TO_SWITCH_ON;
            break;

        }// switch(current state)



        /*Update operational functions (the low level power supply is always TRUE*/
        switch(pCiA402Axis->i16State)
        {
        case STATE_NOT_READY_TO_SWITCH_ON:
        case STATE_SWITCH_ON_DISABLED:
        case STATE_READY_TO_SWITCH_ON:
            pCiA402Axis->bBrakeApplied = TRUE;
            pCiA402Axis->bHighLevelPowerApplied =  FALSE;
            pCiA402Axis->bAxisFunctionEnabled = FALSE;
            pCiA402Axis->bConfigurationAllowed = TRUE;
            break;
        case STATE_SWITCHED_ON:
            pCiA402Axis->bBrakeApplied = TRUE;
            pCiA402Axis->bHighLevelPowerApplied =  TRUE;
            pCiA402Axis->bAxisFunctionEnabled = FALSE;
            pCiA402Axis->bConfigurationAllowed = TRUE;
            break;
        case STATE_OPERATION_ENABLED:
        case STATE_QUICK_STOP_ACTIVE:
        case STATE_FAULT_REACTION_ACTIVE:
            pCiA402Axis->bBrakeApplied = FALSE;
            pCiA402Axis->bHighLevelPowerApplied =  TRUE;
            pCiA402Axis->bAxisFunctionEnabled = TRUE;
            pCiA402Axis->bConfigurationAllowed = FALSE;
            break;
        case STATE_FAULT:
            pCiA402Axis->bBrakeApplied = TRUE;
            pCiA402Axis->bHighLevelPowerApplied =  FALSE;
            pCiA402Axis->bAxisFunctionEnabled = FALSE;
            pCiA402Axis->bConfigurationAllowed = TRUE;
            break;
        default:
            pCiA402Axis->bBrakeApplied = TRUE;
            pCiA402Axis->bHighLevelPowerApplied =  FALSE;
            pCiA402Axis->bAxisFunctionEnabled = FALSE;
            pCiA402Axis->bConfigurationAllowed = TRUE;
            break;
        }

        if (pCiA402Axis->bHighLevelPowerApplied == TRUE)
        {
            StatusWord |= STATUSWORD_VOLTAGE_ENABLED;
        }
        else
        {
            StatusWord &= ~STATUSWORD_VOLTAGE_ENABLED;
        }

        /*state transition finished set controlword complete bit and update status object 0x6041*/
        pCiA402Axis->Objects.objStatusWord = (StatusWord | STATUSWORD_REMOTE);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return TRUE if moving on predefined ramp is finished

 \brief    CiA402-TransitionAction
 \brief this function shall calculate the desired Axis input values to move on a predefined ramp
 \brief if the ramp is finished return TRUE
*////////////////////////////////////////////////////////////////////////////////////////
BOOL CiA402_TransitionAction(INT16 Characteristic,TCiA402Axis *pCiA402Axis)
{
   switch(Characteristic)
   {

   case SLOW_DOWN_RAMP:
#if _WIN32
      #pragma message ("Warning: Implement slowdown ramp")
#else
      #warning "Implement slowdown ramp"
#endif
      return TRUE;
      break;
   case QUICKSTOP_RAMP:
#if _WIN32
      #pragma message ("Warning: Implement quick stop ramp ramp")
#else
      #warning "Implement quick stop ramp ramp"
#endif
      return TRUE;
      break;
   case STOP_ON_CURRENT_LIMIT:
#if _WIN32
      #pragma message ("Warning: Implement slowdown on current limit ramp")
#else
      #warning "Implement slowdown on current limit ramp"
#endif
      return TRUE;
      break;
   case STOP_ON_VOLTAGE_LIMIT:
#if _WIN32
      #pragma message ("Warning: Implement slowdown on voltage limit ramp")
#else
      #warning "Implement slowdown on voltage limit ramp"
#endif
      return TRUE;
      break;
   default:
      break;
   }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    CiA402-Application
 \brief check if a state transition is pending and pass desired ramp-code to CiA402TransitionAction()
 \brief if this functions returns true the state transition is finished.
*////////////////////////////////////////////////////////////////////////////////////////
void CiA402_Application(TCiA402Axis *pCiA402Axis, UINT16 i)
{
    /*clear "Drive follows the command value" flag if the target values from the master overwritten by the local application*/
    if(pCiA402Axis->u16PendingOptionCode != 0 &&
        (pCiA402Axis->Objects.objModesOfOperationDisplay == CYCLIC_SYNC_POSITION_MODE ||
        pCiA402Axis->Objects.objModesOfOperationDisplay == CYCLIC_SYNC_VELOCITY_MODE))
    {
        pCiA402Axis->Objects.objStatusWord &= ~ STATUSWORD_DRIVE_FOLLOWS_COMMAND;
    }
    else
    {
        pCiA402Axis->Objects.objStatusWord |= STATUSWORD_DRIVE_FOLLOWS_COMMAND;
    }


    switch(pCiA402Axis->u16PendingOptionCode)
    {
    case 0x605A:
        /*state transition 11 is pending analyse shutdown option code (0x605A)*/
        {
            UINT16 ramp = pCiA402Axis->Objects.objQuickStopOptionCode;


            /*masked and execute specified quick stop ramp characteristic */
            if(pCiA402Axis->Objects.objQuickStopOptionCode > 4 && pCiA402Axis->Objects.objQuickStopOptionCode <9)
            {
                    if (pCiA402Axis->Objects.objQuickStopOptionCode == 5)
                    {
                        ramp = 1;
                    }
                if (pCiA402Axis->Objects.objQuickStopOptionCode == 6)
                {
                    ramp = 2;
                }
                if (pCiA402Axis->Objects.objQuickStopOptionCode == 7)
                {
                    ramp = 3;
                }
                if (pCiA402Axis->Objects.objQuickStopOptionCode == 8)
                {
                    ramp = 4;
                }
            }

            if(CiA402_TransitionAction(ramp,pCiA402Axis))
            {
                /*quick stop ramp is finished complete state transition*/
                pCiA402Axis->u16PendingOptionCode = 0x0;
                if(pCiA402Axis->Objects.objQuickStopOptionCode > 0 && pCiA402Axis->Objects.objQuickStopOptionCode < 5)
                {
                    if(!CiA402_StateTransition12(pCiA402Axis)){
                        pCiA402Axis->i16State = STATE_SWITCH_ON_DISABLED;    //continue state transition 12
                    }
                }
                else if(pCiA402Axis->Objects.objQuickStopOptionCode > 4 && pCiA402Axis->Objects.objQuickStopOptionCode < 9)
                    pCiA402Axis->Objects.objStatusWord |= STATUSWORD_TARGET_REACHED;
                    pCiA402Axis->Objects.objStatusWord &= ~(STATUSWORD_STATE_SWITCHEDONDISABLED);
            }
        }
        break;
    case 0x605B:
        /*state transition 8 is pending analyse shutdown option code (0x605B)*/
        {
            if(CiA402_TransitionAction(pCiA402Axis->Objects.objShutdownOptionCode,pCiA402Axis))
            {
                /*shutdown ramp is finished complete state transition*/
                pCiA402Axis->u16PendingOptionCode = 0x0;
                if(!CiA402_StateTransition8(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_READY_TO_SWITCH_ON;    //continue state transition 8
                }
            }
        }
        break;
    case 0x605C:
        /*state transition 5 is pending analyse Disable operation option code (0x605C)*/
        {
            if(CiA402_TransitionAction(pCiA402Axis->Objects.objDisableOperationOptionCode,pCiA402Axis))
            {
                /*disable operation ramp is finished complete state transition*/
                pCiA402Axis->u16PendingOptionCode = 0x0;
                if(!CiA402_StateTransition5(pCiA402Axis)){
                    pCiA402Axis->i16State = STATE_SWITCHED_ON;    //continue state transition 5
                }
            }
        }
        break;

    case 0x605E:
        /*state transition 14 is pending analyse Fault reaction option code (0x605E)*/
        {
            if(CiA402_TransitionAction(pCiA402Axis->Objects.objFaultReactionCode,pCiA402Axis))
            {
                /*fault reaction ramp is finished complete state transition*/
                pCiA402Axis->u16PendingOptionCode = 0x0;
                if(!CiA402_StateTransition14(pCiA402Axis)){
                    // Automatic transition
                    pCiA402Axis->i16State = STATE_FAULT;// Transition 14
                }
            }
        }
        break;
    default:
        //pending transition code is invalid => values from the master are used
        pCiA402Axis->Objects.objStatusWord |= STATUSWORD_DRIVE_FOLLOWS_COMMAND;
        break;
    }
    if(bDcSyncActive
        && (pCiA402Axis->u32CycleTime != 0)
        && ((pCiA402Axis->Objects.objSupportedDriveModes >> (pCiA402Axis->Objects.objModesOfOperation - 1)) & 0x1)) //Mode of Operation (0x6060) - 1 specifies the Bit within Supported Drive Modes (0x6502)
    {
#if (_DUMMY_ == 1)
        CiA402_DummyMotionControl(pCiA402Axis, i);
#else
        APPL_MOTOR_MotionControl_Main(pCiA402Axis, i);
#endif
    }



}


/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param     index                 index of the requested object.
 \param     subindex                subindex of the requested object.
 \param     dataSize                received data size of the SDO Download
 \param     pObjEntry            handle to the dictionary object returned by
                                     OBJ_GetObjectHandle which was called before
 \param    pData                    Pointer to the buffer where the written data can be copied from
 \param    bCompleteAccess    Indicates if a complete write of all subindices of the
                                     object shall be done or not

 \return    result of the write operation (0 (success) or an abort code (ABORTIDX_.... defined in
            sdosrv.h))

 \brief    This function writes "Configured Modules" Object 0xF030
*////////////////////////////////////////////////////////////////////////////////////////

UINT8 Write0xF030( UINT16 index, UINT8 subindex, UINT32 dataSize, UINT16 MBXMEM * pData, UINT8 bCompleteAccess )
{

    UINT16 i = subindex;
    UINT16 maxSubindex = sConfiguredModuleIdentList.u16SubIndex0;
    OBJCONST TSDOINFOENTRYDESC OBJMEM *pEntry;
    /* lastSubindex is used for complete access to make loop over the requested entries
       to be read, we initialize this variable with the requested subindex that only
       one loop will be done for a single access */
    UINT8 lastSubindex = subindex;

    if ( bCompleteAccess )
    {
        if ( subindex == 0 )
        {
            /* we change the subindex 0 */
            maxSubindex = (UINT8) pData[0];
        }

        /* we write until the maximum subindex */
        lastSubindex = (UINT8)maxSubindex;
    }
    else
        if (subindex > maxSubindex)
        {
            /* the maximum subindex is reached */
            return ABORTIDX_SUBINDEX_NOT_EXISTING;
        }
    else
    {
        /* we check the write access for single accesses here, a complete write access
           is allowed if at least one entry is writable (in this case the values for the
            read only entries shall be ignored) */
        /* we get the corresponding entry description */
        pEntry = &asEntryDesc0xF030[subindex];

        /* check if we have write access (bits 3-5 (PREOP, SAFEOP, OP) of ObjAccess)
           by comparing with the actual state (bits 1-3 (PREOP, SAFEOP, OP) of AL Status) */
        if (0 == (((UINT8) ((pEntry->ObjAccess & ACCESS_WRITE) >> 2)) & (nAlStatus & STATE_MASK) ))
        {
            /* we don't have write access */
            if ( (pEntry->ObjAccess & ACCESS_WRITE) == 0 )
            {
                /* it is a read only entry */
                return ABORTIDX_READ_ONLY_ENTRY;
            }
            else
            {
                /* we don't have write access in this state */
                return ABORTIDX_IN_THIS_STATE_DATA_CANNOT_BE_READ_OR_STORED;
            }
        }
    }

        /* we use the standard write function */
        for (i = subindex; i <= lastSubindex; i++)
        {
            /* we have to copy the entry */
            if (i == 0)
            {
                /*check if the value for subindex0 is valid */
                if(MAX_AXES < (UINT8) pData[0])
                {
                    return ABORTIDX_VALUE_TOO_GREAT;
                }

                sConfiguredModuleIdentList.u16SubIndex0 =  pData[0];

                /* we increment the destination pointer by 2 because the subindex 0 will be
                    transmitted as UINT16 for a complete access */
                pData++;
            }
            else
            {
                UINT32 CurValue = sConfiguredModuleIdentList.aEntries[(i-1)];
                UINT16 MBXMEM *pVarPtr = (UINT16 MBXMEM *) &sConfiguredModuleIdentList.aEntries[(i-1)];

                pVarPtr[0] = pData[0];
                pVarPtr[1] = pData[1];
                pData += 2;

                /*Check if valid value was written*/
                if((sConfiguredModuleIdentList.aEntries[(i-1)] != (UINT32)CSV_CSP_MODULE_ID)
                && (sConfiguredModuleIdentList.aEntries[(i-1)] != (UINT32)CSP_MODULE_ID)
                && (sConfiguredModuleIdentList.aEntries[(i-1)] != (UINT32)CSV_MODULE_ID)
                && (sConfiguredModuleIdentList.aEntries[(i-1)] != 0))
                {
                    /*write previous value*/
                    sConfiguredModuleIdentList.aEntries[(i-1)] = CurValue;

                    /*reset subindex 0 (if required)*/
                    if(sConfiguredModuleIdentList.aEntries[(i-1)] != 0)
                    {
                        sConfiguredModuleIdentList.u16SubIndex0 = i;
                    }
                    else
                    {
                        /*current entry is 0 => set subindex0 value i-1*/
                        sConfiguredModuleIdentList.u16SubIndex0 = (i-1);
                    }


                    return ABORTIDX_VALUE_EXCEEDED;
                }
           }
        }

        /*Update PDO assign objects and 0xF010 (Module Profile List)*/
        {
        UINT8 cnt = 0;

        /*Update 0xF010.0 */
        sModuleProfileInfo.u16SubIndex0 = sConfiguredModuleIdentList.u16SubIndex0;
        
        /*Update PDO assign SI0*/
        sRxPDOassign.u16SubIndex0 = sConfiguredModuleIdentList.u16SubIndex0;
        sTxPDOassign.u16SubIndex0 = sConfiguredModuleIdentList.u16SubIndex0;

        for (cnt = 0 ; cnt < sConfiguredModuleIdentList.u16SubIndex0; cnt++)
        {
            /*all Modules have the same profile number*/
/*ECATCHANGE_START(V5.13) CIA402 2*/
            sModuleProfileInfo.aEntries[cnt] = (DEVICE_PROFILE_TYPE >> 16);
/*ECATCHANGE_END(V5.13) CIA402 2*/

            switch(sConfiguredModuleIdentList.aEntries[cnt])
            {
                case CSV_CSP_MODULE_ID:
                    sRxPDOassign.aEntries[cnt] = (0x1600 +(0x10*cnt));
                    sTxPDOassign.aEntries[cnt] = (0x1A00 +(0x10*cnt));
                break;
                case CSP_MODULE_ID:
                    sRxPDOassign.aEntries[cnt] = (0x1601 +(0x10*cnt));
                    sTxPDOassign.aEntries[cnt] = (0x1A01 +(0x10*cnt));
                break;
                case CSV_MODULE_ID:
                    sRxPDOassign.aEntries[cnt] = (0x1602 +(0x10*cnt));
                    sTxPDOassign.aEntries[cnt] = (0x1A02 +(0x10*cnt));
                break;
                default:
                    sRxPDOassign.aEntries[cnt] = 0;
                    sTxPDOassign.aEntries[cnt] = 0;

                    sModuleProfileInfo.aEntries[cnt] = 0;
                break;
            }
        }
        }

    return 0;
}

/*-----------------------------------------------------------------------------------------
------
------    generic functions
------
-----------------------------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \brief    The function is called when an error state was acknowledged by the master

*////////////////////////////////////////////////////////////////////////////////////////

void    APPL_AckErrorInd(UINT16 stateTrans)
{
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from INIT to PREOP when
           all general settings were checked to start the mailbox handler. This function
           informs the application about the state transition, the application can refuse
           the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete 
           the transition by calling ECAT_StateChange.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartMailboxHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from PREEOP to INIT
           to stop the mailbox handler. This functions informs the application
           about the state transition, the application cannot refuse
           the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopMailboxHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \param    pIntMask    pointer to the AL Event Mask which will be written to the AL event Mask
                        register (0x204) when this function is succeeded. The event mask can be adapted
                        in this function
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from PREOP to SAFEOP when
             all general settings were checked to start the input handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
            The return code NOERROR_INWORK can be used, if the application cannot confirm
            the state transition immediately, in that case the application need to be complete 
            the transition by calling ECAT_StateChange.
*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartInputHandler(UINT16 *pIntMask)
{
    UINT32 Sync0CycleTime = 0;
    if(sConfiguredModuleIdentList.u16SubIndex0 == 0)
    {
        /* Object 0xF030 was not written before 
        * => update object 0xF010 (Module profile list) and 0xF050 (Detected Module List)*/
    
        UINT8 cnt = 0;


        /*Update 0xF010.0 */
        sModuleProfileInfo.u16SubIndex0 = sRxPDOassign.u16SubIndex0;

        /*Update 0xF050.0*/
        sDetectedModuleIdentList.u16SubIndex0 = sRxPDOassign.u16SubIndex0;
        
        for (cnt = 0 ; cnt < sRxPDOassign.u16SubIndex0; cnt++)
        {
            /*all Modules have the same profile number*/
/*ECATCHANGE_START(V5.13) CIA402 2*/
            sModuleProfileInfo.aEntries[cnt] = (DEVICE_PROFILE_TYPE >> 16);
/*ECATCHANGE_END(V5.13) CIA402 2*/

            switch((sRxPDOassign.aEntries[cnt] & 0x000F)) //get only identification of the PDO mapping object
            {
                case 0x0:   //csv/csp PDO selected
                    sDetectedModuleIdentList.aEntries[cnt] = CSV_CSP_MODULE_ID;
                break;
                case 0x1:   //csp PDO selected
                    sDetectedModuleIdentList.aEntries[cnt] = CSP_MODULE_ID;
                break;
                case 0x2:   //csv PDO selected
                    sDetectedModuleIdentList.aEntries[cnt] = CSV_MODULE_ID;
                break;

            }
            
        }
    }

    HW_EscReadDWord(Sync0CycleTime, ESC_DC_SYNC0_CYCLETIME_OFFSET);
    Sync0CycleTime = SWAPDWORD(Sync0CycleTime);

    /*Init CiA402 structure if the device is in SM Sync mode
    the CiA402 structure will be Initialized after calculation of the Cycle time*/
    if(bDcSyncActive == TRUE)
    {
        UINT16 i;
        Sync0CycleTime = Sync0CycleTime / 1000; //get cycle time in us
        for(i = 0; i< MAX_AXES;i++)
        {
                if (LocalAxes[i].bAxisIsActive)
                {
                    LocalAxes[i].u32CycleTime = Sync0CycleTime;
                }
        }
    }

    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from SAFEOP to PREEOP
             to stop the input handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopInputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from SAFEOP to OP when
             all general settings were checked to start the output handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete 
           the transition by calling ECAT_StateChange.
*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StartOutputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from OP to SAFEOP
             to stop the output handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*////////////////////////////////////////////////////////////////////////////////////////

UINT16 APPL_StopOutputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\return     0(ALSTATUSCODE_NOERROR), NOERROR_INWORK
\param      pInputSize  pointer to save the input process data length
\param      pOutputSize  pointer to save the output process data length

\brief    This function calculates the process data sizes from the actual SM-PDO-Assign
            and PDO mapping
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 APPL_GenerateMapping(UINT16* pInputSize,UINT16* pOutputSize)
{
    UINT16 result = ALSTATUSCODE_NOERROR;
    UINT16 PDOAssignEntryCnt = 0;
    UINT8 AxisIndex = 0;
    UINT16 u16cnt = 0;
    UINT16 InputSize = 0;
    UINT16 OutputSize = 0;
    TOBJECT OBJMEM *pDiCEntry = NULL;


    if (sRxPDOassign.u16SubIndex0 > MAX_AXES)
    {
        return ALSTATUSCODE_NOVALIDOUTPUTS;
    }

    /*Update object dictionary according to activated axis
    which axes are activated is calculated by object 0x1C12*/
    for(PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sRxPDOassign.u16SubIndex0; PDOAssignEntryCnt++)
    {
        /*The PDO mapping objects are specified with an 0x10 offset => get the axis index*/
        AxisIndex = (sRxPDOassign.aEntries[PDOAssignEntryCnt] & 0xF0) >> 4;

        if(AxisIndex == PDOAssignEntryCnt)
        {
            /*Axis is mapped to process data check if axis objects need to be added to the object dictionary*/

            if(!LocalAxes[PDOAssignEntryCnt].bAxisIsActive)
            {
                /*add objects to dictionary*/
                pDiCEntry = LocalAxes[PDOAssignEntryCnt].ObjDic;

                while(pDiCEntry->Index != 0xFFFF)
                {
                    result = COE_AddObjectToDic(pDiCEntry);

                    if(result != 0)
                    {
                        return result;
                    }

                    pDiCEntry++;    //get next entry
                }

                
                LocalAxes[PDOAssignEntryCnt].bAxisIsActive = TRUE;
            }

        }
        else
        {
            /*Axis is not mapped to process data check if axis objects need to be removed from object dictionary*/
            if(LocalAxes[PDOAssignEntryCnt].bAxisIsActive)
            {
                /*add objects to dictionary*/
                pDiCEntry = LocalAxes[PDOAssignEntryCnt].ObjDic;

                while(pDiCEntry->Index != 0xFFFF)
                {
                    COE_RemoveDicEntry(pDiCEntry->Index);

                    pDiCEntry++;    //get next entry
                }

                
                LocalAxes[PDOAssignEntryCnt].bAxisIsActive = FALSE;
            }
        }

    }

    /*Scan object 0x1C12 RXPDO assign*/
    for(PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sRxPDOassign.u16SubIndex0; PDOAssignEntryCnt++)
    {
        switch ((sRxPDOassign.aEntries[PDOAssignEntryCnt] & 0x000F))    //mask Axis type (supported modes)
        {
            case 0:
                /*drive mode supported    csv(cyclic sync velocity) : bit 8
                                        csp (cyclic sync position) : bit 7*/
                LocalAxes[PDOAssignEntryCnt].Objects.objSupportedDriveModes = 0x180;
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap0.u16SubIndex0;u16cnt++)
                {
                    OutputSize +=(UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap0.aEntries[u16cnt] & 0xFF);
                }
                break;
            case 1:
                /*drive mode supported    csp (cyclic sync position) : bit 7*/
                LocalAxes[PDOAssignEntryCnt].Objects.objSupportedDriveModes = 0x80;
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap1.u16SubIndex0;u16cnt++)
                {
                    OutputSize +=(UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap1.aEntries[u16cnt] & 0xFF);
                }
                break;
            case 2:
                    /*drive mode supported    csv(cyclic sync velocity) : bit 8*/
                LocalAxes[PDOAssignEntryCnt].Objects.objSupportedDriveModes= 0x100;
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap2.u16SubIndex0;u16cnt++)
                {
                    OutputSize += (UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sRxPDOMap2.aEntries[u16cnt] & 0xFF);;
                }
                break;
        }
    }

    OutputSize = OutputSize >> 3;

    if(result == 0)
    {
        /*Scan Object 0x1C13 TXPDO assign*/
        for(PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sTxPDOassign.u16SubIndex0; PDOAssignEntryCnt++)
        {
            switch ((sTxPDOassign.aEntries[PDOAssignEntryCnt] & 0x000F))    //mask Axis type (supported modes)
            {
            case 0: /*csp/csv*/
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap0.u16SubIndex0;u16cnt++)
                {
                    InputSize +=(UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap0.aEntries[u16cnt] & 0xFF);
                }
                break;
            case 1: /*csp*/
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap1.u16SubIndex0;u16cnt++)
                {
                    InputSize +=(UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap1.aEntries[u16cnt] & 0xFF);
                }
                break;
            case 2: /*csv*/
                for(u16cnt =0 ; u16cnt < LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap2.u16SubIndex0;u16cnt++)
                {
                    InputSize +=(UINT16)(LocalAxes[PDOAssignEntryCnt].Objects.sTxPDOMap2.aEntries[u16cnt] & 0xFF);
                }
                break;
            }
        }
        
        InputSize = InputSize >> 3;
    }

    *pInputSize = InputSize;
    *pOutputSize = OutputSize;
    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\param      pData  pointer to input process data
\brief      This function will copies the inputs from the local memory to the ESC memory
            to the hardware
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_InputMapping(UINT16* pData)
{
    UINT16 j = 0;
    UINT8 *pTmpData = (UINT8 *)pData;
    UINT8 AxisIndex;

    for (j = 0; j < sTxPDOassign.u16SubIndex0; j++)
    {
        /*The Axis index is based on the PDO mapping offset (0x10)*/
        AxisIndex = ((sTxPDOassign.aEntries[j] & 0xF0) >> 4);

        switch ((sTxPDOassign.aEntries[j]& 0x000F))
        {
        case 0:    //copy csp/csv TxPDO entries
            {
                TCiA402PDO1A00 *pInputs = (TCiA402PDO1A00 *)pTmpData;
                pInputs->ObjErrorCode = SWAPWORD(LocalAxes[AxisIndex].Objects.objErrorCode);
                pInputs->ObjStatusWord = SWAPWORD(LocalAxes[AxisIndex].Objects.objStatusWord);
                pInputs->ObjPositionActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objPositionActualValue);
                pInputs->ObjVelocityActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objVelocityActualValue);
                pInputs->ObjTorqueActualValue = SWAPWORD(LocalAxes[AxisIndex].Objects.objTorqueActualValue);
                pInputs->ObjTouchProbeStatus = SWAPWORD(LocalAxes[AxisIndex].Objects.objTouchProbeStatus);
                pInputs->ObjTouchProbePos1PosValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objTouchProbePos1PosValue);
                pInputs->ObjTouchProbePos1NegValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.ObjTouchProbePos1NegValue);
                pInputs->ObjFollowingErrorActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objFollowingErrorActualValue);
                pInputs->ObjDigitalInputs = SWAPDWORD(LocalAxes[AxisIndex].Objects.objDigitalInputs);
                pInputs->ObjModesOfOperationDisplay = SWAPWORD((LocalAxes[AxisIndex].Objects.objModesOfOperationDisplay & 0x00FF));

                /*shift pointer PDO mapping object following*/
                if(j < (sTxPDOassign.u16SubIndex0 - 1))
                    pTmpData += SIZEOF(TCiA402PDO1A00);
            }
            break;
        case 1://copy csp TxPDO entries
            {
                TCiA402PDO1A01 *pInputs = (TCiA402PDO1A01 *)pTmpData;
                pInputs->ObjErrorCode = SWAPWORD(LocalAxes[AxisIndex].Objects.objErrorCode);
                pInputs->ObjStatusWord = SWAPWORD(LocalAxes[AxisIndex].Objects.objStatusWord);
                pInputs->ObjPositionActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objPositionActualValue);
                pInputs->ObjVelocityActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objVelocityActualValue);
                pInputs->ObjTorqueActualValue  = SWAPWORD(LocalAxes[AxisIndex].Objects.objTorqueActualValue);
                pInputs->ObjTouchProbeStatus = SWAPWORD(LocalAxes[AxisIndex].Objects.objTouchProbeStatus);
                pInputs->ObjTouchProbePos1PosValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objTouchProbePos1PosValue);
                pInputs->ObjTouchProbePos1NegValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.ObjTouchProbePos1NegValue);
                pInputs->ObjFollowingErrorActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objFollowingErrorActualValue);
                pInputs->ObjDigitalInputs = SWAPDWORD(LocalAxes[AxisIndex].Objects.objDigitalInputs);
      //          pInputs->ObjModesOfOperationDisplay = SWAPWORD((LocalAxes[AxisIndex].Objects.objModesOfOperationDisplay & 0x00FF));
               
                /*shift pointer PDO mapping object following*/
                if (j < (sTxPDOassign.u16SubIndex0 - 1))
                {
                    pTmpData += SIZEOF(TCiA402PDO1A01);
                }
            }
            break;
        case 2://copy csv TxPDO entries
            {
                TCiA402PDO1A02 *pInputs = (TCiA402PDO1A02 *)pTmpData;
                pInputs->ObjErrorCode = SWAPWORD(LocalAxes[AxisIndex].Objects.objErrorCode);
                pInputs->ObjStatusWord = SWAPWORD(LocalAxes[AxisIndex].Objects.objStatusWord);
                pInputs->ObjPositionActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objPositionActualValue);
                pInputs->ObjVelocityActualValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objVelocityActualValue);
                pInputs->ObjTorqueActualValue = SWAPWORD(LocalAxes[AxisIndex].Objects.objTorqueActualValue);
                pInputs->ObjTouchProbeStatus = SWAPWORD(LocalAxes[AxisIndex].Objects.objTouchProbeStatus);
                pInputs->ObjTouchProbePos1PosValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.objTouchProbePos1PosValue);
                pInputs->ObjTouchProbePos1NegValue = SWAPDWORD(LocalAxes[AxisIndex].Objects.ObjTouchProbePos1NegValue);
                pInputs->ObjDigitalInputs = SWAPDWORD(LocalAxes[AxisIndex].Objects.objDigitalInputs);
        //        pInputs->ObjModesOfOperationDisplay = SWAPWORD((LocalAxes[AxisIndex].Objects.objModesOfOperationDisplay & 0x00FF));
            
                /*shift pointer PDO mapping object following*/
                if (j < (sTxPDOassign.u16SubIndex0 - 1))
                {
                    pTmpData += SIZEOF(TCiA402PDO1A02);
                }
            }
            break;
        }    //switch TXPDO entry
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
\param      pData  pointer to output process data

\brief    This function will copies the outputs from the ESC memory to the local memory
            to the hardware
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_OutputMapping(UINT16* pData)
{
    UINT16 j = 0;
    UINT8 *pTmpData = (UINT8 *)pData;
    UINT8 AxisIndex;

    for (j = 0; j < sRxPDOassign.u16SubIndex0; j++)
    {
        /*The Axis index is based on the PDO mapping offset (0x10)*/
        AxisIndex = ((sRxPDOassign.aEntries[j] & 0xF0) >> 4);

        switch ((sRxPDOassign.aEntries[j] & 0x000F))
        {
        case 0:    //csp/csv RxPDO    entries
            {
            TCiA402PDO1600 *pOutputs = (TCiA402PDO1600 *)pTmpData;

            LocalAxes[AxisIndex].Objects.objControlWord = SWAPWORD(pOutputs->ObjControlWord);
            LocalAxes[AxisIndex].Objects.objModesOfOperation = SWAPWORD((pOutputs->ObjModesOfOperation & 0x00FF));
            LocalAxes[AxisIndex].Objects.objMaxTorque = SWAPWORD(pOutputs->ObjMaxTorque);
            LocalAxes[AxisIndex].Objects.objTargetPosition = SWAPDWORD(pOutputs->ObjTargetPosition);
            LocalAxes[AxisIndex].Objects.objPositionOffset = SWAPDWORD(pOutputs->ObjPositionOffset);
            LocalAxes[AxisIndex].Objects.objVelocityOffset = SWAPDWORD(pOutputs->ObjVelocityOffset);
            LocalAxes[AxisIndex].Objects.objTorqueOffset = SWAPWORD(pOutputs->ObjTorqueOffset);
            LocalAxes[AxisIndex].Objects.objTouchProbeFunction = SWAPWORD(pOutputs->ObjTouchProbeFunction);
            LocalAxes[AxisIndex].Objects.objPositiveTorqueLimitValue = SWAPWORD(pOutputs->ObjPositiveTorqueLimitValue);
            LocalAxes[AxisIndex].Objects.objNegativeTorqueLimitValue = SWAPWORD(pOutputs->ObjNegativeTorqueLimitValue);
            LocalAxes[AxisIndex].Objects.objTargetVelocity    = SWAPDWORD(pOutputs->ObjTargetVelocity);
            /*shift pointer PDO mapping object following*/
            if (j < (sRxPDOassign.u16SubIndex0 - 1))
            {
                pTmpData += SIZEOF(TCiA402PDO1600);
            }
            }
            break;
        case 1:    //csp RxPDO    entries
            {
            TCiA402PDO1601 *pOutputs = (TCiA402PDO1601 *)pTmpData;

            LocalAxes[AxisIndex].Objects.objControlWord = SWAPWORD(pOutputs->ObjControlWord);
            LocalAxes[AxisIndex].Objects.objMaxTorque = SWAPWORD(pOutputs->ObjMaxTorque);
            LocalAxes[AxisIndex].Objects.objTargetPosition = SWAPDWORD(pOutputs->ObjTargetPosition);
            LocalAxes[AxisIndex].Objects.objPositionOffset = SWAPDWORD(pOutputs->ObjPositionOffset);
            LocalAxes[AxisIndex].Objects.objVelocityOffset = SWAPDWORD(pOutputs->ObjVelocityOffset);
            LocalAxes[AxisIndex].Objects.objTorqueOffset = SWAPWORD(pOutputs->ObjTorqueOffset);
            LocalAxes[AxisIndex].Objects.objTouchProbeFunction = SWAPWORD(pOutputs->ObjTouchProbeFunction);
            LocalAxes[AxisIndex].Objects.objPositiveTorqueLimitValue = SWAPWORD(pOutputs->ObjPositiveTorqueLimitValue);
            LocalAxes[AxisIndex].Objects.objNegativeTorqueLimitValue = SWAPWORD(pOutputs->ObjNegativeTorqueLimitValue);
//            LocalAxes[AxisIndex].Objects.objSoftwarePositionLimit.i32MinLimit = SWAPDWORD(pOutputs->ObjSoftwarePositionLimit.i32MinLimit);
//            LocalAxes[AxisIndex].Objects.objSoftwarePositionLimit.i32MaxLimit = SWAPDWORD(pOutputs->ObjSoftwarePositionLimit.i32MaxLimit);
            LocalAxes[AxisIndex].Objects.objDigitalOutputs.u32MotorRevolutions = SWAPDWORD(pOutputs->Obj60FE_u32ShaftRevolutions);
            
            /*shift pointer PDO mapping object following*/
            if (j < (sRxPDOassign.u16SubIndex0 - 1))
            {
                pTmpData += SIZEOF(TCiA402PDO1601);
            }
            }
            break;
        case 2:    //csv RxPDO    entries
            {
            TCiA402PDO1602 *pOutputs = (TCiA402PDO1602 *)pTmpData;

            LocalAxes[AxisIndex].Objects.objControlWord = SWAPWORD(pOutputs->ObjControlWord);
            LocalAxes[AxisIndex].Objects.objMaxTorque = SWAPWORD(pOutputs->ObjMaxTorque);
            LocalAxes[AxisIndex].Objects.objVelocityOffset = SWAPDWORD(pOutputs->ObjVelocityOffset);
            LocalAxes[AxisIndex].Objects.objTorqueOffset = SWAPDWORD(pOutputs->ObjTorqueOffset);
            LocalAxes[AxisIndex].Objects.objTouchProbeFunction = SWAPWORD(pOutputs->ObjTouchProbeFunction);
            LocalAxes[AxisIndex].Objects.objPositiveTorqueLimitValue = SWAPWORD(pOutputs->ObjPositiveTorqueLimitValue);
            LocalAxes[AxisIndex].Objects.objNegativeTorqueLimitValue = SWAPWORD(pOutputs->ObjNegativeTorqueLimitValue);
            LocalAxes[AxisIndex].Objects.objTargetVelocity    = SWAPDWORD(pOutputs->ObjTargetVelocity);

            /*shift pointer PDO mapping object following*/
            if (j < (sRxPDOassign.u16SubIndex0 - 1))
            {
                pTmpData += SIZEOF(TCiA402PDO1602);
            }
            }
            break;
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////////////
/**
\brief    This function will called from the synchronisation ISR 
            or from the mainloop if no synchronisation is supported
*////////////////////////////////////////////////////////////////////////////////////////
void APPL_Application(void)
{
    UINT16 i;

    for(i = 0; i < MAX_AXES;i++)
    {
            if (LocalAxes[i].bAxisIsActive)
            {
                CiA402_Application(&LocalAxes[i], i);
            }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
/**
 \return    The Explicit Device ID of the EtherCAT slave

 \brief     Calculate the Explicit Device ID
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 APPL_GetDeviceID(void)
{
#if defined(BOARD_RZT2M_RSK)
	return ((APPL_GetDipSw() >> 4) & 0x000f);			// SW3.8-5
#elif defined(BOARD_RZN2L_RSK) | defined(BOARD_RZT2L_RSK)		 
 return (APPL_GetDipSw() & 0x000f);            // SW3.1-4
#else
    return 0x5;
#endif
}

#if defined(BOARD_RZT2M_RSK) | defined(BOARD_RZN2L_RSK) | defined(BOARD_RZT2L_RSK)
/////////////////////////////////////////////////////////////////////////////////////////
/**
 \retuen   UINT16 DIP SW value. Low input level means ON.

 \brief    Get DIP SW
*////////////////////////////////////////////////////////////////////////////////////////
UINT16 APPL_GetDipSw(void)
{
	UINT16 u16DipSw;

	u16DipSw = 0;

	/* DIP SW type structure */
	bsp_dip_sws_t dipsws = g_bsp_dip_sws;

	/* This code uses BSP IO functions to show how it is used.*/
	R_BSP_PinAccessEnable();

	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_0]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x01;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_1]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x02;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_2]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x04;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_3]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x08;

#if defined(BOARD_RZT2M_RSK)
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_4]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x10;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_5]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x20;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_6]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x40;
	if (R_BSP_PinRead((bsp_io_port_pin_t)dipsws.p_sws[BSP_DIPSW_7]) == BSP_IO_LEVEL_LOW) u16DipSw |= 0x80;
#endif

	/* Protect PFS registers */
	R_BSP_PinAccessDisable();

	return u16DipSw;
}
#endif

/** @} */

