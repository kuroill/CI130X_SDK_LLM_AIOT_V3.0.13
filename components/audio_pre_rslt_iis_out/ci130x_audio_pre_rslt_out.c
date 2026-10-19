#include "codec_manager.h"
#include "ci130x_audio_pre_rslt_out.h"
#include <string.h>
#include "ci130x_codec.h"
#include "ci130x_dpmu.h"
#include "board.h"
#include "ci130x_gpio.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "sdk_default_config.h"
#include "ci130x_uart.h"
#include "ci130x_dma.h"
#include "debug_time_consuming.h"
#include "stream_buffer.h"
#include "cias_voice_upload.h"
#include "cias_aiot_protocol.h"
#include "status_share.h"
#include "user_config.h"
#include "ci_audio_wrapfft.h"
#include "ai_uart_i2s_protocol.h"
#include "ci130x_core_misc.h"
#include "system_msg_deal.h"
#if AUTO_GET_AEC_GAIN
#include "alg_preprocess.h"
#include "cias_network_msg_protocol.h"
#include "es7243e.h"
#endif
#if USE_NN_DENOISE && DENOISE_STRENGTH_ADAPT_ENABLE
#include "noise_energy_estimation_api.h"
#endif
#define BUFFER_NUM (4)
typedef struct
{
    audio_pre_rslt_out_init_t init_str;
    // 写了多少次数据
    uint32_t write_data_cnt;
    // 已经发送了多少次数据，写数据频次是固定的，我们不能改变，只能改变发送数据的频度，
    // 如果发送太快，会在mute模式再发送上一个buf，
    // 如果发送太慢，会扔掉一个buf不发送
    uint32_t send_data_cnt;

    int32_t write_send_sub_slave; // 在slave模式下，write和send的差值
    bool codec_start_flag[MAX_CODEC_NUM];        //codec启动标志，需缓存9帧否则会出现buffer无数据填0的情况
    // 是否使用硬件tx merge功能
    bool hardware_tx_merge;
} audio_pre_init_tmp_t;

volatile uint8_t uart_dma_trans_done = 1;

static void uart_dma_read_irq_callback(void)
{
    uart_dma_trans_done = 1;
}

static audio_pre_init_tmp_t sg_init_tmp_str;
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
/* The mailbox producer may run in an ISR. A binary gate serializes its buffer
 * borrow with task-owned start/cancel without using an RTOS mutex in the ISR. */
static SemaphoreHandle_t uplink_gate;
/* One heartbeat task publishes start work; the system task consumes it. */
static volatile uint8_t uplink_start_event_queued;

static BaseType_t uplink_try_take(void)
{
    if(!uplink_gate)
    {
        return pdFALSE;
    }
    if(check_curr_trap())
    {
        return xSemaphoreTakeFromISR(uplink_gate, NULL);
    }
    return xSemaphoreTake(uplink_gate, 0);
}

