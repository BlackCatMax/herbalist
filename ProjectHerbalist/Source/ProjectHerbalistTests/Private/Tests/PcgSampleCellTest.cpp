// Source/ProjectHerbalistTests/Private/Tests/PcgSampleCellTest.cpp
//
// Узел «Sample Herbalist Cell» (2026-09-08) -- переносит состояние клетки на
// чужие точки и выдаёт им MeshKey для PCGMeshSelectorByAttribute.
//
// То же ограничение, что у PcgGridFeedbackTest.cpp: прогнать сам PCG-элемент
// в headless-тесте нельзя, ему нужен исполняющий контекст графа (FPCGContext
// с ExecutionSource), которого в голом editor-мире нет. Поэтому проверяется
// то, что ломается молча и дорого:
//
//   1. У узла ЕСТЬ входной пин. В этом весь смысл его существования: у
//      соседнего «Get Herbalist Grid» входных пинов нет вовсе, он источник и
//      пометить чужие точки не может. Если этот пин однажды исчезнет,
//      компиляция не заметит, а собранный граф просто развалится.
//   2. Имена атрибутов -- контракт с графом, который пользователь собирает
//      руками по этим именам.
//   3. Имена, общие с «Get Herbalist Grid», совпадают ПОБУКВЕННО. Два узла
//      кладут в точку одни и те же оси; разъедься они -- и Attribute Filter,
//      настроенный на выход одного, тихо перестал бы находить их на выходе
//      другого.

