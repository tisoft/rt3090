/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  * 
 * it under the terms of the GNU General Public License as published by  * 
 * the Free Software Foundation; either version 2 of the License, or     * 
 * (at your option) any later version.                                   * 
 *                                                                       * 
 * This program is distributed in the hope that it will be useful,       * 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        * 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         * 
 * GNU General Public License for more details.                          * 
 *                                                                       * 
 * You should have received a copy of the GNU General Public License     * 
 * along with this program; if not, write to the                         * 
 * Free Software Foundation, Inc.,                                       * 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             * 
 *                                                                       * 
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All related WMM ACM IOCTL function body.

***************************************************************************/


/* ----- Compile Option ------ */
#define IEEE80211E_SIMULATION

#include "rt_config.h"

#ifdef WMM_ACM_SUPPORT

/* IEEE802.11E related include files */
#include "acm_extr.h" /* used for other modules */
#include "acm_comm.h" /* used for edca/wmm */
#include "acm_edca.h" /* used for edca/wmm */



/* ----- Local Definition ------ */
#define ACM_QOS_SANITY_CHECK(__pAd)						\
	if (__pAd == NULL)									\
	{													\
		printk("err> __pAd == NULL!\n");				\
		return;											\
	}

#define ACM_ARGC_SANITY_CHECK(__Min, __Max) 			\
	if ((Argc != __Min) && (Argc != __Max)) 			\
	{													\
		printk("11e_err> parameters number error!\n");	\
		return;											\
	}

#define ACM_IN_SANITY_CHECK(__Condition, __Msg) 		\
	if (__Condition)									\
	{													\
		printk __Msg;									\
		goto LabelErr;									\
	}

#define ACM_NIN_DEC_GET(__Src, __Max, __MsgErr)			\
	{													\
		__Src = AcmCmdUtilNumGet(&pArgv);				\
		ACM_IN_SANITY_CHECK((__Src > __Max), __MsgErr);	\
	}

#define ACM_NIN_DEC_MGET(__Src, __Min, __Max, __MsgErr)	\
	{													\
		__Src = AcmCmdUtilNumGet(&pArgv);				\
		ACM_IN_SANITY_CHECK((__Src < __Min) || (__Src > __Max), __MsgErr);	\
	}

#define ACM_RANGE_SANITY_CHECK(__Range, __Min, __Max, __MsgErr) \
	{															\
		ACM_IN_SANITY_CHECK((__Range < __Min) || (__Range > __Max), __MsgErr);\
	}

#define ACM_RATE_MAX	((UINT32)300000000)		/* 300Mbps */

#ifdef IEEE80211E_SIMULATION
typedef struct _ACM_DATA_SIM {

	UCHAR MacSrc[6];
	UCHAR MacDst[6];

	UCHAR Direction;		/* 0: receive; 1:transmit */
	UCHAR Type;				/* 0: 11e; 1: WME */
	UCHAR TID;				/* 0 ~ 7 */
	UCHAR AckPolicy;		/* 0: normal ACK; 1: no ACK */

	UINT32 FrameSize;		/* data size */
	UINT32 FlgIsValidEntry;	/* 1: valid; 0: invalid */

	UINT16 NumSeq:12;
	UINT16 NumFrag:4;
} ACM_DATA_SIM;

static UINT32 gSimDelay = 0;
static UINT32 gSimDelayCount;
#endif // IEEE80211E_SIMULATION //


/* ----- Extern Variable ----- */
#ifdef ACM_MEMORY_TEST
extern UINT32 gAcmMemAllocNum;
extern UINT32 gAcmMemFreeNum;
#endif // ACM_MEMORY_TEST //

extern UCHAR gAcmTestFlag;

extern VOID ap_cmm_peer_assoc_req_action(
										IN PRTMP_ADAPTER pAd,
										IN MLME_QUEUE_ELEM *Elem,
										IN BOOLEAN isReassoc);


/* ----- Private Variable ----- */
static ACM_TCLAS gCMD_TCLAS_Group[ACM_TSPEC_TCLAS_MAX_NUM];
static UINT32 gTLS_Grp_ID;

#define SIM_AP_NAME "SampleACMAP"

#ifdef IEEE80211E_SIMULATION
static UCHAR gMAC_STA[6] = { 0x00, 0x0e, 0x2e, 0x82, 0xe7, 0x6d };
static UCHAR gMAC_AP[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x01 };

static UCHAR gSimTCPFlag;
static UCHAR gSimTCPDSCP;


ACMR_OS_TASK_STRUCT gTaskletSim;
ACMR_OS_TIMER_STRUCT gTimerSim;

static UINT32 gTaskDataSleep;
static ACMR_OS_SPIN_LOCK gSpinLockSim;

static BOOLEAN gCmdFlgIsInit;

#define ACM_DATA_SEM_LOCK(LabelSemErr) \
	do { ACMR_OS_SPIN_LOCK_BH(&gSpinLockSim); \
		if (0) goto LabelSemErr; } while(0);

#define ACM_DATA_SEM_UNLOCK() \
	do { ACMR_OS_SPIN_UNLOCK_BH(&gSpinLockSim); } while(0);

#define ACM_DATA_SEM_LOCK_(LabelSemErr) \
	do { ACMR_OS_SPIN_LOCK_BH(&gSpinLockSim); \
		if (0) goto LabelSemErr; } while(0);

#define ACM_DATA_SEM_UNLOCK_() \
	do { ACMR_OS_SPIN_UNLOCK_BH(&gSpinLockSim); } while(0);

#define ACM_MAX_NUM_OF_SIM_DATA_FLOW     5
static ACM_DATA_SIM gDATA_Sim[ACM_MAX_NUM_OF_SIM_DATA_FLOW];
#endif /* IEEE80211E_SIMULATION */

/* ----- Private Function ----- */
#define ACM_CMD_INPUT_PARAM_DECLARATION	\
	ACMR_PWLAN_STRUC pAd, INT32 Argc, CHAR *pArgv

VOID AcmCmdTclasReset(ACM_CMD_INPUT_PARAM_DECLARATION); //snowpin
VOID AcmCmdTclasCreate(ACM_CMD_INPUT_PARAM_DECLARATION); //snowpin

#ifdef CONFIG_STA_SUPPORT
VOID AcmCmdStreamTSRequest(ACM_CMD_INPUT_PARAM_DECLARATION, UINT16 DialogToken);
VOID AcmCmdStreamTSRequestAdvance(ACM_CMD_INPUT_PARAM_DECLARATION);
#endif // CONFIG_STA_SUPPORT //

