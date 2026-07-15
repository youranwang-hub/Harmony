#include "fingerprint.h"
#include "nfc.h"
#include "system_config.h"
#include "gpio_control.h"
#include "delay.h"
#include "usart_debug.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#define AUTH_ID_LEN 24U
#define AUTH_BACKUP_PASSWORD "25531460"

static uint8_t s_registered = 0U;
static uint8_t s_nfc_registered = 0U;
static uint8_t s_finger_registered = 0U;
static uint8_t s_nfc_verified = 0U;
static uint8_t s_finger_verified = 0U;
static uint8_t s_login = 0U;
static uint8_t s_admin = 0U;
static uint8_t s_registered_admin = 0U;
static char s_name[AUTH_ID_LEN];
static char s_card_id[AUTH_ID_LEN];
static char s_finger_id[AUTH_ID_LEN];
static uint16_t s_finger_page_id = 0xFFFFU;
static volatile uint8_t s_usart1_target = AUTH_USART1_TARGET_NONE;

#if APP_ENABLE_REAL_FINGERPRINT

#define ZN632_USART                USART1
#define ZN632_BUF_LEN              128U
#define ZN632_INDEX_MAX            240U
#define ZN632_IMAGE_TRIES          40U
#define ZN632_CMD_GET_IMAGE        0x01U
#define ZN632_CMD_GEN_CHAR         0x02U
#define ZN632_CMD_SEARCH           0x04U
#define ZN632_CMD_REG_MODEL        0x05U
#define ZN632_CMD_STORE_CHAR       0x06U
#define ZN632_CMD_VRY_PWD          0x13U
#define ZN632_CMD_READ_INDEX_TABLE 0x1FU

static uint8_t s_fpr_rx_buf[ZN632_BUF_LEN];
static volatile uint32_t s_fpr_rx_len;
static uint8_t s_zn632_addr[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
static uint8_t s_zn632_index[32];
static uint8_t s_fpr_ready;

static void fpr_rx_byte(uint8_t byte)
{
    if (s_fpr_rx_len < ZN632_BUF_LEN) {
        s_fpr_rx_buf[s_fpr_rx_len++] = byte;
    }
}

static void fpr_clear_rx(void)
{
    memset(s_fpr_rx_buf, 0, sizeof(s_fpr_rx_buf));
    s_fpr_rx_len = 0U;
}

static void fpr_send_data(const uint8_t *data, uint8_t length)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        USART_SendData(ZN632_USART, data[i]);
        while (USART_GetFlagStatus(ZN632_USART, USART_FLAG_TXE) == RESET) {}
    }
}

static void fpr_send_head(void)
{
    uint8_t head[6];

    head[0] = 0xEFU;
    head[1] = 0x01U;
    head[2] = s_zn632_addr[0];
    head[3] = s_zn632_addr[1];
    head[4] = s_zn632_addr[2];
    head[5] = s_zn632_addr[3];
    fpr_send_data(head, sizeof(head));
}

static int16_t fpr_check_ack(void)
{
    uint16_t i;
    uint16_t len;
    uint16_t temp = 0U;
    uint16_t sum;

    if (s_fpr_rx_len < 12U) {
        return -1;
    }
    if (s_fpr_rx_buf[0] != 0xEFU || s_fpr_rx_buf[1] != 0x01U) {
        return -1;
    }
    if (s_fpr_rx_buf[2] != s_zn632_addr[0] ||
        s_fpr_rx_buf[3] != s_zn632_addr[1] ||
        s_fpr_rx_buf[4] != s_zn632_addr[2] ||
        s_fpr_rx_buf[5] != s_zn632_addr[3]) {
        return -1;
    }

    len = (uint16_t)((s_fpr_rx_buf[7] << 8) | s_fpr_rx_buf[8]);
    if ((uint32_t)(9U + len) > s_fpr_rx_len) {
        return -1;
    }

    for (i = 0U; i <= len; i++) {
        temp = (uint16_t)(temp + s_fpr_rx_buf[6U + i]);
    }

    sum = (uint16_t)((s_fpr_rx_buf[9U + len - 2U] << 8) |
                     s_fpr_rx_buf[9U + len - 1U]);
    if (sum != temp) {
        return -1;
    }

    return (int16_t)s_fpr_rx_buf[9];
}

