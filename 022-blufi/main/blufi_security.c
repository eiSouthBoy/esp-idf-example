/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_random.h"
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
#include "esp_bt.h"
#endif

#include "esp_blufi_api.h"
#include "blufi.h"

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "esp_crc.h"

/*
   The SEC_TYPE_xxx is for self-defined packet data type in the procedure of "BLUFI negotiate key"
   If user use other negotiation procedure to exchange(or generate) key, should redefine the type by yourself.
 */
#define SEC_TYPE_DH_PARAM_LEN   0x00
#define SEC_TYPE_DH_PARAM_DATA  0x01
#define SEC_TYPE_DH_P           0x02
#define SEC_TYPE_DH_G           0x03
#define SEC_TYPE_DH_PUBLIC      0x04


struct blufi_security {
#define DH_SELF_PUB_KEY_LEN     128
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)
    uint8_t  self_public_key[DH_SELF_PUB_KEY_LEN];
#define SHARE_KEY_LEN           128
#define SHARE_KEY_BIT_LEN       (SHARE_KEY_LEN * 8)
    uint8_t  share_key[SHARE_KEY_LEN];
    size_t   share_len;
#define PSK_LEN                 16
    uint8_t  psk[PSK_LEN];
    uint8_t  *dh_param;
    int      dh_param_len;
    uint8_t  iv[16];
    mbedtls_dhm_context dhm;
    mbedtls_aes_context aes;
};
static struct blufi_security *blufi_sec;

static int myrand( void *rng_state, unsigned char *output, size_t len )
{
    esp_fill_random(output, len);
    return( 0 );
}

extern void btc_blufi_report_error(esp_blufi_error_state_t state);

