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

/* \file config_ue.c
 * \brief UE and eNB configuration performed by RRC or as a consequence of RRC procedures
 * \author R. Knopp, K.H. HSU
 * \date 2018
 * \version 0.1
 * \company Eurecom / NTUST
 * \email: knopp@eurecom.fr, kai-hsiang.hsu@eurecom.fr
 * \note
 * \warning
 */

#define _GNU_SOURCE

//#include "mac_defs.h"
#include <NR_MAC_gNB/mac_proto.h>
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC-CellGroupConfig.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "common/utils/nr/nr_common.h"
#include "executables/softmodem-common.h"
#include "SCHED_NR/phy_frame_config_nr.h"
#include "oai_asn1.h"

const long logicalChannelGroup0_NR = 0;
typedef struct NR_LogicalChannelConfig__ul_SpecificParameters LcConfig_UlParamas_t;

const LcConfig_UlParamas_t NR_LCSRB1 = {
    .priority = 1,
    .prioritisedBitRate = NR_LogicalChannelConfig__ul_SpecificParameters__prioritisedBitRate_infinity,
    .logicalChannelGroup = (long *)&logicalChannelGroup0_NR};

const LcConfig_UlParamas_t NR_LCSRB2 = {
    .priority = 3,
    .prioritisedBitRate = NR_LogicalChannelConfig__ul_SpecificParameters__prioritisedBitRate_infinity,
    .logicalChannelGroup = (long *)&logicalChannelGroup0_NR};

const LcConfig_UlParamas_t NR_LCSRB3 = {
    .priority = 1,
    .prioritisedBitRate = NR_LogicalChannelConfig__ul_SpecificParameters__prioritisedBitRate_infinity,
    .logicalChannelGroup = (long *)&logicalChannelGroup0_NR};

// these are the default values for SRB configurations(SRB1 and SRB2) as mentioned in 36.331 pg 258-259
const NR_LogicalChannelConfig_t NR_SRB1_logicalChannelConfig_defaultValue = {.ul_SpecificParameters =
                                                                                 (LcConfig_UlParamas_t *)&NR_LCSRB1};
const NR_LogicalChannelConfig_t NR_SRB2_logicalChannelConfig_defaultValue = {.ul_SpecificParameters =
                                                                                 (LcConfig_UlParamas_t *)&NR_LCSRB2};
const NR_LogicalChannelConfig_t NR_SRB3_logicalChannelConfig_defaultValue = {.ul_SpecificParameters =
                                                                                 (LcConfig_UlParamas_t *)&NR_LCSRB3};