static int16_t fpr_send_cmd_wait(const uint8_t *cmd, uint8_t length, uint16_t wait_ms)
{
    int16_t ret;

    fpr_clear_rx();
    fpr_send_head();
    fpr_send_data(cmd, length);
    delay_ms(wait_ms);
    ret = fpr_check_ack();
    return ret;
}

static void fpr_usart1_init(uint32_t baud)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(ZN632_USART);
    usart.USART_BaudRate = baud;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ZN632_USART, &usart);
    USART_ITConfig(ZN632_USART, USART_IT_RXNE, ENABLE);
    USART_Cmd(ZN632_USART, ENABLE);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 1U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static int16_t fpr_power_on(void)
{
    uint16_t i;

    GPIO_SetBits(GPIOC, GPIO_Pin_9);
    delay_ms(100U);
    fpr_clear_rx();
    GPIO_ResetBits(GPIOC, GPIO_Pin_9);
    for (i = 0U; i < 1000U; i++) {
        if (s_fpr_rx_buf[0] == 0x55U) {
            fpr_clear_rx();
            return 0;
        }
        delay_ms(1U);
    }

    return -1;
}

static int16_t fpr_vry_pwd(void)
{
    uint16_t sum;
    uint8_t cmd[10] = {0};
    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x07U;
    cmd[3] = ZN632_CMD_VRY_PWD;
    cmd[4] = 0U;
    cmd[5] = 0U;
    cmd[6] = 0U;
    cmd[7] = 0U;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6] + cmd[7]);
    cmd[8] = (uint8_t)(sum >> 8);
    cmd[9] = (uint8_t)sum;
    return fpr_send_cmd_wait(cmd, sizeof(cmd), 300U);
}

static int16_t fpr_read_index_table(void)
{
    uint16_t sum;
    int16_t ret;
    uint8_t cmd[7] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x04U;
    cmd[3] = ZN632_CMD_READ_INDEX_TABLE;
    cmd[4] = 0U;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4]);
    cmd[5] = (uint8_t)(sum >> 8);
    cmd[6] = (uint8_t)sum;

    ret = fpr_send_cmd_wait(cmd, sizeof(cmd), 300U);
    if (ret == 0) {
        memcpy(s_zn632_index, s_fpr_rx_buf + 10, sizeof(s_zn632_index));
    }
    fpr_clear_rx();
    return ret;
}

static uint16_t fpr_get_index_empty(void)
{
    uint16_t index = 0U;
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < sizeof(s_zn632_index); i++) {
        for (j = 0U; j < 8U; j++) {
            if ((s_zn632_index[i] & (uint8_t)(0x01U << j)) != 0U) {
                index++;
            } else {
                return index;
            }
        }
    }

    return ZN632_INDEX_MAX;
}

static void fpr_set_index(uint16_t index)
{
    if (index < ZN632_INDEX_MAX) {
        s_zn632_index[index / 8U] |= (uint8_t)(0x01U << (index % 8U));
    }
}

static int16_t fpr_get_image(void)
{
    uint16_t sum;
    uint8_t cmd[6] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x03U;
    cmd[3] = ZN632_CMD_GET_IMAGE;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3]);
    cmd[4] = (uint8_t)(sum >> 8);
    cmd[5] = (uint8_t)sum;
    return fpr_send_cmd_wait(cmd, sizeof(cmd), 500U);
}

static int16_t fpr_gen_char(uint8_t buffer_id)
{
    uint16_t sum;
    uint8_t cmd[7] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x04U;
    cmd[3] = ZN632_CMD_GEN_CHAR;
    cmd[4] = buffer_id;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4]);
    cmd[5] = (uint8_t)(sum >> 8);
    cmd[6] = (uint8_t)sum;
    return fpr_send_cmd_wait(cmd, sizeof(cmd), 500U);
}

