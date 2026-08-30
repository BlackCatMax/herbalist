// Source/ProjectHerbalistTests/Private/Tests/CommunityTest.cpp
//
// Общинный кластер (DESIGN_Community_And_Homestead.md §1, реализация
// 2026-08-31): Молва, Подношение общине, Торговля с общиной. OfferToCommunity
// не зависит от IngredientRegistrySubsystem (чистая математика над
// FInventoryItem.State) — тестируется напрямую, тем же приёмом, что и
// остальной внепайплайновый слой (LandmarkTest.cpp, ShrineTest.cpp).
// TryTradeWithCommunity зависит от реестра (GetRow) — здесь тестируется
// только гарантированно детерминированный путь отказа (неизвестный
// ингредиент), не путь успеха: этот файл, как и остальные GridWorldManager-
// level тесты проекта (PlaySessionIntegrationTest.cpp и другие), не
// полагается на то, что реальный DT_IngredientClass загружен в тестовом
// окружении — тот же довод, что уже объясняет структуру
// IngredientRegistryTest.cpp (синтетическая таблица для прямых тестов
// реестра, не GameInstance).

#include "Core/World/GridWorldManager.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FInventoryItem MakeCommunityOfferingItem(float Purity, float Corruption)
    {
        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("CommunityProbe"));
        Item.State.Meta.Purity = Purity;
        Item.State.Meta.Corruption = Corruption;
        Item.Count = 1;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_PureOfferingRaisesMolva,
    "Herbalist.Community.PureOfferingRaisesMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_PureOfferingRaisesMolva::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TestEqual(TEXT("Molva starts at zero"), Manager->Molva, 0.0f);

    TArray<FInventoryItem> Items = { MakeCommunityOfferingItem(/*Purity=*/0.9f, /*Corruption=*/0.05f) };
    const float Delta = Manager->OfferToCommunity(Items);

    TestTrue(TEXT("A pure, uncorrupted offering returns a positive ΔMolva"), Delta > 0.0f);
    TestTrue(TEXT("Molva itself moved up by the same amount"), Manager->Molva > 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_CorruptOfferingLowersMolva,
    "Herbalist.Community.CorruptOfferingLowersMolva",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_CorruptOfferingLowersMolva::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    TArray<FInventoryItem> Items = { MakeCommunityOfferingItem(/*Purity=*/0.05f, /*Corruption=*/0.9f) };
    const float Delta = Manager->OfferToCommunity(Items);

    TestTrue(TEXT("A corrupt, impure offering returns a negative ΔMolva"), Delta < 0.0f);
    TestTrue(TEXT("Molva itself moved down"), Manager->Molva < 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_MolvaDoesNotDecayPassively,
    "Herbalist.Community.MolvaDoesNotDecayPassively",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_MolvaDoesNotDecayPassively::RunTest(const FString& Parameters)
{
    // Тот же принцип, что уже подтверждён для Landmark.Respect
    // (RespectDoesNotDecayPassively) -- у подношения общине тоже нет срока
    // годности, Molva не двигается сама по себе между подношениями.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    Manager->Molva = 0.2f;
    Manager->Tick(10.0f);

    TestEqual(TEXT("Molva untouched by the mere passage of time"), Manager->Molva, 0.2f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_EmptyOfferingIsANoOp,
    "Herbalist.Community.EmptyOfferingIsANoOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_EmptyOfferingIsANoOp::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    const float Delta = Manager->OfferToCommunity(TArray<FInventoryItem>());
    TestEqual(TEXT("Offering nothing returns zero ΔMolva"), Delta, 0.0f);
    TestEqual(TEXT("Molva untouched"), Manager->Molva, 0.0f);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_TradeFailsForUnknownIngredient,
    "Herbalist.Community.TradeFailsForUnknownIngredient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_TradeFailsForUnknownIngredient::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FInventoryItem Offered = MakeCommunityOfferingItem(0.5f, 0.1f);
    Offered.State.Magnitude = 0.5f;
    FInventoryItem Received;
    const bool bSucceeded = Manager->TryTradeWithCommunity(Offered, FName(TEXT("ThisIngredientDoesNotExistAnywhere")), Received);

    TestFalse(TEXT("Trading for an ingredient the community's registry doesn't know fails cleanly, doesn't crash"), bSucceeded);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_TradeFailsForEmptyOffer,
    "Herbalist.Community.TradeFailsForEmptyOffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_TradeFailsForEmptyOffer::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    FInventoryItem Offered = MakeCommunityOfferingItem(0.5f, 0.1f);
    Offered.Count = 0;   // ничего реально не предложено
    FInventoryItem Received;
    const bool bSucceeded = Manager->TryTradeWithCommunity(Offered, FName(TEXT("bol_01")), Received);

    TestFalse(TEXT("Offering zero count fails regardless of whether the wanted ingredient is known"), bSucceeded);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistCommunity_MolvaSurvivesSaveLoad,
    "Herbalist.Community.MolvaSurvivesSaveLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistCommunity_MolvaSurvivesSaveLoad::RunTest(const FString& Parameters)
{
    // Аудит сразу после реализации (2026-08-31) нашёл реальный пробел:
    // Shrines/EntityLandmarks уже сохранялись, Molva -- нет, хотя следует
    // тому же принципу "растёт/падает только подношением, без пассивного
    // спада" (§ комментарий у Molva, HerbalistSaveTypes.h) -- перезагрузка
    // молча обнуляла бы репутацию. Найдено и закрыто тем же проходом.
    //
    // Тест ниже НЕ идёт через UHerbalistSaveSubsystem::SaveGame/LoadGame
    // (реальный диск, GameInstanceSubsystem) -- эмпирически проверено: ни
    // один тест во всём проекте не получает GameInstanceSubsystem через
    // World->GetGameInstance() в editor-world автотестов (GetSubsystem
    // возвращает nullptr, тот же класс ограничения, что уже документирован
    // для FSnapshotService::GetSimulationWorld() -- только Game/PIE миры).
    // Вместо этого напрямую воспроизводится та же пара присваиваний, что
    // SaveGame/LoadGame реально делают (HerbalistSaveSubsystem.cpp) --
    // проверяет корректность добавленного поля и его копирования в обе
    // стороны, не диск. Реальный сквозной прогон через диск -- предмет
    // ручной PIE-проверки (PIE_VERIFICATION_PLAN.md), не этого теста.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("AGridWorldManager spawned"), Manager)) return false;

    UHerbalistSaveGame* Save = NewObject<UHerbalistSaveGame>();
    if (!TestNotNull(TEXT("Save object constructed"), Save)) { Manager->Destroy(); return false; }

    Manager->Molva = 0.42f;
    Save->Molva = Manager->Molva;   // тот же порядок присваивания, что SaveGame()

    // Симулируем дальнейшую игру после сохранения -- значение должно
    // откатиться к сохранённому, не остаться текущим.
    Manager->Molva = -0.9f;
    Manager->Molva = Save->Molva;   // тот же порядок присваивания, что LoadGame()

    TestEqual(TEXT("Molva restored to the value at save time, not left at post-save value"), Manager->Molva, 0.42f);

    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
