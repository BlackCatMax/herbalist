// HerbalistSaveSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HerbalistSaveSubsystem.generated.h"

// Координатор сохранений v1 (CHANGELOG.md 2026-08-24): собирает состояние из
// AGridWorldManager + компонентов игрока в один UHerbalistSaveGame и обратно.
// GameInstanceSubsystem, как реестры ингредиентов/воды — тот же паттерн:
// не привязан к конкретному актору, переживает смену уровня.
UCLASS()
class PROJECTHERBALIST_API UHerbalistSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static const FString DefaultSlotName;

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool SaveGame(const FString& SlotName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Herbalist|Save")
    bool LoadGame(const FString& SlotName = TEXT(""));
};
