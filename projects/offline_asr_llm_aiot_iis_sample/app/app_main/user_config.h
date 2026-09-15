/**/
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


//**板级配置选择
/*板级配置更多细节请查看:https://document.chipintelli.com/硬件资料-->模块手册
chipintelli提供的部分开发板和模组，可以通过下面的宏选择，也可以参考开发板的板级配置文
件添加自定义板级配置文件*/
#define USE_CI_D02GS01J_BOARD       0   //CI-D0XGS01J，端子模块，芯片型号必须设置为1302
#define USE_CI_D02GS02S_BOARD       0   //CI-D0XGS02S，SMT模块，芯片型号必须设置为1302
#define USE_CI_D12GS01J_BOARD       0   //CI-D0XGS01J，端子模块，芯片型号必须设置为1312JE
#define USE_CI_D06GT01D_BOARD       1   //CI-D06GT01D，开发版，芯片型号必须设置为1306
#define USE_CI_E12GS02J_BOARD       0   //CI-E12GS02J，开发版，芯片型号必须设置为231x
#define USE_CI_D06GT01J_BOARD       0   //CI_D06GT01J, 开发板，型号必须为设置1306-仅配置支持双mic 算法+AEC，其他配置不支持
#define USE_CI_E0XGTD02S_BOARD      0   //CI-E06GT02S, 开发板2305/2306
#define USE_CUS_XXXXXXX_BOARD       0   //用户自定义

#if (USE_CI_D02GS01J_BOARD == 1)
#define CI_CHIP_TYPE                1302    //flash:2MB,SSOP24
#define BOARD_PORT_FILE             "CI-D02GS01J.c"
#elif (USE_CI_D02GS02S_BOARD == 1)
#define CI_CHIP_TYPE                1302    //flash:2MB,SSOP24
#define BOARD_PORT_FILE             "CI-D02GS02S.c"
#elif (USE_CI_D12GS01J_BOARD == 1)
#define CI_CHIP_TYPE                1312    //flash:2MB,SSOP16
#define BOARD_PORT_FILE             "CI-D12GS01J.c"
#elif (USE_CI_D06GT01D_BOARD == 1)
#define CI_CHIP_TYPE                1306    //flash:4MB,QFN40
#define BOARD_PORT_FILE             "CI-D06GT01D.c"
#elif (USE_CI_E12GS02J_BOARD == 1)
#define CI_CHIP_TYPE                2312    //flash:2MB,SSOP16
#define BOARD_PORT_FILE             "CI-E12GS02J.c"
#define USE_BLE                     1
#elif (USE_CI_D06GT01J_BOARD == 1)
#define CI_CHIP_TYPE                1306    //flash:4MB,QFN40 双mipc算法+外部codec 7243e用该板级
#define BOARD_PORT_FILE             "CI-D06GT01J.c"
#elif (USE_CI_E0XGTD02S_BOARD == 1)
#define CI_CHIP_TYPE                2306    
#define BOARD_PORT_FILE            "CI-E06GT02S.c"

#elif (USE_CUS_XXXXXXX_BOARD == 1)
#define CI_CHIP_TYPE                xxxx    //flash:4MB,QFN40
#define BOARD_PORT_FILE             "CI-XXXX.c"
#endif

#ifndef HOST_MIC_USE_NUMBER
#define HOST_MIC_USE_NUMBER            1   //定义mic数量
#endif

//**麦克风、回声参考电路模式配置
#define AUDIO_IN_FROM_DMIC              0       //是否使用数字MIC输入音频

/*注意!! 外部IIS0需要根据SDK当前需求输入正确的信号，例如采样率、通道数和左右通道的数据（mic or ref）*/
#define MIC_RECORD_IIS_SELECT           0 //MIC输入的IIS选择:0，内部codec IIS1   1,外部IIS0;

#define MIC_DIFF_SINGLE                 0   /*1,单端。0，差分（通用模块都是差分模式，省成本的模块为单端(MICN_L 接GND)时，需要配置为SINGLE）)*/
#define REF_DIFF_SINGLE                 1   /*1,单端。0，差分（D类功放是采用单mic aec 使用 差分 mic ; AB类功放是采用单mic aec 使用 单端 mic)*/

