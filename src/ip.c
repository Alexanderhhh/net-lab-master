#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    // 如果数据包的长度小于IP头部长度，丢弃不处理
    if (buf->len < sizeof(ip_hdr_t))
        return;
    // 报头检测，IP头部的版本号是否为IPv4，总长度字段小于或等于收到的包的长度
    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;
    if (hdr->version != IP_VERSION_4)
        return;
    if (swap16(hdr->total_len16) > buf->len)
        return;
    // 先把IP头部的头部校验和字段用其他变量保存起来，
    // 接着将该头部校验和字段置0，然后调用checksum16函数来计算头部校验和，
    // 如果与IP头部的首部校验和字段不一致，丢弃不处理，
    // 如果一致，则再将该头部校验和字段恢复成原来的值。
    uint16_t checksum = hdr->hdr_checksum16;
    hdr->hdr_checksum16 = 0;
    uint16_t checksum_tmp = checksum16((uint16_t *)buf->data, sizeof(ip_hdr_t));
    if (swap16(checksum) != checksum_tmp)
        return;
    hdr->hdr_checksum16 = checksum;
    // 对比目的IP地址是否为本机的IP地址，如果不是，则丢弃不处理。
    if (memcmp(hdr->dst_ip, net_if_ip, NET_IP_LEN) != 0)
        return;
    // 如果接收到的数据包的长度大于IP头部的总长度字段，
    // 则说明该数据包有填充字段，可调用buf_remove_padding()函数去除填充字段。
    if (swap16(hdr->total_len16) < buf->len)
        buf_remove_padding(buf, buf->len - swap16(hdr->total_len16));
    // 如果是不能识别的协议类型，即调用icmp_unreachable()返回ICMP协议不可达信息。
    // 调用buf_remove_header()函数去掉IP报头。
    // 调用net_in()函数向上层传递数据包。
    switch (hdr->protocol)
    {
        case(NET_PROTOCOL_UDP):
        case(NET_PROTOCOL_ICMP):
            buf_remove_header(buf, hdr->hdr_len * IP_HDR_LEN_PER_BYTE);
            net_in(buf, hdr->protocol, hdr->src_ip);
            break;
        default:
            icmp_unreachable(buf, hdr->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
            break;
    }
}

/**
 * @brief 处理一个要发送的ip分片
 *
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TO-DO
    // 调用buf_add_header()增加IP数据报头部缓存空间。
    buf_add_header(buf, sizeof(ip_hdr_t));
    // 填写IP数据报头部字段。
    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;
    hdr->hdr_len = sizeof(ip_hdr_t) / IP_HDR_LEN_PER_BYTE;
    hdr->version = IP_VERSION_4;
    hdr->tos = 0;
    hdr->total_len16 = swap16(buf->len);
    hdr->id16 = swap16(id);
    uint16_t flags_fragment = (offset & 0x1FFFFFFF);
    if (mf == 1)
        flags_fragment |= IP_MORE_FRAGMENT;
    hdr->flags_fragment16 = swap16(flags_fragment);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    memcpy(hdr->src_ip, net_if_ip, NET_IP_LEN);
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    // 先把IP头部的首部校验和字段填0，
    // 再调用checksum16函数计算校验和，然后把计算出来的校验和填入首部校验和字段。
    hdr->hdr_checksum16 = 0;
    hdr->hdr_checksum16 = swap16(checksum16((uint16_t *)buf->data, sizeof(ip_hdr_t)));
    // 调用arp_out函数()将封装后的IP头部和数据发送出去。
    arp_out(buf, ip);
}

int send_id = 0;
/**
 * @brief 处理一个要发送的ip数据包
 *
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TO-DO
    // 首先检查从上层传递下来的数据报包长是否大于IP协议最大负载包长（1500字节（MTU） 减去IP首部长度）。
    size_t eachlen = 1500 - sizeof(ip_hdr_t);
    if (eachlen != 1480)
        printf("ERROR!");
    // 分片发送
    int i = 0;

    for (i = 0; (i + 1) * eachlen < buf->len; i++)
    {
        // 首先调用buf_init()初始化一个ip_buf,将数据报包长截断
        buf_t ip_buf;
        buf_init(&ip_buf, eachlen);
        memcpy(ip_buf.data, buf->data + i * eachlen, eachlen);
        // 调用ip_fragment_out()函数发送出去
        ip_fragment_out(&ip_buf, ip, protocol, send_id, i * (eachlen >> 3), 1);
    }
    if (buf->len - i * eachlen <= 0)
        printf("ERROR");
    // 如果截断后最后的一个分片小于或等于IP协议最大负载包长，
    // 调用buf_init()初始化一个ip_buf，大小等于该分片大小
    buf_t ip_buf;
    buf_init(&ip_buf, buf->len - i * eachlen);
    memcpy(ip_buf.data, buf->data + i * eachlen, buf->len - i * eachlen);
    // 再调用ip_fragment_out()函数发送出去
    ip_fragment_out(&ip_buf, ip, protocol, send_id, i * (eachlen >> 3), 0);
    send_id++;
}

/**
 * @brief 初始化ip协议
 *
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}