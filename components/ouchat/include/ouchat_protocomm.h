//
// Created by Romain on 25/03/2023.
//

#ifndef OUCHAT_ELECTRONICS_OUCHAT_PROTOCOM_H
#define OUCHAT_ELECTRONICS_OUCHAT_PROTOCOM_H

#define DEFAULT_OUCHAT_PROTOCOMM_CONFIG { \
.service_uuid = {\
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,\
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,\
},\
.nu_lookup_count = sizeof(lookup_table) / sizeof(lookup_table[0]),\
.nu_lookup = lookup_table\
};

void ouchat_start_protocomm();

#endif //OUCHAT_ELECTRONICS_OUCHAT_PROTOCOM_H