/*1、双mic + aec算法必须外部挂codec作为信号回采(推荐7243e)
  2、双dmic + 模拟右麦/外部挂codec(推荐7243e)作为信号回采
  3、外部IIS0双麦 + 模拟右麦作为信号回采
*/
#if USE_AEC_MODULE && (USE_BEAMFORMING_MODULE || (USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY)
#if AUDIO_IN_FROM_DMIC
#define REF_IN_FROM_INNER_CODEC         0//采用2 DMIC + 内部codec right通道输入REF信号
#define IF_USE_ANOTHER_CODEC_TO_GET_REF 1//采用2 DMIC + 外挂codec输入REF信号
#elif MIC_RECORD_IIS_SELECT
#define REF_IN_FROM_INNER_CODEC         1//采用外部IIS0双麦 + 模拟右麦作为信号回采
#else
#define IF_USE_ANOTHER_CODEC_TO_GET_REF 1
#if USE_CI_D06GT01J_BOARD != 1   //必须外挂codec使用USE_CI_D06GT01J_BOARD板级
#error "dual mic alg + aec , must use USE_CI_D06GT01J_BOARD board\r\n"
#endif
#endif
#endif

#if IF_USE_ANOTHER_CODEC_TO_GET_REF
#define AUDIO_DATA_PLAY_BY_IIS                         (0)            //通IIS接收音频数据播放配置
#endif

//输出识别中间结果使能
#define ASR_MIDDLE_RESULT_OUT_EN       0   
//0：正常uart0作为日志串口 1：uart0作为调试采音串口 上传双声道数据;多消耗13KB SYS内存
#define DEBUG_AUDIO_UART_UPLOAD_EN                   0
#if DEBUG_AUDIO_UART_UPLOAD_EN
#define USE_UART_SEND_PRE_RSLT_AUDIO_NUMBER       (UART_TypeDef*)(HAL_UART0_BASE)   //用哪个UART口将算法处理后的音频数据送出
#define USE_UART_SEND_PRE_RSLT_AUDIO_BAUD         UART_BaudRate921600               //串口采音波特率
#define USE_UART_SEND_PRE_RSLT_AUDIO_BUF_LEN      1024*10                           //串口采音BUF大小
#define CONFIG_CI_LOG_UART                        0//HAL_UART1_BASE  //配置log输出使用的串口，请勿与protocol共用同一个串口
#else
//**通讯串口配置
#define CONFIG_CI_LOG_UART             HAL_UART0_BASE  //配置log输出使用的串口，请勿与protocol共用同一个串口

// Lenwell ESP32 control link. UART1 carries control frames; IIS0 carries PCM.
#define AI_UART_CONTROL_EN             1
#define AI_UART_CONTROL_UART           HAL_UART1_BASE
#define AI_UART_CONTROL_BAUDRATE       UART_BaudRate921600
#define AI_UART_TASK_POLL_MS           50
#define AI_DOWNLINK_TASK_PRIORITY      3
#endif

#if USE_CI_D02GS01J_BOARD || USE_CI_D02GS02S_BOARD
#define USE_1302_UART0_MODE             1  //1302芯片引脚不足，仅能使用串口0；选择串口0的用途 1：日志 2：WIFI通信串口
#endif

#if (USE_1302_UART0_MODE == 2)
#define CONFIG_CI_LOG_UART             0  //配置log输出使用的串口，请勿与protocol共用同一个串口
#endif

//离在线主要使用与WIFI通信的串口UART_NUM_SEND_PLAY_AUDIO_NUMBER，根据需求增加该串口
#define MSG_COM_USE_UART_EN            0   //0,关闭语音模块通讯协议。1,开启语音模块通讯协议。
#define UART_PROTOCOL_NUMBER           (HAL_UART2_BASE)    //语音模块协议使用的串口，请勿与log共用同一个串口。
#define UART_PROTOCOL_BAUDRATE         (UART_BaudRate921600) //语音模块协议使用的串口波特率。
#define UART_PROTOCOL_VER              2   //语音模块协议版本号:1,一代协议。2,二代协议，255,平台生成协议