/* 使用DH算法对数据进行协商处理 */
void blufi_dh_negotiate_data_handler(uint8_t *data, 
                                     int len, 
                                     uint8_t **output_data, 
                                     int *output_len, 
                                     bool *need_free)
{
    int ret;
    uint8_t type = data[0];

    if (blufi_sec == NULL) 
    {
        BLUFI_ERROR("BLUFI Security is not initialized");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    BLUFI_INFO("--> BLUFI DH negotiate data handle");
    if (type == SEC_TYPE_DH_PARAM_LEN)
        BLUFI_INFO("--> type: SEC_TYPE_DH_PARAM_LEN");
    else if (SEC_TYPE_DH_PARAM_DATA)
        BLUFI_INFO("--> type: SEC_TYPE_DH_PARAM_DATA");
    else
        BLUFI_INFO("--> type: other");

    printf("input data(%d): ", len);
    for (int i = 0; i < len; i++)
    {
        printf("%02x ", data[i]);
    }
    printf("\n");

    switch (type)
    {
    case SEC_TYPE_DH_PARAM_LEN:
        blufi_sec->dh_param_len = ((data[1] << 8) | data[2]);
        BLUFI_INFO("--> malloc dh_param_len: %d", blufi_sec->dh_param_len);
        if (blufi_sec->dh_param)
        {
            free(blufi_sec->dh_param);
            blufi_sec->dh_param = NULL;
        }
        blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);
        if (blufi_sec->dh_param == NULL)
        {
            btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            BLUFI_ERROR("%s, malloc failed\n", __func__);
            return;
        }
        break;
    case SEC_TYPE_DH_PARAM_DATA:
    {
        if (blufi_sec->dh_param == NULL)
        {
            BLUFI_ERROR("%s, blufi_sec->dh_param == NULL\n", __func__);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }

        // 数据拆分
        uint16_t len1 = (((uint16_t)data[1]) << 8) | data[2];
        printf("1 second data len: %d\n", len1);
        uint16_t len2 = (((uint16_t)data[2+len1+1]) << 8) | data[2+len1+2];
        printf("2 second data len: %d\n", len2);
        uint16_t len3 = (((uint16_t)data[(2+len1)+(2+len2)+1]) << 8) | data[(2+len1)+(2+len2)+2];
        printf("3 second data len: %d\n", len3);


        BLUFI_INFO("--> dh_param_len: %d", blufi_sec->dh_param_len);
        uint8_t *param = blufi_sec->dh_param;
        memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
        ret = mbedtls_dhm_read_params(&blufi_sec->dhm, &param, &param[blufi_sec->dh_param_len]);
        if (ret)
        {
            BLUFI_ERROR("%s read param failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
            return;
        }
        free(blufi_sec->dh_param);
        blufi_sec->dh_param = NULL;

        // Print P, G, GY
        // printf("P(%d): \n", blufi_sec->dhm.P.n);
        // for (int i = 0; i < blufi_sec->dhm.P.n; i++)
        //     printf("", blufi_sec->dhm.P.p[i]);
        // printf("\n");

        // printf("G(%d): \n", blufi_sec->dhm.G.n);
        // for (int i = 0; i < blufi_sec->dhm.G.n; i++)
        //     printf("", blufi_sec->dhm.G.p[i]);
        // printf("\n");

        // printf("GY(%d): \n", blufi_sec->dhm.GY.n);
        // for (int i = 0; i < blufi_sec->dhm.GY.n; i++)
        //     printf("", blufi_sec->dhm.GY.p[i]);
        // printf("\n");

        const int dhm_len = mbedtls_dhm_get_len(&blufi_sec->dhm);
        printf("--> dhm read params len: %d, dhm_len: %d\n", ret, dhm_len);
        

        BLUFI_INFO("--> mbedtls_dhm_make_public(), make public key");
        ret = mbedtls_dhm_make_public(&blufi_sec->dhm, 
                                      dhm_len, 
                                      blufi_sec->self_public_key, 
                                      dhm_len, 
                                      myrand, 
                                      NULL);
        if (ret)
        {
            BLUFI_ERROR("%s make public failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
            return;
        }
        printf("public key(%d):\n", DH_SELF_PUB_KEY_LEN);
        for (int i = 0; i < DH_SELF_PUB_KEY_LEN; i++)
        {
            printf("%02x ", blufi_sec->self_public_key[i]);
        }
        printf("\n");

        BLUFI_INFO("--> mbedtls_dhm_calc_secret(), get share key");
        ret = mbedtls_dhm_calc_secret(&blufi_sec->dhm,
                                      blufi_sec->share_key,
                                      SHARE_KEY_BIT_LEN,
                                      &blufi_sec->share_len,
                                      myrand, NULL);
        if (ret)
        {
            BLUFI_ERROR("%s mbedtls_dhm_calc_secret failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
            return;
        }
        printf("share key(%d):\n", blufi_sec->share_len);
        for (int i = 0; i < blufi_sec->share_len; i++)
        {
            printf("%02x ", blufi_sec->share_key[i]);
        }
        printf("\n");


        BLUFI_INFO("--> mbedtls_md5(), md5 for share key");
        ret = mbedtls_md5(blufi_sec->share_key, blufi_sec->share_len, blufi_sec->psk);
        if (ret)
        {
            BLUFI_ERROR("%s mbedtls_md5 failed %d\n", __func__, ret);
            btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
            return;
        }

        // Print PSK
        printf("--> psk(%d): \n", PSK_LEN);
        for (int i = 0; i < PSK_LEN; i++)
        {
            printf("%02x ", blufi_sec->psk[i]);
        }
        printf("\n");

        BLUFI_INFO("--> mbedtls_aes_setkey_enc(), AES encrypt psk 128bit");
        mbedtls_aes_setkey_enc(&blufi_sec->aes, blufi_sec->psk, 128);
        
        // printf("%d\n", 68);
        // for (int i = 0; i < 68; i++)
        // {
        //     printf("%02x ", blufi_sec->aes.buf[i]);
        // }
        // printf("\n");


        /* alloc output data */
        *output_data = &blufi_sec->self_public_key[0];
        *output_len = dhm_len;
        *need_free = false;

        printf("output data(%d): ", *output_len);
        for (int i = 0; i < *output_len; i++)
        {
            printf("%02x ", (*output_data)[i]);
        }
        printf("\n");
    }
    break;
    case SEC_TYPE_DH_P:
        break;
    case SEC_TYPE_DH_G:
        break;
    case SEC_TYPE_DH_PUBLIC:
        break;
    }
}

/* AES数据加密 */
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret = 0;
    size_t iv_offset = 0;
    uint8_t iv0[16] = {0};

    BLUFI_INFO("--> mbedtls_aes_crypt_cfb128() with MBEDTLS_AES_ENCRYPT");
    memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
    iv0[0] = iv8;   /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&blufi_sec->aes, 
                                   MBEDTLS_AES_ENCRYPT, 
                                   crypt_len, 
                                   &iv_offset, 
                                   iv0, 
                                   crypt_data, 
                                   crypt_data);
    if (ret) 
    {
        return -1;
    }

    printf("--> crypt_data(%d): \n", crypt_len);
    for (int i = 0; i < crypt_len; i++)
    {
        printf("%02x ", crypt_data[i]);
    }
    printf("\n");

    return crypt_len;
}

/* AES数据解密 */
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
    iv0[0] = iv8;   /* set iv8 as the iv0[0] */

    BLUFI_INFO("--> mbedtls_aes_crypt_cfb128() with MBEDTLS_AES_DECRYPT");
    ret = mbedtls_aes_crypt_cfb128(&blufi_sec->aes, 
                                   MBEDTLS_AES_DECRYPT, 
                                   crypt_len, 
                                   &iv_offset, 
                                   iv0, 
                                   crypt_data, 
                                   crypt_data);
    if (ret) 
    {
        return -1;
    }

    return crypt_len;
}

/* CRC校验 */
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    /* This iv8 ignore, not used */
    BLUFI_INFO("--> esp_crc16_be()");
    return esp_crc16_be(0, data, len);
}

esp_err_t blufi_security_init(void)
{
    BLUFI_INFO("--> blufi_security_init()");
    blufi_sec = (struct blufi_security *)malloc(sizeof(struct blufi_security));
    if (blufi_sec == NULL) 
    {
        return ESP_FAIL;
    }

    memset(blufi_sec, 0x0, sizeof(struct blufi_security));

    BLUFI_INFO("--> mbedtls_dhm_init()");
    mbedtls_dhm_init(&blufi_sec->dhm);
    BLUFI_INFO("--> mbedtls_aes_init()");
    mbedtls_aes_init(&blufi_sec->aes);

    memset(blufi_sec->iv, 0x0, 16);
    return 0;
}

void blufi_security_deinit(void)
{
    if (blufi_sec == NULL) {
        return;
    }
    if (blufi_sec->dh_param){
        free(blufi_sec->dh_param);
        blufi_sec->dh_param = NULL;
    }
    mbedtls_dhm_free(&blufi_sec->dhm);
    mbedtls_aes_free(&blufi_sec->aes);

    memset(blufi_sec, 0x0, sizeof(struct blufi_security));

    free(blufi_sec);
    blufi_sec =  NULL;
}