static int16_t fpr_reg_model(void)
{
    uint16_t sum;
    uint8_t cmd[6] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x03U;
    cmd[3] = ZN632_CMD_REG_MODEL;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3]);
    cmd[4] = (uint8_t)(sum >> 8);
    cmd[5] = (uint8_t)sum;
    return fpr_send_cmd_wait(cmd, sizeof(cmd), 500U);
}

static int16_t fpr_store_char(uint8_t buffer_id, uint16_t page_id)
{
    uint16_t sum;
    uint8_t cmd[9] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x06U;
    cmd[3] = ZN632_CMD_STORE_CHAR;
    cmd[4] = buffer_id;
    cmd[5] = (uint8_t)(page_id >> 8);
    cmd[6] = (uint8_t)page_id;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6]);
    cmd[7] = (uint8_t)(sum >> 8);
    cmd[8] = (uint8_t)sum;
    return fpr_send_cmd_wait(cmd, sizeof(cmd), 300U);
}

static int16_t fpr_high_speed_search(uint8_t buffer_id, uint16_t *id, uint16_t *score)
{
    uint16_t sum;
    int16_t ret;
    uint8_t cmd[11] = {0};

    cmd[0] = 0x01U;
    cmd[1] = 0x00U;
    cmd[2] = 0x08U;
    cmd[3] = ZN632_CMD_SEARCH;
    cmd[4] = buffer_id;
    cmd[5] = 0U;
    cmd[6] = 0U;
    cmd[7] = (uint8_t)(ZN632_INDEX_MAX >> 8);
    cmd[8] = (uint8_t)ZN632_INDEX_MAX;
    sum = (uint16_t)(cmd[0] + cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6] + cmd[7] + cmd[8]);
    cmd[9] = (uint8_t)(sum >> 8);
    cmd[10] = (uint8_t)sum;

    ret = fpr_send_cmd_wait(cmd, sizeof(cmd), 300U);
    if (ret == 0 && id != 0 && score != 0) {
        *id = (uint16_t)((s_fpr_rx_buf[10] << 8) | s_fpr_rx_buf[11]);
        *score = (uint16_t)((s_fpr_rx_buf[12] << 8) | s_fpr_rx_buf[13]);
    }
    fpr_clear_rx();
    return ret;
}

static uint8_t fpr_wait_image(const char *prompt)
{
    uint8_t i;

    UART_SendString(USART2, prompt);
    for (i = 0U; i < ZN632_IMAGE_TRIES; i++) {
        if (fpr_get_image() == 0) {
            UART_SendString(USART2, "[FPR] Image OK\r\n");
            return 1U;
        }
        delay_ms(100U);
    }

    UART_SendString(USART2, "[FPR] Image timeout\r\n");
    return 0U;
}

static uint8_t fpr_ensure_ready(void)
{
    if (s_fpr_ready != 0U) {
        return 1U;
    }

    Adapter_Auth_SetUsart1Target(AUTH_USART1_TARGET_FPR);
    fpr_usart1_init(56700U);
    if (fpr_power_on() != 0) {
        UART_SendString(USART2, "[FPR] Power on FAIL\r\n");
        return 0U;
    }
    if (fpr_vry_pwd() != 0) {
        UART_SendString(USART2, "[FPR] Password FAIL\r\n");
        return 0U;
    }
    if (fpr_read_index_table() != 0) {
        UART_SendString(USART2, "[FPR] Read index FAIL\r\n");
        return 0U;
    }

    s_fpr_ready = 1U;
    UART_SendString(USART2, "[FPR] ZN632 ready\r\n");
    return 1U;
}