#define CLOUD_UART_PROTOCOL_EN         0   //云端协议使能-只有在启英开发者平台做固件配协议能用
#if CLOUD_UART_PROTOCOL_EN
#define CLOUD_CFG_UART_SEND_EN         1   //使能串口发送数据
#define CLOUD_CFG_PLAY_EN              1   //播报音使能
#define CLOUD_CFG_UART_PORT	         ((UART_TypeDef*)(HAL_UART1_BASE))// HAL_UART0_BASE ~ HAL_UART2_BASE，请勿与log共用同一个串口
#define CLOUD_CFG_UART_BAUND_RATE    UART_BaudRate9600
#endif

//**通信串口引脚开漏模式使能配置
//注:推挽模式的IO只能对接3.3V电平的IO，开漏模式可以对接5V电平的IO(外部需要上拉到5V)
#define UART0_PAD_OPENDRAIN_MODE_EN     0   //0,UART0为推挽模式。1,UART0为开漏模式。
#define UART1_PAD_OPENDRAIN_MODE_EN     0   //0,UART1为推挽模式。1,UART1为开漏模式。
#define UART2_PAD_OPENDRAIN_MODE_EN     0   //0,UART2为推挽模式。1,UART2为开漏模式。
// ota使能
#define CI_OTA_ENABLE                   0   // 1使能 0 不

//**时钟源配置
#ifndef USE_EXTERNAL_CRYSTAL_OSC
#if ((CI_CHIP_TYPE == 1312) || (CI_CHIP_TYPE == 1311) || (CI_CHIP_TYPE == 2305) || (CI_CHIP_TYPE == 2306))
#define USE_EXTERNAL_CRYSTAL_OSC        0
#else
#define USE_EXTERNAL_CRYSTAL_OSC        1           //0:使用内部RC作为时钟源。1:使用外部晶振作为时钟源。
#endif
#endif
//**波特率自适应功能配置
#if (USE_EXTERNAL_CRYSTAL_OSC == 0)             //使用内部RC时,建议开启波特率自适应(需要电控增加对应支持)。
#define UART_BAUDRATE_CALIBRATE         1       //是否使能波特率自适应功能。
#define BAUDRATE_SYNC_PERIOD            300000  // 波特率同步周期，单位毫秒。
#define BAUDRATE_FAST_SYNC_PERIOD       5000    // 一次波特率同步失败后，下一次同步间隔，单位毫秒。
#define BAUD_CALIBRATE_MAX_WAIT_TIME    400     // 等待反馈包的超时时间，单位毫秒。
#endif

//*红外功能配置
#define USE_IR_ENABLE                   0       //红外功能，1:是 0:否。开启红外功能在使用打包工具升级固件时，请取消勾选“升级完成自动运行”，防止重复烧录\
                                                  红外功能涉及多模型切换和红外码库，firmware文件请参考external\firmware参考\ir(红外)\firmware
#if USE_IR_ENABLE
#define UART_CONTOR_SEND_IR             0       //用通信口进行串口协议控制发红外
#define IR_TEST	                        0       //用通信口进行串口协议的产检
#ifndef USE_NIGHT_LIGHT
#define USE_NIGHT_LIGHT                 1       
#endif
#endif
/********************************************离在线参数宏配置开始********************************************/
//*语音上传(通过IIS输出pcm数据，不支持压缩)
#ifndef AUDIO_DATA_UPLOAD_BY_IIS
#define AUDIO_DATA_UPLOAD_BY_IIS                       (1)            //通过IIS上传语音功能配置，消耗12KB内存
#endif

#ifndef AUDIO_DATA_PLAY_BY_IIS
#define AUDIO_DATA_PLAY_BY_IIS                         (1)            //通IIS接收音频数据播放配置
#endif

// The built-in AIOT UART owner is disabled; ai_uart_i2s_protocol owns UART1.
#define WIFI_CMD_BY_UART                               (0)

#define IIS_UPLOAD_IS_WAKEUP                              0           //ESP握手后持续输出，由ESP决定是否上传
#define IIS_DOWNLOAD_BY_CMD                               1           //1:在收到指令后开始IIS播报 0：上电后开始IIS播报

#define UPLOAD_NNDENOISE_AUDIO_DATA_ENABLE                1             //1-上传降噪的音频 0-上传非降噪的音频
#define DENOISE_STRENGTH_ADAPT_ENABLE                     0            //降噪强度自适应调整模式，需在ci_ssp_config.h配置降噪模块中配置自适应模式具体参数

#define LOCAL_REC_VAD_END_ENABLE                          0             //本地识别后立即停止VAD
#define WEAK_VAD_ENABLE                                   0             //1：唤醒后开始检测VAD  0：一直检测VAD

//NN_VAD配置(10ms一帧)
#define NN_VAD_SENSITIVITY                                1              //灵敏度等级，设置0,1,2，对应低，中，高。灵敏度等级越高越灵敏;配合tvad_valid_num调试TVAD的灵敏度
#define NN_VAD_END_DELAY                                  30             //后窗口大小， VAD_ON->VAD_END 最多持续的帧数 ；取值20~40（过小会导致说话停顿就VAD_END了)
#define NN_VAD_VALID_NUM                                  25             //VAD_ON 状态的至少满足的有效帧数，大于该阈值则视为语音有效，低于该阈值识别结果无效

//超时强制VAD END检测
#define VAD_TIMEOUT_CHECK                                 1            //0-不进行超时检测 1-进行超时检测-不可修改
#define VAD_FORCE_OVER_NUM_TIME                           5            //强制结束录音的时间(单位S)，上传音频 

//和WiFi通信串口及参数配置
#if WIFI_CMD_BY_UART
#if (USE_1302_UART0_MODE == 2)
#define UART_NUM_SEND_PLAY_AUDIO_NUMBER                  HAL_UART0_BASE           //网络端交互的串口
#else
#define UART_NUM_SEND_PLAY_AUDIO_NUMBER                  HAL_UART1_BASE           //网络端交互的串口
#endif
#define UART_NUM_SEND_PLAY_AUDIO_BAUDRATE                UART_BaudRate921600      //网络端交互的串口波特率
#endif
#define NETWORK_RECV_BUFF_MAX_SIZE                       (256 + 16)               //接收网络端串口数据最大size-帧头+预留4字节
#define NETWORK_SEND_BUFF_MAX_SIZE                       (256 + 16)               //发送串口数据到网络端最大size,pcm数据分4包传输，避免丢数据
#define NETWORK_SEND_BUFF_NUM                             5                       //发送串口数据到网络端缓冲区个数  

//生产测试使用
#define IIS_CHANNEL_ENG_CALC_EANBLE                       0              //iis通道能量计算
#define ENG_CALC_INTERVAL_FRAME                           10             //iis通道能量10帧计算一次(可根据需求修改)
#define CIAS_HAVE_AUDIO_ENG_MICL                          50             //左MIC有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_MICR                          50             //右MIC有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_REFL                          50             //REFL有音频能量阈值设置，默认50db    
#define CIAS_HAVE_AUDIO_ENG_REFR                          50             //REFR有音频能量阈值设置，默认50db    
#define CIAS_UPLOD_FACTORY_TEST_REAL_VAL                  0              //上传音频上传过程中的实时值 0-不上传 1-上传  

#define CI230X_AUDIO_DATA_OUT_BY_UART                     0             //230X芯片调试使用，上传音频同时通过另外一个串口将音频数据发出
#define UPLOAD_PCM_DATA_ENABLE                            0             //上传音频裸数据不带协议-调试使用 1-使能 0-关闭(默认)
#if UPLOAD_PCM_DATA_ENABLE
#define UPLOAD_PCM_VAD_TAG_ENABLE                         1            //上传pcm数据时带vad 标签
#define NETWORK_SEND_BUFF_MAX_SIZE                      640
#define AUDIO_COMPRESS_SPEEX_ENABLE                       0 
#define AUDIO_SEND_WITH_PROTOCOL_HEADER                   0                    //1-带协议头上传 0-不带协议直接传裸数据
#endif 
#define AUDIO_PLAY_MODE                                   1             //1-支持打断当前播放 0-不支持，顺序播放-暂时不用

/********************************************离在线参数宏配置结束********************************************/
#define USER_CODE_SWITCH_ENABLE     (0)                                 //两份code动态切换功能
#if USER_CODE_SWITCH_ENABLE
#define UART_PROTOCOL_NUMBER           (HAL_UART1_BASE)      
#define UART_PROTOCOL_BAUDRATE         (UART_BaudRate115200)    //TTS默认波特率115200
#define USER_CODE2_SAVE_ADDR           0x4000-8                 //code固件地址存放位置
#endif 

//**语音识别配置
#define USE_SEPARATE_WAKEUP_EN          0       //当前V01900模型同时负责常驻唤醒与本地识别
#define DEFAULT_MODEL_GROUP_ID          1       //模型ID,用于指定上电启动时，默认进入的语言模型。通常0为命令词模型,1为唤醒词模型
    
#if (!USE_SEPARATE_WAKEUP_EN)    
#undef DEFAULT_MODEL_GROUP_ID    
#define DEFAULT_MODEL_GROUP_ID          0
#endif

#define PLAY_WELCOME_EN                 0      //是否在启动时播放开机提示音。1:是 0:否。
#define PLAY_ENTER_WAKEUP_EN            1      //是否在唤醒时播放提示音。1:是 0:否。
#define PLAY_EXIT_WAKEUP_EN             0      //是否在切换到只监听唤词状态时播放提示音。1:是 0:否。
#define PLAY_OTHER_CMD_EN               0      //是否在识别到命令词时播放提示音。1:是 0:否。
#define ADAPTIVE_THRESHOLD              0
#define ASR_SKIP_FRAME_CONFIG           0
#define EXIT_WAKEUP_TIME                30*1000   //退出唤醒超时时间,单位毫秒。超过此配置指定的时间长度内没有识别到任何命令词，就会切换到只监听唤词状态。
    
//**播放器配置  
#define AUDIO_PLAYER_ENABLE             1   //本地唤醒提示音仍由V3官方播放器播放
#define PLAYER_CONTROL_PA               0   //是否有播放器控音频功放开关。0:功放常开,1:播放器在需要播放时才打开,但可能增加一点每一次播放的延迟时间
#define VOLUME_MAX                      7   //设置音量调节的上限值，对应硬件支持的最大音量。
#define VOLUME_MIN                      1   //设置音量调节的下限值，对应最小音量。
#define VOLUME_DEFAULT                  VOLUME_MAX   //ESP同步前使用最大本地回退档位。
#define VOLUME_OUTPUT_MAX_PERCENT       100 //硬件DAC固定满量程，实际用户音量由ESP PCM和本地提示音PCM控制。
#define PLAYBACK_DAC_DIGITAL_GAIN_DB    10  //CI1306 DAC数字增益范围为-117..+10dB。
#define WAKEUP_DING_VOICE_ID            1000

#if AUDIO_PLAYER_ENABLE
#if NET_AUDIO_PLAY_BY_OPUS
#define USE_OPUS_DECODER                1   //为1时加入opus解码器，1:是 0:否。
#else
#define USE_MP3_DECODER                 1   //为1时加入mp3解码器，1:是 0:否。
#define AUDIO_PLAY_SUPPT_MP3_PROMPT     1   //播放器是否开启mp3提示音，1:是 0:否。
#define USE_PROMPT_DECODER              1   //播放器是否支持prompt解码器，1:是 0:否。
#endif
#endif

#define BF_DEEPSE_MODE                  1   //1:全深度分离更耗内存(单双网络都可以用) 0：半深度分离(唤醒词做深度分离，命令词不做，只能用双网络)
#define BF_ASR_VALID_MODE               0   //1:开启ASR打分是否有效判断功能 0：关闭ASR打分是否有效判断功能 该功能只针对全深度分离和半深度分离

