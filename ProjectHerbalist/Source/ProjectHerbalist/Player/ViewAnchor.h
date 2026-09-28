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

    // Помощь в наведении по раскладке (2026-09-28, по PIE: «слишком мелкий
    // трейс, трудно целиться»): луч мимо всех заглушек -- берётся та, что
    // ближе всех к прицелу по углу, если не дальше этого конуса.
    constexpr float ItemPickAssistDegrees = 8.0f;

    template <typename ActorType>
    ActorType* NearestToAim(const TArray<TObjectPtr<ActorType>>& Candidates, const FVector& Start, const FVector& End,
        float MaxDegrees = ItemPickAssistDegrees)
    {
        const FVector Aim = (End - Start).GetSafeNormal();
        float BestCos = FMath::Cos(FMath::DegreesToRadians(MaxDegrees));
        ActorType* Best = nullptr;
        for (ActorType* Candidate : Candidates)
        {
            if (!Candidate)
            {
                continue;
            }
            const float Cos = FVector::DotProduct(Aim, (Candidate->GetActorLocation() - Start).GetSafeNormal());
            if (Cos > BestCos)
            {
                BestCos = Cos;
                Best = Candidate;
            }
        }
        return Best;
    }
}