static VOID AcmCmdBandwidthDisplay(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdStreamDisplay(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdStreamFailDisplay(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdEDCAParamDisplay(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdDATLEnable(ACM_CMD_INPUT_PARAM_DECLARATION);


VOID AcmCmdDeltsSend(ACM_CMD_INPUT_PARAM_DECLARATION); //snowpin
static VOID AcmCmdStreamFailClear(ACM_CMD_INPUT_PARAM_DECLARATION);

#ifdef CONFIG_STA_SUPPORT
static VOID AcmCmdStreamTSNegotiate(ACM_CMD_INPUT_PARAM_DECLARATION);
#endif // CONFIG_STA_SUPPORT //

static VOID AcmCmdUapsdDisplay(ACM_CMD_INPUT_PARAM_DECLARATION);


static VOID AcmCmdStatistics(ACM_CMD_INPUT_PARAM_DECLARATION);

#ifdef CONFIG_STA_SUPPORT
static VOID AcmCmdReAssociate(ACM_CMD_INPUT_PARAM_DECLARATION);
#endif // CONFIG_STA_SUPPORT //


/* ----- Simulation Function ----- */
#ifdef IEEE80211E_SIMULATION
static VOID AcmCmdSimAssocBuild(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimReqRcv(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDel(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDataRv(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDataTx(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDataStop(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDataSuspend(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimDataResume(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimReAssocBuild(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimNonQoSAssocBuild(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimNonQoSDataRv(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimRateSet(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimTcpTxEnable(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimStaMacSet(ACM_CMD_INPUT_PARAM_DECLARATION);
#ifdef CONFIG_STA_SUPPORT
static VOID AcmCmdSimStaAssoc(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimWmeReqTx(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimWmeNeqTx(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimWmeReqFail(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimWmeNegFail(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimWmeAcmReset(ACM_CMD_INPUT_PARAM_DECLARATION);
#endif // CONFIG_STA_SUPPORT //
static VOID AcmCmdSimWmePSEnter(ACM_CMD_INPUT_PARAM_DECLARATION);
static VOID AcmCmdSimReqPsPollRcv(ACM_CMD_INPUT_PARAM_DECLARATION);
#endif // IEEE80211E_SIMULATION //

static VOID AcmCmdTestFlagCtrl(ACM_CMD_INPUT_PARAM_DECLARATION);


/* ----- Utility Function ----- */
static UINT32 AcmCmdUtilHexGet(CHAR **ppArgv);
static UINT32 AcmCmdUtilNumGet(CHAR **ppArgv);
static VOID AcmCmdUtilMacGet(CHAR **ppArgv, UCHAR *pDevMac);

static VOID AcmCmdStreamDisplayOne(ACMR_PWLAN_STRUC pAd,
									ACM_STREAM_INFO *pStream);

UCHAR AcmCmdInfoParse(ACMR_PWLAN_STRUC pAd,
					CHAR **ppArgv,
					ACM_TSPEC *pTspec,
					ACM_TS_INFO *pInfo,
					UCHAR *pStreamType);

UCHAR AcmCmdInfoParseAdvance(ACMR_PWLAN_STRUC pAd,
					CHAR **ppArgv,
					ACM_TSPEC *pTspec,
					ACM_TS_INFO *pInfo,
					UCHAR *pStreamType);

#ifdef IEEE80211E_SIMULATION
#ifdef CONFIG_STA_SUPPORT
static VOID AcmApMgtMacHeaderInit(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACMR_WLAN_HEADER	*pHdr,
	ACM_PARAM_IN	UCHAR				SubType,
	ACM_PARAM_IN	UCHAR				BitToDs,
	ACM_PARAM_IN	UCHAR				*pMacDa,
	ACM_PARAM_IN	UCHAR				*pBssid);
#endif // CONFIG_STA_SUPPORT //


static VOID ACM_CMD_Task_Data_Simulation(ULONG Data);
static VOID ACM_CMD_Sim_Data_Rv(ACMR_PWLAN_STRUC pAd, ACM_DATA_SIM *pInfo);
static VOID ACM_CMD_Sim_nonQoS_Data_Rv(ACMR_PWLAN_STRUC pAd, ACM_DATA_SIM *pInfo);
static VOID ACM_CMD_Sim_Data_Tx(ACMR_PWLAN_STRUC pAd, ACM_DATA_SIM *pInfo);
#endif // IEEE80211E_SIMULATION //




/* =========================== Public Function =========================== */

/*
========================================================================
Routine Description:
	Init command related global parameters.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
VOID ACM_CMD_Init(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd)
{
#ifdef IEEE80211E_SIMULATION
	ACMR_LOCK_INIT(&gSpinLockSim);
	ACMR_TIMER_INIT(pAd, gTimerSim, ACMP_CMD_Timer_Data_Simulation, pAd);
#endif // IEEE80211E_SIMULATION //
} /* End of ACM_CMD_Init */


/*
========================================================================
Routine Description:
	Release command related global parameters.

Arguments:
	pAd				- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
VOID ACM_CMD_Release(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd)
{
#ifdef IEEE80211E_SIMULATION
	AcmCmdSimDataStop(pAd, 0, NULL);
#endif // IEEE80211E_SIMULATION //

	ACMR_LOCK_FREE(&gSpinLockSim);
} /* End of ACM_CMD_Release */




/* =========================== Private Function =========================== */

/*
========================================================================
Routine Description:
	Reset all TCLAS settings.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QSTA.
========================================================================
*/
VOID AcmCmdTclasReset( //snowpin
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	gTLS_Grp_ID = 0;
} /* End of AcmCmdTclasReset */


/*
========================================================================
Routine Description:
	Create a TCLAS for the future stream.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. For QSTA.
	2. Max 5 TCLAS for a stream.
	3. Users need to create TCLAS first.  Then create a stream later.
		If users want to create another stream, users
		shall reset all TCLAS and re-create TCLAS for another stream use.
	4. Command Format:
		wstclasadd [up:0~7] [type:0~2] [mask:hex]

========================================================================
*/
VOID AcmCmdTclasCreate( //snowpin
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_TCLAS *pTclas;
	UINT32 TclasLen[ACM_TSPEC_TCLAS_MAX_NUM+1] = { ACM_TCLAS_TYPE_ETHERNET_LEN,
												ACM_TCLAS_TYPE_IP_V4_LEN,
												ACM_TCLAS_TYPE_8021DQ_LEN };
	UCHAR *pClassifier;
	UINT32 IdByteNum;


	if (gTLS_Grp_ID >= ACM_TSPEC_TCLAS_MAX_NUM)
	{
		printk("\nErr> max TCLAS number is reached! "
				"Pls. reset the number first!\n");
		return;
	} /* End of if */

	pTclas = &gCMD_TCLAS_Group[gTLS_Grp_ID++];
	pTclas->UserPriority = AcmCmdUtilNumGet(&pArgv);
	pTclas->ClassifierType = AcmCmdUtilNumGet(&pArgv);
	pTclas->ClassifierMask = AcmCmdUtilNumGet(&pArgv);

	pClassifier = (UCHAR *)&pTclas->Clasifier;
	for(IdByteNum=0; IdByteNum<(TclasLen[pTclas->ClassifierType]-3); IdByteNum++)
		*pClassifier ++ = AcmCmdUtilHexGet(&pArgv);
	/* End of for */
} /* End of AcmCmdTclasCreate */


#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
	Request a traffic stream with current TCLAS settings.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. Command Format:
		[1-WME] [TID:0~7] [dir:0~3]
		[access:1~3] [UP:0~7] [APSD:0~1] [nom size:byte]
		[inact:sec] [mean data rate:bps] [min phy rate:bps]
		[surp factor:>=1] [tclas processing:0~1] (ack policy:0~3)
	2. dir: 0 - uplink, 1 - dnlink, 3 - bidirectional link
		APSD: 0 - legacy PS, 1 - APSD
========================================================================
*/
VOID AcmCmdStreamTSRequest(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv,
	ACM_PARAM_IN	UINT16              DialogToken)
{
	ACMR_STA_DB *pCdb;
	ACM_FUNC_STATUS Status;
	ACM_TSPEC Tspec, *pTspec;
	ACM_TS_INFO *pInfo;
	UCHAR StreamType, TclasProcessing;
	UCHAR MAC[6];
	ULONG SplFlags;


	/* init */
	pCdb = NULL;
	pTspec = &Tspec;
	pInfo = &Tspec.TsInfo;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* use AP MAC address automatically */
	ACMR_MEM_COPY(MAC, ACMR_AP_ADDR_GET(pAd), 6);

	/* get sta entry */
	pCdb = ACMR_STA_ENTRY_GET(pAd, MAC);
	if (pCdb == NULL)
		return;
	/* End of if */

	/* parse input command */
	if (AcmCmdInfoParse(
							pAd,
							&pArgv,
							pTspec,
							pInfo,
							&StreamType) != 0)
	{
		return;
	} /* End of if */

	/* request the stream */
	TclasProcessing = AcmCmdUtilNumGet(&pArgv);
	pInfo->AckPolicy = AcmCmdUtilNumGet(&pArgv); /* default 0 */

	/* get management semaphore */
	ACM_TSPEC_SEM_LOCK_CHK(pAd, SplFlags, LabelSemErr);

	/* try to find if the stream have already existed in our list */
	Status = ACM_TC_RenegotiationCheck(pAd, MAC, pInfo->UP, pInfo,
										NULL, NULL, NULL);

	/* release semaphore */
	ACM_TSPEC_SEM_UNLOCK(pAd, LabelSemErr);

	if (Status == ACM_RTN_FAIL)
	{
		/* this is a new request */
		ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("\n11e_msg> Send a new TS request!\n"));

		if (ACMP_WME_TC_Request(pAd, pCdb, pTspec,
							gTLS_Grp_ID, gCMD_TCLAS_Group,
							TclasProcessing, StreamType,
							DialogToken) != ACM_RTN_OK)
		{
			printk("err> request the stream fail in AcmCmdStreamTSRequest()!\n");
		} /* End of if */
	}
	else if (Status == ACM_RTN_OK)
	{
		/* this is a negotiate request */
		ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("\n11e_msg> Send a TS negotiated request!\n"));

		if (ACMP_TC_Renegotiate(pAd, pCdb, pTspec,
								gTLS_Grp_ID, gCMD_TCLAS_Group,
								TclasProcessing, StreamType) != ACM_RTN_OK)
		{
			printk("err> negotiate the stream fail in AcmCmdStreamTSRequest()!\n");
		} /* End of if */
	} /* End of if */

	return;

LabelSemErr:
	/* management semaphore get fail */
	ACMR_DEBUG(ACMR_DEBUG_ERR,
				("11e_err> Semaphore Lock! AcmCmdStreamTSRequest()\n"));
	return;
} /* End of AcmCmdStreamTSRequest */
#endif // CONFIG_STA_SUPPORT //


/*
========================================================================
Routine Description:
	Display current bandwidth status.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
========================================================================
*/
static VOID AcmCmdBandwidthDisplay(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_BANDWIDTH_INFO BwInfo, *pInfo;
	UINT32 TimePerc;


	/* init */
	pInfo = &BwInfo;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* display */
	if (ACMP_BandwidthInfoGet(pAd, pInfo) != ACM_RTN_OK)
		return;
	/* End of if */


#ifdef CONFIG_STA_SUPPORT
	printk("\n\t(BSS) Current available bandwidth = %d %%\n",
			(((pInfo->AvalAdmCap<<5)*100)/ACM_TIME_BASE));
#endif // CONFIG_STA_SUPPORT //

#ifdef ACM_CC_FUNC_MBSS
	printk("\tCurrent ACM time for other BSS = %d us\n", pInfo->MbssTotalUsedTime);
#endif // ACM_CC_FUNC_MBSS //

	printk("\t(BSS) Station Count of the BSS: %d\n", pInfo->StationCount);
	printk("\t(BSS) Channel Utilization of the BSS: %d %%\n", (pInfo->ChanUtil*100/255));
	printk("\t(BSS) Available Adimission Capability of the BSS: %d us\n", (pInfo->AvalAdmCap<<5));
	printk("\t(BSS) Channel busy time: %dus\n\n", pInfo->ChanBusyTime);

	TimePerc = pInfo->AcUsedTime * 100;
	TimePerc /= ACM_TIME_BASE;


#ifdef CONFIG_STA_SUPPORT
	printk("\t(STA) Current ACM time for EDCA = %d us\n", pInfo->AcUsedTime);

	/*
		Only time of all uplinks.
		In station, it can not know medium time of all dnlinks.
	*/
	printk("\t(STA) Current ACM bandwidth for EDCA = %d %%\n", TimePerc);

	printk("\t(STA) Current number of requested TSPECs (not yet response) = %d\n",
			pInfo->NumReqLink);

	if ((pInfo->NumAcLinkUp != 0) || (pInfo->NumAcLinkDn != 0) ||
		(pInfo->NumAcLinkDi != 0) || (pInfo->NumAcLinkBi != 0))
	{
		printk("\n\t(STA) EDCA uplinks = %02d", pInfo->NumAcLinkUp);
		printk("\t(STA)EDCA dnlinks = %02d\n", pInfo->NumAcLinkDn);
		printk("\t(STA)EDCA bilinks = %02d\n", pInfo->NumAcLinkBi);
	} /* End of if */
#endif // CONFIG_STA_SUPPORT //
} /* End of AcmCmdBandwidthDisplay */


/*
========================================================================
Routine Description:
	Display current stream status.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. For QAP & QSTA.
	2. Command Format:
		wsshow [1:EDCA, 2:HCCA, 3:ALL] (Client MAC)
	3. If Client MAC doesnt exist, only requested TSPEC & dnlink
		(bidirectional link) are displayed for QAP; only requested
		TSPEC & uplink (bidirectional link) are displayed for QSTA.
		If you want to display uplinks, you should assign client MAC address.
========================================================================
*/
static VOID AcmCmdStreamDisplay(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_STREAM_INFO *pStream, *pStreamNext;
	UINT32 Type;
	UCHAR MacPeer[6];
	UINT32 NumStream, SizeBuf;
	UINT32 Category[2] = { ACM_SM_CATEGORY_REQ, ACM_SM_CATEGORY_ACT };
	UINT32 NumCategory;
	UINT32 IdCateNum, IdStmNum;


	/* init */
	NumCategory = 2;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* init */
	ACMR_MEM_ZERO(MacPeer, sizeof(MacPeer));

	Type = AcmCmdUtilNumGet(&pArgv);
	if (Type == 0)
		Type = 1; /* default: EDCA streams */
	/* End of if */

	if (Argc >= 2)
	{
		/* get Client MAC */
		AcmCmdUtilMacGet(&pArgv, MacPeer);

#ifdef IEEE80211E_SIMULATION
		if (*(UINT32 *)MacPeer == 0x00)
		{
			if (MacPeer[5] == 0x00)
				ACMR_MEM_COPY(MacPeer, gMAC_STA, 6);
			else
				ACMR_MEM_COPY(MacPeer, gMAC_STA, 5);
			/* End of if */
		} /* End of if */
#endif // IEEE80211E_SIMULATION //

		NumCategory = 1; /* input & output TS streams */
		Category[0] = ACM_SM_CATEGORY_PEER;
	} /* End of if */

	for(IdCateNum=0; IdCateNum<NumCategory; IdCateNum++)
	{
		NumStream = ACMP_StreamNumGet(pAd, Category[IdCateNum], Type, MacPeer);

		if (NumStream == 0)
		{
			if (Category[IdCateNum] == ACM_SM_CATEGORY_REQ)
				printk("\n    No any requested TSPEC exists!\n");
			else if (Category[IdCateNum] == ACM_SM_CATEGORY_PEER)
				printk("\n    No any TSPEC exists!\n");
			else
				printk("\n    No any output TSPEC exists!\n");
			/* End of if */
			continue;
		} /* End of if */

		SizeBuf = sizeof(ACM_STREAM_INFO) * NumStream;
		pStream = (ACM_STREAM_INFO *)ACMR_MEM_ALLOC(SizeBuf);

		if (pStream == NULL)
		{
			printk("11e_err> Allocate stream memory fail! "
					"AcmCmdStreamDisplay()\n");
			return;
		} /* End of if */

		if (ACMP_StreamsGet(pAd, Category[IdCateNum], Type,
							&NumStream, MacPeer, pStream) != ACM_RTN_OK)
		{
			printk("11e_err> Get stream information fail! "
					"AcmCmdStreamDisplay()\n");
			ACMR_MEM_FREE(pStream);
			return;
		} /* End of if */

		if (Category[IdCateNum] == ACM_SM_CATEGORY_REQ)
		{
			printk("\n\n    ------------------- All Requested List "
					"-------------------");
		}
		else
		{
			if (Category[IdCateNum] == ACM_SM_CATEGORY_ACT)
			{
				printk("\n\n    ------------------- All OUT stream List "
						"-------------------");
			}
			else
			{
				printk("\n\n    ------------------- The Device stream List "
						"-------------------");
			} /* End of if */
		} /* End of if */

		for(IdStmNum=0, pStreamNext=pStream; IdStmNum<NumStream; IdStmNum++)
		{
			/* display the stream information */
			AcmCmdStreamDisplayOne(pAd, pStreamNext);
			pStreamNext ++;
		} /* End of for */

		ACMR_MEM_FREE(pStream);
	} /* End of while */
} /* End of AcmCmdStreamDisplay */


/*
========================================================================
Routine Description:
	Display fail stream status.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
========================================================================
*/
static VOID AcmCmdStreamFailDisplay(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_STREAM_INFO *pStream, *pStreamNext;
	UINT32 Category, Type;
	UINT32 NumStream, SizeBuf;
	UINT32 IdStmNum;


	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* init */
	Category = ACM_SM_CATEGORY_ERR;
	Type = ACM_ACCESS_POLICY_MIX;

	/* get fail streams */
	NumStream = ACMP_StreamNumGet(pAd, Category, Type, NULL);

	if (NumStream == 0)
	{
		printk("    No any fail TSPEC exists!\n");
		return;
	} /* End of if */

	SizeBuf = sizeof(ACM_STREAM_INFO) * NumStream;
	pStream = (ACM_STREAM_INFO *)ACMR_MEM_ALLOC(SizeBuf);

	if (pStream == NULL)
	{
		printk("11e_err> Allocate stream memory fail! "
				"AcmCmdStreamFailDisplay()\n");
		return;
	} /* End of if */

	if (ACMP_StreamsGet(pAd, Category, Type, &NumStream, NULL, pStream) != \
																ACM_RTN_OK)
	{
		printk("11e_err> Get stream information fail! "
				"AcmCmdStreamFailDisplay()\n");
		ACMR_MEM_FREE(pStream);
		return;
	} /* End of if */

	for(IdStmNum=0, pStreamNext=pStream; IdStmNum<NumStream; IdStmNum++)
	{
		/* display the stream information */
		AcmCmdStreamDisplayOne(pAd, pStreamNext);
		pStreamNext ++;
	} /* End of for */

	ACMR_MEM_FREE(pStream);
} /* End of AcmCmdStreamFailDisplay */


/*
========================================================================
Routine Description:
	Display current EDCA parameters.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
========================================================================
*/
static VOID AcmCmdEDCAParamDisplay(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_CTRL_INFO CtrlInfo, *pInfo = &CtrlInfo;
	UINT32 TimePerc, TimeAcm, TimeAcmMax;
	UINT32 IdAcNum, IdAcOther;


	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* get EDCA information */
	ACMP_ControlInfomationGet(pAd, pInfo);

	/* display information */
	printk("\n    Downgrade information:\n");

	for(IdAcNum=0; IdAcNum<ACM_DEV_NUM_OF_AC; IdAcNum++)
	{
		if (pInfo->FlgIsAcmEnable[IdAcNum])
		{
			if (pInfo->DowngradeAcNum[IdAcNum] < ACM_DEV_NUM_OF_AC)
			{
				printk("    AC%d ACM is enabled and Downgrade AC = %d\n",
						IdAcNum, pInfo->DowngradeAcNum[IdAcNum]);
			}
			else
				printk("    AC%d ACM is enabled and Downgrade AC = NONE\n", IdAcNum);
			/* End of if */
		}
		else
			printk("    AC%d ACM is disabled!\n", IdAcNum);
		/* End of if */
	} /* End of for */

	printk("\n    Channel Utilization Quota information:\n");
	printk("    Minimum Contention Period  = %d/%d service interval\n",
			pInfo->CP_MinNu, pInfo->CP_MinDe);
	printk("    Minimum Best Effort Period = %d/%d service interval\n",
			pInfo->BEK_MinNu, pInfo->BEK_MinDe);

	printk("\n    EDCA AC ACM information:\n");
	printk("    BW/AC\tAC0\t\tAC1\t\tAC2\t\tAC3");


	TimeAcmMax = ACM_TIME_BASE;

	printk("\n    USE BW\t");
	for(IdAcNum=0; IdAcNum<ACM_DEV_NUM_OF_AC; IdAcNum++)
	{
		TimeAcm = pInfo->AcmAcTime[IdAcNum];
		TimePerc = 0;

		for(IdAcOther=0; IdAcOther<ACM_DEV_NUM_OF_AC; IdAcOther++)
		{
			if (IdAcOther == IdAcNum)
				continue;
			/* End of if */

			TimePerc += pInfo->DatlBorAcBw[IdAcNum][IdAcOther];
		} /* End of for */

		TimePerc += TimeAcm;
		TimePerc *= 100;
		TimePerc /= TimeAcmMax;

			printk("%02d%%\t\t", TimePerc);
		/* End of if */
	} /* End of for */


	printk("\n\n");
} /* End of AcmCmdEDCAParamDisplay */


/*
========================================================================
Routine Description:
	Enable or disable Dynamic ATL.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. For QAP.
	2. Command Format: 25
		[enable/disable 1/0] (minimum bw threshold for AC0~AC3)
		(maximum bw threshold for AC0~AC3)
========================================================================
*/
static VOID AcmCmdDATLEnable(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	UCHAR FlgIsEnable;
	UCHAR DatlBwMin[ACM_DEV_NUM_OF_AC];
	UCHAR DatlBwMax[ACM_DEV_NUM_OF_AC];
	UINT32 IdAcNum, SumBw;


	FlgIsEnable = AcmCmdUtilNumGet(&pArgv);

	if (Argc >= 2)
	{
		/* input parameters include minimum & maximum bandwidth threshold */
		for(IdAcNum=0; IdAcNum<ACM_DEV_NUM_OF_AC; IdAcNum++)
			DatlBwMin[IdAcNum] = AcmCmdUtilNumGet(&pArgv);
		/* End of for */

		for(IdAcNum=0, SumBw=0; IdAcNum<ACM_DEV_NUM_OF_AC; IdAcNum++)
		{
			DatlBwMax[IdAcNum] = AcmCmdUtilNumGet(&pArgv);
			SumBw += DatlBwMax[IdAcNum];
		} /* End of for */

		if (SumBw != ACM_DATL_BW_MAX_SUM)
			return;
		/* End of if */

		for(IdAcNum=0; IdAcNum<ACM_DEV_NUM_OF_AC; IdAcNum++)
		{
			if (DatlBwMin[IdAcNum] > DatlBwMax[IdAcNum])
				return; /* min should be <= max */
			/* End of if */
		} /* End of for */

		ACMP_DatlCtrl(pAd, FlgIsEnable, DatlBwMin, DatlBwMax);
	}
	else
		ACMP_DatlCtrl(pAd, FlgIsEnable, NULL, NULL);
	/* End of if */
} /* End of AcmCmdDATLEnable */




/*
========================================================================
Routine Description:
	Send a delts to a peer.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
	[Peer MAC] [TID:0~7]

	We deleted a TS based on the TSID, not Direction or AC ID.
========================================================================
*/
VOID AcmCmdDeltsSend(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACMR_STA_DB	*pCdb;
	ACM_STREAM *pStream;
	ACM_TS_INFO TsInfo;
	UINT32 TID;
	UCHAR MacPeer[6];
	ULONG SplFlags;


	/* init */
	pCdb = NULL;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* get peer mac address */
	AcmCmdUtilMacGet(&pArgv, MacPeer);

#ifdef CONFIG_STA_SUPPORT
	if (*(UINT32 *)MacPeer == 0)
		ACMR_MEM_COPY(MacPeer, ACMR_AP_ADDR_GET(pAd), 6);
	/* End of if */
#endif // CONFIG_STA_SUPPORT //

#ifdef IEEE80211E_SIMULATION
#endif // IEEE80211E_SIMULATION //

	/* get input arguments */
	ACM_NIN_DEC_MGET(TID,  0, 7, ("err> TID fail!\n"));

	/* get sta entry */
	pCdb = ACMR_STA_ENTRY_GET(pAd, MacPeer);
	if (pCdb == NULL)
	{
		printk("11e_err> the peer does NOT exist "
				"0x%02x:%02x:%02x:%02x:%02x:%02x:!\n",
				MacPeer[0], MacPeer[1], MacPeer[2],
				MacPeer[3], MacPeer[4], MacPeer[5]);
		return;
	} /* End of if */


	/* get management semaphore */
	ACM_TSPEC_IRQ_LOCK_CHK(pAd, SplFlags, LabelSemErr);

	/* find the request */
	TsInfo.TSID = TID;

	pStream = ACM_TC_Find(pAd, MacPeer, &TsInfo);
	if (pStream == NULL)
	{
		ACM_TSPEC_IRQ_UNLOCK(pAd, SplFlags, LabelSemErr);
		printk("acm_msg> can not find the stream (TID=%d)!\n", TID);
		return;
	} /* End of if */

	/* delete the stream */

#ifdef CONFIG_STA_SUPPORT
	pStream->Cause = TSPEC_CAUSE_DELETED_BY_QSTA;
#endif // CONFIG_STA_SUPPORT //

	if (ACM_TC_Delete(pAd, pStream) == TRUE)
	{
		ACM_DELTS_SEND(pAd, pStream->pCdb, pStream, LabelSemErr);
	} /* End of if */

	/* release semaphore */
	ACM_TSPEC_IRQ_UNLOCK(pAd, SplFlags, LabelSemErr);

LabelErr:
	return;

LabelSemErr:
	/* management semaphore get fail */
	ACMR_DEBUG(ACMR_DEBUG_ERR,
				("11e_err> Semaphore Lock! AcmCmdDeltsSend()\n"));
	return;
} /* End of AcmCmdDeltsSend */


/*
========================================================================
Routine Description:
	Clear fail stream status.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
========================================================================
*/
static VOID AcmCmdStreamFailClear(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* clear */
	ACMP_StreamFailClear(pAd);
} /* End of AcmCmdStreamFailClear */


#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
	Negotiate a traffic stream with current TCLAS settings.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. Command Format:
		[1-WME] [TID:0~7] [dir:0~3]
		[access:1~3] [UP:0~7] [APSD:0~1] [nom size:byte]
		[inact:sec] [mean data rate:bps] [min phy rate:bps]
		[surp factor:>=1] [tclas processing:0~1]
	2. dir: 0 - uplink, 1 - dnlink, 2 - bidirectional link, 3 - direct link
		access: 1 - EDCA, 2 - HCCA, 3 - EDCA + HCCA
		APSD: 0 - legacy PS, 1 - APSD
========================================================================
*/
static VOID AcmCmdStreamTSNegotiate(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACMR_STA_DB *pCdb;
	ACM_TSPEC Tspec, *pTspec;
	ACM_TS_INFO *pInfo;
	UCHAR StreamType, TclasProcessing;
	UCHAR MAC[6];


	/* init */
	pCdb = NULL;
	pTspec = &Tspec;
	pInfo = &Tspec.TsInfo;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* use AP MAC address automatically */
	ACMR_MEM_COPY(MAC, ACMR_AP_ADDR_GET(pAd), 6);

	/* get sta entry */
	pCdb = ACMR_STA_ENTRY_GET(pAd, MAC);
	if (pCdb == NULL)
		return;
	/* End of if */

	/* parse input command */
	if (AcmCmdInfoParse(
							pAd,
							&pArgv,
							pTspec,
							pInfo,
							&StreamType) != 0)
	{
		return;
	} /* End of if */

	/* request the stream */
	TclasProcessing = AcmCmdUtilNumGet(&pArgv);

	if (ACMP_TC_Renegotiate(pAd, pCdb, pTspec,
							gTLS_Grp_ID, gCMD_TCLAS_Group,
							TclasProcessing, StreamType) != ACM_RTN_OK)
	{
		printk("err> negotiate the stream fail in AcmCmdStreamTSNegotiate()!\n");
	} /* End of if */
} /* End of AcmCmdStreamTSNegotiate */
#endif // CONFIG_STA_SUPPORT //


/*
========================================================================
Routine Description:
	Display UAPSD information for a device.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	For QAP & QSTA.
	[Peer MAC]
========================================================================
*/
static VOID AcmCmdUapsdDisplay(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	UCHAR MacPeer[6];


	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* init */
	ACMR_MEM_ZERO(MacPeer, sizeof(MacPeer));

	if (Argc >= 1)
	{
		/* get Client MAC */
		AcmCmdUtilMacGet(&pArgv, MacPeer);

#ifdef IEEE80211E_SIMULATION
		if (*(UINT32 *)MacPeer == 0x00)
		{
			if (MacPeer[5] == 0x00)
				ACMR_MEM_COPY(MacPeer, gMAC_STA, 6);
			else
				ACMR_MEM_COPY(MacPeer, gMAC_STA, 5);
			/* End of if */
		} /* End of if */
#endif // IEEE80211E_SIMULATION //
	} /* End of if */


#ifdef CONFIG_STA_SUPPORT
	if (ACMR_IS_IN_ACTIVE_MODE(pAd, pCdb))
		printk("\n    EDCA AC UAPSD information: (ACTIVE)\n");
	else
		printk("\n    EDCA AC UAPSD information: (POWER SAVE)\n");
	/* End of if */

	if (pAd->CommonCfg.MaxSPLength != 0)
	{
		printk("    Max SP Length: %d (%d frames)\n",
				pAd->CommonCfg.MaxSPLength, pAd->CommonCfg.MaxSPLength<<1);
	}
	else
		printk("    Max SP Length: 0 (all frames)\n");
	/* End of if */

	printk("    UAPSD/AC   AC0    AC1    AC2    AC3");

	printk("\n    Tr/De      %d/%d    %d/%d    %d/%d    %d/%d",
			pAd->CommonCfg.bACMAPSDTr[0],
			pAd->CommonCfg.bAPSDAC_BE,
			pAd->CommonCfg.bACMAPSDTr[1],
			pAd->CommonCfg.bAPSDAC_BK,
			pAd->CommonCfg.bACMAPSDTr[2],
			pAd->CommonCfg.bAPSDAC_VI,
			pAd->CommonCfg.bACMAPSDTr[3],
			pAd->CommonCfg.bAPSDAC_VO);
#endif // CONFIG_STA_SUPPORT //
	printk("\n");
} /* End of AcmCmdUapsdDisplay */




#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
	Request a advanced traffic stream with current TCLAS settings.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	1. Command Format:
		[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
		[Ack Policy: 0~3] [APSD:0~1] [max size:byte] [nom size:byte]
		[burst size:byte] [inact:sec]
		[peak data rate:bps] [mean data rate:bps] [min data rate:bps]
		[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
	2. dir: 0 - uplink, 1 - dnlink, 3 - bidirectional link
		APSD: 0 - legacy PS, 1 - APSD
========================================================================
*/
VOID AcmCmdStreamTSRequestAdvance(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACMR_STA_DB *pCdb;
	ACM_FUNC_STATUS Status;
	ACM_TSPEC Tspec, *pTspec;
	ACM_TS_INFO *pInfo;
	UCHAR StreamType, TclasProcessing;
	UCHAR MAC[6];
	ULONG SplFlags;


	/* init */
	pCdb = NULL;
	pTspec = &Tspec;
	pInfo = &Tspec.TsInfo;

	/* precondition */
	ACM_QOS_SANITY_CHECK(pAd);

	/* use AP MAC address automatically */
	ACMR_MEM_COPY(MAC, ACMR_AP_ADDR_GET(pAd), 6);

	/* get sta entry */
	pCdb = ACMR_STA_ENTRY_GET(pAd, MAC);
	if (pCdb == NULL)
		return;
	/* End of if */

	/* parse input command */
	if (AcmCmdInfoParseAdvance(
							pAd,
							&pArgv,
							pTspec,
							pInfo,
							&StreamType) != 0)
	{
		return;
	} /* End of if */

	/* request the stream */
	TclasProcessing = AcmCmdUtilNumGet(&pArgv);

	/* get management semaphore */
	ACM_TSPEC_SEM_LOCK_CHK(pAd, SplFlags, LabelSemErr);

	/* try to find if the stream have already existed in our list */
	Status = ACM_TC_RenegotiationCheck(pAd, MAC, pInfo->UP, pInfo,
										NULL, NULL, NULL);

	/* release semaphore */
	ACM_TSPEC_SEM_UNLOCK(pAd, LabelSemErr);

	if (Status == ACM_RTN_FAIL)
	{
		/* this is a new request */
		ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("\n11e_msg> Send a new TS request!\n"));

		if (ACMP_WME_TC_Request(pAd, pCdb, pTspec,
							gTLS_Grp_ID, gCMD_TCLAS_Group,
							TclasProcessing, StreamType,
							0) != ACM_RTN_OK)
		{
			printk("err> request the stream fail in AcmCmdStreamTSRequest()!\n");
		} /* End of if */
	}
	else if (Status == ACM_RTN_OK)
	{
		/* this is a negotiate request */
		ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("\n11e_msg> Send a TS negotiated request!\n"));

		if (ACMP_TC_Renegotiate(pAd, pCdb, pTspec,
								gTLS_Grp_ID, gCMD_TCLAS_Group,
								TclasProcessing, StreamType) != ACM_RTN_OK)
		{
			printk("err> negotiate the stream fail in AcmCmdStreamTSRequest()!\n");
		} /* End of if */
	} /* End of if */

	return;

LabelSemErr:
	/* management semaphore get fail */
	ACMR_DEBUG(ACMR_DEBUG_ERR,
				("11e_err> Semaphore Lock! AcmCmdStreamTSRequest()\n"));
	return;
} /* End of AcmCmdStreamTSRequestAdvance */
#endif // CONFIG_STA_SUPPORT //


/*
========================================================================
Routine Description:
	Display ACM related statistics count.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
========================================================================
*/
VOID AcmCmdStatistics(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	ACM_STATISTICS Stats, *pStats;


	pStats = &Stats;
	ACMP_StatisticsGet(pAd, pStats);

	printk("ACM Related Statistics Count:\n\n");
	printk("*Drop by ACM:\t\t%d\n", pStats->DropByACM);
	printk("*Drop by Time:\t\t%d\n", pStats->DropByAdmittedTime);
	printk("*Priority Change VO:\t%d\n", pStats->PriorityChange[ACM_VO_ID]);
	printk("*Priority Change VI:\t%d\n", pStats->PriorityChange[ACM_VI_ID]);
	printk("*Priority Change BK:\t%d\n", pStats->PriorityChange[ACM_BK_ID]);
	printk("*Priority Change BE:\t%d\n", pStats->PriorityChange[ACM_BE_ID]);
	printk("*Downgrade VO:\t\t%d\n", pStats->Downgrade[ACM_VO_ID]);
	printk("*Downgrade VI:\t\t%d\n", pStats->Downgrade[ACM_VI_ID]);
	printk("*Downgrade BK:\t\t%d\n", pStats->Downgrade[ACM_BK_ID]);
	printk("*Downgrade BE:\t\t%d\n", pStats->Downgrade[ACM_BE_ID]);

#ifdef ACM_CC_FUNC_11N
{
	UINT32 IdBa;

	printk("*Predict AMPDU:\t\t");
	for(IdBa=0; IdBa<sizeof(pStats->AMPDU)/sizeof(pStats->AMPDU[0]); IdBa++)
	{
		if ((IdBa != 0) && ((IdBa & 0x07) == 0))
			printk("\n\t\t\t");
		/* End of if */

		printk("%d\t", pStats->AMPDU[IdBa]);
	} /* End of for */
}
#endif // ACM_CC_FUNC_11N //

	printk("\n");
} /* End of AcmCmdStatistics */


#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
	Send a re-associate frame to the associated AP.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	Used in WMM ACM AP Test Cases in WiFi WMM ACM Test Plan.
========================================================================
*/
VOID AcmCmdReAssociate(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	MLME_QUEUE_ELEM	*pMlmeQueue;
	MLME_ASSOC_REQ_STRUCT *pInfoAssocReq;
	NDIS_STATUS Status;
	PUCHAR pBufOut;
	UCHAR ApMac[6];


	/* init */
	pBufOut = NULL;

	/* get input arguments */
	memcpy(ApMac, gMAC_AP, 6);
	ApMac[5] = AcmCmdUtilHexGet(&pArgv);

	if (ApMac[5] == 0x00)
		memcpy(ApMac, ACMR_AP_ADDR_GET(pAd), 6);
	/* End of if */

	/* allocate probe re-assoc frame buffer */
	Status = MlmeAllocateMemory(pAd, &pBufOut);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		printk("11e_err> allocate auth buffer fail!\n");
		return;
	} /* End of if */

	/* allocate mlme msg queue, dont use local array, the structure size
		is too large */
	pMlmeQueue = ACMR_MEM_ALLOC(sizeof(MLME_QUEUE_ELEM));
	if (pMlmeQueue == NULL)
	{
		printk("11e_err> allocate mlme msg queue fail!\n");
		goto LabelErr;
	} /* End of if */

	/* init/tx association request frame body */
	pInfoAssocReq = (MLME_ASSOC_REQ_STRUCT *)pMlmeQueue->Msg;
	ACMR_MEM_MAC_COPY(pInfoAssocReq->Addr, ApMac);
	pInfoAssocReq->CapabilityInfo = 0x0001;
	pInfoAssocReq->Timeout = ASSOC_TIMEOUT;

	MlmeReassocReqAction(pAd, pMlmeQueue);

	ACMR_MEM_FREE(pMlmeQueue);

LabelErr:
	/* free the frame buffer */
	MlmeFreeMemory(pAd, pBufOut);
	return;
} /* End of AcmCmdReAssociate */
#endif // CONFIG_STA_SUPPORT //








/* =========================== Utility Function ========================== */
/*
========================================================================
Routine Description:
	Get argument number value.

Arguments:
	**ppArgv			- input parameters

Return Value:
	decimal number

Note:
========================================================================
*/
static UINT32 AcmCmdUtilHexGet(
	ACM_PARAM_IN	CHAR	**ppArgv)
{
	CHAR buf[3], *pNum;
	UINT32 ID;
	UCHAR Value;


	pNum = (*ppArgv);

	buf[0] = 0x30;
	buf[1] = 0x30;
	buf[2] = 0;

	for(ID=0; ID<sizeof(buf)-1; ID++)
	{
		if ((*pNum == '_') || (*pNum == 0x00))
			break;
		/* End of if */

		pNum ++;
	} /* End of for */

	if (ID == 0)
		return 0; /* argument length is too small */
	/* End of if */

	if (ID >= 2)
		memcpy(buf, (*ppArgv), 2);
	else
		buf[1] = (**ppArgv);
	/* End of if */

	(*ppArgv) += ID;
	if ((**ppArgv) == '_')
		(*ppArgv) ++; /* skip _ */
	/* End of if */

	ACMR_ARG_ATOH(buf, &Value);
	return (UINT32)Value;
} /* End of AcmCmdUtilHexGet */


/*
========================================================================
Routine Description:
	Get argument number value.

Arguments:
	*pArgv			- input parameters

Return Value:
	decimal number

Note:
========================================================================
*/
static UINT32 AcmCmdUtilNumGet(
	ACM_PARAM_IN	CHAR	**ppArgv)
{
	CHAR buf[20], *pNum;
	UINT32 ID;


	pNum = (*ppArgv);

	for(ID=0; ID<sizeof(buf)-1; ID++)
	{
		if ((*pNum == '_') || (*pNum == 0x00))
			break;
		/* End of if */

		pNum ++;
	} /* End of for */

	if (ID == sizeof(buf)-1)
		return 0; /* argument length is too large */
	/* End of if */

	memcpy(buf, (*ppArgv), ID);
	buf[ID] = 0x00;

	*ppArgv += ID+1; /* skip _ */

	return ACMR_ARG_ATOI(buf);
} /* End of AcmCmdUtilNumGet */


/*
========================================================================
Routine Description:
	Get argument MAC value.

Arguments:
	**ppArgv			- input parameters
	*pDevMac			- MAC address

Return Value:
	None

Note:
========================================================================
*/
static VOID AcmCmdUtilMacGet(
	ACM_PARAM_IN	CHAR	**ppArgv,
	ACM_PARAM_IN	UCHAR	*pDevMac)
{
	CHAR Buf[3];
	CHAR *pMAC = (CHAR *)(*ppArgv);
	UINT32 ID;


	if ((pMAC[0] == '0') && (pMAC[1] == '_'))
	{
		*ppArgv = (&pMAC[2]);
		return;
	} /* End of if */

	ACMR_MEM_ZERO(pDevMac, 6);

	/* must exist 18 octets */
	for(ID=0; ID<18; ID+=2)
	{
		if ((pMAC[ID] == '_') || (pMAC[ID] == 0x00))
		{
			*ppArgv = (&pMAC[ID]+1);
			return;
		} /* End of if */
	} /* End of for */

	/* get mac */
	for(ID=0; ID<18; ID+=3)
	{
		Buf[0] = pMAC[0];
		Buf[1] = pMAC[1];
		Buf[2] = 0x00;

		ACMR_ARG_ATOH(Buf, pDevMac);
		pMAC += 3;
		pDevMac ++;
	} /* End of for */

	*ppArgv += 17+1; /* skip _ */
} /* End of AcmCmdUtilMacGet */


/*
========================================================================
Routine Description:
	Display the stream status.

Arguments:
	pAd				- WLAN control block pointer
	*pStream		- the stream

Return Value:
	None

Note:
	For QAP & QSTA.
========================================================================
*/
static VOID AcmCmdStreamDisplayOne(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACM_STREAM_INFO		*pStream)
{
	ACM_TSPEC *pTspec;
	UINT16 SBA_Temp;


	pTspec = &pStream->Tspec;

	printk("\n=== QSTA MAC = %02x:%02x:%02x:%02x:%02x:%02x",
			pStream->DevMac[0],
			pStream->DevMac[1],
			pStream->DevMac[2],
			pStream->DevMac[3],
			pStream->DevMac[4],
			pStream->DevMac[5]);

	if (ACMR_CB->EdcaCtrlParam.FlgAcmStatus[pStream->AcmAcId])
		printk(" (NORMAL TSPEC)\n");
	else
		printk(" (NULL TSPEC)\n");
	/* End of if */

	if (pTspec->TsInfo.AccessPolicy == ACM_ACCESS_POLICY_EDCA)
	{
		if (pStream->StreamType == ACM_STREAM_TYPE_11E)
			printk("    Stream Type: EDCA");
		else
			printk("    Stream Type: WME");
		/* End of if */
	} /* End of if */

	switch(pStream->Status)
	{
		case TSPEC_STATUS_REQUEST:
			printk("\tStatus: Requesting...\n");
			break;

		case TSPEC_STATUS_ACTIVE:
			printk("\tStatus: Active\n");
			break;

		case TSPEC_STATUS_ACTIVE_SUSPENSION:
			printk("\tStatus: Active but suspended\n");
			break;

		case TSPEC_STATUS_REQ_DELETING:
			printk("\tStatus: Requesting & deleting...\n");
			break;

		case TSPEC_STATUS_ACT_DELETING:
			printk("\tStatus: Active & deleting...\n");
			break;

		case TSPEC_STATUS_RENEGOTIATING:
			printk("\tStatus: Renegotiation...\n");
			break;

		case TSPEC_STATUS_HANDLING:
			printk("\tStatus: Request Handling...\n");
			break;

		case TSPEC_STATUS_FAIL:
			switch(pStream->Cause)
			{
				case TSPEC_CAUSE_UNKNOWN:
					printk("\tStatus: (ERR) Internal Error!\n");
					break;

				case TSPEC_CAUSE_REQ_TIMEOUT:
					printk("\tStatus: (ERR) Request (ADDTS) timeout!\n");
					break;

				case TSPEC_CAUSE_SUGGESTED_TSPEC:
					printk("\tStatus: (ERR) Suggested TSPEC is provided!\n");
					break;

				case TSPEC_CAUSE_REJECTED:
					printk("\tStatus: (ERR) Rejected by QAP!\n");
					break;

				case TSPEC_CAUSE_UNKNOWN_STATUS:
					printk("\tStatus: (ERR) Unknown response status code!\n");
					break;

				case TSPEC_CAUSE_INACTIVITY_TIMEOUT:
					printk("\tStatus: (ERR) Inactivity timeout!\n");
					break;

				case TSPEC_CAUSE_DELETED_BY_QAP:
					printk("\tStatus: (ERR) Deleted by QAP!\n");
					break;

				case TSPEC_CAUSE_DELETED_BY_QSTA:
					printk("\tStatus: (ERR) Deleted by QSTA!\n");
					break;

				case TSPEC_CAUSE_BANDWIDTH:
					printk("\tStatus: (ERR) In order to increase bandwidth!\n");
					break;

				case TSPEC_CAUSE_REJ_MANY_TS:
					printk("\tStatus: (ERR) Reject due to too many TS in a AC!\n");
					break;

				case TSPEC_CASUE_REJ_INVALID_PARAM:
					printk("\tStatus: (ERR) Reject due to invalid parameters!\n");
					break;

				case TSPEC_CAUSE_REJ_INVALID_TOKEN:
					printk("\tStatus: (ERR) Reject due to invalid Dialog Token!\n");
					break;

				default:
					printk("\tStatus: Fatal error, unknown cause!\n");
					break;
			} /* End of switch */
			break;

		default:
			printk("\tStatus: Fatal error, unknown status!\n");
			break;
	} /* End of switch */

	printk("    TSID = %d", pTspec->TsInfo.TSID);

	printk("\tUP = %d", pTspec->TsInfo.UP);
	printk("\tAC ID = %d", pStream->AcmAcId);
	printk("\tUAPSD = %d", pTspec->TsInfo.APSD);

	switch(pTspec->TsInfo.Direction)
	{
		case ACM_DIRECTION_UP_LINK:
			printk("\tDirection = UP LINK\n");
			break;

		case ACM_DIRECTION_DOWN_LINK:
			printk("\tDirection = DOWN LINK\n");
			break;

		case ACM_DIRECTION_DIRECT_LINK:
			printk("\tDirection = DIRECT LINK\n");
			break;

		case ACM_DIRECTION_BIDIREC_LINK:
			printk("\tDirection = BIDIRECTIONAL LINK\n");
			break;
	} /* End of switch */

	if (ACMR_CB->EdcaCtrlParam.FlgAcmStatus[pStream->AcmAcId])
		printk("    Current Inactivity timeout = %u us\n", pStream->InactivityCur);
	else
		printk("    No Inactivity timeout!\n");
	/* End of if */

	if (pTspec->NominalMsduSize & ACM_NOM_MSDU_SIZE_CHECK_BIT)
	{
		printk("    Norminal MSDU Size (Fixed) = %d B\n",
				(pTspec->NominalMsduSize & (~ACM_NOM_MSDU_SIZE_CHECK_BIT)));
	}
	else
	{
		printk("    Norminal MSDU Size (Variable) = %d B\n",
			(pTspec->NominalMsduSize & (~ACM_NOM_MSDU_SIZE_CHECK_BIT)));
	} /* End of if */

	printk("    Inactivity Interval = %u us\n", pTspec->InactivityInt);


	printk("    Mean Data Rate = %d bps\n", pTspec->MeanDataRate);
	printk("    Min Physical Rate = %d bps (%d %d)\n",
			pTspec->MinPhyRate, pStream->PhyModeMin, pStream->McsMin);

	if (pTspec->TsInfo.AccessPolicy != ACM_ACCESS_POLICY_HCCA)
	{
		/* only for EDCA or HCCA + EDCA */
		SBA_Temp = pTspec->SurplusBandwidthAllowance;
		SBA_Temp = (UINT16)(SBA_Temp << ACM_SURPLUS_INT_BIT_NUM);
		SBA_Temp = (UINT16)(SBA_Temp >> ACM_SURPLUS_INT_BIT_NUM);
		SBA_Temp = ACM_SurplusFactorDecimalBin2Dec(SBA_Temp);

		printk("    Surplus factor = %d.%02d",
			(pTspec->SurplusBandwidthAllowance >> ACM_SURPLUS_DEC_BIT_NUM),
			SBA_Temp);

		printk("\t\tMedium Time = %d us\n", (pTspec->MediumTime << 5));
	} /* End of if */
} /* End of AcmCmdStreamDisplayOne */


/*
========================================================================
Routine Description:
	Parse WME input parameters.

Arguments:
	pAd				- WLAN control block pointer
	**ppArgv		- input parameters
	*pTspec			- the output TSPEC
	*pInfo			- the output TS Info
	*pStreamType	- the stream type
	*pTclasProcessing - the TCLAS Processing

Return Value:
	0				- parse successfully
	non 0			- parse fail

Note:
	For QAP & QSTA.

	[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
	[APSD:0~1] [nom size:byte] [inact:sec] [mean data rate:bps]
	[min phy rate:Mbps] [surp factor:>=1]

	where SBA is in 10 ~ 80, i.e. 1.0 ~ 8.0
========================================================================
*/
UCHAR AcmCmdInfoParse(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	CHAR				**ppArgv,
	ACM_PARAM_IN	ACM_TSPEC			*pTspec,
	ACM_PARAM_IN	ACM_TS_INFO			*pInfo,
	ACM_PARAM_IN	UCHAR				*pStreamType)
{
	UINT32 SBA_Integer, SBA_Decimal;
	CHAR *pArgv;


	/* init */
	pArgv = *ppArgv;

	/* get stream type: 11e or WME/WSM */
	ACM_NIN_DEC_GET((*pStreamType), 1, ("err> error type!\n"));

	/* init TS Info */
	pInfo->TrafficType = ACM_TRAFFIC_TYPE_APERIODIC;

	ACM_NIN_DEC_GET(pInfo->TSID, 7, ("err> error TID!\n"));
	ACM_NIN_DEC_GET(pInfo->Direction, 3, ("err> error dir!\n"));
	ACM_NIN_DEC_MGET(pInfo->AccessPolicy, 1, 3, ("err> error access!\n"));

	pInfo->Aggregation = ACM_AGGREGATION_DISABLE;

	ACM_NIN_DEC_GET(pInfo->UP, 7, ("err> error UP!\n"));
	pInfo->AckPolicy = ACM_ACK_POLICY_NORMAL;
	ACM_NIN_DEC_GET(pInfo->APSD, 1, ("err> error APSD!\n"));
	pInfo->Schedule = ACM_SCHEDULE_NO;

	ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("acm_msg> TSInfo : "
				"TID=%d, DIR=%d, ACCESS=%d, UP=%d, APSD=%d\n",
				pInfo->TSID,
				pInfo->Direction,
				pInfo->AccessPolicy,
				pInfo->UP,
				pInfo->APSD));

	/* init TSPEC */
	pTspec->NominalMsduSize = AcmCmdUtilNumGet(&pArgv);
 	pTspec->MaxMsduSize = 0;
	pTspec->MinServInt = 0;
	pTspec->MaxServInt = 0;
	ACM_NIN_DEC_GET(pTspec->InactivityInt, 3600, ("err> error inact!\n"));
	pTspec->InactivityInt *= 1000000; /* unit: microseconds */
	pTspec->SuspensionInt = 0; /* use 0 in EDCA mode, not 0xFFFFFFFF */
	pTspec->ServiceStartTime = 0;
	ACM_NIN_DEC_GET(pTspec->MeanDataRate, ACM_RATE_MAX, ("err> error mean rate!\n"));
	pTspec->MinDataRate = pTspec->MeanDataRate;
	pTspec->PeakDataRate = pTspec->MeanDataRate;
	pTspec->MaxBurstSize = 0;
	pTspec->DelayBound = 0;
	ACM_NIN_DEC_GET(pTspec->MinPhyRate, ACM_RATE_MAX, ("err> error phy rate!\n"));
	ACM_NIN_DEC_MGET(pTspec->SurplusBandwidthAllowance, 10, 80,
					("err> error surp factor!\n"));
	if (pTspec->SurplusBandwidthAllowance < 80)
	{
		SBA_Integer = pTspec->SurplusBandwidthAllowance / 10;
		SBA_Decimal = pTspec->SurplusBandwidthAllowance - SBA_Integer*10;
		SBA_Decimal = ACM_SurplusFactorDecimalDec2Bin(SBA_Decimal*10);
		pTspec->SurplusBandwidthAllowance = \
					(SBA_Integer << ACM_SURPLUS_DEC_BIT_NUM) | SBA_Decimal;
	}
	else
		pTspec->SurplusBandwidthAllowance = 0xffff;
	/* End of if */
	pTspec->MediumTime = 0;

	ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("acm_msg> TSPEC : "
				"NOM SIZE=%d, INACT=%d, MEAN(MIN/MAX) RATE=%d, MIN PHY=%d, "
				"SBA=0x%x\n",
				pTspec->NominalMsduSize,
				pTspec->InactivityInt,
				pTspec->MeanDataRate,
				pTspec->MinPhyRate,
				pTspec->SurplusBandwidthAllowance));

	*ppArgv = pArgv;
	return 0;

LabelErr:
	return 1;
} /* End of AcmCmdInfoParse */


/*
========================================================================
Routine Description:
	Parse WME input parameters.

Arguments:
	pAd				- WLAN control block pointer
	**ppArgv		- input parameters
	*pTspec			- the output TSPEC
	*pInfo			- the output TS Info
	*pStreamType	- the stream type
	*pTclasProcessing - the TCLAS Processing

Return Value:
	0				- parse successfully
	non 0			- parse fail

Note:
	For QAP & QSTA.

	[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
	[Ack Policy: 0~3] [APSD:0~1] [max size:byte] [nom size:byte]
	[burst size:byte] [inact:sec]
	[peak data rate:bps] [mean data rate:bps] [min data rate:bps]
	[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]

	where SBA is in 100 ~ 800, i.e. 1.00 ~ 8.00
========================================================================
*/
UCHAR AcmCmdInfoParseAdvance(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	CHAR				**ppArgv,
	ACM_PARAM_IN	ACM_TSPEC			*pTspec,
	ACM_PARAM_IN	ACM_TS_INFO			*pInfo,
	ACM_PARAM_IN	UCHAR				*pStreamType)
{
	UINT32 SBA_Integer, SBA_Decimal;
	CHAR *pArgv;


	/* init */
	pArgv = *ppArgv;

	/* get stream type: 11e or WME/WSM */
	ACM_NIN_DEC_GET((*pStreamType), 1, ("err> error type!\n"));

	/* init TS Info */
	pInfo->TrafficType = ACM_TRAFFIC_TYPE_APERIODIC;

	ACM_NIN_DEC_GET(pInfo->TSID, 7, ("err> error TID!\n"));
	ACM_NIN_DEC_GET(pInfo->Direction, 3, ("err> error dir!\n"));
	ACM_NIN_DEC_MGET(pInfo->AccessPolicy, 1, 3, ("err> error access!\n"));

	pInfo->Aggregation = ACM_AGGREGATION_DISABLE;

	ACM_NIN_DEC_GET(pInfo->UP, 7, ("err> error UP!\n"));
	ACM_NIN_DEC_GET(pInfo->AckPolicy, 3, ("err> error Ack Policy!\n"));
	ACM_NIN_DEC_GET(pInfo->APSD, 1, ("err> error APSD!\n"));
	pInfo->Schedule = ACM_SCHEDULE_NO;

	ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("acm_msg> TSInfo : "
				"TID=%d, DIR=%d, ACCESS=%d, UP=%d, APSD=%d\n",
				pInfo->TSID,
				pInfo->Direction,
				pInfo->AccessPolicy,
				pInfo->UP,
				pInfo->APSD));

	/* init TSPEC */
 	pTspec->MaxMsduSize = AcmCmdUtilNumGet(&pArgv);
	pTspec->NominalMsduSize = AcmCmdUtilNumGet(&pArgv);
	pTspec->MaxBurstSize = AcmCmdUtilNumGet(&pArgv);
	pTspec->MinServInt = 0;
	pTspec->MaxServInt = 0;
	ACM_NIN_DEC_GET(pTspec->InactivityInt, 3600, ("err> error inact!\n"));
	pTspec->InactivityInt *= 1000000; /* unit: microseconds */
	pTspec->SuspensionInt = 0; /* use 0 in EDCA mode, not 0xFFFFFFFF */
	pTspec->ServiceStartTime = 0;
	ACM_NIN_DEC_GET(pTspec->PeakDataRate, ACM_RATE_MAX, ("err> error peak rate!\n"));
	ACM_NIN_DEC_GET(pTspec->MeanDataRate, ACM_RATE_MAX, ("err> error mean rate!\n"));
	ACM_NIN_DEC_GET(pTspec->MinDataRate, ACM_RATE_MAX, ("err> error min rate!\n"));
	pTspec->DelayBound = 0;
	ACM_NIN_DEC_GET(pTspec->MinPhyRate, ACM_RATE_MAX, ("err> error phy rate!\n"));
	ACM_NIN_DEC_MGET(pTspec->SurplusBandwidthAllowance, 100, 800,
					("err> error surp factor!\n"));
	if (pTspec->SurplusBandwidthAllowance < 800)
	{
		SBA_Integer = pTspec->SurplusBandwidthAllowance / 100;
		SBA_Decimal = pTspec->SurplusBandwidthAllowance - SBA_Integer*100;
		SBA_Decimal = ACM_SurplusFactorDecimalDec2Bin(SBA_Decimal);
		pTspec->SurplusBandwidthAllowance = \
					(SBA_Integer << ACM_SURPLUS_DEC_BIT_NUM) | SBA_Decimal;
	}
	else
		pTspec->SurplusBandwidthAllowance = 0xffff;
	/* End of if */
	pTspec->MediumTime = 0;

	ACMR_DEBUG(ACMR_DEBUG_TRACE,
				("acm_msg> TSPEC : "
				"MAX/NOM SIZE=%d %d, INACT=%d, PEAK/MEAN/MIN RATE=%d %d %d, MIN PHY=%d, "
				"SBA=0x%x\n",
				pTspec->MaxMsduSize,
				pTspec->NominalMsduSize,
				pTspec->InactivityInt,
				pTspec->PeakDataRate,
				pTspec->MeanDataRate,
				pTspec->MinDataRate,
				pTspec->MinPhyRate,
				pTspec->SurplusBandwidthAllowance));

	*ppArgv = pArgv;
	return 0;

LabelErr:
	return 1;
} /* End of AcmCmdInfoParseAdvance */


#ifdef IEEE80211E_SIMULATION
#ifdef CONFIG_STA_SUPPORT
/*
========================================================================
Routine Description:
	Simulate the WLAN header from AP.

Arguments:
	pAd			- WLAN control block pointer

Return Value:
	None

Note:
	1. For QSTA test.
========================================================================
*/
static VOID AcmApMgtMacHeaderInit(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACMR_WLAN_HEADER	*pHdr,
	ACM_PARAM_IN	UCHAR				SubType,
	ACM_PARAM_IN	UCHAR				BitToDs,
	ACM_PARAM_IN	UCHAR				*pMacDa,
	ACM_PARAM_IN	UCHAR				*pBssid)
{
	NdisZeroMemory(pHdr, sizeof(ACMR_WLAN_HEADER));

	pHdr->FC.Type = BTYPE_MGMT;
	pHdr->FC.SubType = SubType;
	pHdr->FC.ToDs = BitToDs;
	COPY_MAC_ADDR(pHdr->Addr1, pMacDa);
	COPY_MAC_ADDR(pHdr->Addr2, pBssid);
	COPY_MAC_ADDR(pHdr->Addr3, pBssid);
} /* End of AcmApMgtMacHeaderInit */
#endif // CONFIG_STA_SUPPORT //




/*
========================================================================
Routine Description:
	QoS Data simulation task.

Arguments:
	Data			- WLAN control block pointer

Return Value:
	None

Note:
	For QAP.
========================================================================
*/
static VOID ACM_CMD_Task_Data_Simulation(ULONG Data)
{
	ACMR_PWLAN_STRUC pAd = (ACMR_PWLAN_STRUC)Data;
	UINT32 IdFlowNum;


	if (gTaskDataSleep == 1)
		goto LabelExit; /* sleeping, nothing to do */
	/* End of while */

	if (gSimDelayCount == 0)
	{
		for(IdFlowNum=0; IdFlowNum<ACM_MAX_NUM_OF_SIM_DATA_FLOW; IdFlowNum++)
		{
			if (gDATA_Sim[IdFlowNum].FlgIsValidEntry == 1)
			{
				switch(gDATA_Sim[IdFlowNum].Direction)
				{
					case 0: /* QoS receive */
						ACM_CMD_Sim_Data_Rv(pAd, &gDATA_Sim[IdFlowNum]);
						break;

					case 1: /* QoS transmission */
						ACM_CMD_Sim_Data_Tx(pAd, &gDATA_Sim[IdFlowNum]);
						break;

					case 2: /* non-QoS receive */
						ACM_CMD_Sim_nonQoS_Data_Rv(pAd, &gDATA_Sim[IdFlowNum]);
						break;
				} /* End of switch */
			} /* End of if */
		} /* End of for */

		gSimDelayCount = gSimDelay;
	}
	else
		gSimDelayCount --;
	/* End of if */

LabelExit:
#ifdef ACMR_HANDLE_IN_TIMER
	ACMR_TIMER_ENABLE(FlgIsTimerEnabled, gTimerSim, ACM_STREAM_CHECK_OFFSET);
#endif // ACMR_HANDLE_IN_TIMER //
} /* End of ACM_CMD_Task_Data_Simulation */


/*
========================================================================
Routine Description:
	Simulation periodical timer.

Arguments:
	Data			- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
VOID ACM_CMD_Timer_Data_Simulation(ULONG Data)
{
#ifndef ACMR_HANDLE_IN_TIMER
	ACMR_TASK_ACTIVATE(gTaskletSim, gTimerSim, 10); /* 100ms */

#else

	ACM_CMD_Task_Data_Simulation(Data);
#endif // ACMR_HANDLE_IN_TIMER //
} /* End of ACM_CMD_Timer_Data_Simulation */


/*
========================================================================
Routine Description:
	Send a QoS data frame to the QAP.

Arguments:
	pAd				- WLAN control block pointer
	*pInfo			- the data flow information

Return Value:
	None

Note:
	For QAP.
========================================================================
*/
static VOID ACM_CMD_Sim_Data_Rv(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACM_DATA_SIM		*pInfo)
{
	/* not support */
} /* End of ACM_CMD_Sim_Data_Rv */


/*
========================================================================
Routine Description:
	Send a non-QoS data frame to the QAP.

Arguments:
	pAd				- WLAN control block pointer
	*pInfo			- the data flow information

Return Value:
	None

Note:
	For QAP.
========================================================================
*/
static VOID ACM_CMD_Sim_nonQoS_Data_Rv(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACM_DATA_SIM		*pInfo)
{
	/* not support */
} /* End of ACM_CMD_Sim_nonQoS_Data_Rv */


/*
========================================================================
Routine Description:
	Transmit a QoS data frame from the upper layer.

Arguments:
	pAd			- WLAN control block pointer
	*pInfo			- the data flow information

Return Value:
	None

Note:
	For QAP.
========================================================================
*/
static VOID ACM_CMD_Sim_Data_Tx(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	ACM_DATA_SIM		*pInfo)
{
	UCHAR			*pBufFrame;
	NDIS_STATUS		Status;
	ACMR_MBUF		*pMblk;
	UCHAR			*pFrameBody;
	UINT32			DataIndex;


	/* init */
	pBufFrame = NULL;
	DataIndex = 0;

	/* get an unused nonpaged memory */
	Status = MlmeAllocateMemory(pAd, &pBufFrame);
	if (Status != NDIS_STATUS_SUCCESS)
		return;
	/* End of if */

	/* allocate action request frame */
	ACMR_PKT_ALLOCATE(pAd, pMblk);
	if (pMblk == NULL)
	{
		printk("11e_err> allocate action frame fail!\n");
		goto LabelErr;
	} /* End of if */

	/* init frame body */
	ACMR_MEM_COPY(pBufFrame, pInfo->MacDst, 6);
	ACMR_MEM_COPY(pBufFrame+6, pInfo->MacSrc, 6);
	*(UINT16 *)(pBufFrame+12) = 0x0800;

	pFrameBody = (UCHAR *)(pBufFrame+14);
	ACMR_MEM_SET(pFrameBody, 'c', pInfo->FrameSize); /* data */

	/* check whether the packet is TCP test packet */
	if (gSimTCPFlag != 0)
	{
#define DATA_SET(value)     pFrameBody[DataIndex++] = value;

		/* prepare TCP packet */
		DATA_SET(0x08); DATA_SET(0x00); /* type/len */
		DATA_SET(0x45); DATA_SET(gSimTCPDSCP); /* version, len, DSCP */
		DATA_SET(0x00); DATA_SET(0x28); /* total length */
		DATA_SET(0x4d); DATA_SET(0x78); /* identifier */
		DATA_SET(0x00); DATA_SET(0x00); DATA_SET(0x38); DATA_SET(0x06);
		DATA_SET(0x97); DATA_SET(0xd9);
		DATA_SET(0xac); DATA_SET(0x14); DATA_SET(0x40); DATA_SET(0xf9);
		DATA_SET(0xac); DATA_SET(0x14); DATA_SET(0x04); DATA_SET(0x5d);

		DATA_SET(0x00); DATA_SET(0x51); DATA_SET(0x06); DATA_SET(0x81);
		DATA_SET(0xdd); DATA_SET(0x56); DATA_SET(0x9d); DATA_SET(0xeb);
		DATA_SET(0xa5); DATA_SET(0xa9); DATA_SET(0xa0); DATA_SET(0x9b);
		DATA_SET(0x50); DATA_SET(0x10); DATA_SET(0x3d); DATA_SET(0x57);
		DATA_SET(0x0c); DATA_SET(0xa5); DATA_SET(0x00); DATA_SET(0x00);
	}
	else
	{
		/* use VLAN */
		pFrameBody = (UCHAR *)(pBufFrame+12);
		DATA_SET(0x81); DATA_SET(0x00); /* type/len */
		DATA_SET(pInfo->TID<<5); DATA_SET(0x00);
	} /* End of if */

	/* send the packet */
	ACMR_PKT_COPY(pMblk, pBufFrame, pInfo->FrameSize);

	rt28xx_packet_xmit(RTPKT_TO_OSPKT(pMblk));

LabelErr:
	/* free the frame buffer */
	MlmeFreeMemory(pAd, pBufFrame);
} /* End of ACM_CMD_Sim_Data_Tx */

#else

/*
========================================================================
Routine Description:
	Simulation periodical timer.

Arguments:
	Data			- WLAN control block pointer

Return Value:
	None

Note:
========================================================================
*/
VOID ACM_CMD_Timer_Data_Simulation(ULONG Data)
{
} /* End of ACM_CMD_Timer_Data_Simulation */
#endif // IEEE80211E_SIMULATION //


/*
========================================================================
Routine Description:
	Enable or disable test flag.

Arguments:
	pAd				- WLAN control block pointer
	Argc			- the number of input parameters
	*pArgv			- input parameters

Return Value:
	None

Note:
	Test flag is used in various test situation.
========================================================================
*/
static VOID AcmCmdTestFlagCtrl(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC	pAd,
	ACM_PARAM_IN	INT32				Argc,
	ACM_PARAM_IN	CHAR				*pArgv)
{
	gAcmTestFlag = AcmCmdUtilNumGet(&pArgv);
} /* End of AcmCmdTestFlagCtrl */




#define ACM_CMD_TCLAS_RESET			1	/* wmm tclas reset */
#define ACM_CMD_TCLAS_ADD			2	/* wmm tclas add */
#define ACM_CMD_EDCA_TS_ADD			3	/* wmm edca ts add */
#define ACM_CMD_EDCA_CHG			4	/* wmm edca change */
#define ACM_CMD_AVAL_SHOW			5	/* wmm available bw show */
#define ACM_CMD_SHOW				6	/* wmm ts show */
#define ACM_CMD_FSHOW				7	/* wmm fail show */
#define ACM_CMD_EDCA_SHOW			8	/* wmm edca info show */
#define ACM_CMD_WME_DATL			9	/* wmm datl */
#define ACM_CMD_WME_ACM_CTRL		10	/* wmm acm ctrl */
#define ACM_CMD_DELTS				11	/* wmm delts */
#define ACM_CMD_FCLEAR				12	/* wmm fclr */
#define ACM_CMD_EDCA_TS_NEG			13	/* wmm edca ts negotiate */
#define ACM_CMD_UAPSD_SHOW			14	/* wmm uapsd show */
#define ACM_CMD_TSPEC_REJECT		15	/* wmm tspec reject */
#define ACM_CMD_EDCA_TS_ADD_ADV		16	/* wmm edca ts add advcance */
#define ACM_CMD_STATS				17	/* wmm statistics */
#define ACM_CMD_REASSOCIATE			18	/* wmm reassociate */

#define ACM_CMD_SM_ASSOC			50	/* wmm sim assoc */
#define ACM_CMD_SM_REQ				51	/* wmm sim req */
#define ACM_CMD_SM_DEL_FRM_QSTA		52	/* wmm sim del */
#define ACM_CMD_SM_DATRV			53	/* wmm sim datrv */
#define ACM_CMD_SM_DATTX			54	/* wmm sim dattx */
#define ACM_CMD_SM_STP				55	/* wmm sim stp */
#define ACM_CMD_SM_SUS				56	/* wmm sim sus */
#define ACM_CMD_SM_RES				57	/* wmm sim res */
#define ACM_CMD_SM_REASSOC			58	/* wmm sim reassoc */
#define ACM_CMD_SM_NASSOC			59	/* wmm sim nassoc */
#define ACM_CMD_SM_NDATRV			60	/* wmm sim ndatarv */
#define ACM_CMD_SM_RATE				61	/* wmm sim rate */
#define ACM_CMD_SM_TCP				62	/* wmm sim tcp */
#define ACM_CMD_SM_STA_MAC_SET		63	/* wmm sim staset */
#define ACM_CMD_SM_STA_ASSOC		64	/* wmm sim sta auth/assoc */
#define ACM_CMD_SM_WME_REQTX		65	/* wmm sim req tx */
#define ACM_CMD_SM_WME_NEQTX		66	/* wmm sim neq tx */
#define ACM_CMD_SM_WME_REQ_FAIL		67	/* wmm sim req fail */
#define ACM_CMD_SM_WME_NEG_FAIL		68	/* wmm sim neg fail */
#define ACM_CMD_SM_ACM_RESET		69	/* wmm sim acm reset */
#define ACM_CMD_SM_STA_PS			70	/* wmm sim station enters PS or ACT */
#define ACM_CMD_SM_REQ_PS_POLL		71	/* wmm sim req & ps poll */
#define ACM_CMD_SM_DEL_FRM_QAP		72	/* wmm sim del */
#define ACM_CMD_SM_TRI_FRM_RCV		73	/* wmm sim trigger frame receive */
#define ACM_CMD_SM_UAPSD_QUE_MAIN	74	/* wmm sim uapsd queue maintain ctrl */
#define ACM_TEST_FLAG				90	/* wmm test flag */

/*
========================================================================
Routine Description:
	Test command.

Arguments:
	pAd				- WLAN control block pointer
	*pArgvIn		- the data flow information

Return Value:
	None

Note:
	All test commands are listed as follows:
		iwpriv ra0 set acm=[cmd id]_[arg1]_[arg2]_......_[argn]
		[cmd id] = xx, such as 00, 01, 02, 03, ...

	1.  Reset all TCLAS elements
	2.  Create a TCLAS element
			[up:0~7] [type:0~2] [mask:hex] [classifier:max 16B]
	3.  Create a QoS EDCA stream
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[APSD:0~1] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
			(ack policy:0~3)
	4.  Change EDCA parameters
			[CPnu] [CPde] [BEnu] [BEde]
	5.  Show current bandwidth status
	6.  Show current streams status
			[1:EDCA] (Client MAC: xx:xx:xx:xx:xx:xx)
	7.  Show fail streams status
	8.  Show current EDCA parameters
	9.  Set DATL parameters
			[enable: 0~1] [minimum bw threshold for AC0~AC3]
			[maximum bw threshold for AC0~AC3]
	10. Enable/disable ACM flag for 4 AC
			[AC0] [AC1] [AC2] [AC3]
	11. Send out a DELTS request frame
			[Peer MAC] [TID:0~7]
	12. Clear the failed list
	13. Negotiate a QoS EDCA stream
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[APSD:0~1] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
	14. Show UAPSD information
			[Device MAC]
	15. Reject all new TSPEC requests
			[Enable/Disable 1/0]
	16. Create a advanced QoS EDCA stream
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[APSD:0~1] [max size:byte] [nom size:byte] [burst size:byte]
			[inact:sec]
			[peak data rate:bps] [mean data rate:bps] [min data rate:bps]
			[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
	17. Show statistics count
	18. Send a reassociate frame to the associated AP


	50. Simulate authentication & assocaition req event
			(Source Client MAC)
	51. Simulate a request frame receive event
			[sta mac:xx:xx:xx:xx:xx:xx]
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[UAPSD:0/1] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
	52. Simulate to delete a actived stream from QSTA
			[Client MAC] [type:0-11e, 1-WME] [TID:0~7] [dir:0~3]
	53. Simulate to continue to receive packets from a QSTA
			[Src Client MAC] [Dst Client MAC] [type:0-11e, 1-WME] [TID:0~7]
			[size] (ack: 0~1) (fragment: 0~1) (rts/cts: 0~1)
	54. Simulate to continue to transmit packets from upper layer
			[Dst Client MAC] [type:0-11e, 1-WME] [TID:0~7] [size] [ack:0~1]
	55. Simulate to stop to continue to send packets to a QSTA
			(Client MAC)
	56. Simulate to suspend to send packets
	57. Simulate to resume to send packets
			(delay)
	58. Clear all failed records
	59. Simulate authentication & assocaition req event (non-QoS)
			[Source Client MAC]
	60. Simulate to continue to receive non-QoS packets from a QSTA
			[Src Client MAC] [Dst Client MAC] [TID:0~7]
	61. Simulate to force AP to transmit packets using the rate
			[Dst Client MAC] [rate]
	62. Simulate to enable or disable TCP tx packet
			[enable: 0~1] [DSCP]
	63. Simulate to reset station MAC address
			[MAC]
	64. Simulate to authenticate and associate a AP
			[AP MAC]
	65. Simulate to send a ADDTS request and receive a ADDTS response
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[ack:0] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1] [tclas processing:0~1]
	66. Simulate to send a negotiated ADDTS request and receive a ADDTS response
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[ack:0] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1]
	67. Simulate to send a ADDTS request and receive a ADDTS fail response
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[ack:0] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1]
	68. Simulate to send a negotiated ADDTS req and receive a failed ADDTS rsp
			[type:1-WME] [TID:0~7] [dir:0~3] [access:1] [UP:0~7]
			[ack:0] [nom size:byte] [inact:sec] [mean data rate:bps]
			[min phy rate:bps] [surp factor:>=1]
	69. Simulate the ACM flag set
			[ACM0] [ACM1] [ACM2] [ACM3]
	70. Simulate the station enters PS mode or ACTIVE mode
			[mode:0~1] [MAC]
	71. Simulate to receive a PS Poll frame
			[MAC]
	72. Simulate to delete a actived stream from QAP
			[type:1-WME] [TID:0~7] [dir:0~3]
	73. Simulate to receive a trigger frame from QSTA
			[MAC]
	74. Simulate to enable/disable UAPSD queue maintain
			[enable: 0~1]

	90. Test Flag
			[ON/OFF]
========================================================================
*/
INT ACM_Ioctl(
	ACM_PARAM_IN	ACMR_PWLAN_STRUC 	pAd,
	ACM_PARAM_IN	PSTRING				pArgvIn)
{
	CHAR BufCmd[3] = { 0, 0, 0 };
	CHAR *pArgv, *pParam;
	UINT32 Command;
	INT32 Argc;


	/* init */
	pArgv = (CHAR *)pArgvIn;

	/* get command type */
	/* command format is iwpriv ra0 set acm=[cmd id]_[arg1]_......_[argn] */
	ACMR_MEM_COPY(BufCmd, pArgv, 2);
	Command = ACMR_ARG_ATOI(BufCmd);
	pArgv += 2; /* skip command field */

	/* get Argc number */
	Argc = 0;
	pParam = pArgv;

	while(1)
	{
		if (*pParam == '_')
			Argc ++;
		/* End of if */

		if ((*pParam == 0x00) || (Argc > 20))
			break;
		/* End of if */

		pParam++;
	} /* End of while */

	pArgv++; /* skip _ points to arg1 */


	/* handle the command */
	switch(Command)
	{
		case ACM_CMD_TCLAS_RESET: /* 1 wmm tclas reset */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> reset TCLAS\n"));
			AcmCmdTclasReset(pAd, Argc, pArgv);
			break;

		case ACM_CMD_TCLAS_ADD: /* 2 wmm tclas add */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> add a TCLAS\n"));
			AcmCmdTclasCreate(pAd, Argc, pArgv);
			break;

#ifdef CONFIG_STA_SUPPORT
		case ACM_CMD_EDCA_TS_ADD: /* 3 wmm edca ts add */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> request a TSPEC\n"));
			AcmCmdStreamTSRequest(pAd, Argc, pArgv, 0); //snowpin
			break;
#endif // CONFIG_STA_SUPPORT //

		case ACM_CMD_EDCA_CHG: /* 4 wmm edca change */
			break;

		case ACM_CMD_AVAL_SHOW: /* 5 wmm available bw show */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show available bw\n"));
			AcmCmdBandwidthDisplay(pAd, 0, pArgv);
			break;

		case ACM_CMD_SHOW: /* 6 wmm ts show */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show ACM status\n"));
			AcmCmdStreamDisplay(pAd, Argc, pArgv);
			break;

		case ACM_CMD_FSHOW: /* 7 wmm fail show */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show failed TS info\n"));
			AcmCmdStreamFailDisplay(pAd, 0, pArgv);
			break;

		case ACM_CMD_EDCA_SHOW: /* 8 wmm edca info show */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show WMM TS info\n"));
			AcmCmdEDCAParamDisplay(pAd, 0, pArgv);
			break;

		case ACM_CMD_WME_DATL: /* 9 wmm datl */
			ACMR_DEBUG(ACMR_DEBUG_TRACE,
					("11e_cmd> enable/disable Dynamic ATL\n"));
			AcmCmdDATLEnable(pAd, Argc, pArgv);
			break;


		case ACM_CMD_DELTS: /* 11 wmm delts */
			ACMR_DEBUG(ACMR_DEBUG_TRACE,
					("11e_cmd> send a DELTS request frame\n"));
			AcmCmdDeltsSend(pAd, Argc, pArgv);
			break;

		case ACM_CMD_FCLEAR: /* 12 wmm fclr */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> clear failed TS info\n"));
			AcmCmdStreamFailClear(pAd, Argc, pArgv);
			break;

#ifdef CONFIG_STA_SUPPORT
		case ACM_CMD_EDCA_TS_NEG: /* 13 wmm edca ts negotiate */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> negotiate a TSPEC\n"));
			AcmCmdStreamTSNegotiate(pAd, Argc, pArgv);
			break;
#endif // CONFIG_STA_SUPPORT //

		case ACM_CMD_UAPSD_SHOW: /* 14 wmm uapsd show */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show UAPSD info\n"));
			AcmCmdUapsdDisplay(pAd, Argc, pArgv);
			break;


#ifdef CONFIG_STA_SUPPORT
		case ACM_CMD_EDCA_TS_ADD_ADV: /* 16 wmm edca ts add advance */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> request a advanced TSPEC\n"));
			AcmCmdStreamTSRequestAdvance(pAd, Argc, pArgv);
			break;
#endif // CONFIG_STA_SUPPORT //

		case ACM_CMD_STATS: /* 17 wmm statistics */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> show statistics\n"));
			AcmCmdStatistics(pAd, Argc, pArgv);
			break;

#ifdef CONFIG_STA_SUPPORT
		case ACM_CMD_REASSOCIATE: /* 18 wmm reassociate */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> send a reassociate frame\n"));
			AcmCmdReAssociate(pAd, Argc, pArgv);
			break;
#endif // CONFIG_STA_SUPPORT //



		case ACM_TEST_FLAG: /* 90 wmm test flag */
			ACMR_DEBUG(ACMR_DEBUG_TRACE,
					("11e_cmd> ON/OFF test flag\n"));
			AcmCmdTestFlagCtrl(pAd, Argc, pArgv);
			break;

		default: /* error command type */
			ACMR_DEBUG(ACMR_DEBUG_TRACE, ("11e_cmd> ERROR! No such command!\n"));
			return -EINVAL; /* input error */
	} /* End of switch */

	return TRUE;
} /* End of ACM_Ioctl */

#endif // WMM_ACM_SUPPORT //

/* End of acm_iocl.c */