void set_tdd_config_nr_ue(fapi_nr_tdd_table_t *tdd_table,
                          int mu,
                          NR_TDD_UL_DL_Pattern_t *pattern)
{
  const int nrofDownlinkSlots = pattern->nrofDownlinkSlots;
  const int nrofDownlinkSymbols = pattern->nrofDownlinkSymbols;
  const int nrofUplinkSlots = pattern->nrofUplinkSlots;
  const int nrofUplinkSymbols = pattern->nrofUplinkSymbols;
  const int nb_periods_per_frame = get_nb_periods_per_frame(pattern->dl_UL_TransmissionPeriodicity);
  const int nb_slots_per_period = ((1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME) / nb_periods_per_frame;
  tdd_table->tdd_period_in_slots = nb_slots_per_period;

  if ((nrofDownlinkSymbols + nrofUplinkSymbols) == 0)
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  else {
    AssertFatal(nrofDownlinkSymbols + nrofUplinkSymbols < 14,"illegal symbol configuration DL %d, UL %d\n",nrofDownlinkSymbols,nrofUplinkSymbols);
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots + 1),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nrofMixed slots 1, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  }

  tdd_table->max_tdd_periodicity_list = (fapi_nr_max_tdd_periodicity_t *) malloc(nb_slots_per_period * sizeof(fapi_nr_max_tdd_periodicity_t));

  for(int memory_alloc = 0 ; memory_alloc < nb_slots_per_period; memory_alloc++)
    tdd_table->max_tdd_periodicity_list[memory_alloc].max_num_of_symbol_per_slot_list =
      (fapi_nr_max_num_of_symbol_per_slot_t *) malloc(NR_NUMBER_OF_SYMBOLS_PER_SLOT*sizeof(fapi_nr_max_num_of_symbol_per_slot_t));

  int slot_number = 0;
  while(slot_number != nb_slots_per_period) {
    if(nrofDownlinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofDownlinkSlots * NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol % NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config = 0;
        if((number_of_symbol + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }

    if (nrofDownlinkSymbols != 0 || nrofUplinkSymbols != 0) {
      for(int number_of_symbol = 0; number_of_symbol < nrofDownlinkSymbols; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 0;
      }
      for(int number_of_symbol = nrofDownlinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofUplinkSymbols; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 2;
      }
      for(int number_of_symbol = NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofUplinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 1;
      }
      slot_number++;
    }

    if(nrofUplinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofUplinkSlots * NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol%NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config = 1;
        if((number_of_symbol + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }
  }
}

void config_common_ue_sa(NR_UE_MAC_INST_t *mac,
                         NR_ServingCellConfigCommonSIB_t *scc,
		         module_id_t module_id,
		         int cc_idP)
{
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;
  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;

  LOG_D(MAC, "Entering SA UE Config Common\n");

  // carrier config
  NR_FrequencyInfoDL_SIB_t *frequencyInfoDL = &scc->downlinkConfigCommon.frequencyInfoDL;
  AssertFatal(frequencyInfoDL->frequencyBandList.list.array[0]->freqBandIndicatorNR,
              "Field mandatory present for DL in SIB1\n");
  mac->nr_band = *frequencyInfoDL->frequencyBandList.list.array[0]->freqBandIndicatorNR;
  int bw_index = get_supported_band_index(frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                          mac->nr_band,
                                          frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.dl_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

  uint64_t dl_bw_khz = (12 * frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth) *
                       (15 << frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing);
  cfg->carrier_config.dl_frequency = (downlink_frequency[cc_idP][0]/1000) - (dl_bw_khz>>1);

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.dl_grid_size[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.dl_k0[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
    }
    else {
      cfg->carrier_config.dl_grid_size[i] = 0;
      cfg->carrier_config.dl_k0[i] = 0;
    }
  }

  NR_FrequencyInfoUL_SIB_t *frequencyInfoUL = &scc->uplinkConfigCommon->frequencyInfoUL;
  mac->p_Max = frequencyInfoUL->p_Max ?
               *frequencyInfoUL->p_Max :
               INT_MIN;
  bw_index = get_supported_band_index(frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                      mac->nr_band,
                                      frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.uplink_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

  if (frequencyInfoUL->absoluteFrequencyPointA == NULL)
    cfg->carrier_config.uplink_frequency = cfg->carrier_config.dl_frequency;
  else
    // TODO check if corresponds to what reported in SIB1
    cfg->carrier_config.uplink_frequency = (downlink_frequency[cc_idP][0]/1000) + uplink_frequency_offset[cc_idP][0];

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.ul_grid_size[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.ul_k0[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
    }
    else {
      cfg->carrier_config.ul_grid_size[i] = 0;
      cfg->carrier_config.ul_k0[i] = 0;
    }
  }

  mac->frame_type = get_frame_type(mac->nr_band, get_softmodem_params()->numerology);
  // cell config
  cfg->cell_config.phy_cell_id = mac->physCellId;
  cfg->cell_config.frame_duplex_type = mac->frame_type;

  // SSB config
  cfg->ssb_config.ss_pbch_power = scc->ss_PBCH_BlockPower;
  cfg->ssb_config.scs_common = get_softmodem_params()->numerology;

  // SSB Table config
  cfg->ssb_table.ssb_offset_point_a = frequencyInfoDL->offsetToPointA;
  cfg->ssb_table.ssb_period = scc->ssb_PeriodicityServingCell;
  cfg->ssb_table.ssb_subcarrier_offset = mac->ssb_subcarrier_offset;

  if (mac->frequency_range == FR1){
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = ((uint32_t) scc->ssb_PositionsInBurst.inOneGroup.buf[0]) << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
  }
  else{
    for (int i=0; i<8; i++){
      if ((scc->ssb_PositionsInBurst.groupPresence->buf[0]>>(7-i))&0x01)
        cfg->ssb_table.ssb_mask_list[i>>2].ssb_mask |= scc->ssb_PositionsInBurst.inOneGroup.buf[0]<<(24-8*(i%4));
    }
  }

  // TDD Table Configuration
  if (cfg->cell_config.frame_duplex_type == TDD){
    set_tdd_config_nr_ue(&cfg->tdd_table_1,
                         cfg->ssb_config.scs_common,
                         &mac->tdd_UL_DL_ConfigurationCommon->pattern1);
    if (mac->tdd_UL_DL_ConfigurationCommon->pattern2) {
      cfg->tdd_table_2 = (fapi_nr_tdd_table_t *) malloc(sizeof(fapi_nr_tdd_table_t));
      set_tdd_config_nr_ue(cfg->tdd_table_2,
                           cfg->ssb_config.scs_common,
                           mac->tdd_UL_DL_ConfigurationCommon->pattern2);
    }
  }

  // PRACH configuration

  uint8_t nb_preambles = 64;
  NR_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP.rach_ConfigCommon->choice.setup;
  if(rach_ConfigCommon->totalNumberOfRA_Preambles != NULL)
     nb_preambles = *rach_ConfigCommon->totalNumberOfRA_Preambles;

  cfg->prach_config.prach_sequence_length = rach_ConfigCommon->prach_RootSequenceIndex.present-1;

  if (rach_ConfigCommon->msg1_SubcarrierSpacing)
    cfg->prach_config.prach_sub_c_spacing = *rach_ConfigCommon->msg1_SubcarrierSpacing;
  else {
    // If absent, the UE applies the SCS as derived from the prach-ConfigurationIndex (for 839)
    int config_index = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
    const int64_t *prach_config_info_p = get_prach_config_info(mac->frequency_range, config_index, mac->frame_type);
    int format = prach_config_info_p[0];
    cfg->prach_config.prach_sub_c_spacing = format == 3 ? 5 : 4;
  }

  cfg->prach_config.restricted_set_config = rach_ConfigCommon->restrictedSetConfig;

  switch (rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM) {
    case 0 :
      cfg->prach_config.num_prach_fd_occasions = 1;
      break;
    case 1 :
      cfg->prach_config.num_prach_fd_occasions = 2;
      break;
    case 2 :
      cfg->prach_config.num_prach_fd_occasions = 4;
      break;
    case 3 :
      cfg->prach_config.num_prach_fd_occasions = 8;
      break;
    default:
      AssertFatal(1==0,"msg1 FDM identifier %ld undefined (0,1,2,3) \n", rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM);
  }

  cfg->prach_config.num_prach_fd_occasions_list = (fapi_nr_num_prach_fd_occasions_t *) malloc(cfg->prach_config.num_prach_fd_occasions*sizeof(fapi_nr_num_prach_fd_occasions_t));
  for (int i=0; i<cfg->prach_config.num_prach_fd_occasions; i++) {
    fapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &cfg->prach_config.num_prach_fd_occasions_list[i];
    prach_fd_occasion->num_prach_fd_occasions = i;
    if (cfg->prach_config.prach_sequence_length)
      prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l139;
    else
      prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l839;
    prach_fd_occasion->k1 = NRRIV2PRBOFFSET(scc->uplinkConfigCommon->initialUplinkBWP.genericParameters.locationAndBandwidth, MAX_BWP_SIZE) +
                                            rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart +
                                            (get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing, frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing ) * i);
    prach_fd_occasion->prach_zero_corr_conf = rach_ConfigCommon->rach_ConfigGeneric.zeroCorrelationZoneConfig;
    prach_fd_occasion->num_root_sequences = compute_nr_root_seq(rach_ConfigCommon,
                                                                nb_preambles, mac->frame_type, mac->frequency_range);
    //prach_fd_occasion->num_unused_root_sequences = ???
  }
  cfg->prach_config.ssb_per_rach = rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present-1;

}

void config_common_ue(NR_UE_MAC_INST_t *mac,
                      NR_ServingCellConfigCommon_t *scc,
		      module_id_t module_id,
		      int cc_idP)
{
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;

  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;
  
  // carrier config
  LOG_D(MAC, "Entering UE Config Common\n");

  AssertFatal(scc->downlinkConfigCommon,
              "Not expecting downlinkConfigCommon to be NULL here\n");

  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  if (frequencyInfoDL) { // NeedM for inter-freq handover
    mac->nr_band = *frequencyInfoDL->frequencyBandList.list.array[0];
    mac->frame_type = get_frame_type(mac->nr_band, get_softmodem_params()->numerology);
    mac->frequency_range = mac->nr_band < 256 ? FR1 : FR2;

    int bw_index = get_supported_band_index(frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                            mac->nr_band,
                                            frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
    cfg->carrier_config.dl_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);
    
    cfg->carrier_config.dl_frequency = from_nrarfcn(mac->nr_band,
                                                    *scc->ssbSubcarrierSpacing,
                                                    frequencyInfoDL->absoluteFrequencyPointA)/1000; // freq in kHz
    
    for (int i = 0; i < 5; i++) {
      if (i == frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
        cfg->carrier_config.dl_grid_size[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
        cfg->carrier_config.dl_k0[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      }
      else {
        cfg->carrier_config.dl_grid_size[i] = 0;
        cfg->carrier_config.dl_k0[i] = 0;
      }
    }
  }

  if (scc->uplinkConfigCommon && scc->uplinkConfigCommon->frequencyInfoUL) {
    NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
    mac->p_Max = frequencyInfoUL->p_Max ?
                 *frequencyInfoUL->p_Max :
                 INT_MIN;

    int bw_index = get_supported_band_index(frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                            *frequencyInfoUL->frequencyBandList->list.array[0],
                                            frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
    cfg->carrier_config.uplink_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

    long *UL_pointA = NULL;
    if (frequencyInfoUL->absoluteFrequencyPointA)
      UL_pointA = frequencyInfoUL->absoluteFrequencyPointA;
    else if (frequencyInfoDL)
      UL_pointA = &frequencyInfoDL->absoluteFrequencyPointA;

    if(UL_pointA)
      cfg->carrier_config.uplink_frequency = from_nrarfcn(*frequencyInfoUL->frequencyBandList->list.array[0],
                                                          *scc->ssbSubcarrierSpacing,
                                                          *UL_pointA) / 1000; // freq in kHz

    for (int i = 0; i < 5; i++) {
      if (i == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
        cfg->carrier_config.ul_grid_size[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
        cfg->carrier_config.ul_k0[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      }
      else {
        cfg->carrier_config.ul_grid_size[i] = 0;
        cfg->carrier_config.ul_k0[i] = 0;
      }
    }
  }

  // cell config
  cfg->cell_config.phy_cell_id = *scc->physCellId;
  cfg->cell_config.frame_duplex_type = mac->frame_type;

  // SSB config
  cfg->ssb_config.ss_pbch_power = scc->ss_PBCH_BlockPower;
  cfg->ssb_config.scs_common = *scc->ssbSubcarrierSpacing;

  // SSB Table config
  if (frequencyInfoDL && frequencyInfoDL->absoluteFrequencySSB) {
    int scs_scaling = 1<<(cfg->ssb_config.scs_common);
    if (frequencyInfoDL->absoluteFrequencyPointA < 600000)
      scs_scaling = scs_scaling*3;
    if (frequencyInfoDL->absoluteFrequencyPointA > 2016666)
      scs_scaling = scs_scaling>>2;
    uint32_t absolute_diff = (*frequencyInfoDL->absoluteFrequencySSB - frequencyInfoDL->absoluteFrequencyPointA);
    cfg->ssb_table.ssb_offset_point_a = absolute_diff/(12*scs_scaling) - 10;
    cfg->ssb_table.ssb_period = *scc->ssb_periodicityServingCell;
    // NSA -> take ssb offset from SCS
    cfg->ssb_table.ssb_subcarrier_offset = absolute_diff%(12*scs_scaling);
  }

  switch (scc->ssb_PositionsInBurst->present) {
  case 1 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    break;
  case 2 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = ((uint32_t) scc->ssb_PositionsInBurst->choice.mediumBitmap.buf[0]) << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    break;
  case 3 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = 0;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    for (int i = 0; i < 4; i++) {
      cfg->ssb_table.ssb_mask_list[0].ssb_mask += (uint32_t) scc->ssb_PositionsInBurst->choice.longBitmap.buf[3 - i] << i * 8;
      cfg->ssb_table.ssb_mask_list[1].ssb_mask += (uint32_t) scc->ssb_PositionsInBurst->choice.longBitmap.buf[7 - i] << i * 8;
    }
    break;
  default:
    AssertFatal(1==0,"SSB bitmap size value %d undefined (allowed values 1,2,3) \n", scc->ssb_PositionsInBurst->present);
  }

  // TDD Table Configuration
  if (cfg->cell_config.frame_duplex_type == TDD){
    set_tdd_config_nr_ue(&cfg->tdd_table_1,
                         cfg->ssb_config.scs_common,
                         &mac->tdd_UL_DL_ConfigurationCommon->pattern1);
    if (mac->tdd_UL_DL_ConfigurationCommon->pattern2) {
      cfg->tdd_table_2 = (fapi_nr_tdd_table_t *) malloc(sizeof(fapi_nr_tdd_table_t));
      set_tdd_config_nr_ue(cfg->tdd_table_2,
                           cfg->ssb_config.scs_common,
                           mac->tdd_UL_DL_ConfigurationCommon->pattern2);
    }
  }

  // PRACH configuration
  uint8_t nb_preambles = 64;
  if (scc->uplinkConfigCommon &&
      scc->uplinkConfigCommon->initialUplinkBWP &&
      scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon) { // all NeedM

    NR_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup;
    if(rach_ConfigCommon->totalNumberOfRA_Preambles != NULL)
      nb_preambles = *rach_ConfigCommon->totalNumberOfRA_Preambles;

    cfg->prach_config.prach_sequence_length = rach_ConfigCommon->prach_RootSequenceIndex.present-1;

    if (rach_ConfigCommon->msg1_SubcarrierSpacing)
      cfg->prach_config.prach_sub_c_spacing = *rach_ConfigCommon->msg1_SubcarrierSpacing;
    else {
      // If absent, the UE applies the SCS as derived from the prach-ConfigurationIndex (for 839)
      int config_index = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
      const int64_t *prach_config_info_p = get_prach_config_info(mac->frequency_range, config_index, mac->frame_type);
      int format = prach_config_info_p[0];
      cfg->prach_config.prach_sub_c_spacing = format == 3 ? 5 : 4;
    }

    cfg->prach_config.restricted_set_config = rach_ConfigCommon->restrictedSetConfig;

    switch (rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM) {
      case 0 :
        cfg->prach_config.num_prach_fd_occasions = 1;
        break;
      case 1 :
        cfg->prach_config.num_prach_fd_occasions = 2;
        break;
      case 2 :
        cfg->prach_config.num_prach_fd_occasions = 4;
        break;
      case 3 :
        cfg->prach_config.num_prach_fd_occasions = 8;
        break;
      default:
        AssertFatal(1==0,"msg1 FDM identifier %ld undefined (0,1,2,3) \n", rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM);
    }

    cfg->prach_config.num_prach_fd_occasions_list = (fapi_nr_num_prach_fd_occasions_t *) malloc(cfg->prach_config.num_prach_fd_occasions*sizeof(fapi_nr_num_prach_fd_occasions_t));
    for (int i = 0; i < cfg->prach_config.num_prach_fd_occasions; i++) {
      fapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &cfg->prach_config.num_prach_fd_occasions_list[i];
      prach_fd_occasion->num_prach_fd_occasions = i;
      if (cfg->prach_config.prach_sequence_length)
        prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l139;
      else
        prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l839;

      prach_fd_occasion->k1 = rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart;
      prach_fd_occasion->prach_zero_corr_conf = rach_ConfigCommon->rach_ConfigGeneric.zeroCorrelationZoneConfig;
      prach_fd_occasion->num_root_sequences = compute_nr_root_seq(rach_ConfigCommon,
                                                                  nb_preambles,
                                                                  mac->frame_type,
                                                                  mac->frequency_range);

      cfg->prach_config.ssb_per_rach = rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present-1;
      //prach_fd_occasion->num_unused_root_sequences = ???
    }
  }
}

void release_common_ss_cset(NR_BWP_PDCCH_t *pdcch)
{
  if (pdcch->otherSI_SS) {
    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->otherSI_SS);
    pdcch->otherSI_SS = NULL;
  }
  if (pdcch->ra_SS) {
    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->ra_SS);
    pdcch->otherSI_SS = NULL;
  }
  if (pdcch->paging_SS) {
    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->paging_SS);
    pdcch->paging_SS = NULL;
  }
  if (pdcch->commonControlResourceSet) {
    ASN_STRUCT_FREE(asn_DEF_NR_ControlResourceSet,
                    pdcch->commonControlResourceSet);
    pdcch->commonControlResourceSet = NULL;
  }
}

void modlist_ss(NR_SearchSpace_t *source, NR_SearchSpace_t *target)
{
  target->searchSpaceId = source->searchSpaceId;
  if (source->controlResourceSetId)
    updateMACie(target->controlResourceSetId,
                source->controlResourceSetId,
                NR_ControlResourceSetId_t);
  if (source->monitoringSlotPeriodicityAndOffset)
    updateMACie(target->monitoringSlotPeriodicityAndOffset,
                source->monitoringSlotPeriodicityAndOffset,
                struct NR_SearchSpace__monitoringSlotPeriodicityAndOffset);
  updateMACie(target->duration,
              source->duration,
              long);
  if (source->monitoringSymbolsWithinSlot)
    updateMACie(target->monitoringSymbolsWithinSlot,
                source->monitoringSymbolsWithinSlot,
                BIT_STRING_t);
  if (source->nrofCandidates)
    updateMACie(target->nrofCandidates,
                source->nrofCandidates,
                struct NR_SearchSpace__nrofCandidates);
  if (source->searchSpaceType)
    updateMACie(target->searchSpaceType,
                source->searchSpaceType,
                struct NR_SearchSpace__searchSpaceType);
}

NR_SearchSpace_t *get_common_search_space(const struct NR_PDCCH_ConfigCommon__commonSearchSpaceList *commonSearchSpaceList,
                                          const NR_BWP_PDCCH_t *pdcch,
                                          const NR_SearchSpaceId_t ss_id)
{
  if (ss_id == 0)
    return pdcch->search_space_zero;

  NR_SearchSpace_t *css = NULL;
  for (int i = 0; i < commonSearchSpaceList->list.count; i++) {
    if (commonSearchSpaceList->list.array[i]->searchSpaceId == ss_id) {
      css = calloc(1, sizeof(*css));
      modlist_ss(commonSearchSpaceList->list.array[i], css);
      break;
    }
  }
  AssertFatal(css, "Couldn't find CSS with Id %ld\n", ss_id);
  return css;
}

void configure_common_ss_coreset(NR_BWP_PDCCH_t *pdcch,
                                 NR_PDCCH_ConfigCommon_t *pdcch_ConfigCommon)
{
  if (pdcch_ConfigCommon) {
    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->otherSI_SS);
    pdcch->otherSI_SS = NULL;
    if (pdcch_ConfigCommon->searchSpaceOtherSystemInformation)
      pdcch->otherSI_SS = get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList,
                                                  pdcch,
                                                  *pdcch_ConfigCommon->searchSpaceOtherSystemInformation);

    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->ra_SS);
    pdcch->ra_SS = NULL;
    if (pdcch_ConfigCommon->ra_SearchSpace) {
      if (pdcch->otherSI_SS &&
          *pdcch_ConfigCommon->ra_SearchSpace == pdcch->otherSI_SS->searchSpaceId)
        pdcch->ra_SS = pdcch->otherSI_SS;
      else
        pdcch->ra_SS = get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList,
                                               pdcch,
                                               *pdcch_ConfigCommon->ra_SearchSpace);
    }

    ASN_STRUCT_FREE(asn_DEF_NR_SearchSpace,
                    pdcch->paging_SS);
    pdcch->paging_SS = NULL;
    if (pdcch_ConfigCommon->pagingSearchSpace) {
      if (pdcch->otherSI_SS &&
          *pdcch_ConfigCommon->pagingSearchSpace == pdcch->otherSI_SS->searchSpaceId)
        pdcch->paging_SS = pdcch->otherSI_SS;
      else if (pdcch->ra_SS &&
               *pdcch_ConfigCommon->pagingSearchSpace == pdcch->ra_SS->searchSpaceId)
        pdcch->paging_SS = pdcch->ra_SS;
      if (!pdcch->paging_SS)
        pdcch->paging_SS = get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList, pdcch,
                                                   *pdcch_ConfigCommon->pagingSearchSpace);
    }

    updateMACie(pdcch->commonControlResourceSet,
                pdcch_ConfigCommon->commonControlResourceSet,
                NR_ControlResourceSet_t);
  }
}

void modlist_coreset(NR_ControlResourceSet_t *source, NR_ControlResourceSet_t *target)
{
  target->controlResourceSetId = source->controlResourceSetId;
  target->frequencyDomainResources.size = source->frequencyDomainResources.size;
  target->frequencyDomainResources.buf = calloc(target->frequencyDomainResources.size,
                                                sizeof(*target->frequencyDomainResources.buf));
  for (int i = 0; i < source->frequencyDomainResources.size; i++)
    target->frequencyDomainResources.buf[i] = source->frequencyDomainResources.buf[i];
  target->duration = source->duration;
  target->precoderGranularity = source->precoderGranularity;
  long *shiftIndex = NULL;
  if (target->cce_REG_MappingType.present == NR_ControlResourceSet__cce_REG_MappingType_PR_interleaved)
    shiftIndex = target->cce_REG_MappingType.choice.interleaved->shiftIndex;
  if (source->cce_REG_MappingType.present == NR_ControlResourceSet__cce_REG_MappingType_PR_interleaved) {
    target->cce_REG_MappingType.present = NR_ControlResourceSet__cce_REG_MappingType_PR_interleaved;
    target->cce_REG_MappingType.choice.interleaved->reg_BundleSize = source->cce_REG_MappingType.choice.interleaved->reg_BundleSize;
    target->cce_REG_MappingType.choice.interleaved->interleaverSize = source->cce_REG_MappingType.choice.interleaved->interleaverSize;
    updateMACie(target->cce_REG_MappingType.choice.interleaved->shiftIndex,
                source->cce_REG_MappingType.choice.interleaved->shiftIndex,
                long);
  }
  else {
    if (shiftIndex)
      free(shiftIndex);
    target->cce_REG_MappingType = source->cce_REG_MappingType;
  }
  updateMACie(target->tci_PresentInDCI,
              source->tci_PresentInDCI,
              long);
  updateMACie(target->pdcch_DMRS_ScramblingID,
              source->pdcch_DMRS_ScramblingID,
              long);
  // TCI States
  if (source->tci_StatesPDCCH_ToReleaseList) {
    for (int i = 0; i < source->tci_StatesPDCCH_ToReleaseList->list.count; i++) {
      long id = *source->tci_StatesPDCCH_ToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->tci_StatesPDCCH_ToAddList->list.count; j++) {
        if(id == *target->tci_StatesPDCCH_ToAddList->list.array[j])
          break;
      }
      if (j < target->tci_StatesPDCCH_ToAddList->list.count)
        asn_sequence_del(&target->tci_StatesPDCCH_ToAddList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  if (source->tci_StatesPDCCH_ToAddList) {
    if (target->tci_StatesPDCCH_ToAddList)
      for (int i = 0; source->tci_StatesPDCCH_ToAddList->list.count; i++)
        ASN_SEQUENCE_ADD(&target->tci_StatesPDCCH_ToAddList->list, source->tci_StatesPDCCH_ToAddList->list.array[i]);
    else
      updateMACie(target->tci_StatesPDCCH_ToAddList,
                  source->tci_StatesPDCCH_ToAddList,
                  struct NR_ControlResourceSet__tci_StatesPDCCH_ToAddList);
  }
  // end TCI States
}

void configure_ss_coreset(NR_BWP_PDCCH_t *pdcch,
                          NR_PDCCH_Config_t *pdcch_Config)
{
  if(!pdcch_Config)
    return;
  if(pdcch_Config->controlResourceSetToAddModList) {
    for (int i = 0; i < pdcch_Config->controlResourceSetToAddModList->list.count; i++) {
      NR_ControlResourceSet_t *source_coreset = pdcch_Config->controlResourceSetToAddModList->list.array[i];
      NR_ControlResourceSet_t *target_coreset = NULL;
      for (int j = 0; j < pdcch->list_Coreset.count; j++) {
        if (pdcch->list_Coreset.array[j]->controlResourceSetId == source_coreset->controlResourceSetId) {
          target_coreset = pdcch->list_Coreset.array[j];
          break;
        }
      }
      if (!target_coreset) {
        target_coreset = calloc(1, sizeof(*target_coreset));
        ASN_SEQUENCE_ADD(&pdcch->list_Coreset, target_coreset);
      }
      modlist_coreset(source_coreset, target_coreset);
    }
  }
  if(pdcch_Config->controlResourceSetToReleaseList) {
    for (int i = 0; i < pdcch_Config->controlResourceSetToReleaseList->list.count; i++) {
      NR_ControlResourceSetId_t id = *pdcch_Config->controlResourceSetToReleaseList->list.array[i];
      for (int j = 0; j < pdcch->list_Coreset.count; j++) {
        if (id == pdcch->list_Coreset.array[j]->controlResourceSetId) {
          asn_sequence_del(&pdcch->list_Coreset, j, 1);
          break;
        }
      }
    }
  }
  if(pdcch_Config->searchSpacesToAddModList) {
    for (int i = 0; i < pdcch_Config->searchSpacesToAddModList->list.count; i++) {
      NR_SearchSpace_t *source_ss = pdcch_Config->searchSpacesToAddModList->list.array[i];
      NR_SearchSpace_t *target_ss = NULL;
      for (int j = 0; j < pdcch->list_SS.count; j++) {
        if (pdcch->list_SS.array[j]->searchSpaceId == source_ss->searchSpaceId) {
          target_ss = pdcch->list_SS.array[j];
          break;
        }
      }
      if (!target_ss) {
        target_ss = calloc(1, sizeof(*target_ss));
        ASN_SEQUENCE_ADD(&pdcch->list_SS, target_ss);
      }
      modlist_ss(source_ss, target_ss);
    }
  }
  if(pdcch_Config->searchSpacesToReleaseList) {
    for (int i = 0; i < pdcch_Config->searchSpacesToReleaseList->list.count; i++) {
      NR_ControlResourceSetId_t id = *pdcch_Config->searchSpacesToReleaseList->list.array[i];
      for (int j = 0; j < pdcch->list_SS.count; j++) {
        if (id == pdcch->list_SS.array[j]->searchSpaceId) {
          asn_sequence_del(&pdcch->list_SS, j, 1);
          break;
        }
      }
    }
  }
}

static int lcid_cmp(const void *lc1, const void *lc2, void *mac_inst)
{
  uint8_t id1 = ((nr_lcordered_info_t *)lc1)->lcids_ordered;
  uint8_t id2 = ((nr_lcordered_info_t *)lc2)->lcids_ordered;
  NR_UE_MAC_INST_t *mac = (NR_UE_MAC_INST_t *)mac_inst;

  NR_LogicalChannelConfig_t **lc_config = &mac->logicalChannelConfig[0];

  AssertFatal(id1 > 0 && id2 > 0, "undefined logical channel identity\n");
  AssertFatal(lc_config[id1 - 1] != NULL || lc_config[id2 - 1] != NULL, "logical channel configuration should be available\n");

  return (lc_config[id1 - 1]->ul_SpecificParameters->priority - lc_config[id2 - 1]->ul_SpecificParameters->priority);
}

void nr_release_mac_config_logicalChannelBearer(module_id_t module_id, long channel_identity)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  if (mac->logicalChannelConfig[channel_identity - 1] != NULL) {
    ASN_STRUCT_FREE(asn_DEF_NR_LogicalChannelConfig,
                    mac->logicalChannelConfig[channel_identity - 1]);
    mac->logicalChannelConfig[channel_identity - 1] = NULL;
  } else {
    LOG_E(NR_MAC, "Trying to release a non configured logical channel bearer %li\n", channel_identity);
  }
}

static uint16_t nr_get_ms_bucketsizeduration(uint8_t bucketsizeduration)
{
  switch (bucketsizeduration) {
    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms50:
      return 50;

    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms100:
      return 100;

    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms150:
      return 150;

    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms300:
      return 300;

    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms500:
      return 500;

    case NR_LogicalChannelConfig__ul_SpecificParameters__bucketSizeDuration_ms1000:
      return 1000;

    default:
      return 0;
  }
}

void nr_configure_mac_config_logicalChannelBearer(module_id_t module_id,
                                                  long channel_identity,
                                                  NR_LogicalChannelConfig_t *lc_config)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);

  LOG_I(NR_MAC, "[MACLogicalChannelConfig]Applying RRC Logical Channel Config %d to lcid %li\n", module_id, channel_identity);
  mac->logicalChannelConfig[channel_identity - 1] = lc_config;

  // initialize the variable Bj for every LCID
  mac->scheduling_info.lc_sched_info[channel_identity - 1].Bj = 0;

  // store the bucket size
  int pbr = nr_get_pbr(lc_config->ul_SpecificParameters->prioritisedBitRate);
  int bsd = nr_get_ms_bucketsizeduration(lc_config->ul_SpecificParameters->bucketSizeDuration);

  // in infinite pbr, the bucket is saturated by pbr
  if (lc_config->ul_SpecificParameters->prioritisedBitRate
      == NR_LogicalChannelConfig__ul_SpecificParameters__prioritisedBitRate_infinity) {
    bsd = 1;
  }
  mac->scheduling_info.lc_sched_info[channel_identity - 1].bucket_size = pbr * bsd;

  if (lc_config->ul_SpecificParameters->logicalChannelGroup != NULL)
    mac->scheduling_info.lc_sched_info[channel_identity - 1].LCGID = *lc_config->ul_SpecificParameters->logicalChannelGroup;
  else
    mac->scheduling_info.lc_sched_info[channel_identity - 1].LCGID = 0;
}

void nr_rrc_mac_config_req_ue_logicalChannelBearer(module_id_t module_id,
                                                   struct NR_CellGroupConfig__rlc_BearerToAddModList *rlc_toadd_list,
                                                   struct NR_CellGroupConfig__rlc_BearerToReleaseList *rlc_torelease_list)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  if (rlc_torelease_list) {
    for (int i = 0; i < rlc_torelease_list->list.count; i++) {
      if (rlc_torelease_list->list.array[i]) {
        int lc_identity = *rlc_torelease_list->list.array[i];
        nr_release_mac_config_logicalChannelBearer(module_id, lc_identity);
      }
    }
  }
  if (rlc_toadd_list) {
    for (int i = 0; i < rlc_toadd_list->list.count; i++) {
      NR_RLC_BearerConfig_t *rlc_bearer = rlc_toadd_list->list.array[i];
      int lc_identity = rlc_bearer->logicalChannelIdentity;
      mac->lc_ordered_info[i].lcids_ordered = lc_identity;
      NR_LogicalChannelConfig_t *mac_lc_config = NULL;
      if (mac->logicalChannelConfig[lc_identity - 1] == NULL) {
        /* setup of new LCID*/
        LOG_D(NR_MAC, "Establishing the logical channel %d\n", lc_identity);
        AssertFatal(rlc_bearer->servedRadioBearer, "servedRadioBearer should be present for LCID establishment\n");
        if (rlc_bearer->servedRadioBearer->present == NR_RLC_BearerConfig__servedRadioBearer_PR_srb_Identity) { /* SRB */
          NR_SRB_Identity_t srb_id = rlc_bearer->servedRadioBearer->choice.srb_Identity;
          if (rlc_bearer->mac_LogicalChannelConfig != NULL) {
            updateMACie(mac_lc_config,
                        rlc_bearer->mac_LogicalChannelConfig,
                        NR_LogicalChannelConfig_t);
          } else {
            LOG_I(NR_RRC, "Applying the default logicalChannelConfig for SRB\n");
            if (srb_id == 1)
              mac_lc_config = (NR_LogicalChannelConfig_t *)&NR_SRB1_logicalChannelConfig_defaultValue;
            else if (srb_id == 2)
              mac_lc_config = (NR_LogicalChannelConfig_t *)&NR_SRB2_logicalChannelConfig_defaultValue;
            else if (srb_id == 3)
              mac_lc_config = (NR_LogicalChannelConfig_t *)&NR_SRB3_logicalChannelConfig_defaultValue;
            else
              AssertFatal(1 == 0, "The logical id %d is not a valid SRB id %li\n", lc_identity, srb_id);
          }
        } else { /* DRB */
          mac_lc_config = rlc_bearer->mac_LogicalChannelConfig;
          AssertFatal(mac_lc_config != NULL, "For DRB, it should be mandatorily present\n");
        }
      } else {
        /* LC is already established, reconfiguring the LC */
        LOG_D(NR_MAC, "Logical channel %d is already established, Reconfiguring now\n", lc_identity);
        if (rlc_bearer->mac_LogicalChannelConfig != NULL) {
          updateMACie(mac_lc_config,
                      rlc_bearer->mac_LogicalChannelConfig,
                      NR_LogicalChannelConfig_t);
        } else {
          /* Need M - Maintains current value */
          continue;
        }
      }
      mac->lc_ordered_info[i].logicalChannelConfig_ordered = mac_lc_config;
      nr_configure_mac_config_logicalChannelBearer(module_id, lc_identity, mac_lc_config);
    }

    // reorder the logical channels as per its priority
    qsort_r(mac->lc_ordered_info, rlc_toadd_list->list.count, sizeof(nr_lcordered_info_t), lcid_cmp, mac);
  }
}

