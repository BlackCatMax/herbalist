// Source/ProjectHerbalistTests/Private/Tests/StorageContainerTest.cpp
//
// Пять типов контейнеров (2026-08-29, "проработка инвентаря и систем
// хранения" -> "больше типов контейнеров"): None/Basket/Sack/Cabinet/
// Cellar/Jar. Изначально (тем же днём, раньше) заведены только два полюса
// (Basket/Cellar), этот заход добавил Sack/Cabinet/Jar тем же принципом.
// Тест проверяет весь спектр разом: Sack хуже Basket хуже None хуже
// Cabinet хуже Cellar хуже Jar -- не только "выше/ниже базовой линии",
// как проверялось для исходных двух, а правильный относительный порядок
// всех шести.

#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
    // Тикает свежий инвентарь заданного типа контейнера с одним подверженным
    // порче предметом и возвращает итоговый Distortion (мера порчи, см.
    // ApplyDecayToItem) -- чем выше, тем хуже хранение. Компонент обязан
    // быть зарегистрирован на живом актора/мире -- голый NewObject() без
    // RegisterComponent() падает в движковый ensure/assert "bRegistered"
    // внутри TickComponent (найдено этим же тестом при первом прогоне).
    float MeasureDecayFor(UWorld* World, EStorageContainerType ContainerType)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        UHerbalistInventoryComponent* Inventory = NewObject<UHerbalistInventoryComponent>(Owner);
        Inventory->RegisterComponent();
        Inventory->ContainerType = ContainerType;

        FInventoryItem Item;
        Item.IngredientID = FName(TEXT("TestHerb"));
        Item.Count = 1;
        Item.bSubjectToDecay = true;
        Item.State.Meta.Stability = 0.0f;   // максимальная Instability -- эффект контейнера виден чище
        Inventory->AddItem(Item, 1);

        FActorComponentTickFunction DummyTick;
        // DecayUpdateInterval = 1.0f -- DeltaTime=1.0 гарантированно
        // пересекает порог накопления на первом же вызове.
        Inventory->TickComponent(1.0f, ELevelTick::LEVELTICK_All, &DummyTick);

        const FInventoryItem* Slot = Inventory->GetSlot(0);
        const float Result = Slot ? Slot->State.Meta.Distortion : -1.0f;

        Owner->Destroy();
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistStorage_ContainerTypesOrderDecayCorrectly,
    "Herbalist.Storage.ContainerTypesOrderDecayCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistStorage_ContainerTypesOrderDecayCorrectly::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    const float SackDecay    = MeasureDecayFor(World, EStorageContainerType::Sack);
    const float BasketDecay  = MeasureDecayFor(World, EStorageContainerType::Basket);
    const float NoneDecay    = MeasureDecayFor(World, EStorageContainerType::None);
    const float CabinetDecay = MeasureDecayFor(World, EStorageContainerType::Cabinet);
    const float CellarDecay  = MeasureDecayFor(World, EStorageContainerType::Cellar);
    const float JarDecay     = MeasureDecayFor(World, EStorageContainerType::Jar);

    TestTrue(TEXT("Sack decays worse than Basket"), SackDecay > BasketDecay);
    TestTrue(TEXT("Basket decays worse than None (baseline)"), BasketDecay > NoneDecay);
    TestTrue(TEXT("None decays worse than Cabinet"), NoneDecay > CabinetDecay);
    TestTrue(TEXT("Cabinet decays worse than Cellar"), CabinetDecay > CellarDecay);
    TestTrue(TEXT("Cellar decays worse than Jar (best of all)"), CellarDecay > JarDecay);

    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
