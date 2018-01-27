/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file pre_processor.c
 * \brief eNB scheduler preprocessing fuction prior to scheduling
 * \author Navid Nikaein and Ankit Bhamri
 * \date 2013 - 2014
 * \email navid.nikaein@eurecom.fr
 * \version 1.0
 * @ingroup _mac

 */

#define _GNU_SOURCE
#include <stdlib.h>

#include "assertions.h"
#include "PHY/defs.h"
#include "PHY/extern.h"

#include "SCHED/defs.h"
#include "SCHED/extern.h"

#include "LAYER2/MAC/defs.h"
#include "LAYER2/MAC/proto.h"
#include "LAYER2/MAC/extern.h"
#include "UTIL/LOG/log.h"
#include "UTIL/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "OCG.h"
#include "OCG_extern.h"
#include "RRC/LITE/extern.h"
#include "RRC/L2_INTERFACE/openair_rrc_L2_interface.h"
#include "rlc.h"



#define DEBUG_eNB_SCHEDULER 1
#define DEBUG_HEADER_PARSING 1
//#define DEBUG_PACKET_TRACE 1

//#define ICIC 0

/*
  #ifndef USER_MODE
  #define msg debug_msg
  #endif
*/

/* this function checks that get_eNB_UE_stats returns
 * a non-NULL pointer for all the active CCs of an UE
 */
/*
int phy_stats_exist(module_id_t Mod_id, int rnti)
{
  int CC_id;
  int i;
  int UE_id          = find_UE_id(Mod_id, rnti);
  UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
  if (UE_id == -1) {
    LOG_W(MAC, "[eNB %d] UE %x not found, should be there (in phy_stats_exist)\n",
	  Mod_id, rnti);
    return 0;
  }
  if (UE_list->numactiveCCs[UE_id] == 0) {
    LOG_W(MAC, "[eNB %d] UE %x has no active CC (in phy_stats_exist)\n",
	  Mod_id, rnti);
    return 0;
  }
  for (i = 0; i < UE_list->numactiveCCs[UE_id]; i++) {
    CC_id = UE_list->ordered_CCids[i][UE_id];
    if (mac_xface->get_eNB_UE_stats(Mod_id, CC_id, rnti) == NULL)
      return 0;
  }
  return 1;
}
*/

// This function stores the downlink buffer for all the logical channels
void
store_dlsch_buffer(module_id_t Mod_id, frame_t frameP,
		   sub_frame_t subframeP)
{

    int UE_id, i;
    rnti_t rnti;
    mac_rlc_status_resp_t rlc_status;
    UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
    UE_TEMPLATE *UE_template;

    for (UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++) {
	if (UE_list->active[UE_id] != TRUE)
	    continue;

	UE_template =
	    &UE_list->UE_template[UE_PCCID(Mod_id, UE_id)][UE_id];

	// clear logical channel interface variables
	UE_template->dl_buffer_total = 0;
	UE_template->dl_pdus_total = 0;

	rnti = UE_RNTI(Mod_id, UE_id);

#if defined(UE_EXPANSION) || defined(UE_EXPANSION_SIM2)
	for (i = DCCH; i <=DTCH; i++) {	// loop over DCCH, DCCH1 and DTCH
#else
    for (i = 0; i < MAX_NUM_LCID; i++) {    // loop over all the logical channels
#endif

	    rlc_status =
		mac_rlc_status_ind(Mod_id, rnti, Mod_id, frameP, subframeP,
				   ENB_FLAG_YES, MBMS_FLAG_NO, i, 0);
	    UE_template->dl_buffer_info[i] = rlc_status.bytes_in_buffer;	//storing the dlsch buffer for each logical channel
	    UE_template->dl_pdus_in_buffer[i] = rlc_status.pdus_in_buffer;
	    UE_template->dl_buffer_head_sdu_creation_time[i] =
		rlc_status.head_sdu_creation_time;
	    UE_template->dl_buffer_head_sdu_creation_time_max =
		cmax(UE_template->dl_buffer_head_sdu_creation_time_max,
		     rlc_status.head_sdu_creation_time);
	    UE_template->dl_buffer_head_sdu_remaining_size_to_send[i] =
		rlc_status.head_sdu_remaining_size_to_send;
	    UE_template->dl_buffer_head_sdu_is_segmented[i] =
		rlc_status.head_sdu_is_segmented;
	    UE_template->dl_buffer_total += UE_template->dl_buffer_info[i];	//storing the total dlsch buffer
	    UE_template->dl_pdus_total +=
		UE_template->dl_pdus_in_buffer[i];

#ifdef DEBUG_eNB_SCHEDULER

	    /* note for dl_buffer_head_sdu_remaining_size_to_send[i] :
	     * 0 if head SDU has not been segmented (yet), else remaining size not already segmented and sent
	     */
	    if (UE_template->dl_buffer_info[i] > 0)
		LOG_D(MAC,
		      "[eNB %d] Frame %d Subframe %d : RLC status for UE %d in LCID%d: total of %d pdus and size %d, head sdu queuing time %d, remaining size %d, is segmeneted %d \n",
		      Mod_id, frameP, subframeP, UE_id,
		      i, UE_template->dl_pdus_in_buffer[i],
		      UE_template->dl_buffer_info[i],
		      UE_template->dl_buffer_head_sdu_creation_time[i],
		      UE_template->
		      dl_buffer_head_sdu_remaining_size_to_send[i],
		      UE_template->dl_buffer_head_sdu_is_segmented[i]);

#endif

	}

	//#ifdef DEBUG_eNB_SCHEDULER
	if (UE_template->dl_buffer_total > 0)
	    LOG_D(MAC,
		  "[eNB %d] Frame %d Subframe %d : RLC status for UE %d : total DL buffer size %d and total number of pdu %d \n",
		  Mod_id, frameP, subframeP, UE_id,
		  UE_template->dl_buffer_total,
		  UE_template->dl_pdus_total);

	//#endif
    }
}


// This function returns the estimated number of RBs required by each UE for downlink scheduling
void
assign_rbs_required(module_id_t Mod_id,
		    frame_t frameP,
		    sub_frame_t subframe,
		    uint16_t
		    nb_rbs_required[MAX_NUM_CCs][NUMBER_OF_UE_MAX],
		    int min_rb_unit[MAX_NUM_CCs])
{

    uint16_t TBS = 0;

    int UE_id, n, i, j, CC_id, pCCid, tmp;
    UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
    eNB_UE_STATS *eNB_UE_stats, *eNB_UE_stats_i, *eNB_UE_stats_j;
    int N_RB_DL;

    // clear rb allocations across all CC_id
    for (UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++) {
	if (UE_list->active[UE_id] != TRUE)
	    continue;

	pCCid = UE_PCCID(Mod_id, UE_id);

	//update CQI information across component carriers
	for (n = 0; n < UE_list->numactiveCCs[UE_id]; n++) {

	    CC_id = UE_list->ordered_CCids[n][UE_id];
	    eNB_UE_stats = &UE_list->eNB_UE_stats[CC_id][UE_id];

	    eNB_UE_stats->dlsch_mcs1 =
		cqi_to_mcs[UE_list->UE_sched_ctrl[UE_id].dl_cqi[CC_id]];

	}

	// provide the list of CCs sorted according to MCS
	for (i = 0; i < UE_list->numactiveCCs[UE_id]; i++) {
	    eNB_UE_stats_i =
		&UE_list->eNB_UE_stats[UE_list->
				       ordered_CCids[i][UE_id]][UE_id];
	    for (j = i + 1; j < UE_list->numactiveCCs[UE_id]; j++) {
		DevAssert(j < MAX_NUM_CCs);
		eNB_UE_stats_j =
		    &UE_list->
		    eNB_UE_stats[UE_list->ordered_CCids[j][UE_id]][UE_id];
		if (eNB_UE_stats_j->dlsch_mcs1 >
		    eNB_UE_stats_i->dlsch_mcs1) {
		    tmp = UE_list->ordered_CCids[i][UE_id];
		    UE_list->ordered_CCids[i][UE_id] =
			UE_list->ordered_CCids[j][UE_id];
		    UE_list->ordered_CCids[j][UE_id] = tmp;
		}
	    }
	}

	if (UE_list->UE_template[pCCid][UE_id].dl_buffer_total > 0) {
	    LOG_D(MAC, "[preprocessor] assign RB for UE %d\n", UE_id);

	    for (i = 0; i < UE_list->numactiveCCs[UE_id]; i++) {
		CC_id = UE_list->ordered_CCids[i][UE_id];
		eNB_UE_stats = &UE_list->eNB_UE_stats[CC_id][UE_id];

		if (eNB_UE_stats->dlsch_mcs1 == 0) {
		    nb_rbs_required[CC_id][UE_id] = 4;	// don't let the TBS get too small
		} else {
		    nb_rbs_required[CC_id][UE_id] = min_rb_unit[CC_id];
		}

		TBS =
		    get_TBS_DL(eNB_UE_stats->dlsch_mcs1,
			       nb_rbs_required[CC_id][UE_id]);

		LOG_D(MAC,
		      "[preprocessor] start RB assignement for UE %d CC_id %d dl buffer %d (RB unit %d, MCS %d, TBS %d) \n",
		      UE_id, CC_id,
		      UE_list->UE_template[pCCid][UE_id].dl_buffer_total,
		      nb_rbs_required[CC_id][UE_id],
		      eNB_UE_stats->dlsch_mcs1, TBS);

		N_RB_DL =
		    to_prb(RC.mac[Mod_id]->common_channels[CC_id].
			   mib->message.dl_Bandwidth);

		/* calculating required number of RBs for each UE */
		while (TBS <
		       UE_list->UE_template[pCCid][UE_id].
		       dl_buffer_total) {
		    nb_rbs_required[CC_id][UE_id] += min_rb_unit[CC_id];

		    if (nb_rbs_required[CC_id][UE_id] > N_RB_DL) {
			TBS =
			    get_TBS_DL(eNB_UE_stats->dlsch_mcs1, N_RB_DL);
			nb_rbs_required[CC_id][UE_id] = N_RB_DL;
			break;
		    }

		    TBS =
			get_TBS_DL(eNB_UE_stats->dlsch_mcs1,
				   nb_rbs_required[CC_id][UE_id]);
		}		// end of while

		LOG_D(MAC,
		      "[eNB %d] Frame %d: UE %d on CC %d: RB unit %d,  nb_required RB %d (TBS %d, mcs %d)\n",
		      Mod_id, frameP, UE_id, CC_id, min_rb_unit[CC_id],
		      nb_rbs_required[CC_id][UE_id], TBS,
		      eNB_UE_stats->dlsch_mcs1);
	    }
	}
    }
}


// This function scans all CC_ids for a particular UE to find the maximum round index of its HARQ processes

int
maxround(module_id_t Mod_id, uint16_t rnti, int frame,
	 sub_frame_t subframe, uint8_t ul_flag)
{

    uint8_t round, round_max = 0, UE_id;
    int CC_id, harq_pid;
    UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
    COMMON_channels_t *cc;

    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {

	cc = &RC.mac[Mod_id]->common_channels[CC_id];

	UE_id = find_UE_id(Mod_id, rnti);
	if (cc->tdd_Config)
	    harq_pid = ((frame * 10) + subframe) % 10;
	else
	    harq_pid = ((frame * 10) + subframe) & 7;

	round = UE_list->UE_sched_ctrl[UE_id].round[CC_id][harq_pid];
	if (round > round_max) {
	    round_max = round;
	}
    }

    return round_max;
}

// This function scans all CC_ids for a particular UE to find the maximum DL CQI
// it returns -1 if the UE is not found in PHY layer (get_eNB_UE_stats gives NULL)
int maxcqi(module_id_t Mod_id, int32_t UE_id)
{
    UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
    int CC_id, n;
    int CQI = 0;

    for (n = 0; n < UE_list->numactiveCCs[UE_id]; n++) {
	CC_id = UE_list->ordered_CCids[n][UE_id];

	if (UE_list->UE_sched_ctrl[UE_id].dl_cqi[CC_id] > CQI) {
	    CQI = UE_list->UE_sched_ctrl[UE_id].dl_cqi[CC_id];
	}
    }

    return CQI;
}

struct sort_ue_dl_params {
    int Mod_idP;
    int frameP;
    int subframeP;
};

static int ue_dl_compare(const void *_a, const void *_b, void *_params)
{
    struct sort_ue_dl_params *params = _params;
    UE_list_t *UE_list = &RC.mac[params->Mod_idP]->UE_list;

    int UE_id1 = *(const int *) _a;
    int UE_id2 = *(const int *) _b;

    int rnti1 = UE_RNTI(params->Mod_idP, UE_id1);
    int pCC_id1 = UE_PCCID(params->Mod_idP, UE_id1);
    int round1 =
	maxround(params->Mod_idP, rnti1, params->frameP, params->subframeP,
		 1);

    int rnti2 = UE_RNTI(params->Mod_idP, UE_id2);
    int pCC_id2 = UE_PCCID(params->Mod_idP, UE_id2);
    int round2 =
	maxround(params->Mod_idP, rnti2, params->frameP, params->subframeP,
		 1);

    int cqi1 = maxcqi(params->Mod_idP, UE_id1);
    int cqi2 = maxcqi(params->Mod_idP, UE_id2);

    if (round1 > round2)
	return -1;
    if (round1 < round2)
	return 1;

    if (UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[1] +
	UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[2] >
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[1] +
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[2])
	return -1;
    if (UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[1] +
	UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[2] <
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[1] +
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[2])
	return 1;

    if (UE_list->
	UE_template[pCC_id1][UE_id1].dl_buffer_head_sdu_creation_time_max >
	UE_list->
	UE_template[pCC_id2][UE_id2].dl_buffer_head_sdu_creation_time_max)
	return -1;
    if (UE_list->
	UE_template[pCC_id1][UE_id1].dl_buffer_head_sdu_creation_time_max <
	UE_list->
	UE_template[pCC_id2][UE_id2].dl_buffer_head_sdu_creation_time_max)
	return 1;

    if (UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_total >
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_total)
	return -1;
    if (UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_total <
	UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_total)
	return 1;

    if (cqi1 > cqi2)
	return -1;
    if (cqi1 < cqi2)
	return 1;

    return 0;
#if 0
    /* The above order derives from the following.  */
    if (round2 > round1) {	// Check first if one of the UEs has an active HARQ process which needs service and swap order
	swap_UEs(UE_list, UE_id1, UE_id2, 0);
    } else if (round2 == round1) {
	// RK->NN : I guess this is for fairness in the scheduling. This doesn't make sense unless all UEs have the same configuration of logical channels.  This should be done on the sum of all information that has to be sent.  And still it wouldn't ensure fairness.  It should be based on throughput seen by each UE or maybe using the head_sdu_creation_time, i.e. swap UEs if one is waiting longer for service.
	//  for(j=0;j<MAX_NUM_LCID;j++){
	//    if (eNB_mac_inst[Mod_id][pCC_id1].UE_template[UE_id1].dl_buffer_info[j] <
	//      eNB_mac_inst[Mod_id][pCC_id2].UE_template[UE_id2].dl_buffer_info[j]){

	// first check the buffer status for SRB1 and SRB2

	if ((UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[1] +
	     UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_info[2]) <
	    (UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[1] +
	     UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_info[2])) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 0);
	} else if (UE_list->UE_template[pCC_id1]
		   [UE_id1].dl_buffer_head_sdu_creation_time_max <
		   UE_list->UE_template[pCC_id2]
		   [UE_id2].dl_buffer_head_sdu_creation_time_max) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 0);
	} else if (UE_list->UE_template[pCC_id1][UE_id1].dl_buffer_total <
		   UE_list->UE_template[pCC_id2][UE_id2].dl_buffer_total) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 0);
	} else if (cqi1 < cqi2) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 0);
	}
    }
