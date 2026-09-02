// Core/World/GridWorldManagerProphetFeathers.cpp
//
// Эффекты перьев вещих птиц (02_GDD/16_Entity_Manifestation.md §16.4,
// подраздел "Особый случай — вещие птицы", 2026-09-02). Четыре эндгейм-
// трофея, каждый строго переиспользует существующий механизм (§16.4:
// "каждый — усиленная/разовая версия чего-то уже существующего, не новая
// система"):
//   - Перо Гамаюна    — гарантия уже задуманного пророческого Зеркальца
//   - Перо Алконоста  — Шапка-невидимка, масштабированная на весь биом
//   - Перо Сирина     — честное чтение уже существующего Malign-триггера
//   - Перо Жар-птицы  — единственный ПОСТОЯННЫЙ эффект (bEternallyPure)
//
// Не часть ArtifactTypes.h/GridWorldManagerArtifacts.cpp: Алконост/Сирин/
// жар-птица не входят в таблицу §21.3 (только Гамаюн, через Зеркальце) —
// у них нет "базового артефакта" в этом смысле, только перо напрямую.
// Тот же паттерн именованных функций на конкретный предмет, что уже
// GridWorldManagerArtifactEffects.cpp применяет к каждому из семи эффектов
// §21.3, не общий registry-цикл — четыре штуки, у каждой содержательно
// разная логика получения/эффекта, обобщать было бы преждевременно.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace
{
    const FName Feather_Gamayun(TEXT("Перо Гамаюна"));
    const FName Feather_Alkonost(TEXT("Перо Алконоста"));
    const FName Feather_Sirin(TEXT("Перо Сирина"));
    const FName Feather_ZharPtitsa(TEXT("Перо Жар-птицы"));
}

bool AGridWorldManager::TryAcquireProphetFeather(FName FeatherID)
{
    if (AcquiredFeathers.Contains(FeatherID)) return false;   // уже добыто

    if (FeatherID == Feather_Gamayun)
    {
        // §16.4, дословно: "требуют уже добытого базового артефакта (Перо
        // Гамаюна бесполезно без Зеркальца) ИЛИ очень редкого мирового
        // события" — единственное из четырёх с этим требованием, три
        // остальных ниже гейтятся вторым условием (сами свои "редкие
        // события", уже посчитанные благим/злым полюсом §16.4 через
        // IsLegendaryManifested — без нового типа триггера).
        static const FName MirrorID(TEXT("Зеркальце"));
        const bool bHasMirror = AcquiredArtifacts.ContainsByPredicate(
            [](const FAcquiredArtifact& A) { return A.ArtifactID == MirrorID; });
        if (!bHasMirror || !IsArtifactWarmed(MirrorID)) return false;
        if (!IsLegendaryManifested(FName(TEXT("Гамаюн")))) return false;
    }
    else if (FeatherID == Feather_Alkonost)
    {
        // "устойчиво чистый биом" — то же самое, что уже проверяет
        // IsLegendaryManifested("Алконост") (Benign-полюс §16.4: низкий
        // MorokField узла ИЛИ высокая Restoration капища рядом).
        if (!IsLegendaryManifested(FName(TEXT("Алконост")))) return false;
    }
    else if (FeatherID == Feather_Sirin)
    {
        // "Malign-спайк" — то же самое, что уже IsLegendaryManifested("Сирин")
        // (Сирин заведён с Pole::Malign в LegendaryEntityTypes.h — "зеркальна
        // благому триггеру").
        if (!IsLegendaryManifested(FName(TEXT("Сирин")))) return false;
    }
    else if (FeatherID == Feather_ZharPtitsa)
    {
        // EntityID в реестре — "жар-птица" (нижний регистр, см.
        // LegendaryEntityTypes.h), не "Жар-птица" — сохраняем как есть,
        // не переименовываю чужой существующий реестр ради косметики.
        if (!IsLegendaryManifested(FName(TEXT("жар-птица")))) return false;
    }
    else
    {
        return false;   // неизвестное перо
    }

    AcquiredFeathers.Add(FeatherID);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Feather] %s acquired"), *FeatherID.ToString());
    return true;
}

