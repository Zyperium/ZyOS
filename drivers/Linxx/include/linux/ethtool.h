#pragma once
#include <linux/gfp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct net_device;

struct ethtool_ops {
    int (*get_link)(struct net_device *dev);
};

#ifdef __cplusplus
}
#endif