#endif
}

// This fuction sorts the UE in order their dlsch buffer and CQI
void sort_UEs(module_id_t Mod_idP, int frameP, sub_frame_t subframeP)
{
    int i;
    int list[NUMBER_OF_UE_MAX];
    int list_size = 0;
    int rnti;
    struct sort_ue_dl_params params = { Mod_idP, frameP, subframeP };

    UE_list_t *UE_list = &RC.mac[Mod_idP]->UE_list;

    for (i = 0; i < NUMBER_OF_UE_MAX; i++) {

	if (UE_list->active[i] == FALSE)
	    continue;
	if ((rnti = UE_RNTI(Mod_idP, i)) == NOT_A_RNTI)
	    continue;
#if 0
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;
#endif
	list[list_size] = i;
	list_size++;
    }

    qsort_r(list, list_size, sizeof(int), ue_dl_compare, &params);

    if (list_size) {
	for (i = 0; i < list_size - 1; i++)
	    UE_list->next[list[i]] = list[i + 1];
	UE_list->next[list[list_size - 1]] = -1;
	UE_list->head = list[0];
    } else {
	UE_list->head = -1;
    }

#if 0


    int UE_id1, UE_id2;
    int pCC_id1, pCC_id2;
    int cqi1, cqi2, round1, round2;
    int i = 0, ii = 0;		//,j=0;
    rnti_t rnti1, rnti2;

    UE_list_t *UE_list = &RC.mac[Mod_idP]->UE_list;

    for (i = UE_list->head; i >= 0; i = UE_list->next[i]) {

	for (ii = UE_list->next[i]; ii >= 0; ii = UE_list->next[ii]) {

	    UE_id1 = i;
	    rnti1 = UE_RNTI(Mod_idP, UE_id1);
	    if (rnti1 == NOT_A_RNTI)
		continue;
	    if (UE_list->UE_sched_ctrl[UE_id1].ul_out_of_sync == 1)
		continue;
	    pCC_id1 = UE_PCCID(Mod_idP, UE_id1);
	    cqi1 = maxcqi(Mod_idP, UE_id1);	//
	    round1 = maxround(Mod_idP, rnti1, frameP, subframeP, 0);

	    UE_id2 = ii;
	    rnti2 = UE_RNTI(Mod_idP, UE_id2);
	    if (rnti2 == NOT_A_RNTI)
		continue;
	    if (UE_list->UE_sched_ctrl[UE_id2].ul_out_of_sync == 1)
		continue;
	    cqi2 = maxcqi(Mod_idP, UE_id2);
	    round2 = maxround(Mod_idP, rnti2, frameP, subframeP, 0);	//mac_xface->get_ue_active_harq_pid(Mod_id,rnti2,subframe,&harq_pid2,&round2,0);
	    pCC_id2 = UE_PCCID(Mod_idP, UE_id2);

	    if (round2 > round1) {	// Check first if one of the UEs has an active HARQ process which needs service and swap order
		swap_UEs(UE_list, UE_id1, UE_id2, 0);
	    } else if (round2 == round1) {
		// RK->NN : I guess this is for fairness in the scheduling. This doesn't make sense unless all UEs have the same configuration of logical channels.  This should be done on the sum of all information that has to be sent.  And still it wouldn't ensure fairness.  It should be based on throughput seen by each UE or maybe using the head_sdu_creation_time, i.e. swap UEs if one is waiting longer for service.
		//  for(j=0;j<MAX_NUM_LCID;j++){
		//    if (eNB_mac_inst[Mod_id][pCC_id1].UE_template[UE_id1].dl_buffer_info[j] <
		//      eNB_mac_inst[Mod_id][pCC_id2].UE_template[UE_id2].dl_buffer_info[j]){

		// first check the buffer status for SRB1 and SRB2

		if ((UE_list->UE_template[pCC_id1][UE_id1].
		     dl_buffer_info[1] +
		     UE_list->UE_template[pCC_id1][UE_id1].
		     dl_buffer_info[2]) <
		    (UE_list->UE_template[pCC_id2][UE_id2].
		     dl_buffer_info[1] +
		     UE_list->UE_template[pCC_id2][UE_id2].
		     dl_buffer_info[2])) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 0);
		} else if (UE_list->UE_template[pCC_id1]
			   [UE_id1].dl_buffer_head_sdu_creation_time_max <
			   UE_list->UE_template[pCC_id2]
			   [UE_id2].dl_buffer_head_sdu_creation_time_max) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 0);
		} else if (UE_list->UE_template[pCC_id1][UE_id1].
			   dl_buffer_total <
			   UE_list->UE_template[pCC_id2][UE_id2].
			   dl_buffer_total) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 0);
		} else if (cqi1 < cqi2) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 0);
		}
	    }
	}
    }
#endif
}

#if defined(UE_EXPANSION) || defined(UE_EXPANSION_SIM2)
inline uint16_t search_rbs_required(uint16_t mcs, uint16_t TBS,uint16_t NB_RB, uint16_t step_size){
  uint16_t nb_rb,i_TBS,tmp_TBS;
  i_TBS=get_I_TBS(mcs);
  for(nb_rb=step_size;nb_rb<NB_RB;nb_rb+=step_size){
    tmp_TBS = TBStable[i_TBS][nb_rb-1]>>3;
    if(TBS<tmp_TBS)return(nb_rb);
  }
  return NB_RB;
}
void pre_scd_nb_rbs_required(    module_id_t     module_idP,
                                 frame_t         frameP,
                                 sub_frame_t     subframeP,
                                 int             min_rb_unit[MAX_NUM_CCs],
                                 uint16_t        nb_rbs_required[MAX_NUM_CCs][NUMBER_OF_UE_MAX])
{
    int                          CC_id=0,UE_id, lc_id, N_RB_DL;
    UE_TEMPLATE                  UE_template;
    eNB_UE_STATS                 *eNB_UE_stats;
    rnti_t                       rnti;
    mac_rlc_status_resp_t        rlc_status;
    uint16_t                     step_size=2;
    
    N_RB_DL = to_prb(RC.mac[module_idP]->common_channels[CC_id].mib->message.dl_Bandwidth);
    if(N_RB_DL==50) step_size=3;
    if(N_RB_DL==100) step_size=4;
    memset(nb_rbs_required, 0, sizeof(uint16_t)*MAX_NUM_CCs*NUMBER_OF_UE_MAX);
    UE_list_t *UE_list = &RC.mac[module_idP]->UE_list;

    for (UE_id = 0; UE_id <NUMBER_OF_UE_MAX; UE_id++) {
        if (pre_scd_activeUE[UE_id] != TRUE)
            continue;

        // store dlsch buffer

        // clear logical channel interface variables
        UE_template.dl_buffer_total = 0;

        rnti = UE_RNTI(module_idP, UE_id);

        for (lc_id = DCCH; lc_id <= DTCH; lc_id++) {
            rlc_status =
                    mac_rlc_status_ind(module_idP, rnti, module_idP, frameP, subframeP,
                            ENB_FLAG_YES, MBMS_FLAG_NO, lc_id, 0);
            UE_template.dl_buffer_total += rlc_status.bytes_in_buffer; //storing the total dlsch buffer
        }
         // end of store dlsch buffer

        // assgin rbs required
        // Calculate the number of RBs required by each UE on the basis of logical channel's buffer
        //update CQI information across component carriers
        eNB_UE_stats = &pre_scd_eNB_UE_stats[CC_id][UE_id];

        eNB_UE_stats->dlsch_mcs1 = cqi_to_mcs[UE_list->UE_sched_ctrl[UE_id].dl_cqi[CC_id]];


        if (UE_template.dl_buffer_total > 0) {
          nb_rbs_required[CC_id][UE_id] = search_rbs_required(eNB_UE_stats->dlsch_mcs1, UE_template.dl_buffer_total, N_RB_DL, step_size);
        }
    }
}
#endif

#ifdef UE_EXPANSION
int cc_id_end(uint8_t *cc_id_flag )
{
  int end_flag = 1;
  for (int CC_id=0;CC_id<MAX_NUM_CCs;CC_id++) {
    if (cc_id_flag[CC_id]==0) {
      end_flag = 0;
      break;
    }
  }
  return end_flag;
}

