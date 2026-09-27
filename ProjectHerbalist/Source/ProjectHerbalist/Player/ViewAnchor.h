// Player/ViewAnchor.h
//
// Раскладка перед глазами, по которой водят взглядом (пестерь, пояс;
// 2026-09-27, по жалобе пользователя «невозможно прицелиться, они двигаются
// вместе с игроком»). Раньше раскладка поворачивалась вместе со взглядом, и
// прицел всегда смотрел в её середину. Теперь поворот раскладки запоминается
// при открытии: мышь водит прицелом по мешочкам. Отвернулся дальше края --
// раскладка подтягивается за взглядом, чтобы не потерять её из виду.
#pragma once

#include "CoreMinimal.h"

namespace HerbalistView
{
    // Насколько взгляд может уйти в сторону от раскладки, прежде чем она
    // потянется следом.
    constexpr float AnchorMaxYawOffsetDegrees = 45.0f;

    // Новый поворот раскладки: прежний, пока взгляд в пределах края, иначе --
    // ровно на краю от взгляда.
    inline float FollowAnchorYaw(float AnchorYaw, float ViewYaw, float MaxOffsetDegrees = AnchorMaxYawOffsetDegrees)
    {
        const float Delta = FRotator::NormalizeAxis(ViewYaw - AnchorYaw);
        if (Delta > MaxOffsetDegrees)
        {
            return FRotator::NormalizeAxis(ViewYaw - MaxOffsetDegrees);
        }
        if (Delta < -MaxOffsetDegrees)
        {
            return FRotator::NormalizeAxis(ViewYaw + MaxOffsetDegrees);
        }
        return AnchorYaw;
    }
}