static void uplink_give(void)
{
    if(check_curr_trap())
    {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xSemaphoreGiveFromISR(uplink_gate, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
    else
    {
        xSemaphoreGive(uplink_gate);
    }
}
#endif

#define PI (3.1416926f)
void sine_wave_generate(int16_t *sine_wave, uint32_t sample_rate, uint32_t wave_fre, uint32_t point_num)
{
    int16_t num_of_one_period = sample_rate / wave_fre; // 每个周期的采样点数
    for (int i = 0; i < point_num; i++)
    {
        sine_wave[i] = (int16_t)(32767.0f * sinf((2 * PI * i) / num_of_one_period));
    }
}
#if AUTO_GET_AEC_GAIN
extern CiasAiotFuncParamTypedef gCiasAiotFuncParam;
extern cm_codec_hw_info_t host_mic_hw_info;
extern audio_capture_t audio_capture;
extern void es7243e_alc_maxgain_set(float gain);
extern int8_t es7243e_alc_gate_en(es7243e_alc_gate_t ALC_gate);
void auto_get_aec_gain(ci_wrapfft_audio * p_wrapfft_audio)
{
    int64_t eng_mic_eng= 0;
    int64_t eng_ref_eng= 0;
    static uint8_t volume = 0;
    uint16_t real_len;
    uint8_t test_rslt = 0;
    static int32_t sum_mic_eng =0;
    static int32_t sum_ref_eng =0;
    static uint8_t cnt =0;
    static uint8_t err_cnt =0;
    static uint8_t frame_cnt =0;
    static int8_t pga_gain_mic = 0;
    static int8_t pga_gain_ref = 0;
    static float  aec_enable_thr = 0;
    static bool calc_flag = false;
    //获取当前mic和ref增益
    if(pga_gain_mic == 0)
    pga_gain_mic = (int8_t)host_mic_hw_info.codec_gain.pga_gain_l;
    if(pga_gain_ref == 0)
    {
        #if !REF_IN_FROM_INNER_CODEC
        if(audio_capture.mic_channel_num == 2)
        {
            extern float es7243e_init_gain;
            pga_gain_ref = (int8_t)es7243e_init_gain;
        }
        else
        #endif
        pga_gain_ref = (int8_t)host_mic_hw_info.codec_gain.pga_gain_r;
    }

    do
    {
        if(gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_MIC)
        {
            if(volume == 0)
            volume = vol_get();
            if (CI_SS_PLAY_STATE_PLAYING == ciss_get(CI_SS_PLAY_STATE))
            {
                calc_flag = true;
                if(cnt >= 60 && frame_cnt<150)
                {
                    for(int i = 0; i < AUDIO_CAP_POINT_NUM_PER_FRM; i++)
                    {
                        eng_mic_eng += p_wrapfft_audio->mic[0][i] * p_wrapfft_audio->mic[0][i];
                    }
                    sum_mic_eng += sqrt(eng_mic_eng/AUDIO_CAP_POINT_NUM_PER_FRM); 
                    frame_cnt++;
                }
                else
                cnt++;
            }
            else if(CI_SS_PLAY_STATE_IDLE == ciss_get(CI_SS_PLAY_STATE) && calc_flag)
            {
                if(sum_mic_eng != 0)
                {
                    if(sum_mic_eng/frame_cnt > ENERGE_UP_THR)
                    {
                        mprintf("cur eng = %d\r\n",sum_mic_eng/frame_cnt);
                        if(pga_gain_mic >= (gCiasAiotFuncParam.aec_min_mic_gain+2))
                        {
                            pga_gain_mic -= 2;
                            mprintf("pga_gain_mic =%d\r\n",pga_gain_mic);
                            cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_LEFT, pga_gain_mic);
                        }
                        else if(volume > VOLUME_MIN)
                        {
                            volume -= 1;
                            mprintf("volume =%d\r\n",volume);
                            audio_play_set_vol_gain(67*volume/VOLUME_MAX + 7);
                        }
                        else
                        {
                            test_rslt = 0;
                            cias_send_cmd_and_data(CIAS_AEC_GAIN_RESULT, &test_rslt, 1, DEF_FILL);
                        }                   
                    }
                    else
                    {
                        mprintf("dst eng = %d\r\n",sum_mic_eng/frame_cnt);
                        gCiasAiotFuncParam.aec_gain_get_state = GET_AEC_GAIN_REF;
                        err_cnt = 0;
                    }
                    sum_mic_eng =0;
                    frame_cnt =0;
                    cnt = 0;
                    prompt_play_by_voice_id(GET_AEC_GAIN_AUDIO_ID, NULL, true);  //播放测试音频
                }
                else
                {
                    err_cnt++;
                    break;
                }
                calc_flag = false;
            }  
        }
        if(gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_REF)
        {
            if (CI_SS_PLAY_STATE_PLAYING == ciss_get(CI_SS_PLAY_STATE))
            {
                calc_flag = true;
                if(cnt >= 60 && frame_cnt<150)
                {
                    for(int i = 0; i < AUDIO_CAP_POINT_NUM_PER_FRM; i++)
                    {
                        eng_mic_eng += p_wrapfft_audio->mic[0][i] * p_wrapfft_audio->mic[0][i];
                        eng_ref_eng += p_wrapfft_audio->ref[0][i] * p_wrapfft_audio->ref[0][i];
                    }
                    sum_mic_eng += sqrt(eng_mic_eng/AUDIO_CAP_POINT_NUM_PER_FRM); 
                    sum_ref_eng += sqrt(eng_ref_eng/AUDIO_CAP_POINT_NUM_PER_FRM);
                    frame_cnt++;
                }
                else
                cnt++;
            }
            else if(CI_SS_PLAY_STATE_IDLE == ciss_get(CI_SS_PLAY_STATE) && calc_flag)
            {
                if(sum_mic_eng != 0 && sum_ref_eng != 0)
                {
                    // mprintf("sum_ref_eng =%d\r\n",sum_ref_eng/frame_cnt);
                    // mprintf("sum_mic_eng =%d\r\n",sum_mic_eng/frame_cnt);
                    mprintf("GET_AEC_GAIN_REF\r\n");
                    if((float)sum_mic_eng/sum_ref_eng > 6.0f)
                    {
                        pga_gain_ref += 2;
                        mprintf("pga_gain_ref =%d\r\n",pga_gain_ref);
                        #if !REF_IN_FROM_INNER_CODEC
                        if(audio_capture.mic_channel_num == 2)
                        {
                            es7243e_alc_gate_en(ES7243E_ALC_ON);
                            es7243e_alc_maxgain_set((float)pga_gain_ref);
                            es7243e_alc_gate_en(ES7243E_ALC_OFF);
                        }
                        else
                        #endif
                        {
                            cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, pga_gain_ref);
                        }
                        prompt_play_by_voice_id(GET_AEC_GAIN_AUDIO_ID, NULL, true);  //播放测试音频
                    }
                    else if(sum_ref_eng/frame_cnt >= ENERGE_UP_THR)
                    {
                        pga_gain_ref -= 2;
                        mprintf("pga_gain_ref =%d\r\n",pga_gain_ref);
                        #if !REF_IN_FROM_INNER_CODEC
                        if(audio_capture.mic_channel_num == 2)
                        {
                            es7243e_alc_gate_en(ES7243E_ALC_ON);
                            es7243e_alc_maxgain_set((float)pga_gain_ref);
                            es7243e_alc_gate_en(ES7243E_ALC_OFF);
                        }
                        else
                        #endif
                        {
                            cm_set_codec_adc_gain(HOST_MIC_RECORD_CODEC_ID, CM_CHA_RIGHT, pga_gain_ref);
                        }
                        prompt_play_by_voice_id(GET_AEC_GAIN_AUDIO_ID, NULL, true);  //播放测试音频
                    }
                    else
                    {
                        gCiasAiotFuncParam.aec_gain_get_state = GET_AEC_GAIN_REF_THR;
                        err_cnt = 0;
                        audio_play_set_vol_gain(67*5/VOLUME_MAX + 7);
                        prompt_play_by_voice_id(GET_AEC_REF_THR_AUDIO_ID, NULL, true);  //播放测试音频
                    }
                    sum_mic_eng =0;
                    frame_cnt =0;
                    cnt = 0;
                    
                }
                else
                {
                    err_cnt++;
                    break;
                }
                calc_flag = false;
            }  
        }
        if(gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_REF_THR)
        {
            if (CI_SS_PLAY_STATE_PLAYING == ciss_get(CI_SS_PLAY_STATE))
            {
                calc_flag = true;
                if(cnt >= 60 && frame_cnt<150)
                {
                    if (aec_enable_thr < 0.001f)
                    {
                        aec_enable_thr = p_wrapfft_audio->cur_aec_ref_eng;
                    }
                    if(aec_enable_thr > p_wrapfft_audio->cur_aec_ref_eng)
                    {
                        aec_enable_thr = p_wrapfft_audio->cur_aec_ref_eng;
                    }
                    frame_cnt++;
                }
                cnt++;
            }
            else if(CI_SS_PLAY_STATE_IDLE == ciss_get(CI_SS_PLAY_STATE) && calc_flag)
            {
                if(aec_enable_thr > 0.001f)
                {
                    mprintf("pga_gain_mic =%d\r\n",pga_gain_mic);
                    mprintf("pga_gain_ref =%d\r\n",pga_gain_ref);
                    mprintf("volume =%d\r\n",volume);
                    mprintf("aec_enable_thr =%d\r\n",(int32_t)aec_enable_thr);
                    uint8_t gain_rslt[7] = {0};
                    gain_rslt[0] = pga_gain_mic;
                    gain_rslt[1] = pga_gain_ref;
                    gain_rslt[2] = volume;
                    gain_rslt[3] = ((int32_t)aec_enable_thr & 0xff);
                    gain_rslt[4] = (((int32_t)aec_enable_thr >> 8) & 0xff);
                    gain_rslt[5] = (((int32_t)aec_enable_thr >> 16) & 0xff);
                    gain_rslt[6] = (((int32_t)aec_enable_thr >> 24) & 0xff);
                    cias_send_cmd_and_data(CIAS_AEC_GAIN_RESULT, &gain_rslt, 7, DEF_FILL);
                    gCiasAiotFuncParam.aec_gain_get_state = GET_AEC_GAIN_IDLE;
                    err_cnt = 0;
                    frame_cnt =0;
                    cnt = 0;
                    aec_enable_thr = 0.0f;
                    pga_gain_ref =0;
                    pga_gain_mic =0;
                    audio_play_set_vol_gain(67*volume/VOLUME_MAX + 7);
                }
                else
                {
                    err_cnt++;
                    break;
                }
                calc_flag = false;
            }
        }
    }while (0);

    if(gCiasAiotFuncParam.aec_gain_get_state != GET_AEC_GAIN_IDLE)
    {
        if(err_cnt >= 3)
        {
            mprintf("aec test err\r\n");
            test_rslt = 0;
            err_cnt = 0;
            frame_cnt =0;
            cnt = 0;
            aec_enable_thr = 0.0f;
            pga_gain_ref =0;
            pga_gain_mic =0;
            cias_send_cmd_and_data(CIAS_AEC_GAIN_RESULT, &test_rslt, 1, DEF_FILL);
            gCiasAiotFuncParam.aec_gain_get_state = GET_AEC_GAIN_IDLE;
        }
        else if(err_cnt != 0 && calc_flag && (CI_SS_PLAY_STATE_IDLE == ciss_get(CI_SS_PLAY_STATE)))
        {
            if(gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_MIC || gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_REF)
            prompt_play_by_voice_id(GET_AEC_GAIN_AUDIO_ID, NULL, true);  //播放测试音频
            if(gCiasAiotFuncParam.aec_gain_get_state == GET_AEC_GAIN_REF_THR)
            prompt_play_by_voice_id(GET_AEC_REF_THR_AUDIO_ID, NULL, true);  //播放测试音频
            calc_flag = false;
        }
        update_awake_time(); // 更新本地唤醒时间
    }
    
}
#endif
#if DEBUG_AUDIO_UART_UPLOAD_EN
StreamBufferHandle_t gUartRecordStreamBuffer = NULL; // 串口采音队列
void audio_pre_rslt_write_data_from_uart(uint32_t addr, uint32_t size)
{
    // init_timer0();
    // timer0_start_count();
    if (!uart_dma_trans_done)
    {
        // mprintf("pre voice data overflow\r\n");
    }
    uart_dma_trans_done = 0;
    extern void set_dma_int_callback(DMACChannelx dmachannel, dma_callback_func_ptr_t func);
    set_dma_int_callback(DMACChannel1, uart_dma_read_irq_callback);

    DMAC_Peripherals uart_dma_peripherals_num = DMAC_Peripherals_UART0_TX;
    uint32_t uart_fifo_addr = UART0FIFO_BASE;
    if (((uint32_t)USE_UART_SEND_PRE_RSLT_AUDIO_NUMBER == (uint32_t)UART1))
    {
        uart_dma_peripherals_num = DMAC_Peripherals_UART1_TX;
        uart_fifo_addr = UART1FIFO_BASE;
    }
    else if (((uint32_t)USE_UART_SEND_PRE_RSLT_AUDIO_NUMBER == (uint32_t)UART2))
    {
        uart_fifo_addr = UART2FIFO_BASE;
        uart_dma_peripherals_num = DMAC_Peripherals_UART2_TX;
    }

    DMAC_M2P_P2M_advance_config(DMACChannel1, uart_dma_peripherals_num, M2P_DMA, addr, uart_fifo_addr, size,
                                TRANSFERWIDTH_8b, BURSTSIZE1, DMAC_AHBMaster1);
    // while(!uart_dma_trans_done);
    // uart_dma_trans_done = 0;
    // timer0_end_count_only_print_time_us();
}