void dlsch_scheduler_pre_ue_select(
    module_id_t     module_idP,
    frame_t         frameP,
    sub_frame_t     subframeP,
    int*            mbsfn_flag,
    uint16_t        nb_rbs_required[MAX_NUM_CCs][NUMBER_OF_UE_MAX],
    DLSCH_UE_SELECT dlsch_ue_select[MAX_NUM_CCs])
{
  eNB_MAC_INST                   *eNB      = RC.mac[module_idP];
  COMMON_channels_t              *cc       = eNB->common_channels;
  UE_list_t                      *UE_list  = &eNB->UE_list;
  UE_sched_ctrl                  *ue_sched_ctl;
  uint8_t                        CC_id;
  int                            UE_id;
  unsigned char                  round             = 0;
  unsigned char                  harq_pid          = 0;
  rnti_t                         rnti;
  uint16_t                       i;
  unsigned char                  aggregation;
  int                            format_flag;
  nfapi_dl_config_request_body_t *DL_req;
  nfapi_dl_config_request_pdu_t  *dl_config_pdu;
  uint16_t                       dlsch_ue_max_num[MAX_NUM_CCs] = {0};
  uint16_t                       saved_dlsch_dci[MAX_NUM_CCs] = {0};
  uint8_t                        end_flag[MAX_NUM_CCs] = {0};

  // Initialization
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    dlsch_ue_max_num[CC_id] = (uint16_t)RC.rrc[module_idP]->configuration.ue_multiple_max[CC_id];

    // save origin DL PDU number
    DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
    saved_dlsch_dci[CC_id] = DL_req->number_pdu;
  }

  // Insert DLSCH(retransmission) UE into selected UE list
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    if (mbsfn_flag[CC_id]>0) {
      continue;
    }

    DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
    for (UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++) {
      if (UE_list->active[UE_id] == FALSE) {
        continue;
      }

      rnti = UE_RNTI(module_idP, UE_id);
      if (rnti == NOT_A_RNTI) {
        continue;
      }

      ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
#if 0
      if (ue_sched_ctl->ul_out_of_sync == 1) {
        continue;
      }
#endif
      if (cc[CC_id].tdd_Config) harq_pid = ((frameP*10)+subframeP)%10;
      else harq_pid = ((frameP*10)+subframeP)&7;

      round = ue_sched_ctl->round[CC_id][harq_pid];
      if (round != 8) {  // retransmission
        if(UE_list->UE_template[CC_id][UE_id].nb_rb[harq_pid] == 0){
          continue;
        }
        switch (get_tmode(module_idP, CC_id, UE_id)) {
          case 1:
          case 2:
          case 7:
            aggregation = get_aggregation(get_bw_index(module_idP, CC_id),
                  ue_sched_ctl->dl_cqi[CC_id],
                  format1);
            break;
          case 3:
            aggregation = get_aggregation(get_bw_index(module_idP,CC_id),
                  ue_sched_ctl->dl_cqi[CC_id],
                  format2A);
            break;
          default:
            LOG_W(MAC,"Unsupported transmission mode %d\n", get_tmode(module_idP,CC_id,UE_id));
            aggregation = 2;
            break;
        }
        format_flag = 1;
        if (!CCE_allocation_infeasible(module_idP,
                                      CC_id,
                                      format_flag,
                                      subframeP,
                                      aggregation,
                                      rnti)) {
          dl_config_pdu = &DL_req->dl_config_pdu_list[DL_req->number_pdu];
          dl_config_pdu->pdu_type                                     = NFAPI_DL_CONFIG_DCI_DL_PDU_TYPE;
          dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti              = rnti;
          dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti_type         = (format_flag == 0)?2:1;
          dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.aggregation_level = aggregation;
          DL_req->number_pdu++;

          nb_rbs_required[CC_id][UE_id] = UE_list->UE_template[CC_id][UE_id].nb_rb[harq_pid];
          // Insert DLSCH(retransmission) UE into selected UE list
          dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].UE_id = UE_id;
          dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].ue_priority = SCH_DL_RETRANS;
          dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].rnti = rnti;
          dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].nb_rb = nb_rbs_required[CC_id][UE_id];
          dlsch_ue_select[CC_id].ue_num++;
          if (dlsch_ue_select[CC_id].ue_num == dlsch_ue_max_num[CC_id]) {
            end_flag[CC_id] = 1;
            break;
          }
        } else {
          if (cc[CC_id].tdd_Config != NULL) { //TDD
            set_ue_dai (subframeP,
                         UE_id,
                         CC_id,
            cc[CC_id].tdd_Config->subframeAssignment,
                         UE_list);
            // update UL DAI after DLSCH scheduling
            set_ul_DAI(module_idP,UE_id,CC_id,frameP,subframeP);
          }

          add_ue_dlsch_info(module_idP,
                            CC_id,
                            UE_id,
                            subframeP,
                            S_DL_NONE);
          end_flag[CC_id] = 1;
          break;
        }
      }
    }
  }
  if(cc_id_end(end_flag) == 1){
    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
      DL_req->number_pdu = saved_dlsch_dci[CC_id];
    }
    return;
  }

  // Insert DLSCH(first transmission) UE into selected UE list (UE_id > last_dlsch_ue_id[CC_id])
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      if (mbsfn_flag[CC_id]>0) {
        continue;
      }

      DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
      for (UE_id = (last_dlsch_ue_id[CC_id]+1); UE_id <NUMBER_OF_UE_MAX; UE_id++) {
        if(end_flag[CC_id] == 1){
          break;
        }

        if (UE_list->active[UE_id] == FALSE) {
          continue;
        }

        rnti = UE_RNTI(module_idP,UE_id);
        if (rnti == NOT_A_RNTI)
          continue;

        ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
#if 0
        if (ue_sched_ctl->ul_out_of_sync == 1) {
          continue;
        }
#endif
       for(i = 0;i<dlsch_ue_select[CC_id].ue_num;i++){
          if(dlsch_ue_select[CC_id].list[i].UE_id == UE_id){
           break;
          }
        }
        if(i < dlsch_ue_select[CC_id].ue_num)
          continue;

        if (cc[CC_id].tdd_Config) harq_pid = ((frameP*10)+subframeP)%10;
        else harq_pid = ((frameP*10)+subframeP)&7;

        round = ue_sched_ctl->round[CC_id][harq_pid];
        if (round == 8) {
            if (nb_rbs_required[CC_id][UE_id] == 0) {
              continue;
            }
            switch (get_tmode(module_idP, CC_id, UE_id)) {
              case 1:
              case 2:
              case 7:
                aggregation = get_aggregation(get_bw_index(module_idP, CC_id),
                      ue_sched_ctl->dl_cqi[CC_id],
                      format1);
                break;
              case 3:
                aggregation = get_aggregation(get_bw_index(module_idP,CC_id),
                      ue_sched_ctl->dl_cqi[CC_id],
                      format2A);
                break;
              default:
                LOG_W(MAC,"Unsupported transmission mode %d\n", get_tmode(module_idP,CC_id,UE_id));
                aggregation = 2;
                break;
            }
            format_flag = 1;
            if (!CCE_allocation_infeasible(module_idP,
                                           CC_id,
                                           format_flag,
                                           subframeP,
                                           aggregation,
                                           rnti)) {
              dl_config_pdu = &DL_req->dl_config_pdu_list[DL_req->number_pdu];
              dl_config_pdu->pdu_type                                     = NFAPI_DL_CONFIG_DCI_DL_PDU_TYPE;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti              = rnti;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti_type         = (format_flag == 0)?2:1;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.aggregation_level = aggregation;
              DL_req->number_pdu++;

              // Insert DLSCH(first transmission) UE into selected selected UE list
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].ue_priority = SCH_DL_FIRST;
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].nb_rb = nb_rbs_required[CC_id][UE_id];
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].UE_id = UE_id;
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].rnti = rnti;
              dlsch_ue_select[CC_id].ue_num++;

          if (dlsch_ue_select[CC_id].ue_num == dlsch_ue_max_num[CC_id]) {
                  end_flag[CC_id] = 1;
                  break;
              }
          }else {
            if (cc[CC_id].tdd_Config != NULL) { //TDD
              set_ue_dai (subframeP,
                           UE_id,
                           CC_id,
              cc[CC_id].tdd_Config->subframeAssignment,
                           UE_list);
              // update UL DAI after DLSCH scheduling
              set_ul_DAI(module_idP,UE_id,CC_id,frameP,subframeP);
            }
           add_ue_dlsch_info(module_idP,
                            CC_id,
                            UE_id,
                            subframeP,
                            S_DL_NONE);
            end_flag[CC_id] = 1;
            break;
          }
      }
    }
  }
  if(cc_id_end(end_flag) == 1){
    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
      DL_req->number_pdu = saved_dlsch_dci[CC_id];
    }
    return;
  }

  // Insert DLSCH(first transmission) UE into selected UE list (UE_id <= last_dlsch_ue_id[CC_id])
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      if (mbsfn_flag[CC_id]>0) {
        continue;
      }

      DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
      for (UE_id = 0; UE_id <= last_dlsch_ue_id[CC_id]; UE_id++) {
        if(end_flag[CC_id] == 1){
          break;
        }

        if (UE_list->active[UE_id] == FALSE) {
          continue;
        }

        rnti = UE_RNTI(module_idP,UE_id);
        if (rnti == NOT_A_RNTI)
          continue;

        ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
#if 0
        if (ue_sched_ctl->ul_out_of_sync == 1) {
          continue;
        }
#endif
        for(i = 0;i<dlsch_ue_select[CC_id].ue_num;i++){
          if(dlsch_ue_select[CC_id].list[i].UE_id == UE_id){
           break;
          }
        }
        if(i < dlsch_ue_select[CC_id].ue_num)
          continue;

        if (cc[CC_id].tdd_Config) harq_pid = ((frameP*10)+subframeP)%10;
        else harq_pid = ((frameP*10)+subframeP)&7;

        round = ue_sched_ctl->round[CC_id][harq_pid];
        if (round == 8) {
            if (nb_rbs_required[CC_id][UE_id] == 0) {
                continue;
             }
            switch (get_tmode(module_idP, CC_id, UE_id)) {
              case 1:
              case 2:
              case 7:
                aggregation = get_aggregation(get_bw_index(module_idP, CC_id),
                      ue_sched_ctl->dl_cqi[CC_id],
                      format1);
                break;
              case 3:
                aggregation = get_aggregation(get_bw_index(module_idP,CC_id),
                      ue_sched_ctl->dl_cqi[CC_id],
                      format2A);
                break;
              default:
                LOG_W(MAC,"Unsupported transmission mode %d\n", get_tmode(module_idP,CC_id,UE_id));
                aggregation = 2;
                break;
            }
            format_flag = 1;
            if (!CCE_allocation_infeasible(module_idP,
                                           CC_id,
                                           format_flag,
                                           subframeP,
                                           aggregation,
                                           rnti)) {
              dl_config_pdu = &DL_req->dl_config_pdu_list[DL_req->number_pdu];
              dl_config_pdu->pdu_type                                     = NFAPI_DL_CONFIG_DCI_DL_PDU_TYPE;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti              = rnti;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.rnti_type         = (format_flag == 0)?2:1;
              dl_config_pdu->dci_dl_pdu.dci_dl_pdu_rel8.aggregation_level = aggregation;
              DL_req->number_pdu++;

              // Insert DLSCH(first transmission) UE into selected selected UE list
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].ue_priority = SCH_DL_FIRST;
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].nb_rb = nb_rbs_required[CC_id][UE_id];
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].UE_id = UE_id;
              dlsch_ue_select[CC_id].list[dlsch_ue_select[CC_id].ue_num].rnti = rnti;
              dlsch_ue_select[CC_id].ue_num++;

              if (dlsch_ue_select[CC_id].ue_num == dlsch_ue_max_num[CC_id]) {
                end_flag[CC_id] = 1;
                break;
              }
            } else {
              if (cc[CC_id].tdd_Config != NULL) { //TDD
                set_ue_dai (subframeP,
                            UE_id,
                            CC_id,
                            cc[CC_id].tdd_Config->subframeAssignment,
                            UE_list);
              // update UL DAI after DLSCH scheduling
                set_ul_DAI(module_idP,UE_id,CC_id,frameP,subframeP);
              }
          add_ue_dlsch_info(module_idP,
                            CC_id,
                            UE_id,
                            subframeP,
                            S_DL_NONE);
              end_flag[CC_id] = 1;
              break;
            }
      }
    }
  }

  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    DL_req          = &eNB->DL_req[CC_id].dl_config_request_body;
    DL_req->number_pdu = saved_dlsch_dci[CC_id];
  }
  return;
}



#endif


// This function assigns pre-available RBS to each UE in specified sub-bands before scheduling is done
void dlsch_scheduler_pre_processor (module_id_t   Mod_id,
                                    frame_t       frameP,
                                    sub_frame_t   subframeP,
                                    int           N_RBG[MAX_NUM_CCs],
                                    int           *mbsfn_flag)
{

#ifndef UE_EXPANSION
  unsigned char rballoc_sub[MAX_NUM_CCs][N_RBG_MAX],harq_pid=0,round=0,total_ue_count;
  uint16_t ii;
  uint16_t                nb_rbs_required_remaining_1[MAX_NUM_CCs][NUMBER_OF_UE_MAX];
  uint16_t r1=0;
//  int rrc_status           = RRC_IDLE;
#else
  unsigned char rballoc_sub[MAX_NUM_CCs][N_RBG_MAX],harq_pid=0,Round=0;
  uint16_t                temp_total_rbs_count;
  unsigned char           temp_total_ue_count;
#endif 
  unsigned char MIMO_mode_indicator[MAX_NUM_CCs][N_RBG_MAX];
  int                     UE_id, i; 
  uint16_t                j;
  uint16_t                nb_rbs_required[MAX_NUM_CCs][NUMBER_OF_UE_MAX];
  uint16_t                nb_rbs_required_remaining[MAX_NUM_CCs][NUMBER_OF_UE_MAX];
//  uint16_t                nb_rbs_required_remaining_1[MAX_NUM_CCs][NUMBER_OF_UE_MAX];
  uint16_t                average_rbs_per_user[MAX_NUM_CCs] = {0};
  rnti_t             rnti;
  int                min_rb_unit[MAX_NUM_CCs];
//  uint16_t r1=0;
  uint8_t CC_id;
  UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;

  int N_RB_DL;
  int transmission_mode = 0;
  UE_sched_ctrl *ue_sched_ctl;
  //  int rrc_status           = RRC_IDLE;
  COMMON_channels_t *cc;

#ifdef TM5
    int harq_pid1 = 0;
    int round1 = 0, round2 = 0;
    int UE_id2;
    uint16_t i1, i2, i3;
    rnti_t rnti1, rnti2;
    LTE_eNB_UE_stats *eNB_UE_stats1 = NULL;
    LTE_eNB_UE_stats *eNB_UE_stats2 = NULL;
    UE_sched_ctrl *ue_sched_ctl1, *ue_sched_ctl2;
#endif

  for (CC_id=0; CC_id<MAX_NUM_CCs; CC_id++) {

	if (mbsfn_flag[CC_id] > 0)	// If this CC is allocated for MBSFN skip it here
	    continue;



	min_rb_unit[CC_id] = get_min_rb_unit(Mod_id, CC_id);

	for (i = 0; i < NUMBER_OF_UE_MAX; i++) {
	    if (UE_list->active[i] != TRUE)
		continue;

	    UE_id = i;
	    // Initialize scheduling information for all active UEs



	    dlsch_scheduler_pre_processor_reset(Mod_id,
						UE_id,
						CC_id,
						frameP,
						subframeP,
						N_RBG[CC_id],
						nb_rbs_required,
						nb_rbs_required_remaining,
						rballoc_sub,
						MIMO_mode_indicator);

	}
    }

#if (!defined(UE_EXPANSION_SIM2)) &&(!defined(UE_EXPANSION))
    // Store the DLSCH buffer for each logical channel
    store_dlsch_buffer(Mod_id, frameP, subframeP);



    // Calculate the number of RBs required by each UE on the basis of logical channel's buffer
    assign_rbs_required(Mod_id, frameP, subframeP, nb_rbs_required,
			min_rb_unit);
#else
    memcpy(nb_rbs_required, pre_nb_rbs_required[dlsch_ue_select_tbl_in_use] , sizeof(uint16_t)*MAX_NUM_CCs*NUMBER_OF_UE_MAX);
#endif

#ifdef UE_EXPANSION
  dlsch_scheduler_pre_ue_select(Mod_id,frameP,subframeP, mbsfn_flag,nb_rbs_required,dlsch_ue_select);
#else
  // Sorts the user on the basis of dlsch logical channel buffer and CQI
  sort_UEs (Mod_id,frameP,subframeP);

  total_ue_count =0;
#endif


 //total_ue_count =0;

#ifdef UE_EXPANSION
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    average_rbs_per_user[CC_id] = 0;
    cc = &RC.mac[Mod_id]->common_channels[CC_id];
    // Get total available RBS count and total UE count
    N_RB_DL = to_prb(cc->mib->message.dl_Bandwidth);
    temp_total_rbs_count = RC.mac[Mod_id]->eNB_stats[CC_id].available_prbs;
    temp_total_ue_count = dlsch_ue_select[CC_id].ue_num;

    for (i = 0; i < dlsch_ue_select[CC_id].ue_num; i++) {
      if(dlsch_ue_select[CC_id].list[i].ue_priority == SCH_DL_MSG2){
          temp_total_ue_count--;
          continue;
      }
      if(dlsch_ue_select[CC_id].list[i].ue_priority == SCH_DL_MSG4){
          temp_total_ue_count--;
          continue;
      }
      UE_id = dlsch_ue_select[CC_id].list[i].UE_id;
      nb_rbs_required[CC_id][UE_id] = dlsch_ue_select[CC_id].list[i].nb_rb;

      average_rbs_per_user[CC_id] = (uint16_t)round((double)temp_total_rbs_count/(double)temp_total_ue_count);
      if( average_rbs_per_user[CC_id] < min_rb_unit[CC_id] ){
        temp_total_ue_count--;
        dlsch_ue_select[CC_id].ue_num--;
        i--;
        continue;
      }

      rnti = dlsch_ue_select[CC_id].list[i].rnti;

      ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
      if (cc->tdd_Config) harq_pid = ((frameP*10)+subframeP)%10;
      else harq_pid = ((frameP*10)+subframeP)&7;
      Round    = ue_sched_ctl->round[CC_id][harq_pid];

      //if (mac_eNB_get_rrc_status(Mod_id, rnti) < RRC_RECONFIGURED || round > 0) {
      if (mac_eNB_get_rrc_status(Mod_id, rnti) < RRC_RECONFIGURED || Round != 8) {  // FIXME
        nb_rbs_required_remaining[CC_id][UE_id] = dlsch_ue_select[CC_id].list[i].nb_rb;
      } else {
        nb_rbs_required_remaining[CC_id][UE_id] = cmin(average_rbs_per_user[CC_id], dlsch_ue_select[CC_id].list[i].nb_rb);
      }

      transmission_mode = get_tmode(Mod_id,CC_id,UE_id);

      LOG_T(MAC,"calling dlsch_scheduler_pre_processor_allocate .. \n ");
      dlsch_scheduler_pre_processor_allocate (Mod_id,
                                              UE_id,
                                              CC_id,
                                              N_RBG[CC_id],
                                              transmission_mode,
                                              min_rb_unit[CC_id],
                                              N_RB_DL,
                                              nb_rbs_required,
                                              nb_rbs_required_remaining,
                                              rballoc_sub,
                                              MIMO_mode_indicator);
      temp_total_rbs_count -= ue_sched_ctl->pre_nb_available_rbs[CC_id];
      temp_total_ue_count--;

      if (ue_sched_ctl->pre_nb_available_rbs[CC_id] == 0) {
        dlsch_ue_select[CC_id].ue_num = i;
        break;
      }

      if (temp_total_rbs_count == 0) {
        dlsch_ue_select[CC_id].ue_num = i+1;
        break;
      }
#ifdef TM5
      // TODO: data channel TM5: to be re-visited
#endif
    }
  }

