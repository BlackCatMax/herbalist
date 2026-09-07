// EntityTemporalAvailabilityTest.cpp
//
// Наличие и распределение существ Низшего ранга по ВРЕМЕННЫМ гейтам:
// время суток, сезон, фаза луны, погода, узкие окна внутри лета
// (2026-09-07, прямой запрос пользователя: "проверить наличие и
// распределение существ в зависимости от времени суток, фаз луны, сезонов
// года, возможно даже от погоды, а не только состояний биомов").
//
// Замер, а не вывод формулой. Гейты зависят только от `GameClockSeconds`
// (погода — ещё и от `RngBaseSeed` через детерминированный шум), поэтому
// прогоняем часы по целому игровому году и считаем, какую долю времени
// каждый гейт открыт. Аналитика здесь дала бы неверный ответ как минимум
// по погоде: `SampleWeatherNoise` интерполирует равномерный шум
// сглаживающим `SmoothStep`, и распределение результата уже не равномерное
// — доля «ветрено» НЕ равна 1 − порог.
//
// Год: `SeasonDurationDays` (117) × 3 сезона × `GameDayMinutes` (32) × 60 =
// 673 920 игровых секунд. Шаг выборки 60 с выбран под самое узкое окно в
// проекте — Купальская ночь (3% лета И ночь, то есть ~0.19% года): при
// более крупном шаге она попадала бы в отчёт нулём и читалась бы как
// «недостижима», хотя это неправда.
//
// Тест НЕ утверждает, какая доля «правильная» — это геймдизайн. Он
// стережёт ровно одно: у каждого существа временное окно НЕНУЛЕВОЕ, то
// есть недостижимых по времени карточек в реестре нет. Сами доли печатаются
// в лог: это единственное место, где распределение видно целиком.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Types/BiomeTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    // Открыты ли ВРЕМЕННЫЕ гейты карточки в текущий момент часов.
    // Осевой порог (Distortion/Purity/...) и `bRequiresBiomeBorder` сюда
    // намеренно не входят: первый зависит от состояния клетки, второй — от
    // её места в сетке, и оба к времени отношения не имеют. Смешивать их с
    // временем значило бы получить число, которое ничего не означает.
    bool TemporalGatesOpen(const AGridWorldManager& Manager, const FAmbientEntityDefinition& Def)
    {
        if (Def.bRequiresNight && !Manager.IsNight()) return false;
        if (Def.bRequiresDusk && !Manager.IsDusk()) return false;
        if (Def.bRequiresSeason && Manager.GetSeason() != Def.RequiredSeason) return false;
        if (Def.bRequiresMoonPhase && Manager.GetMoonPhase() != Def.RequiredMoonPhase) return false;
        if (Def.bRequiresWeather)
        {
            const bool bWeatherOk = (Def.RequiredWeather == EWeatherCondition::Blizzard)
                ? Manager.IsBlizzard() : Manager.IsWindy();
            if (!bWeatherOk) return false;
        }
        if (Def.bRequiresLateSummer && !Manager.IsLateSummer()) return false;
        if (Def.bRequiresKupalaNight && !Manager.IsKupalaNight()) return false;
        return true;
    }

    FString GateSummary(const FAmbientEntityDefinition& Def)
    {
        TArray<FString> Parts;
        if (Def.bRequiresNight) Parts.Add(TEXT("ночь"));
        if (Def.bRequiresDusk) Parts.Add(TEXT("закат"));
        if (Def.bRequiresSeason) Parts.Add(FString::Printf(TEXT("сезон=%d"), static_cast<int32>(Def.RequiredSeason)));
        if (Def.bRequiresMoonPhase) Parts.Add(FString::Printf(TEXT("луна=%d"), static_cast<int32>(Def.RequiredMoonPhase)));
        if (Def.bRequiresWeather) Parts.Add(Def.RequiredWeather == EWeatherCondition::Blizzard ? TEXT("метель") : TEXT("ветер"));
        if (Def.bRequiresLateSummer) Parts.Add(TEXT("конец лета"));
        if (Def.bRequiresKupalaNight) Parts.Add(TEXT("Купальская ночь"));
        if (Def.bRequiresBiomeBorder) Parts.Add(TEXT("[граница биомов -- не время]"));
        if (Def.TriggerAxis != EAmbientTriggerAxis::None)
        {
            Parts.Add(FString::Printf(TEXT("[ось %d %s %.2f -- не время]"),
                static_cast<int32>(Def.TriggerAxis), Def.bTriggerAbove ? TEXT(">=") : TEXT("<="), Def.TriggerThreshold));
        }
        return Parts.Num() > 0 ? FString::Join(Parts, TEXT(" + ")) : FString(TEXT("без временных гейтов"));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistAmbientEntity_EveryCardHasANonZeroTemporalWindow,
    "Herbalist.AmbientEntity.EveryCardHasANonZeroTemporalWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistAmbientEntity_EveryCardHasANonZeroTemporalWindow::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const UHerbalistSettings* Settings = GetDefault<UHerbalistSettings>();
    const float DayLengthSeconds = Settings->GameDayMinutes * 60.0f;
    const float YearSeconds = Settings->SeasonDurationDays * 3.0f * DayLengthSeconds;
    const float SampleStep = 60.0f;
    const int32 SampleCount = FMath::FloorToInt(YearSeconds / SampleStep);

    const TArray<FAmbientEntityDefinition>& Defs = GetAmbientEntityDefinitions();
    if (!TestTrue(TEXT("Реестр Низшего ранга не пуст"), Defs.Num() > 0)) { Manager->Destroy(); return false; }

    TArray<int32> OpenCounts;
    OpenCounts.SetNumZeroed(Defs.Num());

    // Заодно замеряем сами фазы -- без них доли существ не с чем сравнить.
    int32 NightCount = 0, DuskCount = 0, WindyCount = 0, BlizzardCount = 0, KupalaCount = 0, LateSummerCount = 0;

    const float SavedClock = Manager->GetGameClockSeconds();
    for (int32 i = 0; i < SampleCount; ++i)
    {
        Manager->SetGameClockSeconds(i * SampleStep);

        if (Manager->IsNight()) ++NightCount;
        if (Manager->IsDusk()) ++DuskCount;
        if (Manager->IsWindy()) ++WindyCount;
        if (Manager->IsBlizzard()) ++BlizzardCount;
        if (Manager->IsKupalaNight()) ++KupalaCount;
        if (Manager->IsLateSummer()) ++LateSummerCount;

        for (int32 d = 0; d < Defs.Num(); ++d)
        {
            if (TemporalGatesOpen(*Manager, Defs[d])) ++OpenCounts[d];
        }
    }
    Manager->SetGameClockSeconds(SavedClock);

    const float Pct = 100.0f / static_cast<float>(SampleCount);

    AddInfo(FString::Printf(TEXT("Замер по %d выборкам за игровой год (%.0f с, шаг %.0f с)"), SampleCount, YearSeconds, SampleStep));
    AddInfo(FString::Printf(TEXT("Фазы: ночь %.2f%%, закат %.2f%%, ветрено %.2f%%, метель %.2f%%, конец лета %.2f%%, Купальская ночь %.3f%%"),
        NightCount * Pct, DuskCount * Pct, WindyCount * Pct, BlizzardCount * Pct, LateSummerCount * Pct, KupalaCount * Pct));

    int32 Unreachable = 0;
    for (int32 d = 0; d < Defs.Num(); ++d)
    {
        const float Share = OpenCounts[d] * Pct;
        AddInfo(FString::Printf(TEXT("  %-18s %6.2f%% времени года | %s"),
            *Defs[d].EntityID.ToString(), Share, *GateSummary(Defs[d])));

        if (OpenCounts[d] == 0) ++Unreachable;
        TestTrue(FString::Printf(TEXT("[%s] временное окно ненулевое (гейты: %s)"),
            *Defs[d].EntityID.ToString(), *GateSummary(Defs[d])),
            OpenCounts[d] > 0);
    }

    AddInfo(FString::Printf(TEXT("Итого карточек %d, недостижимых по времени %d"), Defs.Num(), Unreachable));

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
