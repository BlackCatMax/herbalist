// BiomeTypes.cpp
#include "BiomeTypes.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Core/Types/BiomeRow.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"

static UDataTable* BiomeDataTable = nullptr;

void FBiomeDefaults::SetBiomeTable(UDataTable* InTable)
{
    // BiomeDataTable — сырой static-указатель вне UPROPERTY/UObject, GC его не
    // видит вообще. LoadObject() в ProjectHerbalistGameModeBase::BeginPlay
    // отдаёт объект без единой закреплённой ссылки на него — ближайший проход
    // сборщика мусора (по умолчанию ~60 сек) собирал таблицу, а следующий вызов
    // GetBiomeRow() падал в EXCEPTION_ACCESS_VIOLATION (обнаружено при первом
    // headless-прогоне в режиме -game дольше минуты — прежде тестировалось
    // только короткими PIE-сессиями/автотестами, где GC не успевал сработать).
    if (BiomeDataTable && BiomeDataTable != InTable)
    {
        BiomeDataTable->RemoveFromRoot();
    }
    BiomeDataTable = InTable;
    if (BiomeDataTable)
    {
        BiomeDataTable->AddToRoot();
    }
}

// Ленивый фолбэк на случай, когда SetBiomeTable никто не позвал.
// Push-инициализация живёт ровно в одном месте — AProjectHerbalistGameModeBase
// ::BeginPlay, — а его нет ни в headless-автотесте (editor-мир без GameMode),
// ни в коммандлетах. До 2026-09-07 это молча означало «все дефолты биомов
// нулевые»: GetDefaultState отдавал пустой FRealState, и весь мир считался от
// Distortion=0/Purity=0 вместо честных 0.25-0.70 из DT_BiomeDefaults.
// Тесты от этого НЕ падали — они просто проверяли математику на нулевых
// константах, что заметно слабее (найдено ревизией математики, MATH_REFERENCE
// §8). Лечим причину, а не тестовый мир: тот же ленивый function-local static,
// что уже несёт GetAmbientEntityDefinitions (Core/Entities/AmbientEntityTypes.h)
// по тому же самому доводу. Push-путь GameMode остаётся и имеет приоритет —
// он просто перестал быть ЕДИНСТВЕННЫМ.
static void EnsureBiomeTableLoaded()
{
    if (BiomeDataTable) return;

    // LoadObject не потокобезопасен. GetBiomeRow зовут в том числе из расчётной
    // части мира, поэтому не check() — вне игрового потока просто сохраняем
    // прежнее поведение (nullptr), а не роняем процесс.
    if (!IsInGameThread()) return;

    // Одна попытка на процесс: в cooked-сборке без этого ассета повторный
    // LoadObject на каждый вызов GetBiomeRow стоил бы дороже самого расчёта.
    static bool bTriedLazyLoad = false;
    if (bTriedLazyLoad) return;
    bTriedLazyLoad = true;

    if (UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_BiomeDefaults")))
    {
        // Через SetBiomeTable, не прямым присваиванием — там же живёт AddToRoot,
        // без которого сырой static-указатель собирает GC примерно через минуту
        // (та же авария, что описана в самом SetBiomeTable).
        FBiomeDefaults::SetBiomeTable(Table);
    }
}

const FBiomeRow* FBiomeDefaults::GetBiomeRow(EBiomeType Biome)
{
    EnsureBiomeTableLoaded();
    if (!BiomeDataTable) return nullptr;
    FName RowName = BiomeTypeToName(Biome);
    return BiomeDataTable->FindRow<FBiomeRow>(RowName, TEXT("GetBiomeRow"));
}

static const TMap<EBiomeType, FName> BiomeToNameMap = {
    { EBiomeType::Tundra,          TEXT("Tundra") },
    { EBiomeType::Taiga,           TEXT("Taiga") },
    { EBiomeType::MixedForest,     TEXT("MixedForest") },
    { EBiomeType::BroadleafForest, TEXT("BroadleafForest") },
    { EBiomeType::ForestSteppe,    TEXT("ForestSteppe") },
    { EBiomeType::Steppe,          TEXT("Steppe") },
    { EBiomeType::Floodplain,      TEXT("Floodplain") },
    { EBiomeType::Bog,             TEXT("Bog") }
};

FName FBiomeDefaults::BiomeTypeToName(EBiomeType Biome)
{
    if (const FName* Name = BiomeToNameMap.Find(Biome))
        return *Name;
    return TEXT("MixedForest");
}

TArray<EBiomeType> FBiomeDefaults::GetAllBiomeTypes()
{
    TArray<EBiomeType> Types;
    BiomeToNameMap.GenerateKeyArray(Types);
    return Types;
}

FRealState FBiomeDefaults::GetDefaultState(EBiomeType Biome)
{
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FRealState();

    FRealState State;
    State.Direction = Row->Direction;
    State.Magnitude = Row->Magnitude;
    State.Meta = Row->Meta;
    State.Direction.NormalizeSum();
    State.Magnitude = FMath::Clamp(State.Magnitude, 0.0f, 1.0f);
    State.Meta.Distortion = FMath::Clamp(State.Meta.Distortion, 0.0f, 1.0f);
    State.Meta.Stability = FMath::Clamp(State.Meta.Stability, 0.0f, 1.0f);
    State.Meta.Purity = FMath::Clamp(State.Meta.Purity, 0.0f, 1.0f);
    State.Meta.Potency = FMath::Clamp(State.Meta.Potency, 0.0f, 1.0f);
    State.Meta.Resonance = FMath::Clamp(State.Meta.Resonance, 0.0f, 1.0f);
    State.Meta.Corruption = FMath::Clamp(State.Meta.Corruption, 0.0f, 1.0f);
    return State;
}

FEnvironment FBiomeDefaults::GetDefaultEnvironment(EBiomeType Biome)
{
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FEnvironment();
    return Row->Environment;
}

FRealState FBiomeDefaults::GetDefaultWaterState(EBiomeType Biome)
{
    const FBiomeRow* Row = GetBiomeRow(Biome);
    if (!Row) return FRealState();
    return Row->DefaultWaterState;
}