#else
    // loop over all active UEs
    for (i = UE_list->head; i >= 0; i = UE_list->next[i]) {
	rnti = UE_RNTI(Mod_id, i);

	if (rnti == NOT_A_RNTI)
	    continue;
#if 0
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;
#endif
	UE_id = i;

	for (ii = 0; ii < UE_num_active_CC(UE_list, UE_id); ii++) {
	    CC_id = UE_list->ordered_CCids[ii][UE_id];
	    ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
	    cc = &RC.mac[Mod_id]->common_channels[ii];
	    if (cc->tdd_Config)
		harq_pid = ((frameP * 10) + subframeP) % 10;
	    else
		harq_pid = ((frameP * 10) + subframeP) & 7;
	    round = ue_sched_ctl->round[CC_id][harq_pid];

	    average_rbs_per_user[CC_id] = 0;


	    if (round != 8) {
		nb_rbs_required[CC_id][UE_id] =
		    UE_list->UE_template[CC_id][UE_id].nb_rb[harq_pid];
	    }
	    //nb_rbs_required_remaining[UE_id] = nb_rbs_required[UE_id];
	    if (nb_rbs_required[CC_id][UE_id] > 0) {
		total_ue_count = total_ue_count + 1;
	    }
	    // hypothetical assignment
	    /*
	     * If schedule is enabled and if the priority of the UEs is modified
	     * The average rbs per logical channel per user will depend on the level of
	     * priority. Concerning the hypothetical assignement, we should assign more
	     * rbs to prioritized users. Maybe, we can do a mapping between the
	     * average rbs per user and the level of priority or multiply the average rbs
	     * per user by a coefficient which represents the degree of priority.
	     */

	    N_RB_DL =
		to_prb(RC.mac[Mod_id]->common_channels[CC_id].mib->
		       message.dl_Bandwidth);

	    if (total_ue_count == 0) {
		average_rbs_per_user[CC_id] = 0;
	    } else if ((min_rb_unit[CC_id] * total_ue_count) <= (N_RB_DL)) {
		average_rbs_per_user[CC_id] =
		    (uint16_t) floor(N_RB_DL / total_ue_count);
	    } else {
		average_rbs_per_user[CC_id] = min_rb_unit[CC_id];	// consider the total number of use that can be scheduled UE
	    }
	}
    }

    // note: nb_rbs_required is assigned according to total_buffer_dl
    // extend nb_rbs_required to capture per LCID RB required
    for (i = UE_list->head; i >= 0; i = UE_list->next[i]) {
	rnti = UE_RNTI(Mod_id, i);

	if (rnti == NOT_A_RNTI)
	    continue;
#if 0
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;
#endif
	for (ii = 0; ii < UE_num_active_CC(UE_list, i); ii++) {
	    CC_id = UE_list->ordered_CCids[ii][i];
	    ue_sched_ctl = &UE_list->UE_sched_ctrl[i];
	    round = ue_sched_ctl->round[CC_id][harq_pid];

	    // control channel or retransmission
	    /* TODO: do we have to check for retransmission? */
	    if (mac_eNB_get_rrc_status(Mod_id, rnti) < RRC_RECONFIGURED
		|| round > 0) {
		nb_rbs_required_remaining_1[CC_id][i] =
		    nb_rbs_required[CC_id][i];
	    } else {
		nb_rbs_required_remaining_1[CC_id][i] =
		    cmin(average_rbs_per_user[CC_id],
			 nb_rbs_required[CC_id][i]);

	    }
	}
    }

    //Allocation to UEs is done in 2 rounds,
    // 1st stage: average number of RBs allocated to each UE
    // 2nd stage: remaining RBs are allocated to high priority UEs
    for (r1 = 0; r1 < 2; r1++) {

	for (i = UE_list->head; i >= 0; i = UE_list->next[i]) {
	    for (ii = 0; ii < UE_num_active_CC(UE_list, i); ii++) {
		CC_id = UE_list->ordered_CCids[ii][i];

		if (r1 == 0) {
		    nb_rbs_required_remaining[CC_id][i] =
			nb_rbs_required_remaining_1[CC_id][i];
		} else {	// rb required based only on the buffer - rb allloctaed in the 1st round + extra reaming rb form the 1st round
		    nb_rbs_required_remaining[CC_id][i] =
			nb_rbs_required[CC_id][i] -
			nb_rbs_required_remaining_1[CC_id][i] +
			nb_rbs_required_remaining[CC_id][i];
		    if (nb_rbs_required_remaining[CC_id][i] < 0)
			abort();
		}

		if (nb_rbs_required[CC_id][i] > 0)
		    LOG_D(MAC,
			  "round %d : nb_rbs_required_remaining[%d][%d]= %d (remaining_1 %d, required %d,  pre_nb_available_rbs %d, N_RBG %d, rb_unit %d)\n",
			  r1, CC_id, i,
			  nb_rbs_required_remaining[CC_id][i],
			  nb_rbs_required_remaining_1[CC_id][i],
			  nb_rbs_required[CC_id][i],
			  UE_list->UE_sched_ctrl[i].
			  pre_nb_available_rbs[CC_id], N_RBG[CC_id],
			  min_rb_unit[CC_id]);

	    }
	}

	if (total_ue_count > 0) {
	    for (i = UE_list->head; i >= 0; i = UE_list->next[i]) {
		UE_id = i;

		for (ii = 0; ii < UE_num_active_CC(UE_list, UE_id); ii++) {
		    CC_id = UE_list->ordered_CCids[ii][UE_id];
		    ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
		    round = ue_sched_ctl->round[CC_id][harq_pid];

		    rnti = UE_RNTI(Mod_id, UE_id);

		    // LOG_D(MAC,"UE %d rnti 0x\n", UE_id, rnti );
		    if (rnti == NOT_A_RNTI)
			continue;
#if 0
		    if (UE_list->UE_sched_ctrl[UE_id].ul_out_of_sync == 1)
			continue;
#endif
		    transmission_mode = get_tmode(Mod_id, CC_id, UE_id);
		    //          mac_xface->get_ue_active_harq_pid(Mod_id,CC_id,rnti,frameP,subframeP,&harq_pid,&round,0);
		    //rrc_status = mac_eNB_get_rrc_status(Mod_id,rnti);
		    /* 1st allocate for the retx */

		    // retransmission in data channels
		    // control channel in the 1st transmission
		    // data channel for all TM
		    LOG_T(MAC,
			  "calling dlsch_scheduler_pre_processor_allocate .. \n ");
		    dlsch_scheduler_pre_processor_allocate(Mod_id, UE_id,
							   CC_id,
							   N_RBG[CC_id],
							   transmission_mode,
							   min_rb_unit
							   [CC_id],
							   to_prb(RC.mac
								  [Mod_id]->common_channels
								  [CC_id].mib->message.dl_Bandwidth),
							   nb_rbs_required,
							   nb_rbs_required_remaining,
							   rballoc_sub,
							   MIMO_mode_indicator);

#ifdef TM5

		    // data chanel TM5: to be revisted
		    if ((round == 0) &&
			(transmission_mode == 5) &&
			(ue_sched_ctl->dl_pow_off[CC_id] != 1)) {

			for (j = 0; j < N_RBG[CC_id]; j += 2) {

			    if ((((j == (N_RBG[CC_id] - 1))
				  && (rballoc_sub[CC_id][j] == 0)
				  && (ue_sched_ctl->
				      rballoc_sub_UE[CC_id][j] == 0))
				 || ((j < (N_RBG[CC_id] - 1))
				     && (rballoc_sub[CC_id][j + 1] == 0)
				     &&
				     (ue_sched_ctl->rballoc_sub_UE
				      [CC_id][j + 1] == 0)))
				&& (nb_rbs_required_remaining[CC_id][UE_id]
				    > 0)) {

				for (ii = UE_list->next[i + 1]; ii >= 0;
				     ii = UE_list->next[ii]) {

				    UE_id2 = ii;
				    rnti2 = UE_RNTI(Mod_id, UE_id2);
				    ue_sched_ctl2 =
					&UE_list->UE_sched_ctrl[UE_id2];
				    round2 = ue_sched_ctl2->round[CC_id];
				    if (rnti2 == NOT_A_RNTI)
					continue;
#if 0				    if (UE_list->
					UE_sched_ctrl
					[UE_id2].ul_out_of_sync == 1)
					continue;
#endif
				    eNB_UE_stats2 =
					UE_list->
					eNB_UE_stats[CC_id][UE_id2];
				    //mac_xface->get_ue_active_harq_pid(Mod_id,CC_id,rnti2,frameP,subframeP,&harq_pid2,&round2,0);

				    if ((mac_eNB_get_rrc_status
					 (Mod_id,
					  rnti2) >= RRC_RECONFIGURED)
					&& (round2 == 0)
					&&
					(get_tmode(Mod_id, CC_id, UE_id2)
					 == 5)
					&& (ue_sched_ctl->
					    dl_pow_off[CC_id] != 1)) {

					if ((((j == (N_RBG[CC_id] - 1))
					      &&
					      (ue_sched_ctl->rballoc_sub_UE
					       [CC_id][j] == 0))
					     || ((j < (N_RBG[CC_id] - 1))
						 &&
						 (ue_sched_ctl->
						  rballoc_sub_UE[CC_id][j +
									1]
						  == 0)))
					    &&
					    (nb_rbs_required_remaining
					     [CC_id]
					     [UE_id2] > 0)) {

					    if ((((eNB_UE_stats2->
						   DL_pmi_single ^
						   eNB_UE_stats1->
						   DL_pmi_single)
						  << (14 - j)) & 0xc000) == 0x4000) {	//MU-MIMO only for 25 RBs configuration

						rballoc_sub[CC_id][j] = 1;
						ue_sched_ctl->
						    rballoc_sub_UE[CC_id]
						    [j] = 1;
						ue_sched_ctl2->
						    rballoc_sub_UE[CC_id]
						    [j] = 1;
						MIMO_mode_indicator[CC_id]
						    [j] = 0;

						if (j < N_RBG[CC_id] - 1) {
						    rballoc_sub[CC_id][j +
								       1] =
							1;
						    ue_sched_ctl->
							rballoc_sub_UE
							[CC_id][j + 1] = 1;
						    ue_sched_ctl2->rballoc_sub_UE
							[CC_id][j + 1] = 1;
						    MIMO_mode_indicator
							[CC_id][j + 1]
							= 0;
						}

						ue_sched_ctl->
						    dl_pow_off[CC_id]
						    = 0;
						ue_sched_ctl2->
						    dl_pow_off[CC_id]
						    = 0;


						if ((j == N_RBG[CC_id] - 1)
						    && ((N_RB_DL == 25)
							|| (N_RB_DL ==
							    50))) {

						    nb_rbs_required_remaining
							[CC_id][UE_id] =
							nb_rbs_required_remaining
							[CC_id][UE_id] -
							min_rb_unit[CC_id]
							+ 1;
						    ue_sched_ctl->pre_nb_available_rbs
							[CC_id] =
							ue_sched_ctl->pre_nb_available_rbs
							[CC_id] +
							min_rb_unit[CC_id]
							- 1;
						    nb_rbs_required_remaining
							[CC_id][UE_id2] =
							nb_rbs_required_remaining
							[CC_id][UE_id2] -
							min_rb_unit[CC_id]
							+ 1;
						    ue_sched_ctl2->pre_nb_available_rbs
							[CC_id] =
							ue_sched_ctl2->pre_nb_available_rbs
							[CC_id] +
							min_rb_unit[CC_id]
							- 1;
						} else {

						    nb_rbs_required_remaining
							[CC_id][UE_id] =
							nb_rbs_required_remaining
							[CC_id][UE_id] - 4;
						    ue_sched_ctl->pre_nb_available_rbs
							[CC_id] =
							ue_sched_ctl->pre_nb_available_rbs
							[CC_id] + 4;
						    nb_rbs_required_remaining
							[CC_id][UE_id2] =
							nb_rbs_required_remaining
							[CC_id][UE_id2] -
							4;
						    ue_sched_ctl2->pre_nb_available_rbs
							[CC_id] =
							ue_sched_ctl2->pre_nb_available_rbs
							[CC_id] + 4;
						}

						break;
					    }
					}
				    }
				}
			    }
			}
		    }
#endif
		}
	    }
	}			// total_ue_count
    }				// end of for for r1 and r2
