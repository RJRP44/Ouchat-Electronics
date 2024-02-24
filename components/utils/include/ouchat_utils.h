//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_OUCHAT_UTILS_H
#define OUCHAT_ELECTRONICS_OUCHAT_UTILS_H

#define ABSCISSA_FROM_1D(i) (uint8_t) floor(i / 8.00)
#define ORDINATE_FROM_1D(i) (i % 8)

#define NEIGHBOURS_FOR_LOOP int8_t x = 0, y = 0; x < 3 && y < 3; y = (y + 1) % 3, x += y ? 0 : 1
#define X_Y_FOR_LOOP int x = 0, y = 0; x < 8 && y < 8; y = (y + 1) % 8, x += y ? 0 : 1

#define MAX_CAT_HEIGHT 400
#define CAT_FLAP_RADIUS 125
#define AVERAGE_CAT_HEIGHT 250
#define MOTION_THRESHOLD 50

#endif //OUCHAT_ELECTRONICS_OUCHAT_UTILS_H
