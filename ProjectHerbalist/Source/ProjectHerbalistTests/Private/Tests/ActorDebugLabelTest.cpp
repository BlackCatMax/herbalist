// Source/ProjectHerbalistTests/Private/Tests/ActorDebugLabelTest.cpp
//
// Подписи акторов в аутлайнере (2026-09-03, жалоба пользователя: "при
// выделении акторов в PIE мне не понятно, что это за актор, т.к. заглушка
// одна на всех. Аналогично с именами ресурсов").
//
// Пока меш-заглушка у всего бестиария один, а движковое имя выглядит как
// "BP_HerbalistResourceActor_C_37", метка в аутлайнере -- единственное,
// что отличает Гнильников от Лешего, а зверобой от крапивы.
//
// Тест редакторный по самой сути проверяемого: SetActorLabel объявлен под
// WITH_EDITOR и в кукнутой сборке не существует вовсе -- поэтому и файл
// целиком под WITH_EDITOR, как и сама метка.

#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistActorLabel_ResourceActorIsNamedByIngredientAndCell,
    "Herbalist.ActorLabel.ResourceActorIsNamedByIngredientAndCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistActorLabel_ResourceActorIsNamedByIngredientAndCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    AHerbalistResourceActor* Resource = World->SpawnActor<AHerbalistResourceActor>();
    if (!TestNotNull(TEXT("Resource actor spawned"), Resource)) { Manager->Destroy(); return false; }

    Resource->Init(FName(TEXT("ste_06")), FText::FromString(TEXT("Зверобой")), nullptr,
        FRealState(), FVector::ZeroVector, Manager, 12, 34, 0.0f, false, false);

    const FString Label = Resource->GetActorLabel();
    TestTrue(FString::Printf(TEXT("Label names the ingredient (got '%s')"), *Label), Label.Contains(TEXT("ste_06")));
    // Координаты не косметика: одинаковых "ste_06" в мире тысячи, без них
    // выделенный актор всё равно неотличим от соседнего.
    TestTrue(FString::Printf(TEXT("Label names the cell (got '%s')"), *Label), Label.Contains(TEXT("12,34")));

    Resource->Destroy();
    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistActorLabel_EntityActorIsNamedBySpeciesAndCell,
    "Herbalist.ActorLabel.EntityActorIsNamedBySpeciesAndCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistActorLabel_EntityActorIsNamedBySpeciesAndCell::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    AHerbalistEntityActor* Entity = World->SpawnActor<AHerbalistEntityActor>();
    if (!TestNotNull(TEXT("Entity actor spawned"), Entity)) { Manager->Destroy(); return false; }

    // Здесь метка нужнее всего: меш-заглушка у ВСЕГО бестиария один.
    Entity->Init(FName(TEXT("Гнильники")), FIntPoint(7, 9), Manager);

    const FString Label = Entity->GetActorLabel();
    TestTrue(FString::Printf(TEXT("Label names the species (got '%s')"), *Label), Label.Contains(TEXT("Гнильники")));
    TestTrue(FString::Printf(TEXT("Label names the cell (got '%s')"), *Label), Label.Contains(TEXT("7,9")));

    Entity->Destroy();
    Manager->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR
