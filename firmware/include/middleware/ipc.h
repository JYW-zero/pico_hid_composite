/*
 * include/middleware/ipc.h
 * 核间通信（Inter-Processor Communication）命令定义
 * 使用 RP2350 SIO FIFO 传递命令
 */
#ifndef MIDDLEWARE_IPC_H
#define MIDDLEWARE_IPC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 命令定义 ==================== */

/* 命令格式：32位
 * bit 31-24: 命令类型
 * bit 23-0:  参数
 */
#define IPC_CMD_TYPE_SHIFT   24
#define IPC_CMD_PARAM_MASK   0x00FFFFFFU

/* 构造命令 */
#define IPC_MAKE_CMD(type, param) \
    ((((uint32_t)(type) << IPC_CMD_TYPE_SHIFT) & 0xFF000000U) | ((uint32_t)(param) & IPC_CMD_PARAM_MASK))

/* 提取命令类型 */
#define IPC_GET_TYPE(cmd)  (((uint32_t)(cmd) >> IPC_CMD_TYPE_SHIFT) & 0xFFU)

/* 提取参数 */
#define IPC_GET_PARAM(cmd) ((uint32_t)(cmd) & IPC_CMD_PARAM_MASK)

/* 命令类型枚举 */
typedef enum
{
    IPC_CMD_NOP     = 0x00,  /* 空操作，回ACK */
    IPC_CMD_SET_DPI = 0x01,  /* 设置DPI，参数：optical_sensor_dpi_e值 */
    IPC_CMD_SLEEP   = 0x02,  /* Core1进入休眠（WFE） */
    IPC_CMD_PAUSE   = 0x03,  /* 暂停扫描，进入等待循环 */
    IPC_CMD_RESUME  = 0x04,  /* 恢复扫描 */
    IPC_CMD_SET_ENCODER_REV = 0x05,  /* 设置编码器方向，参数：0=正常，1=反转 */
    IPC_CMD_SET_JOYSTICK_DZ = 0x06,  /* 设置摇杆死区，参数：死区值 */
    IPC_CMD_PING    = 0xFF,  /* 测试命令，回写ACK */
} ipc_cmd_type_e;

/* 响应码 */
#define IPC_ACK_OK     0x00000000U
#define IPC_ACK_ERR    0xFFFFFFFFU

#ifdef __cplusplus
}
#endif

#endif /* MIDDLEWARE_IPC_H */


