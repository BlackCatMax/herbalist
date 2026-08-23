// ShrineTypes.h
//
// Капища (02_GDD/15_Cycles_And_Shrines.md §15.5), v1: эффекты 1/2/4 из четырёх
// (сопротивление/устойчивость релаксации, надбавка к Coherence, защита
// инвентаря). Типоспецифичные бонусы (таблица шести типов, §15.5 "Типы
// капищ") и глобальное состояние Буяна — сознательно не в этом проходе,
// тот же принцип вертикального среза, что уже был у "Проявление сущностей"
// (4 из 62 карточек бестиария, не всё сразу).
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
        // Влияние капищ на клетку — общий кусок для эффекта 1 (модуляция
        // релаксации, RegenerateCellParameters) и эффекта 2 (надбавка к
        // Coherence, ProcessApplyCommand): при перекрытии радиусов берём
        // максимум по |Restoration| с сохранением знака, а не сумму — иначе
        // два капища рядом давали бы вчетверо эффект без всякого обоснования
        // в дизайн-документе (§15.5 говорит про одно капище, не про сложение).
        inline float GetInfluenceAt(const FIntPoint& CellCoord, const TArray<FShrine>& Shrines, int32 InfluenceRadius)
        {
            float Best = 0.0f;
            for (const FShrine& S : Shrines)
            {
                const int32 Dist = FMath::Max(FMath::Abs(CellCoord.X - S.Cell.X), FMath::Abs(CellCoord.Y - S.Cell.Y));
                if (Dist > InfluenceRadius) continue;
                if (FMath::Abs(S.Restoration) > FMath::Abs(Best))
                {
                    Best = S.Restoration;
                }
            }
            return Best;
        }
    }
}