// 串口录音任务
void uart_record_task(void *p_arg)
{
    uint16_t stream_aviable_len = 0;
    int8_t record_rx_temp[AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t)] = {0};
    int count = 0;
    int8_t *p;
    while (1)
    {
        stream_aviable_len = xStreamBufferBytesAvailable(gUartRecordStreamBuffer);
        if (stream_aviable_len >= AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t))
        {
            // ci_loginfo(LOG_USER,"zt test1\r\n");
            memset(record_rx_temp, 0, AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t));
            int rx_size = xStreamBufferReceiveFromISR(gUartRecordStreamBuffer, record_rx_temp, AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t), portMAX_DELAY);
            if (rx_size > 0)
            {
                audio_pre_rslt_write_data_from_uart((uint32_t)record_rx_temp, rx_size);
            }
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(3));
        }
    }
}
void uart_send_voice_init(void)
{
    UARTDMAConfig((UART_TypeDef *)USE_UART_SEND_PRE_RSLT_AUDIO_NUMBER, USE_UART_SEND_PRE_RSLT_AUDIO_BAUD);
    gUartRecordStreamBuffer = xStreamBufferCreate(USE_UART_SEND_PRE_RSLT_AUDIO_BUF_LEN, 1);
    if(gUartRecordStreamBuffer)
    {
        xTaskCreate(uart_record_task, "uart_record_task", AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t) + 32, NULL, 4, NULL);
    }
    else
    {
        mprintf("uart record init error\r\n");
    }
}
#endif
uint32_t tmp_voice_addr = 0;

uint8_t *uart_send_pre_data_buffer = NULL;
void audio_pre_rslt_out_play_card_init(void)
{
    uint16_t block_size = AUDIO_CAP_POINT_NUM_PER_FRM * 2 * sizeof(int16_t);

    sg_init_tmp_str.init_str.block_size = block_size;
    sg_init_tmp_str.write_data_cnt = 0;
    sg_init_tmp_str.send_data_cnt = 0;
    memset(sg_init_tmp_str.codec_start_flag,false,MAX_CODEC_NUM);
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    uplink_gate = xSemaphoreCreateBinary();
    CI_ASSERT(uplink_gate, "uplink lifecycle gate\r\n");
    /* Keep the gate empty until codec queues and hardware are initialized. */
#endif
#if DEBUG_AUDIO_UART_UPLOAD_EN
    uart_send_voice_init();
    tmp_voice_addr = pvPortMalloc(block_size);
    CI_ASSERT(tmp_voice_addr, "\r\n");
#endif

#if AUDIO_DATA_PLAY_BY_IIS || USE_HP_OUT_NET_AUDIO || USE_HP_OUT_PRE_RSLT_AUDIO
    audio_pre_rslt_out_codec_init_pa_out();
#endif

#if USE_IIS1_OUT_PRE_RSLT_AUDIO || AUDIO_DATA_UPLOAD_BY_IIS
    audio_pre_rslt_out_codec_init();
#endif

#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    xSemaphoreGive(uplink_gate);
#endif
#if AI_UART_CONTROL_EN
    ai_uart_i2s_on_audio_ready();
#endif

}
void codec_output_ctl(int codec_index)
{
    if (MAX_CODEC_NUM <= codec_index)
    {
        mprintf("codec_index err\r\n");
        return;
    }
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    if(PLAY_PRE_AUDIO_CODEC_ID == codec_index)
    {
        /* Only the system-message owner may change external slave TX state. */
        return;
    }
#endif
    int cnt = cm_get_codec_busy_buffer_number(codec_index,CODEC_OUTPUT);
    if (!sg_init_tmp_str.codec_start_flag[codec_index])
    {
        /*busy buffer 大于10帧再开启codec，否则会出现数据量不够的情况*/
        if(cnt > 10)
        {
            mprintf("--%d codec start--\r\n",codec_index);
            cm_start_codec(codec_index, CODEC_OUTPUT);
            sg_init_tmp_str.codec_start_flag[codec_index] = true;
        }
    }
    else
    {
        if(cnt == 0)
        {
            mprintf("--%d codec stop--\r\n",codec_index);
            cm_stop_codec(codec_index, CODEC_OUTPUT);
            sg_init_tmp_str.codec_start_flag[codec_index] = false;
        }
    }
}