#include "Core/PCG/PCGHerbalistSampleCell.h"
#include "Core/PCG/PCGHerbalistGridData.h"
#include "PCGPin.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Commandlets/PcgGrassSeasonSetupCommandlet.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGEdge.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Своё имя, не SourceOf/LoadSource: unity-сборка склеивает все Tests/*.cpp
    // в одну единицу трансляции, и одинаковые тела в анонимных namespace дают
    // MSVC C2084 (наступали на это 2026-08-29, см. шапку TestWorldHelpers.h).
    bool LoadPcgSampleCellSource(const TCHAR* RelativePath, FString& OutSource)
    {
        const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
        return FFileHelper::LoadFileToString(OutSource, *FullPath);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_HasAnInputPinUnlikeTheSourceNode,
    "Herbalist.PcgSampleCell.HasAnInputPinUnlikeTheSourceNode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_HasAnInputPinUnlikeTheSourceNode::RunTest(const FString& Parameters)
{
    UPCGHerbalistSampleCellSettings* Sampler = NewObject<UPCGHerbalistSampleCellSettings>();
    if (!TestNotNull(TEXT("Sampler settings created"), Sampler)) return false;

    // Считать пины скопом нельзя: AllInputPinProperties() возвращает ещё и
    // авто-пины переопределения параметров (у «Get Herbalist Grid» их пять,
    // на чём этот тест сначала и упал). Значение имеет ровно один пин --
    // тот, куда приходят ДАННЫЕ, с меткой DefaultInputLabel.
    auto HasDataInputPin = [](const TArray<FPCGPinProperties>& Pins)
    {
        for (const FPCGPinProperties& Pin : Pins)
        {
            if (Pin.Label == PCGPinConstants::DefaultInputLabel) return true;
        }
        return false;
    };

    // Ровно то различие, ради которого узел написан. Проверяется не «функция
    // вернула массив», а следствие: у сэмплера вход для данных есть, у
    // источника его нет.
    TestTrue(TEXT("Sample Herbalist Cell declares a data input pin"),
        HasDataInputPin(Sampler->AllInputPinProperties()));
    TestTrue(TEXT("Sample Herbalist Cell declares an output pin"),
        Sampler->AllOutputPinProperties().Num() > 0);

    UPCGHerbalistGridSettings* Source = NewObject<UPCGHerbalistGridSettings>();
    if (!TestNotNull(TEXT("Source node settings created"), Source)) return false;

    TestFalse(TEXT("Get Herbalist Grid still has no data input pin -- it is a source, and that is why the sampler exists"),
        HasDataInputPin(Source->AllInputPinProperties()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_DefaultsAreTheDocumentedOnes,
    "Herbalist.PcgSampleCell.DefaultsAreTheDocumentedOnes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_DefaultsAreTheDocumentedOnes::RunTest(const FString& Parameters)
{
    UPCGHerbalistSampleCellSettings* Settings = NewObject<UPCGHerbalistSampleCellSettings>();
    if (!TestNotNull(TEXT("Node settings object created"), Settings)) return false;

    // Эти две строки пользователь вписывает в записи PCGMeshSelectorByAttribute
    // руками -- сопоставление идёт по строке, и смена дефолта здесь молча
    // рассогласовала бы уже настроенный спавнер.
    TestEqual(TEXT("Healthy key default"), Settings->HealthyMeshKey, FString(TEXT("Healthy")));
    TestEqual(TEXT("Degrading key default"), Settings->DegradingMeshKey, FString(TEXT("Degrading")));

    // Довод за этот дефолт -- в комментарии у самого свойства: тихо удалять
    // растительность за краем сетки значит принять решение с далеко идущими
    // последствиями для вида мира, и принимать его умолчанием неправильно.
    TestFalse(TEXT("Points outside the grid are kept by default -- dropping them is opt-in"),
        Settings->bDropPointsOutsideGrid);

    TestTrue(TEXT("Node is a PCG settings type the graph can instantiate"),
        Settings->IsA<UPCGSettings>());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_AttributeNamesAreAStableContract,
    "Herbalist.PcgSampleCell.AttributeNamesAreAStableContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_AttributeNamesAreAStableContract::RunTest(const FString& Parameters)
{
    FString Source;
    if (!TestTrue(TEXT("Sampler source file is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistSampleCell.cpp"), Source)))
    {
        return false;
    }

    const TArray<FString> Expected = {
        TEXT("Distortion"),
        TEXT("Corruption"),
        TEXT("HarvestStress"),
        TEXT("Biome"),
        TEXT("bDegrading"),
        TEXT("MeshKey"),
        TEXT("SeasonKey"),
        TEXT("SeasonMeshKey"),
    };

    for (const FString& Name : Expected)
    {
        const FString Needle = FString::Printf(TEXT("TEXT(\"%s\")"), *Name);
        TestTrue(FString::Printf(TEXT("Attribute '%s' is still declared by the sampler"), *Name),
            Source.Contains(Needle));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_SharedAttributeNamesMatchTheSourceNode,
    "Herbalist.PcgSampleCell.SharedAttributeNamesMatchTheSourceNode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_SharedAttributeNamesMatchTheSourceNode::RunTest(const FString& Parameters)
{
    // Настоящая проверка на разъезд двух узлов. Оба кладут в точку одни и те
    // же оси мира; если один переименует Distortion, а другой нет, ни
    // компиляция, ни отдельные контрактные тесты этого не заметят -- каждый
    // останется верен себе. Ломается при этом граф пользователя.
    FString SamplerSource;
    FString GridSource;

    if (!TestTrue(TEXT("Sampler source is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistSampleCell.cpp"), SamplerSource)))
    {
        return false;
    }
    if (!TestTrue(TEXT("Grid source is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistGridData.cpp"), GridSource)))
    {
        return false;
    }

    const TArray<FString> Shared = {
        TEXT("Distortion"),
        TEXT("Corruption"),
        TEXT("HarvestStress"),
        TEXT("Biome"),
    };

    for (const FString& Name : Shared)
    {
        const FString Needle = FString::Printf(TEXT("TEXT(\"%s\")"), *Name);
        TestTrue(FString::Printf(TEXT("Both nodes still spell '%s' the same way"), *Name),
            SamplerSource.Contains(Needle) && GridSource.Contains(Needle));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_SeasonKeysAndThinningAreNested,
    "Herbalist.PcgSampleCell.SeasonKeysAndThinningAreNested",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_SeasonKeysAndThinningAreNested::RunTest(const FString& Parameters)
{
    // Ключи -- контракт с записями PCGMeshSelectorByAttribute (сопоставление по строке).
    TestEqual(TEXT("SeasonKey зимы"), UPCGHerbalistSampleCellSettings::SeasonKeyFor(ESeason::Winter), FString(TEXT("Winter")));
    TestEqual(TEXT("SeasonKey осени"), UPCGHerbalistSampleCellSettings::SeasonKeyFor(ESeason::Autumn), FString(TEXT("Autumn")));
    TestEqual(TEXT("SeasonMeshKey = MeshKey_Season"), UPCGHerbalistSampleCellSettings::SeasonMeshKeyFor(TEXT("Healthy"), ESeason::Winter), FString(TEXT("Healthy_Winter")));

    const UPCGHerbalistSampleCellSettings* Defaults = GetDefault<UPCGHerbalistSampleCellSettings>();
    TestEqual(TEXT("Весной трава вся"), Defaults->SeasonDensityFor(ESeason::Spring), 1.0f);
    TestEqual(TEXT("Летом трава вся"), Defaults->SeasonDensityFor(ESeason::Summer), 1.0f);
    TestTrue(TEXT("Зимой травы меньше, чем осенью"), Defaults->SeasonDensityFor(ESeason::Winter) < Defaults->SeasonDensityFor(ESeason::Autumn));

    // Доля оставшихся точек близка к доле сезона, и меньшая доля -- подмножество
    // большей: при смене сезона трава не перетасовывается.
    const int32 Points = 20000;
    int32 KeptAutumn = 0, KeptWinter = 0, WinterNotInAutumn = 0;
    for (int32 Seed = 0; Seed < Points; ++Seed)
    {
        const int32 PointSeed = static_cast<int32>(HashCombine(static_cast<uint32>(Seed), 977u));
        const bool bAutumn = UPCGHerbalistSampleCellSettings::KeepPointForSeason(PointSeed, 0.8f);
        const bool bWinter = UPCGHerbalistSampleCellSettings::KeepPointForSeason(PointSeed, 0.4f);
        KeptAutumn += bAutumn ? 1 : 0;
        KeptWinter += bWinter ? 1 : 0;
        WinterNotInAutumn += (bWinter && !bAutumn) ? 1 : 0;
    }
    TestTrue(*FString::Printf(TEXT("Осенью остаётся ~80%% (%d из %d)"), KeptAutumn, Points), FMath::Abs(KeptAutumn / static_cast<float>(Points) - 0.8f) < 0.02f);
    TestTrue(*FString::Printf(TEXT("Зимой остаётся ~40%% (%d из %d)"), KeptWinter, Points), FMath::Abs(KeptWinter / static_cast<float>(Points) - 0.4f) < 0.02f);
    TestEqual(TEXT("Зимний набор -- подмножество осеннего"), WinterNotInAutumn, 0);
    TestTrue(TEXT("Доля 1 -- все точки"), UPCGHerbalistSampleCellSettings::KeepPointForSeason(12345, 1.0f));
    TestFalse(TEXT("Доля 0 -- ни одной"), UPCGHerbalistSampleCellSettings::KeepPointForSeason(12345, 0.0f));

    // Узел не кэшируется: результат зависит от живого мира (сезон, клетки).
    FString Source;
    if (TestTrue(TEXT("Sampler header is readable"), LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistSampleCell.h"), Source)))
    {
        TestTrue(TEXT("IsCacheable возвращает false"), Source.Contains(TEXT("IsCacheable(const UPCGSettings* InSettings) const override { return false; }")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_GrassGraphSamplesCellBeforeNoise,
    "Herbalist.PcgSampleCell.GrassGraphSamplesCellBeforeNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_GrassGraphSamplesCellBeforeNoise::RunTest(const FString& Parameters)
{
    auto FindSampler = [](UPCGGraph* Graph) -> UPCGNode*
    {
        for (UPCGNode* Node : Graph->GetNodes())
        {
            if (Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGHerbalistSampleCellSettings>()) return Node;
        }
        return nullptr;
    };
    auto SamplerWiring = [](const UPCGNode* Sampler, FString& OutFrom, FString& OutTo)
    {
        for (const UPCGPin* Pin : Sampler->GetInputPins())
            for (const UPCGEdge* Edge : Pin->Edges)
                if (Edge && Edge->InputPin && Edge->InputPin->Node && Edge->InputPin->Node->GetSettings())
                    OutFrom = Edge->InputPin->Node->GetSettings()->GetClass()->GetName();
        for (const UPCGPin* Pin : Sampler->GetOutputPins())
            for (const UPCGEdge* Edge : Pin->Edges)
                if (Edge && Edge->OutputPin && Edge->OutputPin->Node && Edge->OutputPin->Node->GetSettings())
                    OutTo = Edge->OutputPin->Node->GetSettings()->GetClass()->GetName();
    };

    UPCGGraph* Asset = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/PCG/PCG_Grass.PCG_Grass"));
    if (!TestNotNull(TEXT("PCG_Grass загружается"), Asset)) return false;

    // Ассет: узел вставлен -run=PcgGrassSeasonSetup между World Raycast и Attribute Noise.
    const UPCGNode* InAsset = FindSampler(Asset);
    if (TestNotNull(TEXT("Sample Herbalist Cell стоит в PCG_Grass"), InAsset))
    {
        FString From, To;
        SamplerWiring(InAsset, From, To);
        TestEqual(TEXT("Вход -- из World Raycast"), From, FString(TEXT("PCGWorldRaycastElementSettings")));
        TestEqual(TEXT("Выход -- в Attribute Noise"), To, FString(TEXT("PCGAttributeNoiseSettings")));
    }

    // Повторный запуск на копии -- ничего не добавляет.
    UPCGGraph* Copy = DuplicateObject<UPCGGraph>(Asset, GetTransientPackage());
    TestEqual(TEXT("Узел уже есть -- вставка не повторяется"), UPcgGrassSeasonSetupCommandlet::InsertSeasonSampler(Copy), 0);

    // Сама вставка: на копии убрать узел, вернуть прямую связь и вставить заново.
    UPCGNode* CopySampler = FindSampler(Copy);
    UPCGNode* Raycast = nullptr;
    UPCGNode* Noise = nullptr;
    FName RaycastLabel, NoiseLabel;
    if (CopySampler)
    {
        for (const UPCGPin* Pin : CopySampler->GetInputPins())
            for (const UPCGEdge* Edge : Pin->Edges)
                if (Edge && Edge->InputPin) { Raycast = Edge->InputPin->Node; RaycastLabel = Edge->InputPin->Properties.Label; }
        for (const UPCGPin* Pin : CopySampler->GetOutputPins())
            for (const UPCGEdge* Edge : Pin->Edges)
                if (Edge && Edge->OutputPin) { Noise = Edge->OutputPin->Node; NoiseLabel = Edge->OutputPin->Properties.Label; }
    }
    if (TestTrue(TEXT("На копии найдены соседи узла"), Raycast && Noise))
    {
        Copy->RemoveNode(CopySampler);
        Copy->AddLabeledEdge(Raycast, RaycastLabel, Noise, NoiseLabel);
        TestNull(TEXT("Узел убран с копии"), FindSampler(Copy));
        TestEqual(TEXT("Вставка на графе без узла -- 1"), UPcgGrassSeasonSetupCommandlet::InsertSeasonSampler(Copy), 1);
        const UPCGNode* Inserted = FindSampler(Copy);
        if (TestNotNull(TEXT("Узел вставлен"), Inserted))
        {
            FString From, To;
            SamplerWiring(Inserted, From, To);
            TestEqual(TEXT("Вставленный: вход из World Raycast"), From, FString(TEXT("PCGWorldRaycastElementSettings")));
            TestEqual(TEXT("Вставленный: выход в Attribute Noise"), To, FString(TEXT("PCGAttributeNoiseSettings")));
        }
    }
    return true;
}

#endif // WITH_AUTOMATION_TESTS
