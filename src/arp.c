#include <string.h>
#include <stdio.h>
#include "net.h"
#include "arp.h"
#include "ethernet.h"
/**
 * @brief 初始的arp包
 *
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type16 = constswap16(ARP_HW_ETHER),
    .pro_type16 = constswap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = NET_IF_IP,
    .sender_mac = NET_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表，<ip,mac>的容器
 *
 */
map_t arp_table;

/**
 * @brief arp buffer，<ip,buf_t>的容器
 *
 */
map_t arp_buf;

/**
 * @brief 打印一条arp表项
 *
 * @param ip 表项的ip地址
 * @param mac 表项的mac地址
 * @param timestamp 表项的更新时间
 */
void arp_entry_print(void *ip, void *mac, time_t *timestamp)
{
    printf("%s | %s | %s\n", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个arp表
 *
 */
void arp_print()
{
    printf("===ARP TABLE BEGIN===\n");
    map_foreach(&arp_table, arp_entry_print);
    printf("===ARP TABLE  END ===\n");
}

/**
 * @brief 发送一个arp请求
 *
 * @param target_ip 想要知道的目标的ip地址
 */
void arp_req(uint8_t *target_ip)
{
    // TO-DO
    // 调用buf_init()对txbuf进行初始化
    buf_init(&txbuf, sizeof(arp_pkt_t));
    // 填写ARP报头
    arp_pkt_t *arp_pkt = (arp_pkt_t *)txbuf.data;
    memcpy(arp_pkt, &arp_init_pkt, sizeof(arp_pkt_t));
    // ARP操作类型为ARP_REQUEST，注意大小端转换
    arp_pkt->opcode16 = constswap16(ARP_REQUEST);
    // memcpy(arp_pkt->opcode16, constswap16(ARP_REQUEST), sizeof(uint16_t));
    memcpy(arp_pkt->target_ip, target_ip, NET_IP_LEN);
    // 调用ethernet_out()发送数据包
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 发送一个arp响应
 *
 * @param target_ip 目标ip地址
 * @param target_mac 目标mac地址
 */
void arp_resp(uint8_t *target_ip, uint8_t *target_mac)
{
    // TO-DO
    // 调用buf_init()对txbuf进行初始化
    buf_init(&txbuf, sizeof(arp_pkt_t));
    // 填写ARP报头
    arp_pkt_t *arp_pkt = (arp_pkt_t *)txbuf.data;
    memcpy(arp_pkt, &arp_init_pkt, sizeof(arp_pkt_t));
    // ARP操作类型为ARP_REPLY，注意大小端转换
    arp_pkt->opcode16 = constswap16(ARP_REPLY);
    // memcpy(arp_pkt->opcode16, constswap16(ARP_REPLY), sizeof(uint16_t));
    memcpy(arp_pkt->target_ip, target_ip, NET_IP_LEN);
    memcpy(arp_pkt->target_mac, target_mac, NET_MAC_LEN);
    // 调用ethernet_out()发送数据包
    ethernet_out(&txbuf, target_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void arp_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    // 首先判断数据长度，如果数据长度小于ARP头部长度，则认为数据包不完整，丢弃不处理
    if (buf->len < sizeof(arp_pkt_t))
    {
        return;
    }
    // 接着，做报头检查，查看报文是否完整，检测内容包括：
    // ARP报头的硬件类型、上层协议类型、MAC硬件地址长度、IP协议地址长度、操作类型，
    // 检测该报头是否符合协议规定
    arp_pkt_t *arp_pkt = (arp_pkt_t *)buf->data;
    if (swap16(arp_pkt->hw_type16) != ARP_HW_ETHER 
        || swap16(arp_pkt->pro_type16) != NET_PROTOCOL_IP 
        || arp_pkt->hw_len != NET_MAC_LEN 
        || arp_pkt->pro_len != NET_IP_LEN 
        || (swap16(arp_pkt->opcode16) != ARP_REQUEST 
            && swap16(arp_pkt->opcode16) != ARP_REPLY))
    {
        return;
    }
    // 调用map_set()函数更新ARP表项
    map_set(&arp_table, arp_pkt->sender_ip, arp_pkt->sender_mac);
    // 调用map_get()函数查看该接收报文的IP地址是否有对应的arp_buf缓存。
    buf_t *cache = map_get(&arp_buf, arp_pkt->sender_ip);

    // 判断接收到的报文是否为ARP_REQUEST请求报文
    // 并且该请求报文的target_ip是本机的IP
    // 则认为是请求本主机MAC地址的ARP请求报文
    // 则调用arp_resp()函数回应一个响应报文
    if (arp_pkt->opcode16 == swap16(ARP_REQUEST))
    {
        if (memcmp(arp_pkt->target_ip, net_if_ip, NET_IP_LEN) == 0)
            arp_resp(arp_pkt->sender_ip, arp_pkt->sender_mac);
    }
    // 如果该接收报文的IP地址没有对应的arp_buf缓存
    // 则说明ARP分组队列里面没有待发送的数据包
    // 也就是上一次调用arp_out()函数发送来自IP层的数据包时
    // 由于没有找到对应的MAC地址进而先发送的ARP request报文
    // 此时收到了该request的应答报文
    // 但是由于ARP分组队列里面没有待发送的数据包
    // 所以没有缓存该应答报文，所以这个应答报文就被丢弃了

    // 如果该接收报文的IP地址有对应的arp_buf缓存
    // 则说明ARP分组队列里面有待发送的数据包。
    // 也就是上一次调用arp_out()函数发送来自IP层的数据包时，
    // 由于没有找到对应的MAC地址进而先发送的ARP request报文，
    // 此时收到了该request的应答报文。
    // 然后，将缓存的数据包arp_buf再发送给以太网层，
    // 即调用ethernet_out()函数直接发出去，
    // 接着调用map_delete()函数将这个缓存的数据包删除掉。
    else
    {
        if (cache != NULL)
        {
            ethernet_out(cache, arp_pkt->sender_mac, NET_PROTOCOL_IP);
            map_delete(&arp_buf, arp_pkt->sender_ip);
        }
    }
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip)
{
    // TO-DO
    // 从arp_table中查找目标ip对应的mac地址
    uint8_t *mac = map_get(&arp_table, ip);
    // 如果找到了，调用ethernet_out()发送数据包
    if (mac != NULL)
    {
        ethernet_out(buf, mac, NET_PROTOCOL_IP);
    }
    // 如果没有找到对应的MAC地址，进一步判断arp_buf是否已经有包了
    // 如果有，则说明正在等待该ip回应ARP请求，此时不能再发送arp请求
    // 如果没有包，则调用map_set()函数将来自IP层的数据包缓存到arp_buf
    // 然后，调用arp_req()函数，发一个请求目标IP地址对应的MAC地址的ARP request报文。
    else
    {
        if (map_get(&arp_buf, ip) != NULL)
        {
            return;
        }
        map_set(&arp_buf, ip, buf);
        arp_req(ip);
    }
}

/**
 * @brief 初始化arp协议
 *
 */
void arp_init()
{
    map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL);
    map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, buf_copy);
    net_add_protocol(NET_PROTOCOL_ARP, arp_in);
    arp_req(net_if_ip);
}