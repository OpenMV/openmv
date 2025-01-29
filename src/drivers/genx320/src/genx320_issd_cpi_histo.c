/**********************************************************************************************************************
 * Copyright (c) Prophesee S.A.                                                                                       *
 *                                                                                                                    *
 * Licensed under the Apache License, Version 2.0 (the "License");                                                    *
 * you may not use this file except in compliance with the License.                                                   *
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0                                 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed   *
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                      *
 * See the License for the specific language governing permissions and limitations under the License.                 *
 *                                                                                                                    *
 * This is an automatically generated file                                                                            *
 **********************************************************************************************************************/

#ifndef __ISSD_CPI_HISTO_H
#define __ISSD_CPI_HISTO_H

#include "psee_issd.h"

static const struct reg_op dcmi_init_seq[] = {
	{.op = WRITE, .args = {.write = {0x00B8UL, 0x00000002}} },	 // saphir/sram_initn
	{.op = WRITE, .args = {.write = {0x00BCUL, 0x00007FFF}} },	 // saphir/sram_pd0
	{.op = WRITE, .args = {.write = {0x9008UL, 0x00000320}} },	 // saphir/ro/time_base_ctrl                Write fields: time_base_enable
	{.op = WRITE, .args = {.write = {0x9008UL, 0x00010320}} },	 // saphir/ro/time_base_ctrl                Write fields: time_base_srst
	{.op = WRITE, .args = {.write = {0x020CUL, 0x00000015}} },	 // saphir/rtc_clk_ctrl                     Write fields: rtc_clk_en|rtc_clk_div
	{.op = WRITE, .args = {.write = {0x0204UL, 0x00004D6C}} },	 // saphir/sys_clk_ctrl                     Write fields: sys_clk_auto_mode
	{.op = WRITE, .args = {.write = {0x0210UL, 0x00000C00}} },	 // saphir/evt_icn_clk_ctrl                 Write fields: ro_clk_en|esp_clk_en
	{.op = WRITE, .args = {.write = {0x9008UL, 0x000000A0}} },	 // saphir/ro/time_base_ctrl                Write fields: us_counter_max
	{.op = WRITE, .args = {.write = {0x004CUL, 0x00201F40}} },	 // saphir/adc_control
	{.op = WRITE, .args = {.write = {0x00B8UL, 0x00000042}} },	 // saphir/sram_initn                       Write fields: cpi_initn
	{.op = WRITE, .args = {.write = {0x00C0UL, 0x00000078}} },	 // saphir/sram_pd1                         Write fields: cpi_pd
	{.op = WRITE, .args = {.write = {0x800CUL, 0x0001014A}} },	 // saphir/cpi/packet_time_control          Write fields: packet_period
	{.op = WRITE, .args = {.write = {0x8008UL, 0x00000140}} },	 // saphir/cpi/packet_size_control          Write fields: packet_size
	{.op = WRITE, .args = {.write = {0x8010UL, 0x00000140}} },	 // saphir/cpi/frame_size_control           Write fields: frame_size
	{.op = WRITE, .args = {.write = {0x8000UL, 0x0001EEA1}} },	 // saphir/cpi/pipeline_control             Write fields: enable|clk_out_en|clk_control_inversion|packet_fixed_size_enable|packet_fixed_rate_enable|frame_fixed_size_enable|output_if_mode|output_data_format|output_width|clk_out_gating_enable
	{.op = WRITE, .args = {.write = {0x8000UL, 0x0001FEA1}} },	 // saphir/cpi/pipeline_control             Write fields: clk_out_gating_enable
	{.op = WRITE, .args = {.write = {0xE000UL, 0x00000005}} },	 // saphir/nfl/pipeline_control             Write fields: enable|bypass
	{.op = WRITE, .args = {.write = {0xC000UL, 0x00000005}} },	 // saphir/afk/pipeline_control             Write fields: enable|bypass
	{.op = WRITE, .args = {.write = {0x4040UL, 0x00000003}} },	 // saphir/ehc/ehc_control
	{.op = WRITE, .args = {.write = {0x4044UL, 0x00000044}} },	 // saphir/ehc/bits_splitting
	{.op = WRITE, .args = {.write = {0x4048UL, 0x00002EE0}} },	 // saphir/ehc/integration_period
	{.op = WRITE, .args = {.write = {0x4000UL, 0x00000001}} },	 // saphir/ehc/pipeline_control
	{.op = WRITE, .args = {.write = {0x9008UL, 0x000000A4}} },	 // saphir/ro/time_base_ctrl                Write fields: time_base_mode|external_mode|external_mode_enable|us_counter_max
	{.op = WRITE, .args = {.write = {0x9000UL, 0x00000200}} },	 // saphir/ro/readout_ctrl                  Write fields: ro_digital_pipe_en
	{.op = WRITE, .args = {.write = {0x001CUL, 0x00000004}} },	 // saphir/dig_soft_reset                   Write fields: analog_rstn
	{.op = WRITE, .args = {.write = {0xA000UL, 0x00000406}} },	 // saphir/ldo/bg                           Write fields: bg_en|bg_buf_en|bg_bypass|bg_chk|bg_adj
	{.op = WRITE, .args = {.write = {0xA01CUL, 0x0007641F}} },	 // saphir/ldo/pmu                          Write fields: pmu_icgm_en|pmu_v2i_en|pmu_v2i_cal_en|mipi_v2i_en
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0xB030UL, 0x00000010}} },	 // saphir/mipi_csi/power                   Write fields: cur_en
	{.op = WRITE, .args = {.write = {0x004CUL, 0x00201F42}} },	 // saphir/adc_control                      Write fields: adc_clk_en
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0xA01CUL, 0x0007E41F}} },	 // saphir/ldo/pmu                          Write fields: pmu_v2i_en|pmu_v2i_adj|pmu_v2i_cal_en
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x004CUL, 0x00201F40}} },	 // saphir/adc_control
	{.op = WRITE, .args = {.write = {0x1218UL, 0x00000111}} },	 // saphir/bias/bgen_cgm_sub                Write fields: bias_cgm_sub_en
	{.op = WRITE, .args = {.write = {0x1218UL, 0x00000111}} },	 // saphir/bias/bgen_cgm_sub                Write fields: bias_cgm_sub_trim
	{.op = WRITE, .args = {.write = {0x1218UL, 0x00000111}} },	 // saphir/bias/bgen_cgm_sub                Write fields: bias_cgm_sub_slp_ctl
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x1220UL, 0x00000001}} },	 // saphir/bias/bgen_cc_en                  Write fields: bias_cc_hv_en
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0xA008UL, 0x00008085}} },	 // saphir/ldo/ldo_lv                       Write fields: ldo_lv_en|ldo_lv_climit_en|ldo_lv_climit|ldo_lv_ind_en|ldo_lv_adj|ldo_lv_ind_vth_ok|ldo_lv_ind_vth_bo
	{.op = WRITE, .args = {.write = {0xA004UL, 0x00008055}} },	 // saphir/ldo/ldo_hv                       Write fields: ldo_hv_en|ldo_hv_climit_en|ldo_hv_climit|ldo_hv_ind_en|ldo_hv_adj|ldo_hv_ind_vth_ok|ldo_hv_ind_vth_bo
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x0070UL, 0x0000051F}} },	 // saphir/cp_ctrl                          Write fields: cp_en|cp_clk_en|cp_adj|cp_cfly|cp_mphase|cp_clk_divider
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x1220UL, 0x00000003}} },	 // saphir/bias/bgen_cc_en                  Write fields: bias_cc_lv_en
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x1208UL, 0x00000030}} },	 // saphir/bias/bgen_ctrl                   Write fields: bias_rstn_hv|bias_rstn_lv
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000420}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000400}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|px_sw_rstn|px_roi_halt_programming
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000822}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x3000UL, 0x00000000}} },	 // saphir/roi/td_roi_y00                   
	{.op = WRITE, .args = {.write = {0x3004UL, 0x00000000}} },	 // saphir/roi/td_roi_y01                   
	{.op = WRITE, .args = {.write = {0x3008UL, 0x00000000}} },	 // saphir/roi/td_roi_y02                   
	{.op = WRITE, .args = {.write = {0x300CUL, 0x00000000}} },	 // saphir/roi/td_roi_y03                   
	{.op = WRITE, .args = {.write = {0x3010UL, 0x00000000}} },	 // saphir/roi/td_roi_y04                   
	{.op = WRITE, .args = {.write = {0x3014UL, 0x00000000}} },	 // saphir/roi/td_roi_y05                   
	{.op = WRITE, .args = {.write = {0x3018UL, 0x00000000}} },	 // saphir/roi/td_roi_y06                   
	{.op = WRITE, .args = {.write = {0x301CUL, 0x00000000}} },	 // saphir/roi/td_roi_y07                   
	{.op = WRITE, .args = {.write = {0x3020UL, 0x00000000}} },	 // saphir/roi/td_roi_y08                   
	{.op = WRITE, .args = {.write = {0x3024UL, 0x00000000}} },	 // saphir/roi/td_roi_y09                   
	{.op = WRITE, .args = {.write = {0x3028UL, 0x00000000}} },	 // saphir/roi/td_roi_y10                   
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000822}} },	 // saphir/roi_ctrl                         Write fields: roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x2000UL, 0x00000000}} },	 // saphir/roi/td_roi_x00                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2004UL, 0x00000000}} },	 // saphir/roi/td_roi_x01                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2008UL, 0x00000000}} },	 // saphir/roi/td_roi_x02                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x200CUL, 0x00000000}} },	 // saphir/roi/td_roi_x03                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2010UL, 0x00000000}} },	 // saphir/roi/td_roi_x04                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2014UL, 0x00000000}} },	 // saphir/roi/td_roi_x05                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2018UL, 0x00000000}} },	 // saphir/roi/td_roi_x06                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x201CUL, 0x00000000}} },	 // saphir/roi/td_roi_x07                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2020UL, 0x00000000}} },	 // saphir/roi/td_roi_x08                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x2024UL, 0x00000000}} },	 // saphir/roi/td_roi_x09                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3000UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y00                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3004UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y01                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3008UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y02                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x300CUL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y03                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3010UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y04                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3014UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y05                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3018UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y06                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x301CUL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y07                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3020UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y08                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x3024UL, 0xFFFFFFFF}} },	 // saphir/roi/td_roi_y09                   Write fields: effective
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000822}} },	 // saphir/roi_ctrl                         Write fields: roi_td_shadow_trigger
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_shadow_trigger
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x1000UL, 0x0301003D}} },	 // saphir/bias/bias_pr_hv0                 Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1004UL, 0x03010027}} },	 // saphir/bias/bias_fo_hv0                 Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1008UL, 0x0101003F}} },	 // saphir/bias/bias_fes_hv0                Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1100UL, 0x03010000}} },	 // saphir/bias/bias_hpf_lv0                Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1104UL, 0x01090018}} },	 // saphir/bias/bias_diff_on_lv0            Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1108UL, 0x01010033}} },	 // saphir/bias/bias_diff_lv0               Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x110CUL, 0x01090013}} },	 // saphir/bias/bias_diff_off_lv0           Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1110UL, 0x01010039}} },	 // saphir/bias/bias_inv_lv0                Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1114UL, 0x03010052}} },	 // saphir/bias/bias_refr_lv0               Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1118UL, 0x03010042}} },	 // saphir/bias/bias_invp_lv0               Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x111CUL, 0x03000074}} },	 // saphir/bias/bias_req_pu_lv0             Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1120UL, 0x010000A4}} },	 // saphir/bias/bias_sm_pdy_lv0             Write fields: bias_en|bias_ctl
	{.op = WRITE, .args = {.write = {0x1208UL, 0x00000035}} },	 // saphir/bias/bgen_ctrl                   Write fields: burst_transfer_hv_bank_0|burst_transfer_lv_bank_0
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en
	{.op = WRITE, .args = {.write = {0x009008, 0x00000184}} },
	{.op = WRITE, .args = {.write = {0x009008, 0x00010184}} },
	{.op = WRITE, .args = {.write = {0x000200, 0x00000000}} },
	{.op = WRITE, .args = {.write = {0x000214, 0x00000440}} },
	{.op = WRITE, .args = {.write = {0x000214, 0x00000441}} },
	{.op = WRITE, .args = {.write = {0x000204, 0x00000634}} },
	{.op = WRITE, .args = {.write = {0x00020C, 0x00000061}} },
	{.op = WRITE, .args = {.write = {0x000204, 0x00000635}} },
	{.op = WRITE, .args = {.write = {0x000204, 0x00000637}} },
	{.op = WRITE, .args = {.write = {0x000210, 0x00000C00}} },
	{.op = WRITE, .args = {.write = {0x00B00C, 0x00000004}} },
	{.op = WRITE, .args = {.write = {0x009008, 0x00000304}} },
	{.op = WRITE, .args = {.write = {0x00004C, 0x00209600}} }
};
static const struct reg_op dcmi_start_seq[] = {
	{.op = WRITE, .args = {.write = {0x8000UL, 0x0001FEA1}} },	 // saphir/cpi/pipeline_control
	{.op = WRITE, .args = {.write = {0x9028UL, 0x00000000}} },	 // saphir/ro/ro_lp_ctrl                    Write fields: lp_output_disable
	{.op = WRITE, .args = {.write = {0x9008UL, 0x00000305}} },	 // saphir/ro/time_base_ctrl                Write fields: time_base_enable
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x002CUL, 0x0022C724}} },	 // saphir/ro_td_ctrl                       Write fields: ro_td_ack_y_rstn|ro_td_arb_y_rstn|ro_td_addr_y_rstn|ro_td_sendreq_y_rstn
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000C02}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|px_sw_rstn
	{.op = DELAY, .args = {.delay = {1000}} }	 // us
};
static const struct reg_op dcmi_stop_seq[] = {
	{.op = WRITE, .args = {.write = {0x0000UL, 0x00000802}} },	 // saphir/roi_ctrl                         Write fields: roi_td_en|px_sw_rstn
	{.op = WRITE, .args = {.write = {0x002CUL, 0x00200624}} },	 // saphir/ro_td_ctrl                       Write fields: ro_td_ack_y_rstn|ro_td_arb_y_rstn|ro_td_addr_y_rstn|ro_td_sendreq_y_rstn
	{.op = WRITE, .args = {.write = {0x9028UL, 0x00000002}} },	 // saphir/ro/ro_lp_ctrl                    Write fields: lp_output_disable|lp_keep_th
	{.op = DELAY, .args = {.delay = {1000}} },	 // us
	{.op = WRITE, .args = {.write = {0x9008UL, 0x00000304}} },	 // saphir/ro/time_base_ctrl                Write fields: time_base_enable
	{.op = WRITE, .args = {.write = {0x8000UL, 0x0001FFA2}} },	 // saphir/cpi/pipeline_control             Write fields: enable|drop_nbackpressure|hot_disable_enable
	{.op = READ, .args = {.read = {0x8004UL, 0x00000002}} },	 // saphir/cpi/pipeline_status              Read fields: busy
	{.op = DELAY, .args = {.delay = {1000}} }	 // us
};
static const struct reg_op dcmi_destroy_seq[] = {
};
const struct issd dcmi_histo = {
	.init = {
		.ops = dcmi_init_seq,
		.len = ARRAY_SIZE(dcmi_init_seq)},
	.start = {
		.ops = dcmi_start_seq,
		.len = ARRAY_SIZE(dcmi_start_seq)},
	.stop = {
		.ops = dcmi_stop_seq,
		.len = ARRAY_SIZE(dcmi_stop_seq)},
	.destroy = {
		.ops = dcmi_destroy_seq,
		.len = ARRAY_SIZE(dcmi_destroy_seq)},
};

#endif