#endif  // end of #ifndef UE_EXPANSION
#ifdef TM5

    // This has to be revisited!!!!
    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
	i1 = 0;
	i2 = 0;
	i3 = 0;

	for (j = 0; j < N_RBG[CC_id]; j++) {
	    if (MIMO_mode_indicator[CC_id][j] == 2) {
		i1 = i1 + 1;
	    } else if (MIMO_mode_indicator[CC_id][j] == 1) {
		i2 = i2 + 1;
	    } else if (MIMO_mode_indicator[CC_id][j] == 0) {
		i3 = i3 + 1;
	    }
	}

	if ((i1 < N_RBG[CC_id]) && (i2 > 0) && (i3 == 0)) {
	    PHY_vars_eNB_g[Mod_id][CC_id]->check_for_SUMIMO_transmissions =
		PHY_vars_eNB_g[Mod_id][CC_id]->
		check_for_SUMIMO_transmissions + 1;
	}

	if (i3 == N_RBG[CC_id] && i1 == 0 && i2 == 0) {
	    PHY_vars_eNB_g[Mod_id][CC_id]->FULL_MUMIMO_transmissions =
		PHY_vars_eNB_g[Mod_id][CC_id]->FULL_MUMIMO_transmissions +
		1;
	}

        if((i1 < N_RBG[CC_id]) && (i3 > 0)) {
		PHY_vars_eNB_g[Mod_id][CC_id]->
		check_for_MUMIMO_transmissions + 1;
	}

	PHY_vars_eNB_g[Mod_id][CC_id]->check_for_total_transmissions =
	    PHY_vars_eNB_g[Mod_id][CC_id]->check_for_total_transmissions +
	    1;

    }

#endif

#ifdef UE_EXPANSION
  for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
      for (i = 0; i < dlsch_ue_select[CC_id].ue_num; i++) {
        if(dlsch_ue_select[CC_id].list[i].ue_priority == SCH_DL_MSG2){
            continue;
        }
        if(dlsch_ue_select[CC_id].list[i].ue_priority == SCH_DL_MSG4){
            continue;
        }
        UE_id = dlsch_ue_select[CC_id].list[i].UE_id;
        ue_sched_ctl = &RC.mac[Mod_id]->UE_list.UE_sched_ctrl[UE_id];
#else
  for(i=UE_list->head; i>=0; i=UE_list->next[i]) {
    UE_id = i;
    ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];

    for (ii=0; ii<UE_num_active_CC(UE_list,UE_id); ii++) {
      CC_id = UE_list->ordered_CCids[ii][UE_id];
#endif
	    //PHY_vars_eNB_g[Mod_id]->mu_mimo_mode[UE_id].dl_pow_off = dl_pow_off[UE_id];

	    if (ue_sched_ctl->pre_nb_available_rbs[CC_id] > 0) {
		LOG_D(MAC,
		      "******************DL Scheduling Information for UE%d ************************\n",
		      UE_id);
		LOG_D(MAC, "dl power offset UE%d = %d \n", UE_id,
		      ue_sched_ctl->dl_pow_off[CC_id]);
		LOG_D(MAC,
		      "***********RB Alloc for every subband for UE%d ***********\n",
		      UE_id);

		for (j = 0; j < N_RBG[CC_id]; j++) {
		    //PHY_vars_eNB_g[Mod_id]->mu_mimo_mode[UE_id].rballoc_sub[i] = rballoc_sub_UE[CC_id][UE_id][i];
		    LOG_D(MAC, "RB Alloc for UE%d and Subband%d = %d\n",
			  UE_id, j,
			  ue_sched_ctl->rballoc_sub_UE[CC_id][j]);
		}

		//PHY_vars_eNB_g[Mod_id]->mu_mimo_mode[UE_id].pre_nb_available_rbs = pre_nb_available_rbs[CC_id][UE_id];
		LOG_D(MAC, "Total RBs allocated for UE%d = %d\n", UE_id,
		      ue_sched_ctl->pre_nb_available_rbs[CC_id]);
	    }
	}
    }
}

#define SF0_LIMIT 1

void
dlsch_scheduler_pre_processor_reset(int module_idP,
				    int UE_id,
				    uint8_t CC_id,
				    int frameP,
				    int subframeP,
				    int N_RBG,
				    uint16_t nb_rbs_required[MAX_NUM_CCs]
				    [NUMBER_OF_UE_MAX],
				    uint16_t
				    nb_rbs_required_remaining
				    [MAX_NUM_CCs][NUMBER_OF_UE_MAX],
				    unsigned char
				    rballoc_sub[MAX_NUM_CCs]
				    [N_RBG_MAX], unsigned char
				    MIMO_mode_indicator[MAX_NUM_CCs]
				    [N_RBG_MAX])
{
    int i, j;
    UE_list_t *UE_list = &RC.mac[module_idP]->UE_list;
    UE_sched_ctrl *ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
    rnti_t rnti = UE_RNTI(module_idP, UE_id);

    uint8_t *vrb_map = RC.mac[module_idP]->common_channels[CC_id].vrb_map;
    int N_RB_DL =
	to_prb(RC.mac[module_idP]->common_channels[CC_id].mib->
	       message.dl_Bandwidth);
    int RBGsize = N_RB_DL / N_RBG, RBGsize_last;
#ifdef SF0_LIMIT
    int sf0_upper = -1, sf0_lower = -1;
#endif

    LOG_D(MAC,"Running preprocessor for UE %d (%x)\n",UE_id,rnti);
    // initialize harq_pid and round

    if (ue_sched_ctl->ta_timer)
	ue_sched_ctl->ta_timer--;

    /*
       eNB_UE_stats *eNB_UE_stats;

       if (eNB_UE_stats == NULL)
       return;


       mac_xface->get_ue_active_harq_pid(module_idP,CC_id,rnti,
       frameP,subframeP,
       &ue_sched_ctl->harq_pid[CC_id],
       &ue_sched_ctl->round[CC_id],
       openair_harq_DL);


       if (ue_sched_ctl->ta_timer == 0) {

       // WE SHOULD PROTECT the eNB_UE_stats with a mutex here ...

       ue_sched_ctl->ta_timer = 20;  // wait 20 subframes before taking TA measurement from PHY
       switch (N_RB_DL) {
       case 6:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update;
       break;

       case 15:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update/2;
       break;

       case 25:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update/4;
       break;

       case 50:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update/8;
       break;

       case 75:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update/12;
       break;

       case 100:
       ue_sched_ctl->ta_update = eNB_UE_stats->timing_advance_update/16;
       break;
       }
       // clear the update in case PHY does not have a new measurement after timer expiry
       eNB_UE_stats->timing_advance_update =  0;
       }
       else {
       ue_sched_ctl->ta_timer--;
       ue_sched_ctl->ta_update =0; // don't trigger a timing advance command
       }


       if (UE_id==0) {
       VCD_SIGNAL_DUMPER_DUMP_VARIABLE_BY_NAME(VCD_SIGNAL_DUMPER_VARIABLES_UE0_TIMING_ADVANCE,ue_sched_ctl->ta_update);
       }
     */

    nb_rbs_required[CC_id][UE_id] = 0;
    ue_sched_ctl->pre_nb_available_rbs[CC_id] = 0;
    ue_sched_ctl->dl_pow_off[CC_id] = 2;
    nb_rbs_required_remaining[CC_id][UE_id] = 0;

    switch (N_RB_DL) {
    case 6:
	RBGsize = 1;
	RBGsize_last = 1;
	break;
    case 15:
	RBGsize = 2;
	RBGsize_last = 1;
	break;
    case 25:
	RBGsize = 2;
	RBGsize_last = 1;
	break;
    case 50:
	RBGsize = 3;
	RBGsize_last = 2;
	break;
    case 75:
	RBGsize = 4;
	RBGsize_last = 3;
	break;
    case 100:
	RBGsize = 4;
	RBGsize_last = 4;
	break;
    default:
	AssertFatal(1 == 0, "unsupported RBs (%d)\n", N_RB_DL);
    }

#ifdef SF0_LIMIT
    switch (N_RBG) {
    case 6:
	sf0_lower = 0;
	sf0_upper = 5;
	break;
    case 8:
	sf0_lower = 2;
	sf0_upper = 5;
	break;
    case 13:
	sf0_lower = 4;
	sf0_upper = 7;
	break;
    case 17:
	sf0_lower = 7;
	sf0_upper = 9;
	break;
    case 25:
	sf0_lower = 11;
	sf0_upper = 13;
	break;
    default:
	AssertFatal(1 == 0, "unsupported RBs (%d)\n", N_RB_DL);
    }
#endif
    // Initialize Subbands according to VRB map
    for (i = 0; i < N_RBG; i++) {
	int rb_size = i == N_RBG - 1 ? RBGsize_last : RBGsize;

	ue_sched_ctl->rballoc_sub_UE[CC_id][i] = 0;
	rballoc_sub[CC_id][i] = 0;
#ifdef SF0_LIMIT
	// for avoiding 6+ PRBs around DC in subframe 0 (avoid excessive errors)
	/* TODO: make it proper - allocate those RBs, do not "protect" them, but
	 * compute number of available REs and limit MCS according to the
	 * TBS table 36.213 7.1.7.2.1-1 (can be done after pre-processor)
	 */
	if (subframeP == 0 && i >= sf0_lower && i <= sf0_upper)
	    rballoc_sub[CC_id][i] = 1;
#endif
	// for SI-RNTI,RA-RNTI and P-RNTI allocations
	for (j = 0; j < rb_size; j++) {
	    if (vrb_map[j + (i * RBGsize)] != 0) {
		rballoc_sub[CC_id][i] = 1;
		LOG_D(MAC, "Frame %d, subframe %d : vrb %d allocated\n",
		      frameP, subframeP, j + (i * RBGsize));
		break;
	    }
	}
	LOG_D(MAC, "Frame %d Subframe %d CC_id %d RBG %i : rb_alloc %d\n",
	      frameP, subframeP, CC_id, i, rballoc_sub[CC_id][i]);
	MIMO_mode_indicator[CC_id][i] = 2;
    }
}


void
dlsch_scheduler_pre_processor_allocate(module_id_t Mod_id,
				       int UE_id,
				       uint8_t CC_id,
				       int N_RBG,
				       int transmission_mode,
				       int min_rb_unit,
				       uint8_t N_RB_DL,
				       uint16_t
				       nb_rbs_required[MAX_NUM_CCs]
				       [NUMBER_OF_UE_MAX],
				       uint16_t
				       nb_rbs_required_remaining
				       [MAX_NUM_CCs]
				       [NUMBER_OF_UE_MAX], unsigned char
				       rballoc_sub[MAX_NUM_CCs]
				       [N_RBG_MAX], unsigned char
				       MIMO_mode_indicator
				       [MAX_NUM_CCs][N_RBG_MAX])
{

    int i;
    UE_list_t *UE_list = &RC.mac[Mod_id]->UE_list;
    UE_sched_ctrl *ue_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];

    for (i = 0; i < N_RBG; i++) {

	if ((rballoc_sub[CC_id][i] == 0) &&
	    (ue_sched_ctl->rballoc_sub_UE[CC_id][i] == 0) &&
	    (nb_rbs_required_remaining[CC_id][UE_id] > 0) &&
	    (ue_sched_ctl->pre_nb_available_rbs[CC_id] <
	     nb_rbs_required[CC_id][UE_id])) {

	    // if this UE is not scheduled for TM5
	    if (ue_sched_ctl->dl_pow_off[CC_id] != 0) {

		if ((i == N_RBG - 1)
		    && ((N_RB_DL == 25) || (N_RB_DL == 50))) {
		    if (nb_rbs_required_remaining[CC_id][UE_id] >=
			min_rb_unit - 1) {
			rballoc_sub[CC_id][i] = 1;
			ue_sched_ctl->rballoc_sub_UE[CC_id][i] = 1;
			MIMO_mode_indicator[CC_id][i] = 1;
			if (transmission_mode == 5) {
			    ue_sched_ctl->dl_pow_off[CC_id] = 1;
			}
			nb_rbs_required_remaining[CC_id][UE_id] =
			    nb_rbs_required_remaining[CC_id][UE_id] -
			    min_rb_unit + 1;
			ue_sched_ctl->pre_nb_available_rbs[CC_id] =
			    ue_sched_ctl->pre_nb_available_rbs[CC_id] +
			    min_rb_unit - 1;
		    }
		} else {
		    if (nb_rbs_required_remaining[CC_id][UE_id] >=
			min_rb_unit) {
			rballoc_sub[CC_id][i] = 1;
			ue_sched_ctl->rballoc_sub_UE[CC_id][i] = 1;
			MIMO_mode_indicator[CC_id][i] = 1;
			if (transmission_mode == 5) {
			    ue_sched_ctl->dl_pow_off[CC_id] = 1;
			}
			nb_rbs_required_remaining[CC_id][UE_id] =
			    nb_rbs_required_remaining[CC_id][UE_id] -
			    min_rb_unit;
			ue_sched_ctl->pre_nb_available_rbs[CC_id] =
			    ue_sched_ctl->pre_nb_available_rbs[CC_id] +
			    min_rb_unit;
		    }
		}
	    }			// dl_pow_off[CC_id][UE_id] ! = 0
	}
    }
}


/// ULSCH PRE_PROCESSOR

