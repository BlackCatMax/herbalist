#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/HerbalistAssetCatalog.h"
#include "HerbalistBootstrapSubsystem.generated.h"

UCLASS()
class PROJECTHERBALIST_API UHerbalistBootstrapSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category="Herbalist|Bootstrap")
    bool IsBootstrapped() const { return bBootstrapped; }

    UFUNCTION(BlueprintCallable, Category="Herbalist|Bootstrap")
    const UHerbalistAssetCatalog* GetCatalog() const { return LoadedCatalog; }

private:
    bool bBootstrapped = false;

    UPROPERTY(Transient)
    TObjectPtr<UHerbalistAssetCatalog> LoadedCatalog = nullptr;

    bool LoadCatalog();
    void InitializeRegistries();
};