void ue_init_config_request(NR_UE_MAC_INST_t *mac, int scs)
{
  int slots_per_frame = nr_slots_per_frame[scs];
  LOG_I(NR_MAC, "Initializing dl and ul config_request. num_slots = %d\n", slots_per_frame);
  mac->dl_config_request = calloc(slots_per_frame, sizeof(*mac->dl_config_request));
  mac->ul_config_request = calloc(slots_per_frame, sizeof(*mac->ul_config_request));
  for (int i = 0; i < slots_per_frame; i++)
    pthread_mutex_init(&(mac->ul_config_request[i].mutex_ul_config), NULL);
}

void nr_rrc_mac_config_req_mib(module_id_t module_id,
                               int cc_idP,
                               NR_MIB_t *mib,
                               int sched_sib)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  AssertFatal(mib, "MIB should not be NULL\n");
  // initialize dl and ul config_request upon first reception of MIB
  mac->mib = mib;    //  update by every reception
  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;
  if (sched_sib == 1)
    mac->get_sib1 = true;
  else if (sched_sib == 2)
    mac->get_otherSI = true;
  nr_ue_decode_mib(module_id,
                   cc_idP);
}

void setup_puschpowercontrol(NR_PUSCH_PowerControl_t *source, NR_PUSCH_PowerControl_t *target)
{
  updateMACie(target->tpc_Accumulation,
              source->tpc_Accumulation,
              long);
  updateMACie(target->msg3_Alpha,
              source->msg3_Alpha,
              NR_Alpha_t);
  if (source->p0_NominalWithoutGrant)
    updateMACie(target->p0_NominalWithoutGrant,
                source->p0_NominalWithoutGrant,
                long);
  if (source->p0_AlphaSets)
    updateMACie(target->p0_AlphaSets,
                source->p0_AlphaSets,
                struct NR_PUSCH_PowerControl__p0_AlphaSets);
  updateMACie(target->twoPUSCH_PC_AdjustmentStates,
              source->twoPUSCH_PC_AdjustmentStates,
              long);
  updateMACie(target->deltaMCS,
              source->deltaMCS,
              long);
  if (source->pathlossReferenceRSToReleaseList) {
    for (int i = 0; i < source->pathlossReferenceRSToReleaseList->list.count; i++) {
      long id = *source->pathlossReferenceRSToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->pathlossReferenceRSToAddModList->list.count; j++) {
        if(id == target->pathlossReferenceRSToAddModList->list.array[j]->pusch_PathlossReferenceRS_Id)
          break;
      }
      if (j < target->pathlossReferenceRSToAddModList->list.count)
        asn_sequence_del(&target->pathlossReferenceRSToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  if (source->pathlossReferenceRSToAddModList) {
    if (!target->pathlossReferenceRSToAddModList)
      target->pathlossReferenceRSToAddModList = calloc(1, sizeof(*target->pathlossReferenceRSToAddModList));
    for (int i = 0; i < source->pathlossReferenceRSToAddModList->list.count; i++) {
      long id = source->pathlossReferenceRSToAddModList->list.array[i]->pusch_PathlossReferenceRS_Id;
      int j;
      for (j = 0; j < target->pathlossReferenceRSToAddModList->list.count; j++) {
        if(id == target->pathlossReferenceRSToAddModList->list.array[j]->pusch_PathlossReferenceRS_Id)
          break;
      }
      if (j == target->pathlossReferenceRSToAddModList->list.count) {
        NR_PUSCH_PathlossReferenceRS_t *rs = calloc(1, sizeof(*rs));
        ASN_SEQUENCE_ADD(&target->pathlossReferenceRSToAddModList->list, rs);
      }
      updateMACie(target->pathlossReferenceRSToAddModList->list.array[j],
                  source->pathlossReferenceRSToAddModList->list.array[i],
                  NR_PUSCH_PathlossReferenceRS_t);
    }
  }
  if (source->sri_PUSCH_MappingToReleaseList) {
    for (int i = 0; i < source->sri_PUSCH_MappingToReleaseList->list.count; i++) {
      long id = *source->sri_PUSCH_MappingToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->sri_PUSCH_MappingToAddModList->list.count; j++) {
        if(id == target->sri_PUSCH_MappingToAddModList->list.array[j]->sri_PUSCH_PowerControlId)
          break;
      }
      if (j < target->sri_PUSCH_MappingToAddModList->list.count)
        asn_sequence_del(&target->sri_PUSCH_MappingToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  if (source->sri_PUSCH_MappingToAddModList) {
    if (!target->sri_PUSCH_MappingToAddModList)
      target->sri_PUSCH_MappingToAddModList = calloc(1, sizeof(*target->sri_PUSCH_MappingToAddModList));
    for (int i = 0; i < source->sri_PUSCH_MappingToAddModList->list.count; i++) {
      long id = source->sri_PUSCH_MappingToAddModList->list.array[i]->sri_PUSCH_PowerControlId;
      int j;
      for (j = 0; j < target->sri_PUSCH_MappingToAddModList->list.count; j++) {
        if(id == target->sri_PUSCH_MappingToAddModList->list.array[j]->sri_PUSCH_PowerControlId)
          break;
      }
      if (j == target->sri_PUSCH_MappingToAddModList->list.count) {
        NR_SRI_PUSCH_PowerControl_t *rs = calloc(1, sizeof(*rs));
        ASN_SEQUENCE_ADD(&target->sri_PUSCH_MappingToAddModList->list, rs);
      }
      updateMACie(target->sri_PUSCH_MappingToAddModList->list.array[j],
                  source->sri_PUSCH_MappingToAddModList->list.array[i],
                  NR_SRI_PUSCH_PowerControl_t);
    }
  }
}

void setup_puschconfig(NR_PUSCH_Config_t *source, NR_PUSCH_Config_t *target)
{
  updateMACie(target->dataScramblingIdentityPUSCH,
              source->dataScramblingIdentityPUSCH,
              long);
  updateMACie(target->txConfig,
              source->txConfig,
              long);
  if (source->dmrs_UplinkForPUSCH_MappingTypeA)
    handleMACsetupreleaseElement(target->dmrs_UplinkForPUSCH_MappingTypeA,
                                 source->dmrs_UplinkForPUSCH_MappingTypeA,
                                 NR_DMRS_UplinkConfig_t,
                                 asn_DEF_NR_SetupRelease_DMRS_UplinkConfig);
  if (source->dmrs_UplinkForPUSCH_MappingTypeB)
    handleMACsetupreleaseElement(target->dmrs_UplinkForPUSCH_MappingTypeB,
                                 source->dmrs_UplinkForPUSCH_MappingTypeB,
                                 NR_DMRS_UplinkConfig_t,
                                 asn_DEF_NR_SetupRelease_DMRS_UplinkConfig);
  if (source->pusch_PowerControl) {
    if (!target->pusch_PowerControl)
      target->pusch_PowerControl = calloc(1, sizeof(*target->pusch_PowerControl));
    setup_puschpowercontrol(source->pusch_PowerControl, target->pusch_PowerControl);
  }
  updateMACie(target->frequencyHopping,
              source->frequencyHopping,
              long);
  if (source->frequencyHoppingOffsetLists)
    updateMACie(target->frequencyHoppingOffsetLists,
                source->frequencyHoppingOffsetLists,
                struct NR_PUSCH_Config__frequencyHoppingOffsetLists);
  target->resourceAllocation = source->resourceAllocation;
  if(source->pusch_TimeDomainAllocationList)
    handleMACsetupreleaseElement(target->pusch_TimeDomainAllocationList,
                                 source->pusch_TimeDomainAllocationList,
                                 NR_PUSCH_TimeDomainResourceAllocationList_t,
                                 asn_DEF_NR_SetupRelease_PUSCH_TimeDomainResourceAllocationList);
  updateMACie(target->pusch_AggregationFactor,
              source->pusch_AggregationFactor,
              long);
  updateMACie(target->mcs_Table,
              source->mcs_Table,
              long);
  updateMACie(target->mcs_TableTransformPrecoder,
              source->mcs_TableTransformPrecoder,
              long);
  updateMACie(target->transformPrecoder,
              source->transformPrecoder,
              long);
  updateMACie(target->codebookSubset,
              source->codebookSubset,
              long);
  updateMACie(target->maxRank,
              source->maxRank,
              long);
  updateMACie(target->rbg_Size,
              source->rbg_Size,
              long);
  updateMACie(target->tp_pi2BPSK,
              source->tp_pi2BPSK,
              long);
  if (source->uci_OnPUSCH) {
    if (source->uci_OnPUSCH->present ==
        NR_SetupRelease_UCI_OnPUSCH_PR_release) {
      ASN_STRUCT_FREE(asn_DEF_NR_UCI_OnPUSCH,
                      target->uci_OnPUSCH);
      target->uci_OnPUSCH = NULL;
    }
    if (source->uci_OnPUSCH->present ==
        NR_SetupRelease_UCI_OnPUSCH_PR_setup) {
      if (target->uci_OnPUSCH) {
        target->uci_OnPUSCH->choice.setup->scaling = source->uci_OnPUSCH->choice.setup->scaling;
        if (source->uci_OnPUSCH->choice.setup->betaOffsets)
          updateMACie(target->uci_OnPUSCH->choice.setup->betaOffsets,
                      source->uci_OnPUSCH->choice.setup->betaOffsets,
                      struct NR_UCI_OnPUSCH__betaOffsets);
      }
    }
  }
}

void setup_pdschconfig(NR_PDSCH_Config_t *source, NR_PDSCH_Config_t *target)
{
  updateMACie(target->dataScramblingIdentityPDSCH,
              source->dataScramblingIdentityPDSCH,
              long);
  if (source->dmrs_DownlinkForPDSCH_MappingTypeA)
    handleMACsetupreleaseElement(target->dmrs_DownlinkForPDSCH_MappingTypeA,
                                 source->dmrs_DownlinkForPDSCH_MappingTypeA,
                                 NR_DMRS_DownlinkConfig_t,
                                 asn_DEF_NR_SetupRelease_DMRS_DownlinkConfig);
  if (source->dmrs_DownlinkForPDSCH_MappingTypeB)
    handleMACsetupreleaseElement(target->dmrs_DownlinkForPDSCH_MappingTypeB,
                                 source->dmrs_DownlinkForPDSCH_MappingTypeB,
                                 NR_DMRS_DownlinkConfig_t,
                                 asn_DEF_NR_SetupRelease_DMRS_DownlinkConfig);
  // TCI States
  if (source->tci_StatesToReleaseList) {
    for (int i = 0; i < source->tci_StatesToReleaseList->list.count; i++) {
      long id = *source->tci_StatesToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->tci_StatesToAddModList->list.count; j++) {
        if(id == target->tci_StatesToAddModList->list.array[j]->tci_StateId)
          break;
      }
      if (j < target->tci_StatesToAddModList->list.count)
        asn_sequence_del(&target->tci_StatesToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  if (source->tci_StatesToAddModList) {
    if (!target->tci_StatesToAddModList)
      target->tci_StatesToAddModList = calloc(1, sizeof(*target->tci_StatesToAddModList));
    for (int i = 0; i < source->tci_StatesToAddModList->list.count; i++) {
      long id = source->tci_StatesToAddModList->list.array[i]->tci_StateId;
      int j;
      for (j = 0; j < target->tci_StatesToAddModList->list.count; j++) {
        if(id == target->tci_StatesToAddModList->list.array[j]->tci_StateId)
          break;
      }
      if (j == target->tci_StatesToAddModList->list.count) {
        NR_TCI_State_t *tci = calloc(1, sizeof(*tci));
        ASN_SEQUENCE_ADD(&target->tci_StatesToAddModList->list, tci);
      }
      updateMACie(target->tci_StatesToAddModList->list.array[j],
                  source->tci_StatesToAddModList->list.array[i],
                  NR_TCI_State_t);
    }
  }
  // end TCI States
  updateMACie(target->vrb_ToPRB_Interleaver,
              source->vrb_ToPRB_Interleaver,
              long);
  target->resourceAllocation = source->resourceAllocation;
  if (source->pdsch_TimeDomainAllocationList)
    handleMACsetupreleaseElement(target->pdsch_TimeDomainAllocationList,
                                 source->pdsch_TimeDomainAllocationList,
                                 NR_PDSCH_TimeDomainResourceAllocationList_t,
                                 asn_DEF_NR_SetupRelease_PDSCH_TimeDomainResourceAllocationList);
  updateMACie(target->pdsch_AggregationFactor,
              source->pdsch_AggregationFactor,
              long);
  // rateMatchPattern
  if (source->rateMatchPatternToReleaseList) {
    for (int i = 0; i < source->rateMatchPatternToReleaseList->list.count; i++) {
      long id = *source->rateMatchPatternToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->rateMatchPatternToAddModList->list.count; j++) {
        if(id == target->rateMatchPatternToAddModList->list.array[j]->rateMatchPatternId)
          break;
      }
      if (j < target->rateMatchPatternToAddModList->list.count)
        asn_sequence_del(&target->rateMatchPatternToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  if (source->rateMatchPatternToAddModList) {
    if (!target->rateMatchPatternToAddModList)
      target->rateMatchPatternToAddModList = calloc(1, sizeof(*target->rateMatchPatternToAddModList));
    for (int i = 0; i < source->rateMatchPatternToAddModList->list.count; i++) {
      long id = source->rateMatchPatternToAddModList->list.array[i]->rateMatchPatternId;
      int j;
      for (j = 0; j < target->rateMatchPatternToAddModList->list.count; j++) {
        if(id == target->rateMatchPatternToAddModList->list.array[j]->rateMatchPatternId)
          break;
      }
      if (j == target->rateMatchPatternToAddModList->list.count) {
        NR_RateMatchPattern_t *rm = calloc(1, sizeof(*rm));
        ASN_SEQUENCE_ADD(&target->rateMatchPatternToAddModList->list, rm);
      }
      updateMACie(target->rateMatchPatternToAddModList->list.array[j],
                  source->rateMatchPatternToAddModList->list.array[i],
                  NR_RateMatchPattern_t);
    }
  }
  // end rateMatchPattern
  updateMACie(target->rateMatchPatternGroup1,
              source->rateMatchPatternGroup1,
              NR_RateMatchPatternGroup_t);
  updateMACie(target->rateMatchPatternGroup2,
              source->rateMatchPatternGroup2,
              NR_RateMatchPatternGroup_t);
  target->rbg_Size = source->rbg_Size;
  updateMACie(target->mcs_Table,
              source->mcs_Table,
              long);
  updateMACie(target->maxNrofCodeWordsScheduledByDCI,
              source->maxNrofCodeWordsScheduledByDCI,
              long);
  target->prb_BundlingType = source->prb_BundlingType;
  AssertFatal(source->zp_CSI_RS_ResourceToAddModList == NULL,
              "Not handled\n");
  AssertFatal(source->aperiodic_ZP_CSI_RS_ResourceSetsToAddModList == NULL,
              "Not handled\n");
  AssertFatal(source->sp_ZP_CSI_RS_ResourceSetsToAddModList == NULL,
              "Not handled\n");
}

void setup_sr_resource(NR_SchedulingRequestResourceConfig_t *source,
                       NR_SchedulingRequestResourceConfig_t *target)
{
  target->schedulingRequestResourceId = source->schedulingRequestResourceId;
  target->schedulingRequestID = source->schedulingRequestID;
  if (source->periodicityAndOffset)
    updateMACie(target->periodicityAndOffset,
                source->periodicityAndOffset,
                struct NR_SchedulingRequestResourceConfig__periodicityAndOffset);
  if (source->resource)
    updateMACie(target->resource,
                source->resource,
                NR_PUCCH_ResourceId_t);
}

void setup_pucchconfig(NR_PUCCH_Config_t *source, NR_PUCCH_Config_t *target)
{
  // PUCCH-ResourceSet
  if (source->resourceSetToAddModList) {
    if (!target->resourceSetToAddModList)
      target->resourceSetToAddModList = calloc(1, sizeof(*target->resourceSetToAddModList));
    for (int i = 0; i < source->resourceSetToAddModList->list.count; i++) {
      long id = source->resourceSetToAddModList->list.array[i]->pucch_ResourceSetId;
      int j;
      for (j = 0; j < target->resourceSetToAddModList->list.count; j++) {
        if(id == target->resourceSetToAddModList->list.array[j]->pucch_ResourceSetId)
          break;
      }
      if (j == target->resourceSetToAddModList->list.count) {
        NR_PUCCH_ResourceSet_t *set = calloc(1, sizeof(*set));
        ASN_SEQUENCE_ADD(&target->resourceSetToAddModList->list, set);
      }
      updateMACie(target->resourceSetToAddModList->list.array[j],
                  source->resourceSetToAddModList->list.array[i],
                  NR_PUCCH_ResourceSet_t);
    }
  }
  if (source->resourceSetToReleaseList) {
    for (int i = 0; i < source->resourceSetToReleaseList->list.count; i++) {
      long id = *source->resourceSetToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->resourceSetToAddModList->list.count; j++) {
        if(id == target->resourceSetToAddModList->list.array[j]->pucch_ResourceSetId)
          break;
      }
      if (j < target->resourceSetToAddModList->list.count)
        asn_sequence_del(&target->resourceSetToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  // PUCCH-Resource
  if (source->resourceToAddModList) {
    if (!target->resourceToAddModList)
      target->resourceToAddModList = calloc(1, sizeof(*target->resourceToAddModList));
    for (int i = 0; i < source->resourceToAddModList->list.count; i++) {
      long id = source->resourceToAddModList->list.array[i]->pucch_ResourceId;
      int j;
      for (j = 0; j < target->resourceToAddModList->list.count; j++) {
        if(id == target->resourceToAddModList->list.array[j]->pucch_ResourceId)
          break;
      }
      if (j == target->resourceToAddModList->list.count) {
        NR_PUCCH_Resource_t *res = calloc(1, sizeof(*res));
        ASN_SEQUENCE_ADD(&target->resourceToAddModList->list, res);
      }
      updateMACie(target->resourceToAddModList->list.array[j],
                  source->resourceToAddModList->list.array[i],
                  NR_PUCCH_Resource_t);
    }
  }
  if (source->resourceToReleaseList) {
    for (int i = 0; i < source->resourceToReleaseList->list.count; i++) {
      long id = *source->resourceToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->resourceToAddModList->list.count; j++) {
        if(id == target->resourceToAddModList->list.array[j]->pucch_ResourceId)
          break;
      }
      if (j < target->resourceToAddModList->list.count)
        asn_sequence_del(&target->resourceToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  // PUCCH-FormatConfig
  if (source->format1)
    handleMACsetupreleaseElement(target->format1,
                                 source->format1,
                                 NR_PUCCH_FormatConfig_t,
                                 asn_DEF_NR_SetupRelease_PUCCH_FormatConfig);
  if (source->format2)
    handleMACsetupreleaseElement(target->format2,
                                 source->format2,
                                 NR_PUCCH_FormatConfig_t,
                                 asn_DEF_NR_SetupRelease_PUCCH_FormatConfig);
  if (source->format3)
    handleMACsetupreleaseElement(target->format3,
                                 source->format3,
                                 NR_PUCCH_FormatConfig_t,
                                 asn_DEF_NR_SetupRelease_PUCCH_FormatConfig);
  if (source->format4)
    handleMACsetupreleaseElement(target->format4,
                                 source->format4,
                                 NR_PUCCH_FormatConfig_t,
                                 asn_DEF_NR_SetupRelease_PUCCH_FormatConfig);
  // SchedulingRequestResourceConfig
  if (source->schedulingRequestResourceToAddModList) {
    if (!target->schedulingRequestResourceToAddModList)
      target->schedulingRequestResourceToAddModList = calloc(1, sizeof(*target->schedulingRequestResourceToAddModList));
    for (int i = 0; i < source->schedulingRequestResourceToAddModList->list.count; i++) {
      long id = source->schedulingRequestResourceToAddModList->list.array[i]->schedulingRequestResourceId;
      int j;
      for (j = 0; j < target->schedulingRequestResourceToAddModList->list.count; j++) {
        if(id == target->schedulingRequestResourceToAddModList->list.array[j]->schedulingRequestResourceId)
          break;
      }
      if (j == target->schedulingRequestResourceToAddModList->list.count) {
        NR_SchedulingRequestResourceConfig_t *res = calloc(1, sizeof(*res));
        ASN_SEQUENCE_ADD(&target->schedulingRequestResourceToAddModList->list, res);
      }
      setup_sr_resource(source->schedulingRequestResourceToAddModList->list.array[i],
                        target->schedulingRequestResourceToAddModList->list.array[j]);
    }
  }
  if (source->schedulingRequestResourceToReleaseList) {
    for (int i = 0; i < source->schedulingRequestResourceToReleaseList->list.count; i++) {
      long id = *source->schedulingRequestResourceToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->schedulingRequestResourceToAddModList->list.count; j++) {
        if(id == target->schedulingRequestResourceToAddModList->list.array[j]->schedulingRequestResourceId)
          break;
      }
      if (j < target->schedulingRequestResourceToAddModList->list.count)
        asn_sequence_del(&target->schedulingRequestResourceToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }

  if(source->multi_CSI_PUCCH_ResourceList)
    updateMACie(target->multi_CSI_PUCCH_ResourceList,
                source->multi_CSI_PUCCH_ResourceList,
                struct NR_PUCCH_Config__multi_CSI_PUCCH_ResourceList);
  if(source->dl_DataToUL_ACK)
    updateMACie(target->dl_DataToUL_ACK,
                source->dl_DataToUL_ACK,
                struct NR_PUCCH_Config__dl_DataToUL_ACK);
  // PUCCH-SpatialRelationInfo
  if (source->spatialRelationInfoToAddModList) {
    if (!target->spatialRelationInfoToAddModList)
      target->spatialRelationInfoToAddModList = calloc(1, sizeof(*target->spatialRelationInfoToAddModList));
    for (int i = 0; i < source->spatialRelationInfoToAddModList->list.count; i++) {
      long id = source->spatialRelationInfoToAddModList->list.array[i]->pucch_SpatialRelationInfoId;
      int j;
      for (j = 0; j < target->spatialRelationInfoToAddModList->list.count; j++) {
        if(id == target->spatialRelationInfoToAddModList->list.array[j]->pucch_SpatialRelationInfoId)
          break;
      }
      if (j == target->spatialRelationInfoToAddModList->list.count) {
        NR_PUCCH_SpatialRelationInfo_t *res = calloc(1, sizeof(*res));
        ASN_SEQUENCE_ADD(&target->spatialRelationInfoToAddModList->list, res);
      }
      updateMACie(target->spatialRelationInfoToAddModList->list.array[j],
                  source->spatialRelationInfoToAddModList->list.array[i],
                  NR_PUCCH_SpatialRelationInfo_t);
    }
  }
  if (source->spatialRelationInfoToReleaseList) {
    for (int i = 0; i < source->spatialRelationInfoToReleaseList->list.count; i++) {
      long id = *source->spatialRelationInfoToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->spatialRelationInfoToAddModList->list.count; j++) {
        if(id == target->spatialRelationInfoToAddModList->list.array[j]->pucch_SpatialRelationInfoId)
          break;
      }
      if (j < target->spatialRelationInfoToAddModList->list.count)
        asn_sequence_del(&target->spatialRelationInfoToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }

  if (source->pucch_PowerControl) {
    if (!target->pucch_PowerControl)
      target->pucch_PowerControl = calloc(1, sizeof(*target->pucch_PowerControl));
    updateMACie(target->pucch_PowerControl->deltaF_PUCCH_f0,
                source->pucch_PowerControl->deltaF_PUCCH_f0,
                long);
    updateMACie(target->pucch_PowerControl->deltaF_PUCCH_f1,
                source->pucch_PowerControl->deltaF_PUCCH_f1,
                long);
    updateMACie(target->pucch_PowerControl->deltaF_PUCCH_f2,
                source->pucch_PowerControl->deltaF_PUCCH_f2,
                long);
    updateMACie(target->pucch_PowerControl->deltaF_PUCCH_f3,
                source->pucch_PowerControl->deltaF_PUCCH_f3,
                long);
    updateMACie(target->pucch_PowerControl->deltaF_PUCCH_f4,
                source->pucch_PowerControl->deltaF_PUCCH_f4,
                long);
    if (source->pucch_PowerControl->p0_Set)
      updateMACie(target->pucch_PowerControl->p0_Set,
                  source->pucch_PowerControl->p0_Set,
                  struct NR_PUCCH_PowerControl__p0_Set);
    if (source->pucch_PowerControl->pathlossReferenceRSs)
      updateMACie(target->pucch_PowerControl->pathlossReferenceRSs,
                  source->pucch_PowerControl->pathlossReferenceRSs,
                  struct NR_PUCCH_PowerControl__pathlossReferenceRSs);
    updateMACie(target->pucch_PowerControl->twoPUCCH_PC_AdjustmentStates,
                source->pucch_PowerControl->twoPUCCH_PC_AdjustmentStates,
                long);
  }
}

void setup_srsresourceset(NR_SRS_ResourceSet_t *source, NR_SRS_ResourceSet_t *target)
{
  target->srs_ResourceSetId = source->srs_ResourceSetId;
  if (source->srs_ResourceIdList)
    updateMACie(target->srs_ResourceIdList,
                source->srs_ResourceIdList,
                struct NR_SRS_ResourceSet__srs_ResourceIdList);
  target->resourceType = source->resourceType;
  target->usage = source->usage;
  updateMACie(target->alpha,
              source->alpha,
              NR_Alpha_t);
  if (source->p0)
    updateMACie(target->p0,
                source->p0,
                long);
  if (source->pathlossReferenceRS)
    updateMACie(target->pathlossReferenceRS,
                source->pathlossReferenceRS,
                struct NR_PathlossReferenceRS_Config);
  updateMACie(target->srs_PowerControlAdjustmentStates,
              source->srs_PowerControlAdjustmentStates,
              long);
}

void setup_srsconfig(NR_SRS_Config_t *source, NR_SRS_Config_t *target)
{
  updateMACie(target->tpc_Accumulation,
              source->tpc_Accumulation,
              long);
  // SRS-Resource
  if (source->srs_ResourceToAddModList) {
    if (!target->srs_ResourceToAddModList)
      target->srs_ResourceToAddModList = calloc(1, sizeof(*target->srs_ResourceToAddModList));
    for (int i = 0; i < source->srs_ResourceToAddModList->list.count; i++) {
      long id = source->srs_ResourceToAddModList->list.array[i]->srs_ResourceId;
      int j;
      for (j = 0; j < target->srs_ResourceToAddModList->list.count; j++) {
        if(id == target->srs_ResourceToAddModList->list.array[j]->srs_ResourceId)
          break;
      }
      if (j == target->srs_ResourceToAddModList->list.count) {
        NR_SRS_Resource_t *res = calloc(1, sizeof(*res));
        ASN_SEQUENCE_ADD(&target->srs_ResourceToAddModList->list, res);
      }
      updateMACie(target->srs_ResourceToAddModList->list.array[j],
                  source->srs_ResourceToAddModList->list.array[i],
                  NR_SRS_Resource_t);
    }
  }
  if (source->srs_ResourceToReleaseList) {
    for (int i = 0; i < source->srs_ResourceToReleaseList->list.count; i++) {
      long id = *source->srs_ResourceToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->srs_ResourceToAddModList->list.count; j++) {
        if(id == target->srs_ResourceToAddModList->list.array[j]->srs_ResourceId)
          break;
      }
      if (j < target->srs_ResourceToAddModList->list.count)
        asn_sequence_del(&target->srs_ResourceToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
  // SRS-ResourceSet
  if (source->srs_ResourceSetToAddModList) {
    if (!target->srs_ResourceSetToAddModList)
      target->srs_ResourceSetToAddModList = calloc(1, sizeof(*target->srs_ResourceSetToAddModList));
    for (int i = 0; i < source->srs_ResourceSetToAddModList->list.count; i++) {
      long id = source->srs_ResourceSetToAddModList->list.array[i]->srs_ResourceSetId;
      int j;
      for (j = 0; j < target->srs_ResourceSetToAddModList->list.count; j++) {
        if(id == target->srs_ResourceSetToAddModList->list.array[j]->srs_ResourceSetId)
          break;
      }
      if (j == target->srs_ResourceSetToAddModList->list.count) {
        NR_SRS_ResourceSet_t *res = calloc(1, sizeof(*res));
        ASN_SEQUENCE_ADD(&target->srs_ResourceSetToAddModList->list, res);
      }
      setup_srsresourceset(source->srs_ResourceSetToAddModList->list.array[i],
                           target->srs_ResourceSetToAddModList->list.array[j]);
    }
  }
  if (source->srs_ResourceSetToReleaseList) {
    for (int i = 0; i < source->srs_ResourceSetToReleaseList->list.count; i++) {
      long id = *source->srs_ResourceSetToReleaseList->list.array[i];
      int j;
      for (j = 0; j < target->srs_ResourceSetToAddModList->list.count; j++) {
        if(id == target->srs_ResourceSetToAddModList->list.array[j]->srs_ResourceSetId)
          break;
      }
      if (j < target->srs_ResourceSetToAddModList->list.count)
        asn_sequence_del(&target->srs_ResourceSetToAddModList->list, j, 1);
      else
        LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
    }
  }
}

NR_UE_DL_BWP_t *get_dl_bwp_structure(NR_UE_MAC_INST_t *mac,
                                     int bwp_id,
                                     bool setup)
{
  NR_UE_DL_BWP_t *bwp = NULL;
  for (int i = 0; i < mac->dl_BWPs.count; i++) {
    if (bwp_id == mac->dl_BWPs.array[i]->bwp_id) {
      bwp = mac->dl_BWPs.array[i];
      break;
    }
  }
  if (!bwp && setup) {
    bwp = calloc(1, sizeof(*bwp));
    ASN_SEQUENCE_ADD(&mac->dl_BWPs, bwp);
  }
  if (!setup) {
    bwp->n_dl_bwp = mac->dl_BWPs.count - 1;
    bwp->bw_tbslbrm = 0;
    for (int i = 0; i < mac->dl_BWPs.count; i++) {
      if (mac->dl_BWPs.array[i]->BWPSize > bwp->bw_tbslbrm)
        bwp->bw_tbslbrm = mac->dl_BWPs.array[i]->BWPSize;
    }
  }
  return bwp;
}

NR_UE_UL_BWP_t *get_ul_bwp_structure(NR_UE_MAC_INST_t *mac,
                                     int bwp_id,
                                     bool setup)
{
  NR_UE_UL_BWP_t *bwp = NULL;
  for (int i = 0; i < mac->ul_BWPs.count; i++) {
    if (bwp_id == mac->ul_BWPs.array[i]->bwp_id) {
      bwp = mac->ul_BWPs.array[i];
      break;
    }
  }
  if (!bwp && setup) {
    bwp = calloc(1, sizeof(*bwp));
    ASN_SEQUENCE_ADD(&mac->ul_BWPs, bwp);
  }
  if (!setup) {
    bwp->n_ul_bwp = mac->ul_BWPs.count - 1;
    bwp->bw_tbslbrm = 0;
    for (int i = 0; i < mac->ul_BWPs.count; i++) {
      if (mac->ul_BWPs.array[i]->BWPSize > bwp->bw_tbslbrm)
        bwp->bw_tbslbrm = mac->ul_BWPs.array[i]->BWPSize;
    }
  }
  return bwp;
}

void configure_dedicated_BWP_dl(NR_UE_MAC_INST_t *mac,
                                int bwp_id,
                                NR_BWP_DownlinkDedicated_t *dl_dedicated)
{
  if(dl_dedicated) {
    NR_UE_DL_BWP_t *bwp = get_dl_bwp_structure(mac, bwp_id, true);
    bwp->bwp_id = bwp_id;
    NR_BWP_PDCCH_t *pdcch = &mac->config_BWP_PDCCH[bwp_id];
    if(dl_dedicated->pdsch_Config) {
      if (dl_dedicated->pdsch_Config->present ==
          NR_SetupRelease_PDSCH_Config_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_PDSCH_Config,
                        bwp->pdsch_Config);
        bwp->pdsch_Config = NULL;
      }
      if (dl_dedicated->pdsch_Config->present ==
          NR_SetupRelease_PDSCH_Config_PR_setup) {
        if (!bwp->pdsch_Config)
          bwp->pdsch_Config = calloc(1, sizeof(*bwp->pdsch_Config));
        setup_pdschconfig(dl_dedicated->pdsch_Config->choice.setup, bwp->pdsch_Config);
      }
    }
    if(dl_dedicated->pdcch_Config) {
      if (dl_dedicated->pdcch_Config->present ==
          NR_SetupRelease_PDCCH_Config_PR_release) {
        for (int i = 0; pdcch->list_Coreset.count; i++)
          asn_sequence_del(&pdcch->list_Coreset, i, 1);
        for (int i = 0; pdcch->list_SS.count; i++)
          asn_sequence_del(&pdcch->list_SS, i, 1);
      }
      if (dl_dedicated->pdcch_Config->present ==
          NR_SetupRelease_PDCCH_Config_PR_setup)
        configure_ss_coreset(pdcch,
                             dl_dedicated->pdcch_Config->choice.setup);
    }
  }
}

void configure_dedicated_BWP_ul(NR_UE_MAC_INST_t *mac,
                                int bwp_id,
                                NR_BWP_UplinkDedicated_t *ul_dedicated)
{
  if(ul_dedicated) {
    NR_UE_UL_BWP_t *bwp = get_ul_bwp_structure(mac, bwp_id, true);
    bwp->bwp_id = bwp_id;
    if(ul_dedicated->pucch_Config) {
      if (ul_dedicated->pucch_Config->present ==
          NR_SetupRelease_PUCCH_Config_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_PUCCH_Config,
                        bwp->pucch_Config);
        bwp->pucch_Config = NULL;
      }
      if (ul_dedicated->pucch_Config->present ==
          NR_SetupRelease_PUCCH_Config_PR_setup) {
        if (!bwp->pucch_Config)
          bwp->pucch_Config = calloc(1, sizeof(*bwp->pucch_Config));
        setup_pucchconfig(ul_dedicated->pucch_Config->choice.setup, bwp->pucch_Config);
      }
    }
    if(ul_dedicated->pusch_Config) {
      if (ul_dedicated->pusch_Config->present ==
          NR_SetupRelease_PUSCH_Config_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_PUSCH_Config,
                        bwp->pusch_Config);
        bwp->pusch_Config = NULL;
      }
      if (ul_dedicated->pusch_Config->present ==
          NR_SetupRelease_PUSCH_Config_PR_setup) {
        if (!bwp->pusch_Config)
          bwp->pusch_Config = calloc(1, sizeof(*bwp->pusch_Config));
        setup_puschconfig(ul_dedicated->pusch_Config->choice.setup, bwp->pusch_Config);
      }
    }
    if(ul_dedicated->srs_Config) {
      if (ul_dedicated->srs_Config->present ==
          NR_SetupRelease_SRS_Config_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_SRS_Config,
                        bwp->srs_Config);
        bwp->srs_Config = NULL;
      }
      if (ul_dedicated->srs_Config->present ==
          NR_SetupRelease_SRS_Config_PR_setup) {
        if (!bwp->srs_Config)
          bwp->srs_Config = calloc(1, sizeof(*bwp->srs_Config));
        setup_srsconfig(ul_dedicated->srs_Config->choice.setup, bwp->srs_Config);
      }
    }
    AssertFatal(!ul_dedicated->configuredGrantConfig, "configuredGrantConfig not supported\n");
  }
}

void configure_common_BWP_dl(NR_UE_MAC_INST_t *mac,
                             int bwp_id,
                             NR_BWP_DownlinkCommon_t *dl_common)
{
  if(dl_common) {
    NR_UE_DL_BWP_t *bwp = get_dl_bwp_structure(mac, bwp_id, true);
    bwp->bwp_id = bwp_id;
    NR_BWP_t *dl_genericParameters = &dl_common->genericParameters;
    bwp->scs = dl_genericParameters->subcarrierSpacing;
    bwp->cyclicprefix = dl_genericParameters->cyclicPrefix;
    bwp->BWPSize = NRRIV2BW(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    bwp->BWPStart = NRRIV2PRBOFFSET(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    if (bwp_id == 0) {
      bwp->initial_BWPSize = bwp->BWPSize;
      bwp->initial_BWPStart = bwp->BWPStart;
    }
    if (dl_common->pdsch_ConfigCommon) {
      if (dl_common->pdsch_ConfigCommon->present ==
          NR_SetupRelease_PDSCH_ConfigCommon_PR_setup)
        updateMACie(bwp->tdaList_Common,
                    dl_common->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList,
                    NR_PDSCH_TimeDomainResourceAllocationList_t);
      if (dl_common->pdsch_ConfigCommon->present ==
          NR_SetupRelease_PDSCH_ConfigCommon_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_PDSCH_TimeDomainResourceAllocationList,
                        bwp->tdaList_Common);
        bwp->tdaList_Common = NULL;
      }
    }
    NR_BWP_PDCCH_t *pdcch = &mac->config_BWP_PDCCH[bwp_id];
    if (dl_common->pdcch_ConfigCommon) {
      if (dl_common->pdcch_ConfigCommon->present ==
          NR_SetupRelease_PDCCH_ConfigCommon_PR_setup)
        configure_common_ss_coreset(pdcch,
                                    dl_common->pdcch_ConfigCommon->choice.setup);
      if (dl_common->pdcch_ConfigCommon->present ==
          NR_SetupRelease_PDCCH_ConfigCommon_PR_release)
        release_common_ss_cset(pdcch);
    }
  }
}

void configure_common_BWP_ul(NR_UE_MAC_INST_t *mac,
                             int bwp_id,
                             NR_BWP_UplinkCommon_t *ul_common)
{
  if (ul_common) {
    NR_UE_UL_BWP_t *bwp = get_ul_bwp_structure(mac, bwp_id, true);
    bwp->bwp_id = bwp_id;
    NR_BWP_t *ul_genericParameters = &ul_common->genericParameters;
    bwp->scs = ul_genericParameters->subcarrierSpacing;
    bwp->cyclicprefix = ul_genericParameters->cyclicPrefix;
    bwp->BWPSize = NRRIV2BW(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    bwp->BWPStart = NRRIV2PRBOFFSET(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    if (bwp_id == 0) {
      bwp->initial_BWPSize = bwp->BWPSize;
      bwp->initial_BWPStart = bwp->BWPStart;
    }
    if (ul_common->rach_ConfigCommon) {
      handleMACsetuprelease(bwp->rach_ConfigCommon,
                            ul_common->rach_ConfigCommon,
                            NR_RACH_ConfigCommon_t,
                            asn_DEF_NR_RACH_ConfigCommon);
    }
    if (ul_common->pucch_ConfigCommon)
      handleMACsetuprelease(bwp->pucch_ConfigCommon,
                            ul_common->pucch_ConfigCommon,
                            NR_PUCCH_ConfigCommon_t,
                            asn_DEF_NR_PUCCH_ConfigCommon);
    if (ul_common->pusch_ConfigCommon) {
      if (ul_common->pusch_ConfigCommon->present ==
          NR_SetupRelease_PUSCH_ConfigCommon_PR_setup) {
        updateMACie(bwp->tdaList_Common,
                    ul_common->pusch_ConfigCommon->choice.setup->pusch_TimeDomainAllocationList,
                    NR_PUSCH_TimeDomainResourceAllocationList_t);
        updateMACie(bwp->msg3_DeltaPreamble,
                    ul_common->pusch_ConfigCommon->choice.setup->msg3_DeltaPreamble,
                    long);
      }
      if (ul_common->pusch_ConfigCommon->present ==
          NR_SetupRelease_PUSCH_ConfigCommon_PR_release) {
        ASN_STRUCT_FREE(asn_DEF_NR_PUSCH_TimeDomainResourceAllocationList,
                        bwp->tdaList_Common);
        bwp->tdaList_Common = NULL;
        free(bwp->msg3_DeltaPreamble);
        bwp->msg3_DeltaPreamble = NULL;
      }
    }
  }
}

void nr_rrc_mac_config_req_sib1(module_id_t module_id,
                                int cc_idP,
                                NR_SI_SchedulingInfo_t *si_SchedulingInfo,
                                NR_ServingCellConfigCommonSIB_t *scc)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  AssertFatal(scc, "SIB1 SCC should not be NULL\n");

  updateMACie(mac->tdd_UL_DL_ConfigurationCommon,
              scc->tdd_UL_DL_ConfigurationCommon,
              NR_TDD_UL_DL_ConfigCommon_t);
  updateMACie(mac->si_SchedulingInfo,
              si_SchedulingInfo,
              NR_SI_SchedulingInfo_t);

  config_common_ue_sa(mac, scc, module_id, cc_idP);
  configure_common_BWP_dl(mac,
                          0, // bwp-id
                          &scc->downlinkConfigCommon.initialDownlinkBWP);
  if (scc->uplinkConfigCommon)
    configure_common_BWP_ul(mac,
                            0, // bwp-id
                            &scc->uplinkConfigCommon->initialUplinkBWP);
  // set current BWP only if coming from non-connected state
  // otherwise it is just a periodically update of the SIB1 content
  if(mac->state < UE_CONNECTED) {
    mac->current_DL_BWP = get_dl_bwp_structure(mac, 0, false);
    AssertFatal(mac->current_DL_BWP, "Couldn't find DL-BWP0\n");
    mac->current_UL_BWP = get_ul_bwp_structure(mac, 0, false);
    AssertFatal(mac->current_UL_BWP, "Couldn't find DL-BWP0\n");
  }

  // Setup the SSB to Rach Occasions mapping according to the config
  build_ssb_to_ro_map(mac);

  if (!get_softmodem_params()->emulate_l1)
    mac->if_module->phy_config_request(&mac->phy_config);
  mac->phy_config_request_sent = true;

  ASN_STRUCT_FREE(asn_DEF_NR_SI_SchedulingInfo,
                  si_SchedulingInfo);
  ASN_STRUCT_FREE(asn_DEF_NR_ServingCellConfigCommonSIB,
                  scc);
}

void handle_reconfiguration_with_sync(NR_UE_MAC_INST_t *mac,
                                      module_id_t module_id,
                                      int cc_idP,
                                      const NR_ReconfigurationWithSync_t *reconfigurationWithSync)
{
  mac->crnti = reconfigurationWithSync->newUE_Identity;
  LOG_I(NR_MAC, "Configuring CRNTI %x\n", mac->crnti);

  RA_config_t *ra = &mac->ra;
  if (reconfigurationWithSync->rach_ConfigDedicated) {
    AssertFatal(reconfigurationWithSync->rach_ConfigDedicated->present ==
                NR_ReconfigurationWithSync__rach_ConfigDedicated_PR_uplink,
                "RACH on supplementaryUplink not supported\n");
    ra->rach_ConfigDedicated = reconfigurationWithSync->rach_ConfigDedicated->choice.uplink;
  }

  if (reconfigurationWithSync->spCellConfigCommon) {
    NR_ServingCellConfigCommon_t *scc = reconfigurationWithSync->spCellConfigCommon;
    if (scc->downlinkConfigCommon)
      configure_common_BWP_dl(mac,
                              0, // bwp-id
                              scc->downlinkConfigCommon->initialDownlinkBWP);
    if (scc->uplinkConfigCommon)
      configure_common_BWP_ul(mac,
                              0, // bwp-id
                              scc->uplinkConfigCommon->initialUplinkBWP);
    if (scc->physCellId)
      mac->physCellId = *scc->physCellId;
    mac->dmrs_TypeA_Position = scc->dmrs_TypeA_Position;
    updateMACie(mac->tdd_UL_DL_ConfigurationCommon,
                scc->tdd_UL_DL_ConfigurationCommon,
                NR_TDD_UL_DL_ConfigCommon_t);
    config_common_ue(mac, scc, module_id, cc_idP);
  }

  mac->state = UE_NOT_SYNC;
  ra->ra_state = RA_UE_IDLE;
  nr_ue_mac_default_configs(mac);

  if (!get_softmodem_params()->emulate_l1) {
    mac->synch_request.Mod_id = module_id;
    mac->synch_request.CC_id = cc_idP;
    mac->synch_request.synch_req.target_Nid_cell = mac->physCellId;
    mac->if_module->synch_request(&mac->synch_request);
    mac->if_module->phy_config_request(&mac->phy_config);
    mac->phy_config_request_sent = true;
  }
}

void configure_physicalcellgroup(NR_UE_MAC_INST_t *mac,
                                 const NR_PhysicalCellGroupConfig_t *phyConfig)
{
  mac->pdsch_HARQ_ACK_Codebook = phyConfig->pdsch_HARQ_ACK_Codebook;
  mac->harq_ACK_SpatialBundlingPUCCH = phyConfig->harq_ACK_SpatialBundlingPUCCH ? true : false;
  mac->harq_ACK_SpatialBundlingPUSCH = phyConfig->harq_ACK_SpatialBundlingPUSCH ? true : false;

  NR_P_Max_t *p_NR_FR1 = phyConfig->p_NR_FR1;
  NR_P_Max_t *p_UE_FR1 = phyConfig->ext1 ?
                         phyConfig->ext1->p_UE_FR1 :
                         NULL;
  if (p_NR_FR1 == NULL)
    mac->p_Max_alt = p_UE_FR1 == NULL ? INT_MIN : *p_UE_FR1;
  else
    mac->p_Max_alt = p_UE_FR1 == NULL ? *p_NR_FR1 :
                                        (*p_UE_FR1 < *p_NR_FR1 ?
                                        *p_UE_FR1 : *p_NR_FR1);
}

void configure_maccellgroup(NR_UE_MAC_INST_t *mac, const NR_MAC_CellGroupConfig_t *mcg)
{
  NR_UE_SCHEDULING_INFO *si = &mac->scheduling_info;
  if (mcg->drx_Config)
    LOG_E(NR_MAC, "DRX not implemented! Configuration not handled!\n");
  if (mcg->schedulingRequestConfig) {
    const NR_SchedulingRequestConfig_t *src = mcg->schedulingRequestConfig;
    if (src->schedulingRequestToReleaseList) {
      for (int i = 0; i < src->schedulingRequestToReleaseList->list.count; i++) {
        if (*src->schedulingRequestToReleaseList->list.array[i] == si->sr_id) {
          si->SR_COUNTER = 0;
          si->sr_ProhibitTimer = 0;
          si->sr_ProhibitTimer_Running = 0;
          si->sr_id = -1; // invalid init value
        }
        else
          LOG_E(NR_MAC, "Cannot release SchedulingRequestConfig. Not configured.\n");
      }
    }
    if (src->schedulingRequestToAddModList) {
      for (int i = 0; i < src->schedulingRequestToAddModList->list.count; i++) {
        NR_SchedulingRequestToAddMod_t *sr = src->schedulingRequestToAddModList->list.array[i];
        AssertFatal(si->sr_id == -1 ||
                    si->sr_id == sr->schedulingRequestId,
                    "Current implementation cannot handle more than 1 SR configuration\n");
        si->sr_id = sr->schedulingRequestId;
        si->sr_TransMax = sr->sr_TransMax;
        if (sr->sr_ProhibitTimer)
          LOG_E(NR_MAC, "SR prohibit timer not properly implemented\n");
      }
    }
  }
  if (mcg->bsr_Config) {
    si->periodicBSR_Timer = mcg->bsr_Config->periodicBSR_Timer;
    si->retxBSR_Timer = mcg->bsr_Config->retxBSR_Timer;
    if (mcg->bsr_Config->logicalChannelSR_DelayTimer)
      LOG_E(NR_MAC, "Handling of logicalChannelSR_DelayTimer not implemented\n");
  }
  if (mcg->tag_Config) {
    // TODO TAG not handled
    if(mcg->tag_Config->tag_ToAddModList) {
      for (int i = 0; i < mcg->tag_Config->tag_ToAddModList->list.count; i++) {
        if (mcg->tag_Config->tag_ToAddModList->list.array[i]->timeAlignmentTimer !=
            NR_TimeAlignmentTimer_infinity)
          LOG_E(NR_MAC, "TimeAlignmentTimer not handled\n");
      }
    }
  }
  if (mcg->phr_Config) {
    // TODO configuration when PHR is implemented
  }
}

static void configure_csiconfig(NR_UE_ServingCell_Info_t *sc_info,
                                struct NR_SetupRelease_CSI_MeasConfig *csi_MeasConfig_sr)
{
  switch (csi_MeasConfig_sr->present) {
    case NR_SetupRelease_CSI_MeasConfig_PR_NOTHING :
      break;
    case NR_SetupRelease_CSI_MeasConfig_PR_release :
      ASN_STRUCT_FREE(asn_DEF_NR_CSI_MeasConfig,
                      sc_info->csi_MeasConfig);
      sc_info->csi_MeasConfig = NULL;
      break;
    case NR_SetupRelease_CSI_MeasConfig_PR_setup :
      if (!sc_info->csi_MeasConfig) {  // setup
        updateMACie(sc_info->csi_MeasConfig,
                    csi_MeasConfig_sr->choice.setup,
                    NR_CSI_MeasConfig_t);
      }
      else { // modification
        NR_CSI_MeasConfig_t *target = sc_info->csi_MeasConfig;
        NR_CSI_MeasConfig_t *csi_MeasConfig = csi_MeasConfig_sr->choice.setup;
        if (csi_MeasConfig->reportTriggerSize)
          updateMACie(target->reportTriggerSize,
                      csi_MeasConfig->reportTriggerSize,
                      long);
        if (csi_MeasConfig->aperiodicTriggerStateList)
          handleMACsetuprelease(sc_info->aperiodicTriggerStateList,
                                csi_MeasConfig->aperiodicTriggerStateList,
                                NR_CSI_AperiodicTriggerStateList_t,
                                asn_DEF_NR_CSI_AperiodicTriggerStateList);
        if (csi_MeasConfig->semiPersistentOnPUSCH_TriggerStateList)
          handleMACsetupreleaseElement(target->semiPersistentOnPUSCH_TriggerStateList,
                                       csi_MeasConfig->semiPersistentOnPUSCH_TriggerStateList,
                                       NR_CSI_SemiPersistentOnPUSCH_TriggerStateList_t,
                                       asn_DEF_NR_SetupRelease_CSI_SemiPersistentOnPUSCH_TriggerStateList);
        // NZP-CSI-RS-Resources
        if (csi_MeasConfig->nzp_CSI_RS_ResourceToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->nzp_CSI_RS_ResourceToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->nzp_CSI_RS_ResourceToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->nzp_CSI_RS_ResourceToAddModList->list.count; j++) {
              if(id == target->nzp_CSI_RS_ResourceToAddModList->list.array[j]->nzp_CSI_RS_ResourceId)
                break;
            }
            if (j < target->nzp_CSI_RS_ResourceToAddModList->list.count)
              asn_sequence_del(&target->nzp_CSI_RS_ResourceToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->nzp_CSI_RS_ResourceToAddModList) {
          for (int i = 0; i < csi_MeasConfig->nzp_CSI_RS_ResourceToAddModList->list.count; i++) {
            NR_NZP_CSI_RS_Resource_t *res = csi_MeasConfig->nzp_CSI_RS_ResourceToAddModList->list.array[i];
            long id = res->nzp_CSI_RS_ResourceId;
            int j;
            for (j = 0; j < target->nzp_CSI_RS_ResourceToAddModList->list.count; j++) {
              if(id == target->nzp_CSI_RS_ResourceToAddModList->list.array[j]->nzp_CSI_RS_ResourceId)
                break;
            }
            if (j < target->nzp_CSI_RS_ResourceToAddModList->list.count) { // modify
              NR_NZP_CSI_RS_Resource_t *mac_res = target->nzp_CSI_RS_ResourceToAddModList->list.array[j];
              mac_res->resourceMapping = res->resourceMapping;
              mac_res->powerControlOffset = res->powerControlOffset;
              updateMACie(mac_res->powerControlOffsetSS,
                          res->powerControlOffsetSS,
                          long);
              mac_res->scramblingID = res->scramblingID;
              if (res->periodicityAndOffset)
                updateMACie(mac_res->periodicityAndOffset,
                            res->periodicityAndOffset,
                            NR_CSI_ResourcePeriodicityAndOffset_t);
              if (res->qcl_InfoPeriodicCSI_RS)
                updateMACie(mac_res->qcl_InfoPeriodicCSI_RS,
                            res->qcl_InfoPeriodicCSI_RS,
                            NR_TCI_StateId_t);
            }
            else { // add
              asn1cSequenceAdd(target->nzp_CSI_RS_ResourceToAddModList->list,
                               NR_NZP_CSI_RS_Resource_t,
                               nzp_csi_rs);
              updateMACie(nzp_csi_rs, res, NR_NZP_CSI_RS_Resource_t);
            }
          }
        }
        // NZP-CSI-RS-ResourceSets
        if (csi_MeasConfig->nzp_CSI_RS_ResourceSetToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->nzp_CSI_RS_ResourceSetToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->nzp_CSI_RS_ResourceSetToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->nzp_CSI_RS_ResourceSetToAddModList->list.count; j++) {
              if(id == target->nzp_CSI_RS_ResourceSetToAddModList->list.array[j]->nzp_CSI_ResourceSetId)
                break;
            }
            if (j < target->nzp_CSI_RS_ResourceSetToAddModList->list.count)
              asn_sequence_del(&target->nzp_CSI_RS_ResourceSetToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList) {
          for (int i = 0; i < csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.count; i++) {
            NR_NZP_CSI_RS_ResourceSet_t *res = csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[i];
            long id = res->nzp_CSI_ResourceSetId;
            int j;
            for (j = 0; j < target->nzp_CSI_RS_ResourceSetToAddModList->list.count; j++) {
              if(id == target->nzp_CSI_RS_ResourceSetToAddModList->list.array[j]->nzp_CSI_ResourceSetId)
                break;
            }
            if (j < target->nzp_CSI_RS_ResourceSetToAddModList->list.count) { // modify
              NR_NZP_CSI_RS_ResourceSet_t *mac_res = target->nzp_CSI_RS_ResourceSetToAddModList->list.array[j];
              updateMACie(mac_res, res, NR_NZP_CSI_RS_ResourceSet_t);
            }
            else { // add
              asn1cSequenceAdd(target->nzp_CSI_RS_ResourceSetToAddModList->list,
                               NR_NZP_CSI_RS_ResourceSet_t,
                               nzp_csi_rsset);
              updateMACie(nzp_csi_rsset, res, NR_NZP_CSI_RS_ResourceSet_t);
            }
          }
        }
        // CSI-IM-Resource
        if (csi_MeasConfig->csi_IM_ResourceToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->csi_IM_ResourceToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->csi_IM_ResourceToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->csi_IM_ResourceToAddModList->list.count; j++) {
              if(id == target->csi_IM_ResourceToAddModList->list.array[j]->csi_IM_ResourceId)
                break;
            }
            if (j < target->csi_IM_ResourceToAddModList->list.count)
              asn_sequence_del(&target->csi_IM_ResourceToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->csi_IM_ResourceToAddModList) {
          for (int i = 0; i < csi_MeasConfig->csi_IM_ResourceToAddModList->list.count; i++) {
            NR_CSI_IM_Resource_t *res = csi_MeasConfig->csi_IM_ResourceToAddModList->list.array[i];
            long id = res->csi_IM_ResourceId;
            int j;
            for (j = 0; j < target->csi_IM_ResourceToAddModList->list.count; j++) {
              if(id == target->csi_IM_ResourceToAddModList->list.array[j]->csi_IM_ResourceId)
                break;
            }
            if (j < target->csi_IM_ResourceToAddModList->list.count) { // modify
              NR_CSI_IM_Resource_t *mac_res = target->csi_IM_ResourceToAddModList->list.array[j];
              if (res->csi_IM_ResourceElementPattern)
                updateMACie(mac_res->csi_IM_ResourceElementPattern,
                            res->csi_IM_ResourceElementPattern,
                            struct NR_CSI_IM_Resource__csi_IM_ResourceElementPattern);
              if (res->freqBand)
                updateMACie(mac_res->freqBand,
                            res->freqBand,
                            NR_CSI_FrequencyOccupation_t);
              if (res->periodicityAndOffset)
                updateMACie(mac_res->periodicityAndOffset,
                            res->periodicityAndOffset,
                            NR_CSI_ResourcePeriodicityAndOffset_t);
            }
            else { // add
              asn1cSequenceAdd(target->csi_IM_ResourceToAddModList->list,
                               NR_CSI_IM_Resource_t,
                               csi_im);
              updateMACie(csi_im, res, NR_CSI_IM_Resource_t);
            }
          }
        }
        // CSI-IM-ResourceSets
        if (csi_MeasConfig->csi_IM_ResourceSetToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->csi_IM_ResourceSetToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->csi_IM_ResourceSetToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->csi_IM_ResourceSetToAddModList->list.count; j++) {
              if(id == target->csi_IM_ResourceSetToAddModList->list.array[j]->csi_IM_ResourceSetId)
                break;
            }
            if (j < target->csi_IM_ResourceSetToAddModList->list.count)
              asn_sequence_del(&target->csi_IM_ResourceSetToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->csi_IM_ResourceSetToAddModList) {
          for (int i = 0; i < csi_MeasConfig->csi_IM_ResourceSetToAddModList->list.count; i++) {
            NR_CSI_IM_ResourceSet_t *res = csi_MeasConfig->csi_IM_ResourceSetToAddModList->list.array[i];
            long id = res->csi_IM_ResourceSetId;
            int j;
            for (j = 0; j < target->csi_IM_ResourceSetToAddModList->list.count; j++) {
              if(id == target->csi_IM_ResourceSetToAddModList->list.array[j]->csi_IM_ResourceSetId)
                break;
            }
            if (j < target->csi_IM_ResourceSetToAddModList->list.count) { // modify
              NR_CSI_IM_ResourceSet_t *mac_res = target->csi_IM_ResourceSetToAddModList->list.array[j];
              updateMACie(mac_res, res, NR_CSI_IM_ResourceSet_t);
            }
            else { // add
              asn1cSequenceAdd(target->csi_IM_ResourceSetToAddModList->list,
                               NR_CSI_IM_ResourceSet_t,
                               csi_im_set);
              updateMACie(csi_im_set, res, NR_CSI_IM_ResourceSet_t);
            }
          }
        }
        // CSI-SSB-ResourceSets
        if (csi_MeasConfig->csi_SSB_ResourceSetToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->csi_SSB_ResourceSetToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->csi_SSB_ResourceSetToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->csi_SSB_ResourceSetToAddModList->list.count; j++) {
              if(id == target->csi_SSB_ResourceSetToAddModList->list.array[j]->csi_SSB_ResourceSetId)
                break;
            }
            if (j < target->csi_SSB_ResourceSetToAddModList->list.count)
              asn_sequence_del(&target->csi_SSB_ResourceSetToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->csi_SSB_ResourceSetToAddModList) {
          for (int i = 0; i < csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.count; i++) {
            NR_CSI_SSB_ResourceSet_t *res = csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[i];
            long id = res->csi_SSB_ResourceSetId;
            int j;
            for (j = 0; j < target->csi_SSB_ResourceSetToAddModList->list.count; j++) {
              if(id == target->csi_SSB_ResourceSetToAddModList->list.array[j]->csi_SSB_ResourceSetId)
                break;
            }
            if (j < target->csi_SSB_ResourceSetToAddModList->list.count) { // modify
              NR_CSI_SSB_ResourceSet_t *mac_res = target->csi_SSB_ResourceSetToAddModList->list.array[j];
              updateMACie(mac_res, res, NR_CSI_SSB_ResourceSet_t);
            }
            else { // add
              asn1cSequenceAdd(target->csi_SSB_ResourceSetToAddModList->list,
                               NR_CSI_SSB_ResourceSet_t,
                               csi_ssb_set);
              updateMACie(csi_ssb_set, res, NR_CSI_SSB_ResourceSet_t);
            }
          }
        }
        // CSI-ResourceConfigs
        if (csi_MeasConfig->csi_ResourceConfigToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->csi_ResourceConfigToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->csi_ResourceConfigToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->csi_ResourceConfigToAddModList->list.count; j++) {
              if(id == target->csi_ResourceConfigToAddModList->list.array[j]->csi_ResourceConfigId)
                break;
            }
            if (j < target->csi_ResourceConfigToAddModList->list.count)
              asn_sequence_del(&target->csi_ResourceConfigToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->csi_ResourceConfigToAddModList) {
          for (int i = 0; i < csi_MeasConfig->csi_ResourceConfigToAddModList->list.count; i++) {
            NR_CSI_ResourceConfig_t *res = csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[i];
            long id = res->csi_ResourceConfigId;
            int j;
            for (j = 0; j < target->csi_ResourceConfigToAddModList->list.count; j++) {
              if(id == target->csi_ResourceConfigToAddModList->list.array[j]->csi_ResourceConfigId)
                break;
            }
            if (j < target->csi_ResourceConfigToAddModList->list.count) { // modify
              NR_CSI_ResourceConfig_t *mac_res = target->csi_ResourceConfigToAddModList->list.array[j];
              updateMACie(mac_res, res, NR_CSI_ResourceConfig_t);
            }
            else { // add
              asn1cSequenceAdd(target->csi_ResourceConfigToAddModList->list,
                               NR_CSI_ResourceConfig_t,
                               csi_config);
              updateMACie(csi_config, res, NR_CSI_ResourceConfig_t);
            }
          }
        }
        // CSI-ReportConfigs
        if (csi_MeasConfig->csi_ReportConfigToReleaseList) {
          for (int i = 0; i < csi_MeasConfig->csi_ReportConfigToReleaseList->list.count; i++) {
            long id = *csi_MeasConfig->csi_ReportConfigToReleaseList->list.array[i];
            int j;
            for (j = 0; j < target->csi_ReportConfigToAddModList->list.count; j++) {
              if(id == target->csi_ReportConfigToAddModList->list.array[j]->reportConfigId)
                break;
            }
            if (j < target->csi_ReportConfigToAddModList->list.count)
              asn_sequence_del(&target->csi_ReportConfigToAddModList->list, j, 1);
            else
              LOG_E(NR_MAC, "Element not present in the list, impossible to release\n");
          }
        }
        if (csi_MeasConfig->csi_ReportConfigToAddModList) {
          for (int i = 0; i < csi_MeasConfig->csi_ReportConfigToAddModList->list.count; i++) {
            NR_CSI_ReportConfig_t *res = csi_MeasConfig->csi_ReportConfigToAddModList->list.array[i];
            long id = res->reportConfigId;
            int j;
            for (j = 0; j < target->csi_ReportConfigToAddModList->list.count; j++) {
              if(id == target->csi_ReportConfigToAddModList->list.array[j]->reportConfigId)
                break;
            }
            if (j < target->csi_ReportConfigToAddModList->list.count) { // modify
              NR_CSI_ReportConfig_t *mac_res = target->csi_ReportConfigToAddModList->list.array[j];
              updateMACie(mac_res, res, NR_CSI_ReportConfig_t);
            }
            else { // add
              asn1cSequenceAdd(target->csi_ReportConfigToAddModList->list,
                               NR_CSI_ReportConfig_t,
                               csi_rep);
              updateMACie(csi_rep, res, NR_CSI_ReportConfig_t);
            }
          }
        }
      }
      break;
    default :
      AssertFatal(false, "Invalid case\n");
  }
}

static void configure_servingcell_info(NR_UE_ServingCell_Info_t *sc_info,
                                       NR_ServingCellConfig_t *scd)
{
  if (scd->csi_MeasConfig)
    configure_csiconfig(sc_info, scd->csi_MeasConfig);

  if (scd->supplementaryUplink)
    updateMACie(sc_info->supplementaryUplink,
                scd->supplementaryUplink,
                NR_UplinkConfig_t);
  if (scd->crossCarrierSchedulingConfig)
    updateMACie(sc_info->crossCarrierSchedulingConfig,
                scd->crossCarrierSchedulingConfig,
                NR_CrossCarrierSchedulingConfig_t);
  if (scd->pdsch_ServingCellConfig) {
    switch (scd->pdsch_ServingCellConfig->present) {
      case NR_SetupRelease_PDSCH_ServingCellConfig_PR_NOTHING :
        break;
      case NR_SetupRelease_PDSCH_ServingCellConfig_PR_release :
        // release all configurations
        if (sc_info->pdsch_CGB_Transmission) {
          ASN_STRUCT_FREE(asn_DEF_NR_PDSCH_CodeBlockGroupTransmission,
                          sc_info->pdsch_CGB_Transmission);
          sc_info->pdsch_CGB_Transmission = NULL;
        }
        if (sc_info->xOverhead_PDSCH) {
          free(sc_info->xOverhead_PDSCH);
          sc_info->xOverhead_PDSCH = NULL;
        }
        if (sc_info->maxMIMO_Layers_PDSCH) {
          free(sc_info->maxMIMO_Layers_PDSCH);
          sc_info->maxMIMO_Layers_PDSCH = NULL;
        }
        break;
      case NR_SetupRelease_PDSCH_ServingCellConfig_PR_setup : {
        NR_PDSCH_ServingCellConfig_t *pdsch_servingcellconfig = scd->pdsch_ServingCellConfig->choice.setup;
        if (pdsch_servingcellconfig->codeBlockGroupTransmission)
          handleMACsetuprelease(sc_info->pdsch_CGB_Transmission,
                                pdsch_servingcellconfig->codeBlockGroupTransmission,
                                NR_PDSCH_CodeBlockGroupTransmission_t,
                                asn_DEF_NR_PDSCH_CodeBlockGroupTransmission);
        updateMACie(sc_info->xOverhead_PDSCH,
                    pdsch_servingcellconfig->xOverhead,
                    long);
        if (pdsch_servingcellconfig->ext1 &&
            pdsch_servingcellconfig->ext1->maxMIMO_Layers)
          updateMACie(sc_info->maxMIMO_Layers_PDSCH,
                      pdsch_servingcellconfig->ext1->maxMIMO_Layers,
                      long);
        break;
      }
      default :
        AssertFatal(false, "Invalid case\n");
    }
  }
  if (scd->uplinkConfig &&
      scd->uplinkConfig->pusch_ServingCellConfig) {
    switch (scd->uplinkConfig->pusch_ServingCellConfig->present) {
      case NR_SetupRelease_PUSCH_ServingCellConfig_PR_NOTHING :
        break;
      case NR_SetupRelease_PUSCH_ServingCellConfig_PR_release :
        // release all configurations
        if (sc_info->pusch_CGB_Transmission) {
          ASN_STRUCT_FREE(asn_DEF_NR_PUSCH_CodeBlockGroupTransmission,
                          sc_info->pusch_CGB_Transmission);
          sc_info->pdsch_CGB_Transmission = NULL;
        }
        if (sc_info->rateMatching_PUSCH) {
          free(sc_info->rateMatching_PUSCH);
          sc_info->rateMatching_PUSCH = NULL;
        }
        if (sc_info->xOverhead_PUSCH) {
          free(sc_info->xOverhead_PUSCH);
          sc_info->xOverhead_PUSCH = NULL;
        }
        if (sc_info->maxMIMO_Layers_PUSCH) {
          free(sc_info->maxMIMO_Layers_PUSCH);
          sc_info->maxMIMO_Layers_PUSCH = NULL;
        }
        break;
      case NR_SetupRelease_PUSCH_ServingCellConfig_PR_setup : {
        NR_PUSCH_ServingCellConfig_t *pusch_servingcellconfig = scd->uplinkConfig->pusch_ServingCellConfig->choice.setup;
        updateMACie(sc_info->rateMatching_PUSCH,
                    pusch_servingcellconfig->rateMatching,
                    long);
        updateMACie(sc_info->xOverhead_PUSCH,
                    pusch_servingcellconfig->xOverhead,
                    long);
        if (pusch_servingcellconfig->ext1 &&
            pusch_servingcellconfig->ext1->maxMIMO_Layers)
          updateMACie(sc_info->maxMIMO_Layers_PUSCH,
                      pusch_servingcellconfig->ext1->maxMIMO_Layers,
                      long);
        if (pusch_servingcellconfig->codeBlockGroupTransmission)
          handleMACsetuprelease(sc_info->pusch_CGB_Transmission,
                                pusch_servingcellconfig->codeBlockGroupTransmission,
                                NR_PUSCH_CodeBlockGroupTransmission_t,
                                asn_DEF_NR_PUSCH_CodeBlockGroupTransmission);
        break;
      }
      default :
        AssertFatal(false, "Invalid case\n");
    }
  }
}

void release_dl_BWP(NR_UE_MAC_INST_t *mac, int bwp_id)
{
  NR_UE_DL_BWP_t *bwp = NULL;
  for (int i = 0; i < mac->dl_BWPs.count; i++) {
    if (bwp_id == mac->dl_BWPs.array[i]->bwp_id) {
      bwp = mac->dl_BWPs.array[i];
      asn_sequence_del(&mac->dl_BWPs, i, 0);
      break;
    }
  }
  AssertFatal(bwp, "No DL-BWP %d to release\n", bwp_id);
  free(bwp->cyclicprefix);
  ASN_STRUCT_FREE(asn_DEF_NR_PDSCH_TimeDomainResourceAllocationList,
                  bwp->tdaList_Common);
  bwp->tdaList_Common = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_PDSCH_Config,
                  bwp->pdsch_Config);
  bwp->pdsch_Config = NULL;
  free(bwp);

  NR_BWP_PDCCH_t *pdcch = &mac->config_BWP_PDCCH[bwp_id];
  release_common_ss_cset(pdcch);
  for (int i = 0; pdcch->list_Coreset.count; i++)
    asn_sequence_del(&pdcch->list_Coreset, i, 1);
  for (int i = 0; pdcch->list_SS.count; i++)
    asn_sequence_del(&pdcch->list_SS, i, 1);
}

void release_ul_BWP(NR_UE_MAC_INST_t *mac, int bwp_id)
{
  NR_UE_UL_BWP_t *bwp = NULL;
  for (int i = 0; i < mac->ul_BWPs.count; i++) {
    if (bwp_id == mac->ul_BWPs.array[i]->bwp_id) {
      bwp = mac->ul_BWPs.array[i];
      asn_sequence_del(&mac->ul_BWPs, i, 0);
      break;
    }
  }
  AssertFatal(bwp, "No UL-BWP %d to release\n", bwp_id);
  free(bwp->cyclicprefix);
  ASN_STRUCT_FREE(asn_DEF_NR_RACH_ConfigCommon,
                  bwp->rach_ConfigCommon);
  bwp->rach_ConfigCommon = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_PUSCH_TimeDomainResourceAllocationList,
                  bwp->tdaList_Common);
  bwp->tdaList_Common = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_ConfiguredGrantConfig,
                  bwp->configuredGrantConfig);
  bwp->configuredGrantConfig = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_PUSCH_Config,
                  bwp->pusch_Config);
  bwp->pusch_Config = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_PUCCH_Config,
                  bwp->pucch_Config);
  bwp->pucch_Config = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_PUCCH_ConfigCommon,
                  bwp->pucch_ConfigCommon);
  bwp->pucch_ConfigCommon = NULL;
  ASN_STRUCT_FREE(asn_DEF_NR_SRS_Config,
                  bwp->srs_Config);
  bwp->srs_Config = NULL;
  free(bwp->msg3_DeltaPreamble);
  free(bwp);
}

void configure_BWPs(NR_UE_MAC_INST_t *mac, NR_ServingCellConfig_t *scd)
{
  configure_dedicated_BWP_dl(mac,
                             0,
                             scd->initialDownlinkBWP);
  if (scd->downlinkBWP_ToReleaseList) {
    for (int i = 0; i < scd->downlinkBWP_ToReleaseList->list.count; i++)
      release_dl_BWP(mac, *scd->downlinkBWP_ToReleaseList->list.array[i]);
  }
  if (scd->downlinkBWP_ToAddModList) {
    for (int i = 0; i < scd->downlinkBWP_ToAddModList->list.count; i++) {
      NR_BWP_Downlink_t *bwp = scd->downlinkBWP_ToAddModList->list.array[i];
      configure_common_BWP_dl(mac,
                              bwp->bwp_Id,
                              bwp->bwp_Common);
      configure_dedicated_BWP_dl(mac,
                                 bwp->bwp_Id,
                                 bwp->bwp_Dedicated);
    }
  }
  if (scd->firstActiveDownlinkBWP_Id) {
    mac->current_DL_BWP = get_dl_bwp_structure(mac, *scd->firstActiveDownlinkBWP_Id, false);
    AssertFatal(mac->current_DL_BWP, "Couldn't find DL-BWP %ld\n", *scd->firstActiveDownlinkBWP_Id);
  }

  if (scd->uplinkConfig) {
    configure_dedicated_BWP_ul(mac,
                               0,
                               scd->uplinkConfig->initialUplinkBWP);
    if (scd->uplinkConfig->uplinkBWP_ToReleaseList) {
      for (int i = 0; i < scd->uplinkConfig->uplinkBWP_ToReleaseList->list.count; i++)
        release_ul_BWP(mac, *scd->uplinkConfig->uplinkBWP_ToReleaseList->list.array[i]);
    }
    if (scd->uplinkConfig->uplinkBWP_ToAddModList) {
      for (int i = 0; i < scd->uplinkConfig->uplinkBWP_ToAddModList->list.count; i++) {
        NR_BWP_Uplink_t *bwp = scd->uplinkConfig->uplinkBWP_ToAddModList->list.array[i];
        configure_common_BWP_ul(mac,
                                bwp->bwp_Id,
                                bwp->bwp_Common);
        configure_dedicated_BWP_ul(mac,
                                   bwp->bwp_Id,
                                   bwp->bwp_Dedicated);
      }
    }
    if (scd->uplinkConfig->firstActiveUplinkBWP_Id) {
      mac->current_UL_BWP = get_ul_bwp_structure(mac, *scd->uplinkConfig->firstActiveUplinkBWP_Id, false);
      AssertFatal(mac->current_UL_BWP, "Couldn't find DL-BWP %ld\n", *scd->uplinkConfig->firstActiveUplinkBWP_Id);
    }
  }
}

void nr_rrc_mac_config_req_cg(module_id_t module_id,
                              int cc_idP,
                              NR_CellGroupConfig_t *cell_group_config)
{
  LOG_I(MAC,"Applying CellGroupConfig from gNodeB\n");
  AssertFatal(cell_group_config, "CellGroupConfig should not be NULL\n");
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);

  if (cell_group_config->mac_CellGroupConfig)
    configure_maccellgroup(mac, cell_group_config->mac_CellGroupConfig);

  if (cell_group_config->physicalCellGroupConfig)
    configure_physicalcellgroup(mac, cell_group_config->physicalCellGroupConfig);

  if (cell_group_config->spCellConfig) {
    NR_SpCellConfig_t *spCellConfig = cell_group_config->spCellConfig;
    NR_ServingCellConfig_t *scd = spCellConfig->spCellConfigDedicated;
    mac->servCellIndex = spCellConfig->servCellIndex ? *spCellConfig->servCellIndex : 0;
    if (spCellConfig->reconfigurationWithSync) {
      LOG_A(NR_MAC, "Received reconfigurationWithSync\n");
      handle_reconfiguration_with_sync(mac, module_id, cc_idP, spCellConfig->reconfigurationWithSync);
    }
    if (scd) {
      configure_servingcell_info(&mac->sc_info, scd);
      configure_BWPs(mac, scd);
    }
  }

  // Setup the SSB to Rach Occasions mapping according to the config
  build_ssb_to_ro_map(mac);

  if (!mac->dl_config_request || !mac->ul_config_request)
    ue_init_config_request(mac, mac->current_DL_BWP->scs);

  ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig,
                  cell_group_config);
}
