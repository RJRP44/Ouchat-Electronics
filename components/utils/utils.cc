//
// Created by Romain on 09/03/2024.
//

#include "utils.h"

namespace ouchat
{
    float utils::normalize(const float x, const float min, const float max)
    {
        return (x - min) / (max - min);
    }

    float utils::denormalize(const float n, const float min, const float max)
    {
        return n * (max - min) + min;
    }
} // ouchat