#if USE_DEREVERB_MODULE
#define DEREVERB_FREQ_RANGE_INDEX       0  //默认0:算法起效频率160HZ-4800HZ 消耗28KB内存  1: 算法起效频率0-8000HZ 消耗49KB内存     
#endif
#if USE_AEC_MODULE
#define AEC_INTERRUPT_TYPE              2  //默认2: 命令词和唤醒词都可打断  1: 只有命令词能打断   0:只有唤醒词能打断
#endif
//**自学习功能-请在安静环境下，用清晰洪亮的声音进行指令学习，避免环境噪音过大和学习者声音过小导致学习不成功 
//**注意：在线SDK只支持唤醒词学习，为了避免指令词被学习成模版，请确保cmd_info.xls中词条语义ID、命令词ID与需学习的词条语义ID、命令词ID不重复                                    
#if USE_CWSL
#define CWSL_WAKEUP_NUMBER          2           // 可学习的唤醒词数量-最大支持2个
#define WAKE_UP_ID                  1           // 学习的唤醒词对应的命令词ID
#define CWSL_REG_TIMES              1           // 学习时 每个词需说几遍，默认 1 遍即可,支持1、2遍,FOR_REG_2TIMES_FLOW_V2 配置 1时,最大支持 3 遍;
#define CWSL_WAKEUP_THRESHOLD       37          // 学习的唤醒词阈值门限，越小越灵敏，默认 37, 最小可配置到 32;
#define CWSL_CMD_THRESHOLD          35          // 学习的命令词阈值门限，越小越灵敏，默认 35，最小可配置到 30；
#define FOR_REG_2TIMES_FLOW_V2      0           // 学习时，说两遍/三遍逻辑，版本二流程，后续均和第一次的比较，一致学习成功，不一致，最多支持说 3 次\
                                                     FOR_REG_2TIMES_FLOW_V2 配置 1时, CWSL_REG_TIMES 必须是 2或3	
#define CWSL_REG_VAD_LEVEL          0           // 学习过程，灵敏度选项配置： 0 低灵敏度，可减少噪声对学习的干扰，需学习过程大声说话；1 高灵敏度，但也可以导致干扰噪声干扰学习
#define CICWSL_TOTAL_TEMPLATE       CWSL_WAKEUP_NUMBER*3           //可存储模板数量

#if (CWSL_REG_TIMES == 3)
#define FOR_REG_2TIMES_FLOW_V2      1
#elif (CWSL_REG_TIMES > 3)
#error "The CWSL_REG_TIMES max 3\r\n"
#endif
#endif

#if USE_WMAN_VPR
#define VP_USE_FRM_LEN                  1200                            //声纹计算的窗长，单位为ms，建议范围1200-1500，值越大消耗内存越多（每增加100，内存增加8KB）
#define VP_CMPT_SKIP_NUM                0
#define VPT_SIZE                        (192*sizeof(float))             //模板大小-不可修改
#define NVDATA_ID_VP_NUMBER             NVDATA_ID_VP_MOULD_INFO         //存储已添加了的模板数量-不可修改
#define VP_SLIDE_TIME_PER_CMPT          1                               //声纹每次计算，滑窗的次数-不可修改
#define WMAN_PLAY_EN                    1                               //男女声纹识别播报
#endif
#if     USE_VPR
#define VP_USE_FRM_LEN                  1200      //声纹计算的窗长，单位为ms, 建议范围1200-1500，值越大消耗内存越多（每增加100，内存增加8KB）
#define VP_CMPT_SKIP_NUM                0         //-不可修改
#define VP_THR_FOR_MATCH                (0.52f)   //声纹阈值-建议范围(0.48-0.68)，值越大，灵敏度越低，误识越低，识别率下降，需要更严格的匹配注册的模版
#define VP_THR_FOR_SAME_MATCH           (0.50f)   //同一用户，判断是否重复所用声纹阈值-不可修改
#define VP_SLIDE_TIME_PER_CMPT          3         //声纹每次计算，滑窗-不可修改
#define VP_REC_TIMES                    3         //声纹注册时重复录入次数 -注册时的次数
#define MAX_VP_TEMPLATE_NUM             3         //声纹识别功能允许的最大模版(用户)数,最大4个 重要说明：每个模版单次约占0.8KB NV空间，三次2.4KB

#define MAX_VP_REG_TIME                 10        //注册声纹时最大超时等待时间（秒)
#define VPT_SIZE                        (192*sizeof(float))   //模板大小  -不可修改
#define NVDATA_ID_VP_NUMBER             0xA0000001      //存储模板数量NV基地址 -不可修改
#define NVDATA_ID_VP_INFO               0xA0000002      //存储模板ID NV基地址，每个用户模版数是重复录入次数-不可修改
                                                        //输出给用户的id就是（地址-0xA0000002/VP_REC_TIMES 
