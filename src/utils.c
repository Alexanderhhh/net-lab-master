#include "utils.h"
#include <stdio.h>
#include <string.h>
/**
 * @brief ip转字符串
 *
 * @param ip ip地址
 * @return char* 生成的字符串
 */
char *iptos(uint8_t *ip)
{
    static char output[3 * 4 + 3 + 1];
    sprintf(output, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return output;
}

/**
 * @brief mac转字符串
 *
 * @param mac mac地址
 * @return char* 生成的字符串
 */
char *mactos(uint8_t *mac)
{
    static char output[2 * 6 + 5 + 1];
    sprintf(output, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return output;
}

/**
 * @brief 时间戳转字符串
 *
 * @param timestamp 时间戳
 * @return char* 生成的字符串
 */
char *timetos(time_t timestamp)
{
    static char output[20];
    struct tm *utc_time = gmtime(&timestamp);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
    sprintf(output, "%04d-%02d-%02d %02d:%02d:%02d", utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday, utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);
    return output;
#pragma GCC diagnostic pop
}

/**
 * @brief ip前缀匹配
 *
 * @param ipa 第一个ip
 * @param ipb 第二个ip
 * @return uint8_t 两个ip相同的前缀长度
 */
uint8_t ip_prefix_match(uint8_t *ipa, uint8_t *ipb)
{
    uint8_t count = 0;
    for (size_t i = 0; i < 4; i++)
    {
        uint8_t flag = ipa[i] ^ ipb[i];
        for (size_t j = 0; j < 8; j++)
        {
            if (flag & (1 << 7))
                return count;
            else
                count++, flag <<= 1;
        }
    }
    return count;
}

/**
 * @brief 计算16位校验和
 *
 * @param buf 要计算的数据包
 * @param len 要计算的长度
 * @return uint16_t 校验和
 */
uint16_t checksum16(uint16_t *data, size_t len)
{
    // TO-DO
    uint8_t *ptr = (uint8_t *)data;
    uint32_t tmp = 0;
    int i = 0;
    // 把data看成是每16个bit（即2个字节）组成一个数，相加
    for (i = 0; i + 1 < len; i += 2)
    {
        tmp += (ptr[i] << 8) + ptr[i + 1];
        while ((tmp >> 16) > 0)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
    }
    if (len > i)
    {
        tmp += (ptr[i] << 8);
        // 判断相加后32bit结果值的高16位是否为0，
        // 如果不为0，则将高16位和低16位相加，依次循环，直至高16位为0为止。
        while ((tmp >> 16) > 0)
            tmp = (tmp & 0xFFFF) + (tmp >> 16);
    }
    // 将上述的和（低16位）取反，即得到校验和。
    return ~(uint16_t)tmp;
}