#pragma once
#include <linux/gfp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct net_device;
struct sk_buff;

struct net_device_ops {
    int      (*ndo_open)(struct net_device *dev);
    int      (*ndo_stop)(struct net_device *dev);
    int      (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void     (*ndo_tx_timeout)(struct net_device *dev, unsigned int txqueue);
};

#ifdef __cplusplus
}
#endif