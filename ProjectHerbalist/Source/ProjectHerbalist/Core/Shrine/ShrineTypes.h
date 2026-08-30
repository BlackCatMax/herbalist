// ShrineTypes.h
//
// Капища (02_GDD/15_Cycles_And_Shrines.md §15.5). Все 4 эффекта реализованы:
// 1 — сопротивление/устойчивость релаксации (GridWorldManagerCore.cpp), 2 —
// надбавка к Coherence (Simulation::PipelineV2::ProcessApplyCommand), 3 —
// типоспецифичные бонусы (Лесное/Водное/Каменное/Пограничное, там же и
// BiomeGraphSubsystem.cpp; тип резолвится от клетки при регистрации,
// AGridWorldManager::ResolveShrineTypeForCell — до 2026-08-30 был жёстко
// Ancestral, находка финального аудита), 4 — защита инвентаря
// (HerbalistInventoryComponent::TickComponent). Глобальное состояние Буяна —
// отдельный механизм, не капище (см. CheckBuyanCondition), не эта задача.
//
// Капище v1 не ищется/не открывается отдельно — оно ЕСТЬ там, где стоит
// котёл (см. обсуждение в сессии: котлы всегда в строго определённых местах,
// жилище игрока и будущие мастерские, "особая аура" привязана к самому
// месту варки, не к произвольной клетке). AAlchemyTableActor::BeginPlay
// регистрирует капище на своей клетке через AGridWorldManager::RegisterShrine.
#pragma once

#include "CoreMinimal.h"
#include "ShrineTypes.generated.h"

UENUM(BlueprintType)
enum class EShrineType : uint8
{
    Ancestral,   // Родовое — любой биом
    Forest,      // Лесное — Тайга, Смешанный/Широколиственный лес
    Water,       // Водное — Болото, Речная пойма
    Stone,       // Каменное — Степь, Тундра
    Border       // Пограничное — на границе биомов графа
};

USTRUCT()
struct PROJECTHERBALIST_API FShrine
{
    GENERATED_BODY()

    UPROPERTY()
    FIntPoint Cell = FIntPoint(-1, -1);

    UPROPERTY()
    EShrineType Type = EShrineType::Ancestral;

    // [-1, 1] в теории (осквернение уводит ниже нуля, §15.5 "Restoration:
    // рост и спад"), на практике клампится в местах роста/спада, не здесь —
    // сама величина должна уметь быть отрицательной.
    UPROPERTY()
    float Restoration = 0.0f;
};

namespace HerbalistCore
{
    namespace Shrine
    {
        // Капище с наибольшим |Restoration| в радиусе — общий выбор для
        // эффектов 1/2 (см. GetInfluenceAt ниже) и эффекта 3 (типоспецифичные
        // бонусы, §15.5 "Типы капищ"), которому вдобавок к Restoration нужен
        // ещё и Type самого победившего капища, не только число. При
        // перекрытии радиусов берём максимум по |Restoration|, а не сумму —
        // два капища рядом не должны давать вчетверо эффект без обоснования
        // в дизайн-документе (§15.5 говорит про одно капище, не про сложение).
        inline const FShrine* FindDominantShrine(const FIntPoint& CellCoord, const TArray<FShrine>& Shrines, int32 InfluenceRadius)
        {
            const FShrine* Best = nullptr;
            for (const FShrine& S : Shrines)
            {
                const int32 Dist = FMath::Max(FMath::Abs(CellCoord.X - S.Cell.X), FMath::Abs(CellCoord.Y - S.Cell.Y));
                if (Dist > InfluenceRadius) continue;
                if (!Best || FMath::Abs(S.Restoration) > FMath::Abs(Best->Restoration))
                {
                    Best = &S;
                }
            }
            return Best;
        }

        inline float GetInfluenceAt(const FIntPoint& CellCoord, const TArray<FShrine>& Shrines, int32 InfluenceRadius)
        {
            const FShrine* Dominant = FindDominantShrine(CellCoord, Shrines, InfluenceRadius);
            return Dominant ? Dominant->Restoration : 0.0f;
        }
    }
}