static uint8_t fpr_register_real(uint16_t *page_id)
{
    uint16_t found_id = 0U;
    uint16_t score = 0U;
    uint16_t empty_id;

    if (fpr_ensure_ready() == 0U) {
        return 0U;
    }

    if (fpr_wait_image("[FPR] Put finger for enroll step 1\r\n") == 0U) {
        return 0U;
    }
    if (fpr_gen_char(1U) != 0) {
        UART_SendString(USART2, "[FPR] GenChar step1 FAIL\r\n");
        return 0U;
    }

    if (fpr_high_speed_search(1U, &found_id, &score) == 0) {
        *page_id = found_id;
        UART_SendString(USART2, "[FPR] Finger already exists, reuse ID\r\n");
        return 1U;
    }

    UART_SendString(USART2, "[FPR] Remove finger, then place same finger again\r\n");
    delay_ms(1500U);

    if (fpr_wait_image("[FPR] Put finger for enroll step 2\r\n") == 0U) {
        return 0U;
    }
    if (fpr_gen_char(2U) != 0) {
        UART_SendString(USART2, "[FPR] GenChar step2 FAIL\r\n");
        return 0U;
    }
    if (fpr_reg_model() != 0) {
        UART_SendString(USART2, "[FPR] RegModel FAIL\r\n");
        return 0U;
    }

    empty_id = fpr_get_index_empty();
    if (empty_id >= ZN632_INDEX_MAX) {
        UART_SendString(USART2, "[FPR] Library full\r\n");
        return 0U;
    }
    if (fpr_store_char(2U, empty_id) != 0) {
        UART_SendString(USART2, "[FPR] StoreChar FAIL\r\n");
        return 0U;
    }

    fpr_set_index(empty_id);
    *page_id = empty_id;
    UART_SendString(USART2, "[FPR] Enroll OK\r\n");
    return 1U;
}

static uint8_t fpr_verify_real(uint16_t expected_id, uint16_t *matched_id, uint16_t *score)
{
    if (fpr_ensure_ready() == 0U) {
        return 0U;
    }
    if (fpr_wait_image("[FPR] Put finger for verify\r\n") == 0U) {
        return 0U;
    }
    if (fpr_gen_char(1U) != 0) {
        UART_SendString(USART2, "[FPR] Verify GenChar FAIL\r\n");
        return 0U;
    }
    if (fpr_high_speed_search(1U, matched_id, score) != 0) {
        UART_SendString(USART2, "[FPR] Finger NOT match\r\n");
        return 0U;
    }
    if (expected_id != 0xFFFFU && *matched_id != expected_id) {
        UART_SendString(USART2, "[FPR] Finger ID mismatch\r\n");
        return 0U;
    }

    UART_SendString(USART2, "[FPR] Verify OK\r\n");
    return 1U;
}

#endif

static void auth_beep_ok(void)
{
    Board_Buzzer_Set(1U);
    delay_ms(80U);
    Board_Buzzer_Set(0U);
}

static void set_name(uint8_t admin)
{
    if (admin != 0U) {
        strncpy(s_name, "ADMIN-01", sizeof(s_name) - 1U);
    } else {
        strncpy(s_name, "USER-01", sizeof(s_name) - 1U);
    }
    s_name[sizeof(s_name) - 1U] = '\0';
}

static void refresh_registered_state(void)
{
    s_registered = (s_nfc_registered != 0U && s_finger_registered != 0U) ? 1U : 0U;
}

static uint8_t register_nfc(void)
{
    char card_id[AUTH_ID_LEN];

    if (NFC_ReadCard(card_id, sizeof(card_id)) == 0U) {
        return 0U;
    }

    strncpy(s_card_id, card_id, sizeof(s_card_id) - 1U);
    s_card_id[sizeof(s_card_id) - 1U] = '\0';
    s_nfc_registered = 1U;
    refresh_registered_state();
    return 1U;
}

static uint8_t register_finger(void)
{
#if APP_ENABLE_REAL_FINGERPRINT
    uint16_t page_id;
    char text[AUTH_ID_LEN];

    if (fpr_register_real(&page_id) == 0U) {
        return 0U;
    }
    s_finger_page_id = page_id;
    snprintf(text, sizeof(text), "FINGER-%03u", page_id);
    strncpy(s_finger_id, text, sizeof(s_finger_id) - 1U);
#else
    strncpy(s_finger_id, "FINGER-001", sizeof(s_finger_id) - 1U);
    s_finger_page_id = 1U;
#endif
    s_finger_id[sizeof(s_finger_id) - 1U] = '\0';
    s_finger_registered = 1U;
    refresh_registered_state();
    return 1U;
}