/* Called by the existing heartbeat task, never by the mailbox producer.
 * The codec owner applies prefill once. Pending starts must keep progressing
 * with both a full pool and an empty queue after DMA has consumed the prefill. */
void audio_pre_rslt_request_start(void)
{
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    CI_ASSERT(!check_curr_trap(), "uplink start request requires task context\r\n");
    if(uplink_start_event_queued || !ai_uart_i2s_peer_ready() ||
       (pdTRUE != uplink_try_take()))
    {
        return;
    }
    int needed = !sg_init_tmp_str.codec_start_flag[PLAY_PRE_AUDIO_CODEC_ID];
    if(needed)
    {
        /* Set before enqueue: the system owner can preempt the sender. */
        uplink_start_event_queued = 1;
    }
    uplink_give();
    if(needed)
    {
        sys_msg_t event;
        memset(&event, 0, sizeof(event));
        event.msg_type = SYS_MSG_TYPE_AI_I2S_UPLINK_START;
        if(pdPASS != send_msg_to_sys_task(&event, NULL))
        {
            uplink_start_event_queued = 0;
        }
    }
#endif
}

void audio_pre_rslt_handle_start(void)
{
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    CI_ASSERT(!check_curr_trap(), "uplink start owner requires task context\r\n");
    if(ai_uart_i2s_peer_ready() && (pdTRUE == uplink_try_take()))
    {
        int cnt = cm_get_codec_busy_buffer_number(PLAY_PRE_AUDIO_CODEC_ID, CODEC_OUTPUT);
        if(ai_uart_i2s_peer_ready() &&
           !sg_init_tmp_str.codec_start_flag[PLAY_PRE_AUDIO_CODEC_ID])
        {
            int result = cm_try_start_slave_output(PLAY_PRE_AUDIO_CODEC_ID, 11);
            if(result == 0)
            {
                sg_init_tmp_str.codec_start_flag[PLAY_PRE_AUDIO_CODEC_ID] = true;
                mprintf("[AI_I2S] continuous uplink started bufferedFrames=%d owner=system context=task\r\n", cnt);
            }
            else if(result < 0)
            {
                mprintf("[AI_I2S] start failed codec=%d reason=%s owner=system\r\n",
                    PLAY_PRE_AUDIO_CODEC_ID,
                    (result == -2) ? "interrupt_context" : "invalid_slave_output");
            }
        }
        uplink_give();
    }
    uplink_start_event_queued = 0;
#endif
}

extern CiasAiotRunParamTypedef gCiasAiotRunParam;
extern CiasAiotFuncParamTypedef gCiasAiotFuncParam;
#if IIS_CHANNEL_ENG_CALC_EANBLE
/*计算能量*/
void alg_calc_audio_eng(short *p_audio_pcm_data, uint16_t audio_fft_data_len, uint32_t *p_dst_db, uint32_t *P_dst_eng)
{
    float eng_tmp_avg = 0.0f, eng_tmp = 0.0f, dst_db_cur = 0.0f;
    for(int i = 0; i < audio_fft_data_len; i++)
    {
        eng_tmp += p_audio_pcm_data[i]*p_audio_pcm_data[i];
    }
    eng_tmp_avg = sqrtf(eng_tmp/audio_fft_data_len/32767.0f);
    *p_dst_db = (uint32_t)(20*log10f(eng_tmp_avg/6.0f) + 102.0f);
    *P_dst_eng = sqrt(eng_tmp/audio_fft_data_len);
}

static void audio_eng_calc(ci_wrapfft_audio * p_wrapfft_audio)
{
    static int calc_eng_frame_interval = 0; 
    if(calc_eng_frame_interval++ == ENG_CALC_INTERVAL_FRAME)
    {
        calc_eng_frame_interval = 0;
        //计算左mic能量和DB
        if(gCiasAiotFuncParam.micl_eng_db_calc_flag)
        {
            alg_calc_audio_eng(p_wrapfft_audio->mic[0], AUDIO_CAP_POINT_NUM_PER_FRM, &gCiasAiotFuncParam.micl_db, &gCiasAiotFuncParam.micl_eng);
            //mprintf("micl dst_db = %d\r\n", gCiasAiotFuncParam.micl_db);
           // mprintf("micl dst_eng = %d\r\n", gCiasAiotFuncParam.micl_eng);
        }
        //计算右mic能量和DB
        if(gCiasAiotFuncParam.micr_eng_db_calc_flag)
        {
            alg_calc_audio_eng(p_wrapfft_audio->mic[1], AUDIO_CAP_POINT_NUM_PER_FRM, &gCiasAiotFuncParam.micr_db, &gCiasAiotFuncParam.micr_eng);
           // mprintf("micr dst_db = %d\r\n", gCiasAiotFuncParam.micr_db);
           // mprintf("micr dst_eng = %d\r\n", gCiasAiotFuncParam.micr_eng);
        }
        //计算参考信号左通道能量和DB
        if(gCiasAiotFuncParam.refl_eng_db_calc_flag)
        {
            alg_calc_audio_eng(p_wrapfft_audio->ref[0], AUDIO_CAP_POINT_NUM_PER_FRM, &gCiasAiotFuncParam.refl_db, &gCiasAiotFuncParam.refl_eng);
           // mprintf("refl dst_db = %d\r\n", gCiasAiotFuncParam.refl_db);
           // mprintf("refl dst_eng = %d\r\n", gCiasAiotFuncParam.refl_eng);
        }
        //计算参考信号右通道能量和DB
        if(gCiasAiotFuncParam.refr_eng_db_calc_flag)
        {
            alg_calc_audio_eng(p_wrapfft_audio->ref[1], AUDIO_CAP_POINT_NUM_PER_FRM, &gCiasAiotFuncParam.refr_db, &gCiasAiotFuncParam.refr_eng);
          //  mprintf("refr dst_db = %d\r\n", gCiasAiotFuncParam.refr_db);
          //  mprintf("refr dst_eng = %d\r\n", gCiasAiotFuncParam.refr_eng);
        }
    }   
}
#endif