#define NVDATA_ID_VP_MODE               0xA0000003      //存储模板NV基地址 -不可修改
#if (MAX_VP_TEMPLATE_NUM > 4)
#error "The vpr template num max 4\r\n"
#endif
#endif

#if USE_SED_CRY || USE_SED_SNORE
#define NO_ASR_FLOW                     1         //不可修改
#if     USE_SED_CRY
#define THRESHOLD_CRY                   0.53f     //可根据具体需求修改,范围为(0~1)float类型-建议范围(0.5-0.6f),值越大，灵敏度越低
#define TIMES_CRY                       3         //可根据具体需求修改,最大5次(算法计算几次给结果)
#elif   USE_SED_SNORE
#define THRESHOLD_SNORE                 0.50f     //可根据具体需求修改,范围为(0~1)float类型-建议范围(0.5-0.6f),值越大，灵敏度越低
#define TIMES_SNORE                     3         //可根据具体需求修改,最大5次(算法计算几次给结果)
#endif

#if TIMES_CRY > 5
#error "The times should be less than or equal to 5\r\n"
#endif        
#endif

#if USE_NN_DOA
#if !USE_AEC_MODULE
#define NN_DOA_OUT_TYPE                 1         //doa输出类型：1-唤醒词输出角度  2-命令词输出角度 3-唤醒次和命令词都输出角度
#endif
#endif

#if USE_GCC_DOA
#define GCC_DOA_OUT_TYPE                 1        //doa输出类型：0-周期输出角度  1-唤醒词和命令词都输出角度
#endif
#if USE_BEAMFORMING_MODULE  || (USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY
#if USE_CI_D12GS01J_BOARD
 #error "USE_CI_D12GS01J_BOARD not support dual mic alg !\r\n"    //131x不支持双mic算法
#endif
#define HOST_CODEC_CHA_NUM  2
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    1
#else
#define HOST_CODEC_CHA_NUM              1
#define OFFLINE_DUAL_MIC_ALG_SUPPORT    0
#endif

#if USE_AEC_MODULE
    #define IF_JUST_CLOSE_HPOUT_WHILE_NO_PLAY   1
    #define HOST_CODEC_CHA_NUM  2
    #define OFFLINE_DUAL_MIC_ALG_SUPPORT    0

    #if USE_SED_SNORE || USE_SED_CRY
    #error "aec + sed detection alg not support!\r\n"    //事件检测不支持AEC
    #endif
#endif


#if USE_CI_D12GS01J_BOARD
#if USE_BEAMFORMING_MODULE  || (USE_NN_DOA || USE_GCC_DOA)|| USE_DEREVERB_MODULE || USE_DUAL_MIC_ANY
 #error "USE_CI_D12GS01J_BOARD not support aec and dual mic alg!\r\n"    //131x不支持aec和双 mic算法
#endif
#endif

#if REF_IN_FROM_INNER_CODEC && IF_USE_ANOTHER_CODEC_TO_GET_REF
#error "no support REF_IN_FROM_INNER_CODEC && IF_USE_ANOTHER_CODEC_TO_GET_REF toghter\r\n"
#endif

#if (MIC_RECORD_IIS_SELECT&&IF_USE_ANOTHER_CODEC_TO_GET_REF)
#error "no support MIC_RECORD_IIS_SELECT && IF_USE_ANOTHER_CODEC_TO_GET_REF toghter\r\n"
#endif

#if (AUDIO_DATA_PLAY_BY_IIS&&IF_USE_ANOTHER_CODEC_TO_GET_REF)
#error "no support AUDIO_DATA_PLAY_BY_IIS && IF_USE_ANOTHER_CODEC_TO_GET_REF toghter\r\n"
#endif

#if (AUDIO_DATA_PLAY_BY_IIS&&MIC_RECORD_IIS_SELECT)
#error "no support AUDIO_DATA_PLAY_BY_IIS && MIC_RECORD_IIS_SELECT toghter\r\n"
#endif

#if ((CI_CHIP_TYPE == 1302) && (MIC_RECORD_IIS_SELECT || AUDIO_DATA_UPLOAD_BY_IIS \
    || AUDIO_DATA_PLAY_BY_IIS || IF_USE_ANOTHER_CODEC_TO_GET_REF) && AUDIO_IN_FROM_DMIC)
