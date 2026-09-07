// BiomeDefaultsDocumentationTest.cpp
//
// Стережёт связь «документация -> игра» для дефолтов биомов.
//
// Предыстория (2026-09-07). Пока автотесты молча гонялись на НУЛЕВЫХ
// дефолтах биомов, настоящих чисел не видел никто, и `DT_BiomeDefaults`
// незаметно уехал от компендиума в 35 значениях: у Болота в строке СУШИ
// стояли числа его же водного состояния (Distortion 0.75, Stability и
// Purity 0.25), Тайга была грязнее документированного (Purity 0.70 против
// 0.80), Тундра — искажённее (0.35 против 0.30). Расхождение имело прямые
// последствия в игре: Болото стояло РОВНО на пороге призыва Болотного царя
// (0.750 против порога 0.75), а Ржавые духи срабатывали на нетронутом
// Болоте (Stability 0.250 против их порога «ниже 0.3»).
//
// Направление сверки задано пользователем прямо: правда — документация,
// а не ассет и не json-экспорт (`CSV_tabs/DT_BiomeDefaults.json`, который,
// к слову, компендиуму как раз соответствовал точно). Ассет приведён к
// карточкам коммандлетом `-run=BiomeDefaultsSync`, а этот тест не даёт им
// разойтись снова — молча, как в прошлый раз, уже не получится.
//
// Обе стороны читают ОДНУ таблицу чисел (`Commandlets/DocumentedBiomeValues.h`):
// две копии однажды разошлись бы, и тест начал бы стеречь не то, что
// записывает коммандлет.

#include "Commandlets/DocumentedBiomeValues.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Types/BiomeRow.h"
#include "Core/World/GridWorldManager.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeDefaults_AssetMatchesTheCompendium,
    "Herbalist.BiomeDefaults.AssetMatchesTheCompendium",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeDefaults_AssetMatchesTheCompendium::RunTest(const FString& Parameters)
{
    const TArray<FDocumentedBiome>& Documented = HerbalistDocumentedBiomes::Get();
    if (!TestEqual(TEXT("Документированы все 8 биомов"), Documented.Num(), FBiomeDefaults::GetAllBiomeTypes().Num()))
    {
        return false;
    }

    for (const FDocumentedBiome& Doc : Documented)
    {
        const FString BiomeName = FBiomeDefaults::BiomeTypeToName(Doc.Biome).ToString();

        // Читаем СЫРУЮ строку, не GetDefaultState: та нормализует Direction
        // (NormalizeSum) и клампит Meta, то есть сверять с карточкой её
        // вывод нельзя — карточка задаёт значения ДО нормализации.
        const FBiomeRow* Row = FBiomeDefaults::GetBiomeRow(Doc.Biome);
        if (!TestNotNull(*FString::Printf(TEXT("[%s] строка есть в DT_BiomeDefaults"), *BiomeName), Row))
        {
            continue;
        }

        struct FCheck { const TCHAR* Field; float Asset; float Doc; };
        const FCheck Checks[] = {
            { TEXT("Direction.Body"),        Row->Direction.Body,        Doc.Body },
            { TEXT("Direction.Mind"),        Row->Direction.Mind,        Doc.Mind },
            { TEXT("Direction.Spirit"),      Row->Direction.Spirit,      Doc.Spirit },
            { TEXT("Direction.Nature"),      Row->Direction.Nature,      Doc.Nature },
            { TEXT("Magnitude"),             Row->Magnitude,             Doc.Magnitude },
            { TEXT("Meta.Distortion"),       Row->Meta.Distortion,       Doc.Distortion },
            { TEXT("Meta.Stability"),        Row->Meta.Stability,        Doc.Stability },
            { TEXT("Meta.Purity"),           Row->Meta.Purity,           Doc.Purity },
            { TEXT("Meta.Potency"),          Row->Meta.Potency,          Doc.Potency },
            { TEXT("Meta.Resonance"),        Row->Meta.Resonance,        Doc.Resonance },
            { TEXT("Meta.Corruption"),       Row->Meta.Corruption,       Doc.Corruption },
            { TEXT("Environment.Toxicity"),  Row->Environment.Toxicity,  Doc.Toxicity },
            { TEXT("Environment.Fertility"), Row->Environment.Fertility, Doc.Fertility },
            { TEXT("Environment.Moisture"),  Row->Environment.Moisture,  Doc.Moisture },
            // Выведенное компендиумом значение (extract_biomes.py), не прямое
            // из фронтматтера -- см. довод в DocumentedBiomeValues.h. Стоит
            // здесь потому, что в ассете оно полгода было нейтральной
            // единицей у всех восьми биомов, то есть механика зарастания
            // молча не работала вовсе.
            { TEXT("StressRecoveryMultiplier"), Row->StressRecoveryMultiplier, Doc.StressRecoveryMultiplier },
        };

        TestTrue(FString::Printf(TEXT("[%s] DisplayName ассет '%s' == карточка '%s'"),
            *BiomeName, *Row->DisplayName.ToString(), Doc.DisplayName),
            Row->DisplayName.ToString().Equals(Doc.DisplayName, ESearchCase::CaseSensitive));

        for (const FCheck& C : Checks)
        {
            TestTrue(FString::Printf(TEXT("[%s] %s: ассет %.3f == карточка %.3f (%s)"),
                *BiomeName, C.Field, C.Asset, C.Doc, Doc.CardPath),
                FMath::IsNearlyEqual(C.Asset, C.Doc, 0.001f));
        }
    }

    return true;
}