bool AGridWorldManager::EatGamayunFeather()
{
    const int32 Index = AcquiredFeathers.IndexOfByKey(Feather_Gamayun);
    if (Index == INDEX_NONE) return false;

    AcquiredFeathers.RemoveAt(Index);
    bGamayunPropheticGuaranteed = true;
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Feather] Перо Гамаюна eaten -- Зеркальце's prophetic reading is now guaranteed"));
    return true;
}

bool AGridWorldManager::UseAlkonostFeatherOnBiome(EBiomeType Biome)
{
    const int32 Index = AcquiredFeathers.IndexOfByKey(Feather_Alkonost);
    if (Index == INDEX_NONE) return false;

    AcquiredFeathers.RemoveAt(Index);

    // "тем же таймером" (прямая формулировка задачи) — переиспользует
    // InvisibilityCapDurationSeconds, не отдельная настройка.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DurationSeconds = Settings ? Settings->InvisibilityCapDurationSeconds : 300.0f;
    AlkonostSuppressedBiome = Biome;
    AlkonostSuppressionExpiryGameSeconds = GameClockSeconds + DurationSeconds;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Feather] Перо Алконоста spent -- biome %d suppressed until %.1f"),
        (int32)Biome, AlkonostSuppressionExpiryGameSeconds);
    return true;
}

bool AGridWorldManager::IsAlkonostSuppressionActiveForBiome(EBiomeType Biome) const
{
    return Biome == AlkonostSuppressedBiome && GameClockSeconds < AlkonostSuppressionExpiryGameSeconds;
}

bool AGridWorldManager::UseSirinFeatherOnCell(const FIntPoint& Cell, FText& OutDisclosure)
{
    OutDisclosure = FText::GetEmpty();

    const int32 Index = AcquiredFeathers.IndexOfByKey(Feather_Sirin);
    if (Index == INDEX_NONE) return false;

    const FGridCell* Target = GetCellConst(Cell.X, Cell.Y);
    if (!Target) return false;

    // "при активном Malign-спайке Легендарного уровня в биоме" — тот же
    // триггер, что уже вызывает опасный/зеркальный полюс §16.4, не новый:
    // ищем любое Malign-определение того же биома, что и целевая клетка,
    // сейчас проявленное.
    bool bMalignSpikeActive = false;
    for (const FLegendaryEntityDefinition& Def : GetLegendaryEntityDefinitions())
    {
        if (Def.Pole == ELegendaryPole::Malign && Def.Biome == Target->Biome && IsLegendaryManifested(Def.EntityID))
        {
            bMalignSpikeActive = true;
            break;
        }
    }
    if (!bMalignSpikeActive) return false;

    AcquiredFeathers.RemoveAt(Index);

    // Честное чтение — та же прямая честность (Cell->State напрямую, не
    // через PerceiveRealState), что уже UseHornOnCell/
    // UseLanternDisclosureOnCell.
    OutDisclosure = FText::FromString(FString::Printf(TEXT(
        "Перо Сирина на миг снимает морок -- клетка (%d,%d): Purity=%.2f, Corruption=%.2f, Distortion=%.2f."),
        Cell.X, Cell.Y, Target->State.Meta.Purity, Target->State.Meta.Corruption, Target->State.Meta.Distortion));

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Feather] Перо Сирина spent at (%d,%d) during an active Malign spike"), Cell.X, Cell.Y);
    return true;
}

bool AGridWorldManager::UseZharPtitsaFeatherOnCell(const FIntPoint& Cell)
{
    const int32 Index = AcquiredFeathers.IndexOfByKey(Feather_ZharPtitsa);
    if (Index == INDEX_NONE) return false;

    FGridCell* Target = GetCell(Cell.X, Cell.Y);
    if (!Target) return false;

    Target->bEternallyPure = true;
    MarkCellDirty(Cell.X, Cell.Y);
    AcquiredFeathers.RemoveAt(Index);

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Feather] Перо Жар-птицы spent -- cell (%d,%d) marked eternally pure"), Cell.X, Cell.Y);
    return true;
}

bool AGridWorldManager::IsCellEternallyPure(const FIntPoint& Cell) const
{
    const FGridCell* Target = GetCellConst(Cell.X, Cell.Y);
    return Target && Target->bEternallyPure;
}