#error "1302, no support dmic && iis\r\n"
#endif

#if USE_BEAMFORMING_MODULE && USE_PWK
 #error "bf + pwk algorithm, not support\r\n"
#endif
//双麦算法和AEC在不接外部codec做AEC信号回采，不能同时使用
#if ((USE_NN_DOA || USE_GCC_DOA) || USE_DUAL_MIC_ANY)  && USE_AEC_MODULE
  #if AUDIO_IN_FROM_DMIC
    #if !IF_USE_ANOTHER_CODEC_TO_GET_REF && !REF_IN_FROM_INNER_CODEC
    #error "doa + aec algorithm, requires external codec1\r\n"
    #endif
  #else
    #if !IF_USE_ANOTHER_CODEC_TO_GET_REF
    #error "doa + aec algorithm, requires external codec\r\n"
    #endif
  #endif
#endif

#if USE_DEREVERB_MODULE || (USE_NN_DOA || USE_GCC_DOA)
    #if HOST_MIC_USE_NUMBER == 1
    #error "algorithm requires tow mic, please set HOST_MIC_USE_NUMBER = 2\r\n"
    #endif
#endif

#if (AUDIO_DATA_UPLOAD_BY_UART&&(USE_PWK))                       //语音上传功能不能开USE_PWK
#error "ONLY USE_NULL WITH NO PWK SUPPORT AUDIO_DATA_UPLOAD_BY_UART!"
#endif

#if USE_ALC_AUTO_SWITCH_MODULE && ((USE_NN_DOA || USE_GCC_DOA) || USE_DEREVERB_MODULE || USE_BEAMFORMING_MODULE || USE_AEC_MODULE)
#error "two mic alg, no alc auto\r\n"
#endif



//单麦就近唤醒 和 动态ALC和深度降噪,自学习，事件检测算法不能同时使用
#if USE_PWK && (USE_ALC_AUTO_SWITCH_MODULE || USE_NN_DENOISE || USE_CWSL || USE_SED_CRY || USE_SED_SNORE)
#error "can't be toghter pwk\r\n"
#endif

//语音模块协议 和 云端协议使能不能同时使用
#if MSG_COM_USE_UART_EN && CLOUD_UART_PROTOCOL_EN
#error "MSG_COM_USE_UART_EN and CLOUD_UART_PROTOCOL_EN, can't be toghter\r\n"
#endif
//开启红外功能必须用二代协议
#if USE_IR_ENABLE && (UART_PROTOCOL_VER != 2)
#error "use ir function, UART_PROTOCOL_VER must set 2!\r\n"
#endif

#if AUDIO_COMPRESS_SPEEX_ENABLE && AUDIO_COMPRESS_OPUS_ENABLE
#error  "audio compress not support opus and speex at the same time"
#endif
#if NET_AUDIO_PLAY_BY_MP3 && NET_AUDIO_PLAY_BY_PCM
#error  "audio play not support mp3 and pcm at the same time"
#elif NET_AUDIO_PLAY_BY_MP3 && NET_AUDIO_PLAY_BY_OPUS
#error  "audio play not support mp3 and opus at the same time"
#elif NET_AUDIO_PLAY_BY_OPUS && NET_AUDIO_PLAY_BY_PCM
#error  "audio play not support opus and pcm at the same time"
#endif
//opus+mp3算法组合不支持自学习和双麦算法，内存不足
#if AUDIO_COMPRESS_OPUS_ENABLE && NET_AUDIO_PLAY_BY_MP3 && USE_CWSL
#error  "opus compress and mp3 play not support cwsl at the same time"
#endif
#if NET_AUDIO_PLAY_BY_OPUS && AUDIO_COMPRESS_OPUS_ENABLE
#error  "opus play and opus record not support at the same time"
#endif
#if AUDIO_COMPRESS_SPEEX_ENABLE && NET_AUDIO_PLAY_BY_OPUS
#error  "speex compress and opus play not support at the same time"
#endif
#if USE_EXTERNAL_CRYSTAL_OSC != 1
#error  "aiot application must be USE_EXTERNAL_CRYSTAL_OSC  = 1"
#endif







#endif /* _USER_CONFIG_H_ */