// Документированное значение доехало не только до ДАННЫХ, но и до
// ПОВЕДЕНИЯ. Тест выше сверяет таблицу с карточками; этот проверяет, что
// из таблицы величину кто-то читает и она различает биомы.
//
// Нужен именно потому, что сверка данных сама по себе этого не ловит.
// `StressRecoveryMultiplier` полгода стоял в ассете нейтральной единицей у
// всех восьми биомов: механика «место держит след дольше или меньше» была
// формально реализована (`RegenerateCellParameters` читает поле и делит на
// него скорость зарастания), но фактически выключена — все биомы зарастали
// одинаково. Если завтра поле снова обнулят или перестанут читать, сверка
// данных промолчит, а этот тест упадёт.
//
// Болото (1.717) против Смешанного леса (0.588) — крайние значения шкалы,
// разница почти втрое. Сезонный множитель у обеих клеток общий (часы одни),
// поэтому в сравнении он сокращается.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistBiomeDefaults_StressRecoverySpeedDiffersByBiome,
    "Herbalist.BiomeDefaults.StressRecoverySpeedDiffersByBiome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistBiomeDefaults_StressRecoverySpeedDiffersByBiome::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    FGridCell* SlowCell = Manager->GetCell(2, 2);   // Болото -- держит след дольше всех
    FGridCell* FastCell = Manager->GetCell(4, 4);   // Смешанный лес -- зарастает быстрее всех
    if (!TestNotNull(TEXT("Cells exist"), SlowCell) || !FastCell) { Manager->Destroy(); return false; }

    SlowCell->Biome = EBiomeType::Bog;
    FastCell->Biome = EBiomeType::MixedForest;
    SlowCell->HarvestStress = 1.0f;
    FastCell->HarvestStress = 1.0f;

    // Заметный отрезок: зарастание идёт за игровые СУТКИ, шаг в секунду дал
    // бы разницу в шестом знаке.
    for (int32 i = 0; i < 200; ++i)
    {
        Manager->RegenerateCellParameters(60.0f);
    }

    const float SlowLeft = SlowCell->HarvestStress;
    const float FastLeft = FastCell->HarvestStress;

    TestTrue(FString::Printf(TEXT("Оба биома реально зарастают (Болото %.4f, Смешанный лес %.4f -- оба ниже старта 1.0)"),
        SlowLeft, FastLeft),
        SlowLeft < 1.0f && FastLeft < 1.0f);

    TestTrue(FString::Printf(TEXT("Болото (множитель 1.717) держит след ДОЛЬШЕ Смешанного леса (0.588): осталось %.4f против %.4f"),
        SlowLeft, FastLeft),
        SlowLeft > FastLeft + KINDA_SMALL_NUMBER);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