#if DEBUG_AUDIO_UART_UPLOAD_EN
void audio_debug_uart0_upload(int16_t *left, int16_t *right, ci_wrapfft_audio *p_wrapfft_audio)
{
    uint32_t block_size = sg_init_tmp_str.init_str.block_size;
    int16_t *pcm_data_p = (int16_t *)tmp_voice_addr;
    int num = block_size / sizeof(int16_t) / 2;

    // static int cnt = 0;

    // if (cnt == 30)
    // {
    //     uart_send_voice_init();
    // }
    // else if (cnt >= 30)
    // {

        // audio_pre_rslt_write_data_from_uart((uint32_t)tmp_voice_addr,block_size);
        // #if USE_UART_SEND_PRE_RSLT_RECORD_LEFT
        // int8_t *src_addr = (int8_t *)left;
        // #else
        // int8_t *src_addr = (int8_t *)right;
        // #endif
        for (int i = 0; i < num; i++)
        {
            pcm_data_p[2 * i] = left[i];
            pcm_data_p[2 * i + 1] = right[i];
        }
        if (gUartRecordStreamBuffer)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            int ret = xStreamBufferSendFromISR(gUartRecordStreamBuffer, (int8_t *)pcm_data_p, block_size, &xHigherPriorityTaskWoken);
            if (ret != block_size)
            {
                ci_loginfo(LOG_USER,"zt test2\r\n");
                mprintf("xSpeexRecordStreamBuffer send error, send len = %d\r\n", ret);
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    // }
    // else
    // cnt++;
}
#endif
#if USE_AUDIO_UPLOAD_BY_IIS
/* One mailbox/audio producer collects a complete window. A bounded snapshot
 * ring transfers diagnostics to the existing heartbeat task: no UART, heap or
 * blocking call is allowed in the audio/mailbox interrupt. */
#define AEC_PATH_REPORT_CAPACITY 4U
#define AEC_PATH_FIRST_WINDOW_FRAMES 8U
#define AEC_PATH_WINDOW_FRAMES 64U
#define AEC_REF_SNAPSHOT_SAMPLES 64U
typedef struct
{
    uint32_t frames;
    uint32_t samples;
    uint32_t aec_frames;
    uint64_t abs_sum[3];
    int64_t sum[3];
    uint32_t peak[3];
    uint32_t clipped[3];
}aec_path_window_t;

typedef struct
{
    aec_path_window_t window;
    uint32_t playback;
    uint32_t total_frames;
    uint32_t phase; /* 0=first, 1=playing, 2=end */
    uint32_t pga_reg[2];
    uint32_t adc_digital[2];
    uint32_t alc[2];
    uint32_t pga_control_mask;
    uint32_t dac_analog;
    uint32_t dac_digital;
    uint32_t ref_frame, ref_abs;
    uint32_t ref_samples;
    int16_t ref_pcm[AEC_REF_SNAPSHOT_SAMPLES];
}aec_path_report_t;

static uint32_t aec_ref_frame, aec_ref_abs, aec_ref_samples;
static int16_t aec_ref_pcm[AEC_REF_SNAPSHOT_SAMPLES];
static aec_path_window_t aec_path_window;
static uint32_t aec_path_playback;
static uint32_t aec_path_total_frames;
static aec_path_report_t aec_path_reports[AEC_PATH_REPORT_CAPACITY];
static volatile uint32_t aec_path_report_head;
static volatile uint32_t aec_path_report_count;
static volatile uint32_t aec_path_dropped_reports;

static void queue_aec_path_report(uint32_t phase)
{
    aec_path_report_t report = {0};
    report.window = aec_path_window;
    report.playback = aec_path_playback;
    report.total_frames = aec_path_total_frames;
    report.phase = phase;
    if(phase == 2U)
    {
        report.ref_frame = aec_ref_frame;
        report.ref_abs = aec_ref_abs;
        report.ref_samples = aec_ref_samples;
        memcpy(report.ref_pcm, aec_ref_pcm, sizeof(report.ref_pcm));
    }
    /* Preserve the previous interrupt mask in both task and mailbox contexts.
     * Only snapshot copies/register reads are inside this short critical section. */
    UBaseType_t saved_mask = portSET_INTERRUPT_MASK_FROM_ISR();
    if(aec_path_report_count < AEC_PATH_REPORT_CAPACITY)
    {
        for(uint32_t i = 0; i < 2U; i++)
        {
            report.pga_reg[i] = CODEC->pga_gain_reg[i];
            report.adc_digital[i] = CODEC->adc_dig_gain_reg[i] & 0xffU;
            report.alc[i] = (CODEC->alc_reg[i].reg49 >> 6) & 1U;
        }
        report.pga_control_mask = (CODEC->rega >> 4) & 3U;
        report.dac_analog = CODEC->reg2d & 0x1fU;
        report.dac_digital = CODEC->reg6 & 0xffU;
        uint32_t tail = (aec_path_report_head + aec_path_report_count) % AEC_PATH_REPORT_CAPACITY;
        aec_path_reports[tail] = report;
        aec_path_report_count++;
    }
    else
    {
        aec_path_dropped_reports++;
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR(saved_mask);
    memset(&aec_path_window, 0, sizeof(aec_path_window));
}

static void report_aec_path_levels(ci_wrapfft_audio *audio, uint32_t sample_count)
{
    if(CI_SS_PLAY_STATE_PLAYING != ciss_get(CI_SS_PLAY_STATE))
    {
        if(aec_path_window.samples || (aec_path_total_frames && aec_ref_samples))
        {
            queue_aec_path_report(2U);
        }
        aec_path_total_frames = 0;
        return;
    }
    if(NULL == audio || NULL == audio->mic[0] || NULL == audio->ref[0] ||
       NULL == audio->dst[0] || 0 == sample_count)
    {
        return;
    }
    if(aec_path_total_frames == 0)
    {
        aec_path_playback++;
        aec_ref_frame = aec_ref_abs = aec_ref_samples = 0;
    }
    uint32_t ref_abs = 0, ref_peak = 0, ref_peak_index = 0;
    const int16_t *channels[3] = {audio->mic[0], audio->ref[0], audio->dst[0]};
    for(uint32_t i = 0; i < sample_count; i++)
    {
        for(uint32_t channel = 0; channel < 3U; channel++)
        {
            int32_t value = channels[channel][i];
            uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
            if(channel == 1U)
            {
                ref_abs += magnitude;
                if(magnitude > ref_peak)
                {
                    ref_peak = magnitude;
                    ref_peak_index = i;
                }
            }
            aec_path_window.abs_sum[channel] += magnitude;
            aec_path_window.sum[channel] += value;
            if(magnitude > aec_path_window.peak[channel])
            {
                aec_path_window.peak[channel] = magnitude;
            }
            if(value == -32768 || value == 32767)
            {
                aec_path_window.clipped[channel]++;
            }
        }
    }
    /* One original REF waveform from the strongest post-startup frame.
     * It is PA-feedback ADC evidence, not the PCM queued into the DAC. */
    if(aec_path_total_frames >= AEC_PATH_FIRST_WINDOW_FRAMES &&
       sample_count >= AEC_REF_SNAPSHOT_SAMPLES && ref_abs >= aec_ref_abs)
    {
        uint32_t start = ref_peak_index > AEC_REF_SNAPSHOT_SAMPLES / 2U ?
            ref_peak_index - AEC_REF_SNAPSHOT_SAMPLES / 2U : 0U;
        if(start > sample_count - AEC_REF_SNAPSHOT_SAMPLES)
        {
            start = sample_count - AEC_REF_SNAPSHOT_SAMPLES;
        }
        memcpy(aec_ref_pcm, audio->ref[0] + start, sizeof(aec_ref_pcm));
        aec_ref_frame = aec_path_total_frames + 1U;
        aec_ref_abs = ref_abs;
        aec_ref_samples = AEC_REF_SNAPSHOT_SAMPLES;
    }
    aec_path_window.samples += sample_count;
    aec_path_window.frames++;
    aec_path_window.aec_frames += ciss_get(CI_SS_AEC_WORK_STATE) ? 1U : 0U;
    aec_path_total_frames++;
    if(aec_path_total_frames == AEC_PATH_FIRST_WINDOW_FRAMES ||
       aec_path_window.frames >= AEC_PATH_WINDOW_FRAMES)
    {
        queue_aec_path_report(aec_path_total_frames == AEC_PATH_FIRST_WINDOW_FRAMES ? 0U : 1U);
    }
}

void audio_pre_rslt_report_diagnostics(void)
{
    if(check_curr_trap())
    {
        return;
    }
    aec_path_report_t report;
    uint32_t dropped;
    UBaseType_t saved_mask = portSET_INTERRUPT_MASK_FROM_ISR();
    if(aec_path_report_count == 0)
    {
        portCLEAR_INTERRUPT_MASK_FROM_ISR(saved_mask);
        return;
    }
    report = aec_path_reports[aec_path_report_head];
    aec_path_report_head = (aec_path_report_head + 1U) % AEC_PATH_REPORT_CAPACITY;
    aec_path_report_count--;
    dropped = aec_path_dropped_reports;
    portCLEAR_INTERRUPT_MASK_FROM_ISR(saved_mask);

    const aec_path_window_t *w = &report.window;
    const char *phase = report.phase == 0U ? "first" : (report.phase == 1U ? "playing" : "end");
    if(w->samples)
    {
        mprintf("[AEC_PATH] playback=%lu phase=%s frames=%lu totalFrames=%lu samples=%lu aecFrames=%lu micAbs=%lu refAbs=%lu dstAbs=%lu micPeak=%lu refPeak=%lu dstPeak=%lu micClipPermille=%lu refClipPermille=%lu dstClipPermille=%lu micMean=%ld refMean=%ld dstMean=%ld droppedReports=%lu\r\n",
            (unsigned long)report.playback, phase, (unsigned long)w->frames,
            (unsigned long)report.total_frames, (unsigned long)w->samples,
            (unsigned long)w->aec_frames,
            (unsigned long)(w->abs_sum[0] / w->samples),
            (unsigned long)(w->abs_sum[1] / w->samples),
            (unsigned long)(w->abs_sum[2] / w->samples),
            (unsigned long)w->peak[0], (unsigned long)w->peak[1], (unsigned long)w->peak[2],
            (unsigned long)((uint64_t)w->clipped[0] * 1000U / w->samples),
            (unsigned long)((uint64_t)w->clipped[1] * 1000U / w->samples),
            (unsigned long)((uint64_t)w->clipped[2] * 1000U / w->samples),
            (long)(w->sum[0] / w->samples), (long)(w->sum[1] / w->samples),
            (long)(w->sum[2] / w->samples), (unsigned long)dropped);
        mprintf("[AUDIO_GAIN] playback=%lu totalFrames=%lu micPgaReg=0x%02lx refPgaReg=0x%02lx micDigitalCode=%lu refDigitalCode=%lu micAlc=%lu refAlc=%lu pgaControlMask=%lu dacAnalogCode=%lu dacDigitalCode=%lu evidence=window_end_registers\r\n",
            (unsigned long)report.playback, (unsigned long)report.total_frames,
            (unsigned long)report.pga_reg[0], (unsigned long)report.pga_reg[1],
            (unsigned long)report.adc_digital[0], (unsigned long)report.adc_digital[1],
            (unsigned long)report.alc[0], (unsigned long)report.alc[1],
            (unsigned long)report.pga_control_mask,
            (unsigned long)report.dac_analog, (unsigned long)report.dac_digital);
    }
    if(report.phase == 2U && report.ref_samples)
    {
        static const char digits[] = "0123456789abcdef";
        char hex[AEC_REF_SNAPSHOT_SAMPLES * 4U + 1U];
        for(uint32_t i = 0; i < report.ref_samples; i++)
        {
            uint16_t value = (uint16_t)report.ref_pcm[i];
            hex[i * 4U] = digits[(value >> 4) & 15U];
            hex[i * 4U + 1U] = digits[value & 15U];
            hex[i * 4U + 2U] = digits[(value >> 12) & 15U];
            hex[i * 4U + 3U] = digits[(value >> 8) & 15U];
        }
        hex[report.ref_samples * 4U] = 0;
        mprintf("[REF_PCM] playback=%lu selectedFrame=%lu frameAbs=%lu samples=%lu sampleRate=16000 encoding=s16le hex=%s\r\n",
            (unsigned long)report.playback, (unsigned long)report.ref_frame,
            (unsigned long)report.ref_abs, (unsigned long)report.ref_samples, hex);
    }
}

void audio_pre_rslt_upload_by_iis(int16_t *left, int16_t *right, ci_wrapfft_audio *p_wrapfft_audio)
{
    uint32_t write_pcm_addr = 0;
    uint32_t block_size = sg_init_tmp_str.init_str.block_size;
    int num = block_size / sizeof(int16_t) / 2;
    report_aec_path_levels(p_wrapfft_audio, (uint32_t)num);
#if AI_UART_CONTROL_EN
    if(!ai_uart_i2s_peer_ready() || (pdTRUE != uplink_try_take()))
    {
        return;
    }
    if(!ai_uart_i2s_peer_ready())
    {
        uplink_give();
        return;
    }
#endif
#if IIS_UPLOAD_IS_WAKEUP
    if ((gCiasAiotRunParam.is_wake_up_flag || gCiasAiotRunParam.is_always_iis_flag))  
#endif 
    {
        cm_get_pcm_buffer(PLAY_PRE_AUDIO_CODEC_ID, &write_pcm_addr, 0); // TODO HSL
        if (0 == write_pcm_addr)
        {
#if AI_UART_CONTROL_EN
            uplink_give();
#endif
            return;
        }
        int16_t *pcm_data_p = (int16_t *)write_pcm_addr;
        for (int i = 0; i < num; i++)
        {     
            if (gCiasAiotFuncParam.upload_audio_by_denoise) // 上传降噪音频，目前AEC时不进行降噪
            {
                pcm_data_p[2 * i] = right[i];   
                pcm_data_p[2 * i + 1] = right[i];
            }
            else
            {
                if(ciss_get(CI_SS_AEC_WORK_STATE)) 
                {
                    pcm_data_p[2 * i] = right[i];   
                    pcm_data_p[2 * i + 1] = right[i];
                }
                else
                {
                    pcm_data_p[2 * i] = p_wrapfft_audio->asr_src_data[i];   //降噪进行时使用降噪前的数据
                    pcm_data_p[2 * i + 1] = p_wrapfft_audio->asr_src_data[i];
                }
            }
        }
        cm_write_codec(PLAY_PRE_AUDIO_CODEC_ID, (void *)write_pcm_addr, 0);
    }
#if !AI_UART_CONTROL_EN
    codec_output_ctl(PLAY_PRE_AUDIO_CODEC_ID);
#endif
    sg_init_tmp_str.send_data_cnt++;
    sg_init_tmp_str.write_data_cnt++;
#if AI_UART_CONTROL_EN
    uplink_give();
#endif
}
#elif USE_AUDIO_UPLOAD_BY_HPOUT
void audio_pre_rslt_upload_by_hpout(int16_t *left, int16_t *right, ci_wrapfft_audio *p_wrapfft_audio)
{
    int ret = 0;
    uint32_t block_size = sg_init_tmp_str.init_str.block_size;
    int num = block_size / sizeof(int16_t) / 2;
#if HPOUT_UPLOAD_IS_WAKEUP
    if ((gCiasAiotRunParam.is_wake_up_flag || gCiasAiotRunParam.is_always_hpout_flag)) 
#endif   
    {
        uint32_t write_pcm_addr_cpy = 0;
        cm_get_pcm_buffer(PLAY_CODEC_ID, &write_pcm_addr_cpy, 10); // TODO HSL
        if (0 == write_pcm_addr_cpy)
        {
            mprintf("write_pcm_addr_cpy buffer is overflow\r\n");
            return;
        }
        int16_t *pcm_data_p_cpy = (int16_t *)write_pcm_addr_cpy;
        for (int i = 0; i < num; i++)
        {
            if (gCiasAiotFuncParam.upload_audio_by_denoise) // 上传降噪音频，目前AEC时不进行降噪
            {
                pcm_data_p_cpy[2 * i] = right[i];   
                pcm_data_p_cpy[2 * i + 1] = right[i];
            }
            else
            {
                if(ciss_get(CI_SS_AEC_WORK_STATE)) 
                {
                    pcm_data_p_cpy[2 * i] = right[i];   
                    pcm_data_p_cpy[2 * i + 1] = right[i];
                }
                else
                {
                    pcm_data_p_cpy[2 * i] = p_wrapfft_audio->asr_src_data[i];   //降噪进行时使用降噪前的数据
                    pcm_data_p_cpy[2 * i + 1] = p_wrapfft_audio->asr_src_data[i];
                }
            }
        }
        cm_write_codec(PLAY_CODEC_ID, (void *)write_pcm_addr_cpy, 0);
    }
    codec_output_ctl(PLAY_CODEC_ID);
    
#if USE_IIS1_OUT_PRE_RSLT_AUDIO
    uint32_t write_pcm_addr = 0;
    cm_get_pcm_buffer(PLAY_PRE_AUDIO_CODEC_ID, &write_pcm_addr, 0); // TODO HSL
    int16_t *pcm_data_p = (int16_t *)write_pcm_addr;
    for (int i = 0; i < num; i++)
    {
        pcm_data_p[2 * i] = left[i];
        pcm_data_p[2 * i + 1] = right[i];
    }
    if (0 == write_pcm_addr)
    {
        return;
    }
    cm_write_codec(PLAY_PRE_AUDIO_CODEC_ID, (void *)write_pcm_addr, 0);
    codec_output_ctl(PLAY_PRE_AUDIO_CODEC_ID);
#endif

    sg_init_tmp_str.send_data_cnt++;
    sg_init_tmp_str.write_data_cnt++;
}
#elif USE_AUDIO_UPLOAD_BY_UART
void audio_pre_rslt_upload_by_uart(int16_t *left, int16_t *right, ci_wrapfft_audio *p_wrapfft_audio)
{
    int ret = 0;
    uint32_t block_size = sg_init_tmp_str.init_str.block_size;
    int num = block_size / sizeof(int16_t) / 2;
    extern int8_t speex_src_temp_buf[PCM_ALG_FRAME_LEN];  //p_wrapfft_audio->asr_src_data);
#if AUDIO_DATA_UPLOAD_BY_UART
    if (!gCiasAiotFuncParam.upload_play_full_duplex)
    {
        if (CI_SS_PLAY_STATE_PLAYING == ciss_get(CI_SS_PLAY_STATE))
        {
            return;
        }
    }
#if UPLOAD_PCM_DATA_ENABLE
    if (1)
#else
    // mprintf("gCiasAiotRunParam.is_wake_up_flag = %d\r\n", gCiasAiotRunParam.is_wake_up_flag);
    // mprintf("gCiasAiotRunParam.stop_collect_pcm_flag = %d\r\n", gCiasAiotRunParam.stop_collect_pcm_flag);
    if (gCiasAiotRunParam.is_wake_up_flag && !gCiasAiotRunParam.stop_collect_pcm_flag)
#endif
    {
        if (gCiasAiotRunParam.is_vad_on_flag)
        {
            gCiasAiotRunParam.vad_start_pcm_frame_count++;
        }
        if (xStreamBufferIsFull(gCiasAiotRunParam.pcm_compress_stream_buffer)) // 缓存满开始清老的数据
        {
            // mprintf("wakeup_pcm_frame_len is full, remove pcm data: %d frame\r\n", PCM_MSG_STREAM_NUM - PCM_ALG_ROOLBACK_FRAME_LEN);

            for (int i = 0; i < (PCM_MSG_STREAM_NUM - PCM_ALG_ROOLBACK_FRAME_LEN); i++)
            {
                int rx_size = xStreamBufferReceiveFromISR(gCiasAiotRunParam.pcm_compress_stream_buffer, speex_src_temp_buf, PCM_ALG_FRAME_LEN, pdMS_TO_TICKS(5));
                if (rx_size != PCM_ALG_FRAME_LEN)
                {
                    mprintf("roll back pcm_compress_stream_buf rcv error1\r\n");
                }
                else
                {
                    gCiasAiotRunParam.wake_up_pcm_frame_count--;
                }
            }
        }
        gCiasAiotRunParam.wake_up_pcm_frame_count++;
        if (gCiasAiotRunParam.pcm_compress_stream_buffer)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            uint8_t *p_upload_src = NULL;

            if (gCiasAiotFuncParam.upload_audio_by_denoise) // 上传降噪音频，目前AEC时不进行降噪
            {
                ret = xStreamBufferSendFromISR(gCiasAiotRunParam.pcm_compress_stream_buffer, (int8_t *)right, PCM_ALG_FRAME_LEN, &xHigherPriorityTaskWoken);
            }
            else
            {
                if(ciss_get(CI_SS_AEC_WORK_STATE)) 
                {
                    p_upload_src = right;
                }
                else
                {
                    p_upload_src = p_wrapfft_audio->asr_src_data;   //降噪进行时使用降噪前的数据
                }
                //上传降噪之前，AEC处理后的数据
                ret = xStreamBufferSendFromISR(gCiasAiotRunParam.pcm_compress_stream_buffer, (int8_t *)p_upload_src, PCM_ALG_FRAME_LEN, &xHigherPriorityTaskWoken);
            }
 
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            if (ret != PCM_ALG_FRAME_LEN)
            {
                mprintf("xSpeexRecordStreamBuffer send error, send len = %d\r\n", ret);
            }
        }
    }
#endif
#if USE_IIS1_OUT_PRE_RSLT_AUDIO || USE_HP_OUT_PRE_RSLT_AUDIO
#if USE_IIS1_OUT_PRE_RSLT_AUDIO
    uint32_t write_pcm_addr = 0;
    cm_get_pcm_buffer(PLAY_PRE_AUDIO_CODEC_ID, &write_pcm_addr, 0); // TODO HSL
    int16_t *pcm_data_p = (int16_t *)write_pcm_addr;
    for (int i = 0; i < num; i++)
    {
        pcm_data_p[2 * i] = left[i];
        pcm_data_p[2 * i + 1] = right[i];
    }
    if (0 == write_pcm_addr)
    {
        return;
    }
    cm_write_codec(PLAY_PRE_AUDIO_CODEC_ID, (void *)write_pcm_addr, 0);
#endif

#if USE_HP_OUT_PRE_RSLT_AUDIO
    uint32_t write_pcm_addr_cpy = 0;
    cm_get_pcm_buffer(PLAY_CODEC_ID, &write_pcm_addr_cpy, 0); // TODO HSL
    int16_t *pcm_data_p_cpy = (int16_t *)write_pcm_addr_cpy;
    for (int i = 0; i < num; i++)
    {
        pcm_data_p_cpy[2 * i] = left[i];
        pcm_data_p_cpy[2 * i + 1] = right[i];
    }
    if (0 == write_pcm_addr_cpy)
    {
        return;
    }
    cm_write_codec(PLAY_CODEC_ID, (void *)write_pcm_addr_cpy, 0);
#endif
    #if USE_IIS1_OUT_PRE_RSLT_AUDIO
    codec_output_ctl(PLAY_PRE_AUDIO_CODEC_ID);
    #endif
    #if USE_HP_OUT_PRE_RSLT_AUDIO && !NET_AUDIO_PLAY_BY_PCM
    codec_output_ctl(PLAY_CODEC_ID);
    #endif
#endif
    sg_init_tmp_str.send_data_cnt++;
    sg_init_tmp_str.write_data_cnt++;
}

#else

#endif
/**
 * @brief 写数据到发送端
 *
 * @param rslt 处理结果的起始指针
 * @param origin 原始数据的起始指针
 *  IIS AI 上行支持 mailbox 中断回调；只同步复制 PCM，不在回调内启停硬件。
 */
void audio_pre_rslt_write_data(int16_t *left, int16_t *right, uint32_t wrapfft_audio_addr)
{
    ci_wrapfft_audio *p_wrapfft_audio = (ci_wrapfft_audio *)wrapfft_audio_addr;   //p_wrapfft_audio->asr_src_data);

    #if USE_NN_DENOISE && DENOISE_STRENGTH_ADAPT_ENABLE
    extern noise_est_config_t nn_noise_est_config;
    if(nn_noise_est_config.noise_est_debug)
    {
        mprintf("noise_log_energy =%d\r\n",(int32_t)p_wrapfft_audio->noise_log_energy);
    }
    #endif
#if AUTO_GET_AEC_GAIN
    auto_get_aec_gain(p_wrapfft_audio);
#endif
#if DEBUG_AUDIO_UART_UPLOAD_EN
    audio_debug_uart0_upload(left,right,p_wrapfft_audio);
#endif
#if IIS_CHANNEL_ENG_CALC_EANBLE
    audio_eng_calc(p_wrapfft_audio);
#endif
#if USE_AUDIO_UPLOAD_BY_IIS
    audio_pre_rslt_upload_by_iis(left,right,p_wrapfft_audio);
#elif USE_AUDIO_UPLOAD_BY_HPOUT
    audio_pre_rslt_upload_by_hpout(left,right,p_wrapfft_audio);
#elif USE_AUDIO_UPLOAD_BY_UART
    audio_pre_rslt_upload_by_uart(left,right,p_wrapfft_audio);
#else
#endif
}

/**
 * @brief 语音前处理输出停止
 *
 */
int audio_pre_rslt_stop(void)
{
#if AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS
    /* Initialization has not exposed a producer yet. */
    if(!uplink_gate)
    {
        return 1;
    }
    if(pdTRUE != uplink_try_take())
    {
        return 0;
    }
    int result = cm_cancel_slave_output(PLAY_PRE_AUDIO_CODEC_ID);
    if(result == 0)
    {
        sg_init_tmp_str.codec_start_flag[PLAY_PRE_AUDIO_CODEC_ID] = false;
        sg_init_tmp_str.send_data_cnt = 0;
        mprintf("[AI_I2S] uplink canceled dma=off buffers=reclaimed iis=deferred\r\n");
    }
    uplink_give();
    return (result == 0) ? 1 : ((result > 0) ? 0 : -1);
#else
#if USE_IIS1_OUT_PRE_RSLT_AUDIO
    cm_stop_codec(PLAY_PRE_AUDIO_CODEC_ID, CODEC_OUTPUT);
    sg_init_tmp_str.codec_start_flag[PLAY_PRE_AUDIO_CODEC_ID] = false;
#endif

#if USE_HP_OUT_PRE_RSLT_AUDIO
    cm_stop_codec(PLAY_CODEC_ID, CODEC_OUTPUT);
#endif
    sg_init_tmp_str.send_data_cnt = 0;
    return 1;
#endif
}

/**
 * @brief 语音前处理输出开始
 *
 */
void audio_pre_rslt_start(void)
{
#if !(AI_UART_CONTROL_EN && USE_AUDIO_UPLOAD_BY_IIS)
    sg_init_tmp_str.send_data_cnt = 0;
#endif
}

/********** (C) COPYRIGHT Chipintelli Technology Co., Ltd. *****END OF FILE****/
