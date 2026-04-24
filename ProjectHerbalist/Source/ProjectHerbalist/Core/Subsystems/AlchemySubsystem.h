#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AlchemySubsystem.generated.h"

UCLASS()
class PROJECTHERBALIST_API UAlchemySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static constexpr const TCHAR* IngredientTablePath = TEXT("/Game/Herbalist/Data/DT_IngredientClass.DT_IngredientClass");
};