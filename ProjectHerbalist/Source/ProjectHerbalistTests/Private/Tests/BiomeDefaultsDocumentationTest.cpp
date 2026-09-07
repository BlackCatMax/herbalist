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
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

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
        };

        for (const FCheck& C : Checks)
        {
            TestTrue(FString::Printf(TEXT("[%s] %s: ассет %.3f == карточка %.3f (%s)"),
                *BiomeName, C.Field, C.Asset, C.Doc, Doc.CardPath),
                FMath::IsNearlyEqual(C.Asset, C.Doc, 0.001f));
        }
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