#ifndef UE_EXPANSION
void ulsch_scheduler_pre_processor(module_id_t module_idP,
                                   int frameP,
                                   sub_frame_t subframeP,
                                   uint16_t *first_rb)
{

    int16_t i;
    uint16_t UE_id, n, r;
    uint8_t CC_id, harq_pid;
    uint16_t nb_allocated_rbs[MAX_NUM_CCs][NUMBER_OF_UE_MAX],
	total_allocated_rbs[MAX_NUM_CCs],
	average_rbs_per_user[MAX_NUM_CCs];
    int16_t total_remaining_rbs[MAX_NUM_CCs];
    uint16_t max_num_ue_to_be_scheduled = 0;
    uint16_t total_ue_count = 0;
    rnti_t rnti = -1;
    UE_list_t *UE_list = &RC.mac[module_idP]->UE_list;
    UE_TEMPLATE *UE_template = 0;
    int N_RB_DL;
    int N_RB_UL;
    LOG_D(MAC, "In ulsch_preprocessor: assign max mcs min rb\n");
    // maximize MCS and then allocate required RB according to the buffer occupancy with the limit of max available UL RB
    assign_max_mcs_min_rb(module_idP, frameP, subframeP, first_rb);

    LOG_D(MAC, "In ulsch_preprocessor: sort ue \n");
    // sort ues
    sort_ue_ul(module_idP, frameP, subframeP);


    // we need to distribute RBs among UEs
    // step1:  reset the vars
    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
	N_RB_DL =
	    to_prb(RC.mac[module_idP]->common_channels[CC_id].mib->
		   message.dl_Bandwidth);
	N_RB_UL =
	    to_prb(RC.mac[module_idP]->common_channels[CC_id].
		   ul_Bandwidth);
	total_allocated_rbs[CC_id] = 0;
	total_remaining_rbs[CC_id] = 0;
	average_rbs_per_user[CC_id] = 0;

	for (i = UE_list->head_ul; i >= 0; i = UE_list->next_ul[i]) {
	    nb_allocated_rbs[CC_id][i] = 0;
	}
    }

    LOG_D(MAC, "In ulsch_preprocessor: step2 \n");
    // step 2: calculate the average rb per UE
    total_ue_count = 0;
    max_num_ue_to_be_scheduled = 0;

    for (i = UE_list->head_ul; i >= 0; i = UE_list->next_ul[i]) {

	rnti = UE_RNTI(module_idP, i);

	if (rnti == NOT_A_RNTI)
	    continue;

	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;


	UE_id = i;

	LOG_D(MAC, "In ulsch_preprocessor: handling UE %d/%x\n", UE_id,
	      rnti);
	for (n = 0; n < UE_list->numactiveULCCs[UE_id]; n++) {
	    // This is the actual CC_id in the list
	    CC_id = UE_list->ordered_ULCCids[n][UE_id];
	    LOG_D(MAC,
		  "In ulsch_preprocessor: handling UE %d/%x CCid %d\n",
		  UE_id, rnti, CC_id);
	    UE_template = &UE_list->UE_template[CC_id][UE_id];
	    average_rbs_per_user[CC_id] = 0;

	    if (UE_template->pre_allocated_nb_rb_ul > 0) {
		total_ue_count += 1;
	    }
	    /*
	       if((mac_xface->get_nCCE_max(module_idP,CC_id,3,subframeP) - nCCE_to_be_used[CC_id])  > (1<<aggregation)) {
	       nCCE_to_be_used[CC_id] = nCCE_to_be_used[CC_id] + (1<<aggregation);
	       max_num_ue_to_be_scheduled+=1;
	       } */

	    max_num_ue_to_be_scheduled += 1;

	    if (total_ue_count == 0) {
		average_rbs_per_user[CC_id] = 0;
	    } else if (total_ue_count == 1) {	// increase the available RBs, special case,
		average_rbs_per_user[CC_id] =
		    N_RB_UL - first_rb[CC_id] + 1;
	    } else if ((total_ue_count <= (N_RB_DL - first_rb[CC_id]))
		       && (total_ue_count <= max_num_ue_to_be_scheduled)) {
		average_rbs_per_user[CC_id] =
		    (uint16_t) floor((N_RB_UL - first_rb[CC_id]) /
				     total_ue_count);
	    } else if (max_num_ue_to_be_scheduled > 0) {
		average_rbs_per_user[CC_id] =
		    (uint16_t) floor((N_RB_UL - first_rb[CC_id]) /
				     max_num_ue_to_be_scheduled);
	    } else {
		average_rbs_per_user[CC_id] = 1;
		LOG_W(MAC,
		      "[eNB %d] frame %d subframe %d: UE %d CC %d: can't get average rb per user (should not be here)\n",
		      module_idP, frameP, subframeP, UE_id, CC_id);
	    }
	}
    }
    if (total_ue_count > 0)
	LOG_D(MAC,
	      "[eNB %d] Frame %d subframe %d: total ue to be scheduled %d/%d\n",
	      module_idP, frameP, subframeP, total_ue_count,
	      max_num_ue_to_be_scheduled);

    //LOG_D(MAC,"step3\n");

    // step 3: assigne RBS
    for (i = UE_list->head_ul; i >= 0; i = UE_list->next_ul[i]) {
	rnti = UE_RNTI(module_idP, i);

	if (rnti == NOT_A_RNTI)
	    continue;
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;

	UE_id = i;

	for (n = 0; n < UE_list->numactiveULCCs[UE_id]; n++) {
	    // This is the actual CC_id in the list
	    CC_id = UE_list->ordered_ULCCids[n][UE_id];
	    harq_pid =
		subframe2harqpid(&RC.mac[module_idP]->
				 common_channels[CC_id], frameP,
				 subframeP);


	    //      mac_xface->get_ue_active_harq_pid(module_idP,CC_id,rnti,frameP,subframeP,&harq_pid,&round,openair_harq_UL);

	    if (UE_list->UE_sched_ctrl[UE_id].round_UL[CC_id] > 0) {
		nb_allocated_rbs[CC_id][UE_id] =
		    UE_list->UE_template[CC_id][UE_id].nb_rb_ul[harq_pid];
	    } else {
		nb_allocated_rbs[CC_id][UE_id] =
		    cmin(UE_list->
			 UE_template[CC_id][UE_id].pre_allocated_nb_rb_ul,
			 average_rbs_per_user[CC_id]);
	    }

	    total_allocated_rbs[CC_id] += nb_allocated_rbs[CC_id][UE_id];
	    LOG_D(MAC,
		  "In ulsch_preprocessor: assigning %d RBs for UE %d/%x CCid %d, harq_pid %d\n",
		  nb_allocated_rbs[CC_id][UE_id], UE_id, rnti, CC_id,
		  harq_pid);
	}
    }

    // step 4: assigne the remaining RBs and set the pre_allocated rbs accordingly
    for (r = 0; r < 2; r++) {

	for (i = UE_list->head_ul; i >= 0; i = UE_list->next_ul[i]) {
	    rnti = UE_RNTI(module_idP, i);

	    if (rnti == NOT_A_RNTI)
		continue;
	    if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
		continue;
	    UE_id = i;

	    for (n = 0; n < UE_list->numactiveULCCs[UE_id]; n++) {
		// This is the actual CC_id in the list
		CC_id = UE_list->ordered_ULCCids[n][UE_id];
		UE_template = &UE_list->UE_template[CC_id][UE_id];
		total_remaining_rbs[CC_id] =
		    N_RB_UL - first_rb[CC_id] - total_allocated_rbs[CC_id];

		if (total_ue_count == 1) {
		    total_remaining_rbs[CC_id] += 1;
		}

		if (r == 0) {
		    while ((UE_template->pre_allocated_nb_rb_ul > 0) &&
			   (nb_allocated_rbs[CC_id][UE_id] <
			    UE_template->pre_allocated_nb_rb_ul)
			   && (total_remaining_rbs[CC_id] > 0)) {
			nb_allocated_rbs[CC_id][UE_id] =
			    cmin(nb_allocated_rbs[CC_id][UE_id] + 1,
				 UE_template->pre_allocated_nb_rb_ul);
			total_remaining_rbs[CC_id]--;
			total_allocated_rbs[CC_id]++;
		    }
		} else {
		    UE_template->pre_allocated_nb_rb_ul =
			nb_allocated_rbs[CC_id][UE_id];
		    LOG_D(MAC,
			  "******************UL Scheduling Information for UE%d CC_id %d ************************\n",
			  UE_id, CC_id);
		    LOG_D(MAC,
			  "[eNB %d] total RB allocated for UE%d CC_id %d  = %d\n",
			  module_idP, UE_id, CC_id,
			  UE_template->pre_allocated_nb_rb_ul);
		}
	    }
	}
    }

    for (CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {

	if (total_allocated_rbs[CC_id] > 0) {
	    LOG_D(MAC, "[eNB %d] total RB allocated for all UEs = %d/%d\n",
		  module_idP, total_allocated_rbs[CC_id],
		  N_RB_UL - first_rb[CC_id]);
	}
    }
}
#endif

void
assign_max_mcs_min_rb(module_id_t module_idP, int frameP,
		      sub_frame_t subframeP, uint16_t * first_rb)
{

    int i;
    uint16_t n, UE_id;
    uint8_t CC_id;
    rnti_t rnti = -1;
    int mcs;
    int rb_table_index = 0, tbs, tx_power;
    eNB_MAC_INST *eNB = RC.mac[module_idP];
    UE_list_t *UE_list = &eNB->UE_list;

    UE_TEMPLATE *UE_template;
    int Ncp;
    int N_RB_UL;

    for (i = 0; i < NUMBER_OF_UE_MAX; i++) {
	if (UE_list->active[i] != TRUE)
	    continue;

	rnti = UE_RNTI(module_idP, i);

	if (rnti == NOT_A_RNTI)
	    continue;
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;

	if (UE_list->UE_sched_ctrl[i].phr_received == 1)
	    mcs = 20;		// if we've received the power headroom information the UE, we can go to maximum mcs
	else
	    mcs = 10;		// otherwise, limit to QPSK PUSCH

	UE_id = i;

	for (n = 0; n < UE_list->numactiveULCCs[UE_id]; n++) {
	    // This is the actual CC_id in the list
	    CC_id = UE_list->ordered_ULCCids[n][UE_id];

	    if (CC_id >= MAX_NUM_CCs) {
		LOG_E(MAC,
		      "CC_id %u should be < %u, loop n=%u < numactiveULCCs[%u]=%u",
		      CC_id, MAX_NUM_CCs, n, UE_id,
		      UE_list->numactiveULCCs[UE_id]);
	    }

	    AssertFatal(CC_id < MAX_NUM_CCs,
			"CC_id %u should be < %u, loop n=%u < numactiveULCCs[%u]=%u",
			CC_id, MAX_NUM_CCs, n, UE_id,
			UE_list->numactiveULCCs[UE_id]);

	    UE_template = &UE_list->UE_template[CC_id][UE_id];

	    Ncp = RC.mac[module_idP]->common_channels[CC_id].Ncp;
	    N_RB_UL =
		to_prb(RC.mac[module_idP]->common_channels[CC_id].
		       ul_Bandwidth);
	    // if this UE has UL traffic
	    if (UE_template->ul_total_buffer > 0) {


		tbs = get_TBS_UL(mcs, 3) << 3;	// 1 or 2 PRB with cqi enabled does not work well!
		rb_table_index = 2;

		// fixme: set use_srs flag
		tx_power =
		    estimate_ue_tx_power(tbs, rb_table[rb_table_index], 0,
					 Ncp, 0);

		while ((((UE_template->phr_info - tx_power) < 0)
			|| (tbs > UE_template->ul_total_buffer))
		       && (mcs > 3)) {
		    // LOG_I(MAC,"UE_template->phr_info %d tx_power %d mcs %d\n", UE_template->phr_info,tx_power, mcs);
		    mcs--;
		    tbs = get_TBS_UL(mcs, rb_table[rb_table_index]) << 3;
		    tx_power = estimate_ue_tx_power(tbs, rb_table[rb_table_index], 0, Ncp, 0);	// fixme: set use_srs
		}

		while ((tbs < UE_template->ul_total_buffer) &&
		       (rb_table[rb_table_index] <
			(N_RB_UL - first_rb[CC_id]))
		       && ((UE_template->phr_info - tx_power) > 0)
		       && (rb_table_index < 32)) {

		    rb_table_index++;
		    tbs = get_TBS_UL(mcs, rb_table[rb_table_index]) << 3;
		    tx_power =
			estimate_ue_tx_power(tbs, rb_table[rb_table_index],
					     0, Ncp, 0);
		}

		UE_template->ue_tx_power = tx_power;

		if (rb_table[rb_table_index] >
		    (N_RB_UL - first_rb[CC_id] - 1)) {
		    rb_table_index--;
		}
		// 1 or 2 PRB with cqi enabled does not work well
		if (rb_table[rb_table_index] < 3) {
		    rb_table_index = 2;	//3PRB
		}

		UE_template->pre_assigned_mcs_ul = mcs;
		UE_template->pre_allocated_rb_table_index_ul =
		    rb_table_index;
		UE_template->pre_allocated_nb_rb_ul =
		    rb_table[rb_table_index];
		LOG_D(MAC,
		      "[eNB %d] frame %d subframe %d: for UE %d CC %d: pre-assigned mcs %d, pre-allocated rb_table[%d]=%d RBs (phr %d, tx power %d)\n",
		      module_idP, frameP, subframeP, UE_id, CC_id,
		      UE_template->pre_assigned_mcs_ul,
		      UE_template->pre_allocated_rb_table_index_ul,
		      UE_template->pre_allocated_nb_rb_ul,
		      UE_template->phr_info, tx_power);
	    } else {
		/* if UE has pending scheduling request then pre-allocate 3 RBs */
		//if (UE_template->ul_active == 1 && UE_template->ul_SR == 1) {
		if (UE_is_to_be_scheduled(module_idP, CC_id, i)) {
		    /* use QPSK mcs */
		    UE_template->pre_assigned_mcs_ul = 10;
		    UE_template->pre_allocated_rb_table_index_ul = 2;
		    UE_template->pre_allocated_nb_rb_ul = 3;
		} else {
		    UE_template->pre_assigned_mcs_ul = 0;
		    UE_template->pre_allocated_rb_table_index_ul = -1;
		    UE_template->pre_allocated_nb_rb_ul = 0;
		}
	    }
	}
    }
}

struct sort_ue_ul_params {
    int module_idP;
    int frameP;
    int subframeP;
};