static uint8_t verify_nfc(void)
{
    char card_id[AUTH_ID_LEN];

    if (s_nfc_registered == 0U) {
        return 0U;
    }

    if (NFC_ReadCard(card_id, sizeof(card_id)) == 0U) {
        return 0U;
    }

    if (strcmp(card_id, s_card_id) != 0) {
        UART_SendString(USART2, "[NFC] Card UID mismatch\r\n");
        return 0U;
    }

    s_nfc_verified = 1U;
    return 1U;
}

static uint8_t verify_finger(void)
{
#if APP_ENABLE_REAL_FINGERPRINT
    uint16_t matched_id = 0U;
    uint16_t score = 0U;
    char text[64];
#endif

    if (s_finger_registered == 0U) {
        return 0U;
    }

#if APP_ENABLE_REAL_FINGERPRINT
    if (fpr_verify_real(s_finger_page_id, &matched_id, &score) == 0U) {
        return 0U;
    }
    snprintf(text, sizeof(text), "[FPR] Match ID:%u Score:%u\r\n", matched_id, score);
    UART_SendString(USART2, text);
#endif

    s_finger_verified = 1U;
    return 1U;
}

static void update_login_if_ready(uint8_t admin)
{
    if (s_registered != 0U &&
        s_nfc_verified != 0U &&
        s_finger_verified != 0U) {
        s_login = 1U;
        s_admin = admin ? 1U : s_registered_admin;
        set_name(s_admin);
    }
}

static uint8_t auth_password_matches(const char *password)
{
    if (password == 0 || strcmp(password, AUTH_BACKUP_PASSWORD) != 0) {
        return 0U;
    }

    return 1U;
}

static void set_backup_identity(uint8_t admin)
{
    s_registered = 1U;
    s_nfc_registered = 1U;
    s_finger_registered = 1U;
    s_nfc_verified = 1U;
    s_finger_verified = 1U;
    s_login = 1U;
    s_admin = admin ? 1U : s_admin;
    if (admin != 0U) {
        s_registered_admin = 1U;
    }
    strncpy(s_name, admin ? "PASS-ADMIN" : "PASS-USER", sizeof(s_name) - 1U);
    strncpy(s_card_id, "PASS-CARD", sizeof(s_card_id) - 1U);
    strncpy(s_finger_id, "PASS-FPR", sizeof(s_finger_id) - 1U);
    s_name[sizeof(s_name) - 1U] = '\0';
    s_card_id[sizeof(s_card_id) - 1U] = '\0';
    s_finger_id[sizeof(s_finger_id) - 1U] = '\0';
}

void Adapter_Auth_Init(void)
{
    Adapter_Auth_ClearRegistration();
    NFC_Init();
}

uint8_t Adapter_Auth_Register(AuthMethod method, uint8_t admin)
{
    uint8_t ok = 0U;

    if (method == AUTH_METHOD_NFC) {
        ok = register_nfc();
    } else if (method == AUTH_METHOD_FINGER) {
        ok = register_finger();
    } else if (method == AUTH_METHOD_BOTH) {
        ok = (register_nfc() != 0U && register_finger() != 0U) ? 1U : 0U;
    } else {
        ok = 0U;
    }

    if (ok != 0U) {
        s_registered_admin = admin ? 1U : 0U;
        s_nfc_verified = 0U;
        s_finger_verified = 0U;
        s_login = 0U;
        s_admin = 0U;
        set_name(s_registered_admin);
        auth_beep_ok();
    }

    refresh_registered_state();
    return ok;
}

uint8_t Adapter_Auth_Verify(AuthMethod method, uint8_t admin)
{
    uint8_t ok = 0U;

    if (s_registered == 0U) {
        return 0U;
    }

    if (admin != 0U && s_registered_admin == 0U) {
        return 0U;
    }

    if (method == AUTH_METHOD_NFC) {
        ok = verify_nfc();
    } else if (method == AUTH_METHOD_FINGER) {
        ok = verify_finger();
    } else if (method == AUTH_METHOD_BOTH) {
        ok = (verify_nfc() != 0U && verify_finger() != 0U) ? 1U : 0U;
    } else {
        ok = 0U;
    }

    if (ok != 0U) {
        update_login_if_ready(admin);
        auth_beep_ok();
    }

    return ok;
}

