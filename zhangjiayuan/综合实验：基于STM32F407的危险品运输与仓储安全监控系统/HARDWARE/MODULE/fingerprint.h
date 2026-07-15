#ifndef __ADAPTER_AUTH_H__
#define __ADAPTER_AUTH_H__

#include <stdint.h>

typedef enum {
    AUTH_METHOD_NONE = 0,
    AUTH_METHOD_NFC,
    AUTH_METHOD_FINGER,
    AUTH_METHOD_BOTH
} AuthMethod;

#define AUTH_USART1_TARGET_NONE 0U
#define AUTH_USART1_TARGET_NFC  1U
#define AUTH_USART1_TARGET_FPR  2U

void Adapter_Auth_Init(void);
uint8_t Adapter_Auth_Register(AuthMethod method, uint8_t admin);
uint8_t Adapter_Auth_Verify(AuthMethod method, uint8_t admin);
uint8_t Adapter_Auth_Login(AuthMethod method, uint8_t admin);
uint8_t Adapter_Auth_BackupVerify(const char *password, uint8_t admin);
uint8_t Adapter_Auth_PasswordLogin(const char *password);
void Adapter_Auth_Logout(void);
void Adapter_Auth_ClearRegistration(void);
uint8_t Adapter_Auth_IsRegistered(void);
uint8_t Adapter_Auth_IsNfcRegistered(void);
uint8_t Adapter_Auth_IsFingerRegistered(void);
uint8_t Adapter_Auth_IsNfcVerified(void);
uint8_t Adapter_Auth_IsFingerVerified(void);
uint8_t Adapter_Auth_IsLoggedIn(void);
uint8_t Adapter_Auth_IsAdmin(void);
const char *Adapter_Auth_UserName(void);
const char *Adapter_Auth_CardId(void);
const char *Adapter_Auth_FingerId(void);
const char *Adapter_Auth_StatusText(void);
void Adapter_Auth_SetUsart1Target(uint8_t target);
void Adapter_Auth_USART1_IRQHandler(void);

#endif