static int ue_ul_compare(const void *_a, const void *_b, void *_params)
{
    struct sort_ue_ul_params *params = _params;
    UE_list_t *UE_list = &RC.mac[params->module_idP]->UE_list;

    int UE_id1 = *(const int *) _a;
    int UE_id2 = *(const int *) _b;

    int rnti1 = UE_RNTI(params->module_idP, UE_id1);
    int pCCid1 = UE_PCCID(params->module_idP, UE_id1);
    int round1 = maxround(params->module_idP, rnti1, params->frameP,
			  params->subframeP, 1);

    int rnti2 = UE_RNTI(params->module_idP, UE_id2);
    int pCCid2 = UE_PCCID(params->module_idP, UE_id2);
    int round2 = maxround(params->module_idP, rnti2, params->frameP,
			  params->subframeP, 1);

    if (round1 > round2)
	return -1;
    if (round1 < round2)
	return 1;

    if (UE_list->UE_template[pCCid1][UE_id1].ul_buffer_info[LCGID0] >
	UE_list->UE_template[pCCid2][UE_id2].ul_buffer_info[LCGID0])
	return -1;
    if (UE_list->UE_template[pCCid1][UE_id1].ul_buffer_info[LCGID0] <
	UE_list->UE_template[pCCid2][UE_id2].ul_buffer_info[LCGID0])
	return 1;

    if (UE_list->UE_template[pCCid1][UE_id1].ul_total_buffer >
	UE_list->UE_template[pCCid2][UE_id2].ul_total_buffer)
	return -1;
    if (UE_list->UE_template[pCCid1][UE_id1].ul_total_buffer <
	UE_list->UE_template[pCCid2][UE_id2].ul_total_buffer)
	return 1;

    if (UE_list->UE_template[pCCid1][UE_id1].pre_assigned_mcs_ul >
	UE_list->UE_template[pCCid2][UE_id2].pre_assigned_mcs_ul)
	return -1;
    if (UE_list->UE_template[pCCid1][UE_id1].pre_assigned_mcs_ul <
	UE_list->UE_template[pCCid2][UE_id2].pre_assigned_mcs_ul)
	return 1;

    return 0;

#if 0
    /* The above order derives from the following.
     * The last case is not handled: "if (UE_list->UE_template[pCCid2][UE_id2].ul_total_buffer > 0 )"
     * I don't think it makes a big difference.
     */
    if (round2 > round1) {
	swap_UEs(UE_list, UE_id1, UE_id2, 1);
    } else if (round2 == round1) {
	if (UE_list->UE_template[pCCid1][UE_id1].ul_buffer_info[LCGID0] <
	    UE_list->UE_template[pCCid2][UE_id2].ul_buffer_info[LCGID0]) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 1);
	} else if (UE_list->UE_template[pCCid1][UE_id1].ul_total_buffer <
		   UE_list->UE_template[pCCid2][UE_id2].ul_total_buffer) {
	    swap_UEs(UE_list, UE_id1, UE_id2, 1);
	} else if (UE_list->UE_template[pCCid1][UE_id1].
		   pre_assigned_mcs_ul <
		   UE_list->UE_template[pCCid2][UE_id2].
		   pre_assigned_mcs_ul) {
	    if (UE_list->UE_template[pCCid2][UE_id2].ul_total_buffer > 0) {
		swap_UEs(UE_list, UE_id1, UE_id2, 1);
	    }
	}
    }
#endif
}

void sort_ue_ul(module_id_t module_idP, int frameP, sub_frame_t subframeP)
{
    int i;
    int list[NUMBER_OF_UE_MAX];
    int list_size = 0;
    int rnti;
    struct sort_ue_ul_params params = { module_idP, frameP, subframeP };

    UE_list_t *UE_list = &RC.mac[module_idP]->UE_list;

    for (i = 0; i < NUMBER_OF_UE_MAX; i++) {
	if (UE_list->active[i] == FALSE)
	    continue;
	if ((rnti = UE_RNTI(module_idP, i)) == NOT_A_RNTI)
	    continue;
	if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
	    continue;

	list[list_size] = i;
	list_size++;
    }

    qsort_r(list, list_size, sizeof(int), ue_ul_compare, &params);

    if (list_size) {
	for (i = 0; i < list_size - 1; i++)
	    UE_list->next_ul[list[i]] = list[i + 1];
	UE_list->next_ul[list[list_size - 1]] = -1;
	UE_list->head_ul = list[0];
    } else {
	UE_list->head_ul = -1;
    }

#if 0
    int UE_id1, UE_id2;
    int pCCid1, pCCid2;
    int round1, round2;
    int i = 0, ii = 0;
    rnti_t rnti1, rnti2;

    UE_list_t *UE_list = &RC.mac[module_idP]->UE_list;

    for (i = UE_list->head_ul; i >= 0; i = UE_list->next_ul[i]) {

	//LOG_I(MAC,"sort ue ul i %d\n",i);
	for (ii = UE_list->next_ul[i]; ii >= 0; ii = UE_list->next_ul[ii]) {
	    //LOG_I(MAC,"sort ul ue 2 ii %d\n",ii);

	    UE_id1 = i;
	    rnti1 = UE_RNTI(module_idP, UE_id1);

	    if (rnti1 == NOT_A_RNTI)
		continue;
	    if (UE_list->UE_sched_ctrl[i].ul_out_of_sync == 1)
		continue;


	    pCCid1 = UE_PCCID(module_idP, UE_id1);
	    round1 = maxround(module_idP, rnti1, frameP, subframeP, 1);

	    UE_id2 = ii;
	    rnti2 = UE_RNTI(module_idP, UE_id2);

	    if (rnti2 == NOT_A_RNTI)
		continue;
	    if (UE_list->UE_sched_ctrl[UE_id2].ul_out_of_sync == 1)
		continue;

	    pCCid2 = UE_PCCID(module_idP, UE_id2);
	    round2 = maxround(module_idP, rnti2, frameP, subframeP, 1);

	    if (round2 > round1) {
		swap_UEs(UE_list, UE_id1, UE_id2, 1);
	    } else if (round2 == round1) {
		if (UE_list->
		    UE_template[pCCid1][UE_id1].ul_buffer_info[LCGID0] <
		    UE_list->UE_template[pCCid2][UE_id2].
		    ul_buffer_info[LCGID0]) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 1);
		} else if (UE_list->UE_template[pCCid1][UE_id1].
			   ul_total_buffer <
			   UE_list->UE_template[pCCid2][UE_id2].
			   ul_total_buffer) {
		    swap_UEs(UE_list, UE_id1, UE_id2, 1);
		} else if (UE_list->
			   UE_template[pCCid1][UE_id1].pre_assigned_mcs_ul
			   <
			   UE_list->
			   UE_template[pCCid2][UE_id2].pre_assigned_mcs_ul)
		{
		    if (UE_list->UE_template[pCCid2][UE_id2].
			ul_total_buffer > 0) {
			swap_UEs(UE_list, UE_id1, UE_id2, 1);
		    }
		}
	    }
	}
    }
#endif
}
#ifdef UE_EXPANSION
void ulsch_scheduler_pre_ue_select(
    module_id_t       module_idP,
    frame_t           frameP,
    sub_frame_t       subframeP,
    ULSCH_UE_SELECT   ulsch_ue_select[MAX_NUM_CCs])
{
  eNB_MAC_INST *eNB=RC.mac[module_idP];
  COMMON_channels_t *cc;
  int CC_id,UE_id;
  int ret;
  uint16_t i;
  uint8_t ue_first_num[MAX_NUM_CCs];
  uint8_t first_ue_total[MAX_NUM_CCs][20];
  uint8_t first_ue_id[MAX_NUM_CCs][20];
  uint8_t ul_inactivity_num[MAX_NUM_CCs];
  uint8_t ul_inactivity_id[MAX_NUM_CCs][20];
//  LTE_DL_FRAME_PARMS *frame_parms;
  uint8_t ulsch_ue_max_num[MAX_NUM_CCs];
  uint16_t saved_ulsch_dci[MAX_NUM_CCs];
  rnti_t rnti;
  UE_sched_ctrl *UE_sched_ctl = NULL;
  uint8_t cc_id_flag[MAX_NUM_CCs];
  uint8_t harq_pid = 0,round = 0;
  UE_list_t *UE_list= &eNB->UE_list;


  uint8_t                        aggregation = 2;
  int                            format_flag;
  nfapi_hi_dci0_request_body_t   *HI_DCI0_req;
  nfapi_hi_dci0_request_pdu_t    *hi_dci0_pdu;


  for ( CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++ ) {
      //save ulsch dci number
      saved_ulsch_dci[CC_id] = eNB->HI_DCI0_req[CC_id].hi_dci0_request_body.number_of_dci;
      // maximum multiplicity number
      ulsch_ue_max_num[CC_id] =RC.rrc[module_idP]->configuration.ue_multiple_max[CC_id];

      cc_id_flag[CC_id] = 0;
      ue_first_num[CC_id] = 0;
      ul_inactivity_num[CC_id] = 0;

  }
  // UE round >0
  for ( UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++ ) {
      if (UE_list->active[UE_id] == FALSE)
          continue;

      rnti = UE_RNTI(module_idP,UE_id);
      if (rnti ==NOT_A_RNTI)
        continue;

      CC_id = UE_PCCID(module_idP,UE_id);
      if (UE_list->UE_template[CC_id][UE_id].configured == FALSE)
        continue;

      if (UE_list->UE_sched_ctrl[UE_id].ul_out_of_sync == 1)
        continue;

      // UL DCI
      HI_DCI0_req   = &eNB->HI_DCI0_req[CC_id].hi_dci0_request_body;
      if ( (ulsch_ue_select[CC_id].ue_num >= ulsch_ue_max_num[CC_id]) || (cc_id_flag[CC_id] == 1) ) {
        cc_id_flag[CC_id] = 1;
        HI_DCI0_req->number_of_dci = saved_ulsch_dci[CC_id];
        ret = cc_id_end(cc_id_flag);
        if ( ret == 0 ) {
            continue;
        }
        if ( ret == 1 ) {
            return;
        }
      }

      cc = &eNB->common_channels[CC_id];
      //harq_pid
      harq_pid = subframe2harqpid(cc,(frameP+(subframeP>=6 ? 1 : 0)),((subframeP+4)%10));
      //round
      round = UE_list->UE_sched_ctrl[UE_id].round_UL[CC_id][harq_pid];

      if ( round > 0 ) {
          hi_dci0_pdu   = &HI_DCI0_req->hi_dci0_pdu_list[HI_DCI0_req->number_of_dci+HI_DCI0_req->number_of_hi];
          format_flag = 2;
          if (CCE_allocation_infeasible(module_idP,CC_id,format_flag,subframeP,aggregation,rnti) == 1) {
              cc_id_flag[CC_id] = 1;
              continue;
          } else {
              hi_dci0_pdu->pdu_type                               = NFAPI_HI_DCI0_DCI_PDU_TYPE;
              hi_dci0_pdu->dci_pdu.dci_pdu_rel8.rnti              = rnti;
              hi_dci0_pdu->dci_pdu.dci_pdu_rel8.aggregation_level = aggregation;
              HI_DCI0_req->number_of_dci++;

              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ue_priority = SCH_UL_RETRANS;
              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].start_rb = eNB->UE_list.UE_template[CC_id][UE_id].first_rb_ul[harq_pid];
              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].nb_rb = eNB->UE_list.UE_template[CC_id][UE_id].nb_rb_ul[harq_pid];
              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].UE_id = UE_id;
              ulsch_ue_select[CC_id].ue_num++;
              continue;
          }
      }
      //
      if ( UE_id > last_ulsch_ue_id[CC_id] && ((ulsch_ue_select[CC_id].ue_num+ue_first_num[CC_id]) < ulsch_ue_max_num[CC_id]) ) {
        if ( UE_list->UE_template[CC_id][UE_id].ul_total_buffer > 0 ) {
          first_ue_id[CC_id][ue_first_num[CC_id]]= UE_id;
          first_ue_total[CC_id][ue_first_num[CC_id]] = UE_list->UE_template[CC_id][UE_id].ul_total_buffer;
          ue_first_num[CC_id]++;
          continue;
        }
        if ( UE_list->UE_template[CC_id][UE_id].ul_SR > 0 ) {
          first_ue_id[CC_id][ue_first_num[CC_id]]= UE_id;
          first_ue_total[CC_id] [ue_first_num[CC_id]] = 0;
          ue_first_num[CC_id]++;
          continue;
        }
        if ( (ulsch_ue_select[CC_id].ue_num+ul_inactivity_num[CC_id] ) < ulsch_ue_max_num[CC_id] ) {
            UE_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
            if ( ((UE_sched_ctl->ul_inactivity_timer>20)&&(UE_sched_ctl->ul_scheduled==0))  ||
              ((UE_sched_ctl->ul_inactivity_timer>10)&&(UE_sched_ctl->ul_scheduled==0)&&(mac_eNB_get_rrc_status(module_idP,UE_RNTI(module_idP,UE_id)) < RRC_CONNECTED))) {
            ul_inactivity_id[CC_id][ul_inactivity_num[CC_id]]= UE_id;
            ul_inactivity_num[CC_id] ++;
            continue;
          }
        }
      }

  }

  for ( CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++ ) {
    HI_DCI0_req   = &eNB->HI_DCI0_req[CC_id].hi_dci0_request_body;
    for ( int temp = 0; temp < ue_first_num[CC_id]; temp++ ) {
      if ( (ulsch_ue_select[CC_id].ue_num >= ulsch_ue_max_num[CC_id]) || (cc_id_flag[CC_id] == 1) ) {
        cc_id_flag[CC_id] = 1;
        HI_DCI0_req->number_of_dci = saved_ulsch_dci[CC_id];
        break;
      }

      hi_dci0_pdu   = &HI_DCI0_req->hi_dci0_pdu_list[HI_DCI0_req->number_of_dci+HI_DCI0_req->number_of_hi];
      format_flag = 2;
      rnti = UE_RNTI(module_idP,first_ue_id[CC_id][temp]);
      if (CCE_allocation_infeasible(module_idP,CC_id,format_flag,subframeP,aggregation,rnti) == 1) {
    	  cc_id_flag[CC_id] = 1;
          break;
      } else {
          hi_dci0_pdu->pdu_type                               = NFAPI_HI_DCI0_DCI_PDU_TYPE;
          hi_dci0_pdu->dci_pdu.dci_pdu_rel8.rnti              = rnti;
          hi_dci0_pdu->dci_pdu.dci_pdu_rel8.aggregation_level = aggregation;
          HI_DCI0_req->number_of_dci++;

          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ue_priority = SCH_UL_FIRST;
          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ul_total_buffer = first_ue_total[CC_id][temp];
          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].UE_id = first_ue_id[CC_id][temp];
          ulsch_ue_select[CC_id].ue_num++;
      }
    }
  }

  for ( UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++ ) {
    if (UE_list->active[UE_id] == FALSE)
        continue;

    rnti = UE_RNTI(module_idP,UE_id);
    if (rnti ==NOT_A_RNTI)
        continue;

    CC_id = UE_PCCID(module_idP,UE_id);

    if (UE_id > last_ulsch_ue_id[CC_id])
        continue;

    if (UE_list->UE_template[CC_id][UE_id].configured == FALSE)
        continue;

    if (UE_list->UE_sched_ctrl[UE_id].ul_out_of_sync == 1)
        continue;

    if ( (ulsch_ue_select[CC_id].ue_num >= ulsch_ue_max_num[CC_id]) || (cc_id_flag[CC_id] == 1) ) {
        cc_id_flag[CC_id] = 1;
        HI_DCI0_req->number_of_dci = saved_ulsch_dci[CC_id];
        ret = cc_id_end(cc_id_flag);
        if ( ret == 0 ) {
            continue;
        }
        if ( ret == 1 ) {
            return;
        }
    }

    for(i = 0;i<ulsch_ue_select[CC_id].ue_num;i++){
      if(ulsch_ue_select[CC_id].list[i].UE_id == UE_id){
       break;
      }
    }
    if(i < ulsch_ue_select[CC_id].ue_num)
      continue;

    HI_DCI0_req   = &eNB->HI_DCI0_req[CC_id].hi_dci0_request_body;
    //SR BSR
    if ( (UE_list->UE_template[CC_id][UE_id].ul_total_buffer > 0) || (UE_list->UE_template[CC_id][UE_id].ul_SR > 0) ) {
        hi_dci0_pdu   = &HI_DCI0_req->hi_dci0_pdu_list[HI_DCI0_req->number_of_dci+HI_DCI0_req->number_of_hi];
        format_flag = 2;
        if (CCE_allocation_infeasible(module_idP,CC_id,format_flag,subframeP,aggregation,rnti) == 1) {
            cc_id_flag[CC_id] = 1;
            continue;
        } else {
              hi_dci0_pdu->pdu_type                               = NFAPI_HI_DCI0_DCI_PDU_TYPE;
              hi_dci0_pdu->dci_pdu.dci_pdu_rel8.rnti              = rnti;
              hi_dci0_pdu->dci_pdu.dci_pdu_rel8.aggregation_level = aggregation;
              HI_DCI0_req->number_of_dci++;

              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ue_priority = SCH_UL_FIRST;
              if(UE_list->UE_template[CC_id][UE_id].ul_total_buffer > 0)
                  ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ul_total_buffer = UE_list->UE_template[CC_id][UE_id].ul_total_buffer;
              else if(UE_list->UE_template[CC_id][UE_id].ul_SR > 0)
                  ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ul_total_buffer = 0;
              ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].UE_id = UE_id;
              ulsch_ue_select[CC_id].ue_num++;
              continue;
          }
    }
    //inactivity UE
    if ( (ulsch_ue_select[CC_id].ue_num+ul_inactivity_num[CC_id]) < ulsch_ue_max_num[CC_id] ) {
        UE_sched_ctl = &UE_list->UE_sched_ctrl[UE_id];
        if ( ((UE_sched_ctl->ul_inactivity_timer>20)&&(UE_sched_ctl->ul_scheduled==0))  ||
            ((UE_sched_ctl->ul_inactivity_timer>10)&&(UE_sched_ctl->ul_scheduled==0)&&(mac_eNB_get_rrc_status(module_idP,UE_RNTI(module_idP,UE_id)) < RRC_CONNECTED))) {
          ul_inactivity_id[CC_id][ul_inactivity_num[CC_id]]= UE_id;
          ul_inactivity_num[CC_id]++;
          continue;
        }
    }
  }

  for ( CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++ ) {
    HI_DCI0_req   = &eNB->HI_DCI0_req[CC_id].hi_dci0_request_body;
    for ( int temp = 0; temp < ul_inactivity_num[CC_id]; temp++ ) {
      if ( (ulsch_ue_select[CC_id].ue_num >= ulsch_ue_max_num[CC_id]) || (cc_id_flag[CC_id] == 1) ) {
        HI_DCI0_req   = &eNB->HI_DCI0_req[CC_id].hi_dci0_request_body;
        cc_id_flag[CC_id] = 1;
        break;
      }

      hi_dci0_pdu   = &HI_DCI0_req->hi_dci0_pdu_list[HI_DCI0_req->number_of_dci+HI_DCI0_req->number_of_hi];
      format_flag = 2;
      rnti = UE_RNTI(module_idP,ul_inactivity_id[CC_id][temp]);
      if (CCE_allocation_infeasible(module_idP,CC_id,format_flag,subframeP,aggregation,rnti) == 1) {
          cc_id_flag[CC_id] = 1;
          continue;
      } else {
          hi_dci0_pdu->pdu_type                               = NFAPI_HI_DCI0_DCI_PDU_TYPE;
          hi_dci0_pdu->dci_pdu.dci_pdu_rel8.rnti              = rnti;
          hi_dci0_pdu->dci_pdu.dci_pdu_rel8.aggregation_level = aggregation;
          HI_DCI0_req->number_of_dci++;

          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ue_priority = SCH_UL_INACTIVE;
          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].ul_total_buffer = 0;
          ulsch_ue_select[CC_id].list[ulsch_ue_select[CC_id].ue_num].UE_id = ul_inactivity_id[CC_id][temp];
          ulsch_ue_select[CC_id].ue_num++;
      }
    }
    HI_DCI0_req->number_of_dci = saved_ulsch_dci[CC_id];
  }
  return;
}