uint8_t Adapter_Auth_Login(AuthMethod method, uint8_t admin)
{
    return Adapter_Auth_Verify(method, admin);
}

uint8_t Adapter_Auth_BackupVerify(const char *password, uint8_t admin)
{
    if (auth_password_matches(password) == 0U) {
        return 0U;
    }

    set_backup_identity(admin);
    auth_beep_ok();
    return 1U;
}

uint8_t Adapter_Auth_PasswordLogin(const char *password)
{
    return Adapter_Auth_BackupVerify(password, 1U);
}

void Adapter_Auth_Logout(void)
{
    s_login = 0U;
    s_admin = 0U;
    s_nfc_verified = 0U;
    s_finger_verified = 0U;
    strncpy(s_name, "NONE", sizeof(s_name) - 1U);
    s_name[sizeof(s_name) - 1U] = '\0';
}

void Adapter_Auth_ClearRegistration(void)
{
    s_registered = 0U;
    s_nfc_registered = 0U;
    s_finger_registered = 0U;
    s_nfc_verified = 0U;
    s_finger_verified = 0U;
    s_login = 0U;
    s_admin = 0U;
    s_registered_admin = 0U;
    s_finger_page_id = 0xFFFFU;
#if APP_ENABLE_REAL_FINGERPRINT
    s_fpr_ready = 0U;
#endif
    strncpy(s_name, "NONE", sizeof(s_name) - 1U);
    strncpy(s_card_id, "NONE", sizeof(s_card_id) - 1U);
    strncpy(s_finger_id, "NONE", sizeof(s_finger_id) - 1U);
    s_name[sizeof(s_name) - 1U] = '\0';
    s_card_id[sizeof(s_card_id) - 1U] = '\0';
    s_finger_id[sizeof(s_finger_id) - 1U] = '\0';
}

uint8_t Adapter_Auth_IsRegistered(void)
{
    return s_registered;
}

uint8_t Adapter_Auth_IsNfcRegistered(void)
{
    return s_nfc_registered;
}

uint8_t Adapter_Auth_IsFingerRegistered(void)
{
    return s_finger_registered;
}

uint8_t Adapter_Auth_IsNfcVerified(void)
{
    return s_nfc_verified;
}

uint8_t Adapter_Auth_IsFingerVerified(void)
{
    return s_finger_verified;
}

uint8_t Adapter_Auth_IsLoggedIn(void)
{
    return s_login;
}

uint8_t Adapter_Auth_IsAdmin(void)
{
    return s_admin;
}

const char *Adapter_Auth_UserName(void)
{
    return s_name;
}

const char *Adapter_Auth_CardId(void)
{
    return s_card_id;
}

const char *Adapter_Auth_FingerId(void)
{
    return s_finger_id;
}

const char *Adapter_Auth_StatusText(void)
{
    if (s_login != 0U) {
        return s_admin ? "LOGIN ADMIN" : "LOGIN USER";
    }
    if (s_registered == 0U) {
        if (s_nfc_registered == 0U && s_finger_registered == 0U) {
            return "REGISTER NFC+FPR";
        }
        if (s_nfc_registered == 0U) {
            return "REGISTER NFC";
        }
        return "REGISTER FPR";
    }
    if (s_nfc_verified == 0U && s_finger_verified == 0U) {
        return "VERIFY NFC+FPR";
    }
    if (s_nfc_verified == 0U) {
        return "VERIFY NFC";
    }
    if (s_finger_verified == 0U) {
        return "VERIFY FPR";
    }
    return "READY";
}

void Adapter_Auth_SetUsart1Target(uint8_t target)
{
    s_usart1_target = target;
}

void Adapter_Auth_USART1_IRQHandler(void)
{
    uint8_t byte;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET) {
        return;
    }

    byte = (uint8_t)USART_ReceiveData(USART1);
    if (s_usart1_target == AUTH_USART1_TARGET_NFC) {
        NFC_USART1_RxByte(byte);
    }
#if APP_ENABLE_REAL_FINGERPRINT
    else if (s_usart1_target == AUTH_USART1_TARGET_FPR) {
        fpr_rx_byte(byte);
    }
#endif
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
}