uint8_t find_rb_table_index(uint8_t average_rbs)
{
  int i;
  for ( i = 0; i < 34; i++ ) {
    if ( rb_table[i] > average_rbs ) {
      return (i-1);
    }
  }
  return i;
}

void ulsch_scheduler_pre_processor(module_id_t module_idP,
                                   frame_t frameP,
                                   sub_frame_t subframeP,
                                   ULSCH_UE_SELECT ulsch_ue_select[MAX_NUM_CCs])
{
  int                CC_id,ulsch_ue_num;
  eNB_MAC_INST       *eNB = RC.mac[module_idP];
  UE_list_t          *UE_list= &eNB->UE_list;
  UE_TEMPLATE        *UE_template = NULL;
  LTE_DL_FRAME_PARMS *frame_parms = NULL;
  uint8_t            ue_num_temp;
  uint8_t            total_rbs=0;
  uint8_t            average_rbs;
  uint16_t           first_rb[MAX_NUM_CCs];
  uint8_t            mcs;
  uint8_t            rb_table_index;
  uint32_t           tbs;
  int16_t            tx_power;
  int                UE_id;
  rnti_t             rnti;
  LOG_D(MAC,"In ulsch_preprocessor: ulsch ue select\n");
  //ue select
  ulsch_scheduler_pre_ue_select(module_idP,frameP,subframeP,ulsch_ue_select);

  // MCS and RB assgin
  for ( CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++ ) {
    frame_parms = &(RC.eNB[module_idP][CC_id]->frame_parms);
    if(frame_parms->N_RB_UL == 25){
      first_rb[CC_id] = 1;
    }else{
      first_rb[CC_id] = 2;
    } 
    ue_num_temp       = ulsch_ue_select[CC_id].ue_num;
    for ( ulsch_ue_num = 0; ulsch_ue_num < ulsch_ue_select[CC_id].ue_num; ulsch_ue_num++ ) {

      UE_id = ulsch_ue_select[CC_id].list[ulsch_ue_num].UE_id;

      if (ulsch_ue_select[CC_id].list[ulsch_ue_num].ue_priority == SCH_UL_MSG3) {
        first_rb[CC_id] ++;
        ue_num_temp--;
        continue;
      }

      if (ulsch_ue_select[CC_id].list[ulsch_ue_num].ue_priority == SCH_UL_PRACH) {
        first_rb[CC_id] = ulsch_ue_select[CC_id].list[ulsch_ue_num].start_rb+ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb;
        ue_num_temp--;
        continue;
      }

      rnti = UE_RNTI(CC_id,UE_id);
      if(frame_parms->N_RB_UL == 25){
        if ( first_rb[CC_id] >= frame_parms->N_RB_UL-1 ){
            LOG_W(MAC,"[eNB %d] frame %d subframe %d, UE %d/%x CC %d: dropping, not enough RBs\n",
                   module_idP,frameP,subframeP,UE_id,rnti,CC_id);
          break;
        }
        // calculate the average rb ( remain UE)
        total_rbs = frame_parms->N_RB_UL-1-first_rb[CC_id];
      }else{
        if ( first_rb[CC_id] >= frame_parms->N_RB_UL-2 ){
            LOG_W(MAC,"[eNB %d] frame %d subframe %d, UE %d/%x CC %d: dropping, not enough RBs\n",
                   module_idP,frameP,subframeP,UE_id,rnti,CC_id);
          break;
        }
        // calculate the average rb ( remain UE)
        total_rbs = frame_parms->N_RB_UL-2-first_rb[CC_id];
      }
      average_rbs = (int)round((double)total_rbs/(double)ue_num_temp);
      if ( average_rbs < 3 ) {
        ue_num_temp--;
        ulsch_ue_num--;
        ulsch_ue_select[CC_id].ue_num--;
        continue;
      }
      if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].ue_priority == SCH_UL_RETRANS ) {
        if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb <= average_rbs ) {
            // assigne RBS(nb_rb)
            ulsch_ue_select[CC_id].list[ulsch_ue_num].start_rb = first_rb[CC_id];
            first_rb[CC_id] = first_rb[CC_id] + ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb;
        }
        if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb > average_rbs ) {
          if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb <= total_rbs ) {
              // assigne RBS(average_rbs)
              ulsch_ue_select[CC_id].list[ulsch_ue_num].start_rb = first_rb[CC_id];
              first_rb[CC_id] = first_rb[CC_id] + ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb;
          } else {
              // assigne RBS(remain rbs)
              ulsch_ue_select[CC_id].list[ulsch_ue_num].start_rb = first_rb[CC_id];
              rb_table_index = 2;
              while(rb_table[rb_table_index] <= total_rbs){
                rb_table_index++;
              }
              ulsch_ue_select[CC_id].list[ulsch_ue_num].nb_rb = rb_table[rb_table_index-1];
              first_rb[CC_id] = first_rb[CC_id] + rb_table[rb_table_index-1];
          }
        }
      }else{
        UE_template = &UE_list->UE_template[CC_id][UE_id];
        if ( UE_list->UE_sched_ctrl[UE_id].phr_received == 1 ) {
          mcs = 20;
        } else {
          mcs = 10;
        }
        if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].ue_priority  == SCH_UL_FIRST ) {
          if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].ul_total_buffer > 0 ) {
            rb_table_index = 2;
            tbs = get_TBS_UL(mcs,rb_table[rb_table_index])<<3;
            tx_power= estimate_ue_tx_power(tbs,rb_table[rb_table_index],0,frame_parms->Ncp,0);

            while ( (((UE_template->phr_info - tx_power) < 0 ) || (tbs > UE_template->ul_total_buffer)) && (mcs > 3) ) {
              mcs--;
              tbs = get_TBS_UL(mcs,rb_table[rb_table_index])<<3;
              tx_power= estimate_ue_tx_power(tbs,rb_table[rb_table_index],0,frame_parms->Ncp,0);
            }

            if(frame_parms->N_RB_UL == 25){
              while ( (tbs < UE_template->ul_total_buffer) && (rb_table[rb_table_index]<(frame_parms->N_RB_UL-1-first_rb[CC_id])) &&
                     ((UE_template->phr_info - tx_power) > 0) && (rb_table_index < 32 )) {
                rb_table_index++;
                tbs = get_TBS_UL(mcs,rb_table[rb_table_index])<<3;
                tx_power= estimate_ue_tx_power(tbs,rb_table[rb_table_index],0,frame_parms->Ncp,0);
              }
            }else{
                while ( (tbs < UE_template->ul_total_buffer) && (rb_table[rb_table_index]<(frame_parms->N_RB_UL-2-first_rb[CC_id])) &&
                       ((UE_template->phr_info - tx_power) > 0) && (rb_table_index < 32 )) {
                  rb_table_index++;
                  tbs = get_TBS_UL(mcs,rb_table[rb_table_index])<<3;
                  tx_power= estimate_ue_tx_power(tbs,rb_table[rb_table_index],0,frame_parms->Ncp,0);
                }
            }
            if ( rb_table[rb_table_index]<3 ) {
              rb_table_index=2;
            }
            if ( rb_table[rb_table_index] <= average_rbs ) {
              // assigne RBS( nb_rb)
              first_rb[CC_id] = first_rb[CC_id] + rb_table[rb_table_index];
              UE_list->UE_template[CC_id][UE_id].pre_allocated_nb_rb_ul = rb_table[rb_table_index];
              UE_list->UE_template[CC_id][UE_id].pre_allocated_rb_table_index_ul = rb_table_index;
              UE_list->UE_template[CC_id][UE_id].pre_assigned_mcs_ul = mcs;
            }
            if ( rb_table[rb_table_index] > average_rbs ) {
              // assigne RBS(average_rbs)
              rb_table_index = find_rb_table_index(average_rbs);
              if (rb_table_index>=34){
                  LOG_W(MAC,"[eNB %d] frame %d subframe %d, UE %d/%x CC %d: average RBs %d > 100\n",
                         module_idP,frameP,subframeP,UE_id,rnti,CC_id,average_rbs);
                  break;
              }
              first_rb[CC_id] = first_rb[CC_id] + rb_table[rb_table_index];
              UE_list->UE_template[CC_id][UE_id].pre_allocated_nb_rb_ul = rb_table[rb_table_index];
              UE_list->UE_template[CC_id][UE_id].pre_allocated_rb_table_index_ul = rb_table_index;
              UE_list->UE_template[CC_id][UE_id].pre_assigned_mcs_ul = mcs;
            }
          }else {
            // assigne RBS( 3 RBs)
            first_rb[CC_id] = first_rb[CC_id] + 3;
            UE_list->UE_template[CC_id][UE_id].pre_allocated_nb_rb_ul = 3;
            UE_list->UE_template[CC_id][UE_id].pre_allocated_rb_table_index_ul = 2;
            UE_list->UE_template[CC_id][UE_id].pre_assigned_mcs_ul = 10;
          }
        }else if ( ulsch_ue_select[CC_id].list[ulsch_ue_num].ue_priority  == SCH_UL_INACTIVE ) {
          // assigne RBS( 3 RBs)
          first_rb[CC_id] = first_rb[CC_id] + 3;
          UE_list->UE_template[CC_id][UE_id].pre_allocated_nb_rb_ul = 3;
          UE_list->UE_template[CC_id][UE_id].pre_allocated_rb_table_index_ul = 2;
          UE_list->UE_template[CC_id][UE_id].pre_assigned_mcs_ul = 10;
        }
      }
      ue_num_temp--;
    }
  }
}
#endif
