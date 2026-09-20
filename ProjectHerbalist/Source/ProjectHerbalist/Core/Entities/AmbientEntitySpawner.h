// AmbientEntitySpawner.h
//
// Ручной спавнер Низших (DESIGN_Entity_Spawners.md, решение пользователя
// 2026-09-20: «хочется что-то контролировать иногда»). Ставится на уровень,
// сам регистрируется в AGridWorldManager при BeginPlay -- тот же приём, что
// уже у капища (ARitualShrineActor) и домашних хранилищ: актор уровня
// говорит симуляции о себе сам, симуляция его не ищет.
//
// В своём радиусе отменяет автоматические спавнеры (bOverridesAuto), поэтому
// поставленный рукой спавнер -- не добавка к процедурному заселению, а
// замена ему в этом месте.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientEntitySpawner.generated.h"

class AGridWorldManager;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AAmbientEntitySpawner : public AActor
{
    GENERATED_BODY()

public:
    AAmbientEntitySpawner();

    // Радиус зоны в метрах: где появляются и бродят особи. 0 и меньше --
    // берётся общий AmbientSpawnerRadiusMeters из настроек.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Spawner", meta = (ClampMin = "0.0"))
    float RadiusMeters = 0.0f;

    // Кого выпускать. Пусто -- любой Низший, подходящий биому и состоянию
    // клетки центра, как у автоматического.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Spawner")
    TArray<FName> AllowedEntityIDs;

    // Потолок особей. 0 и меньше -- потолок карточки (PackSize).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Spawner", meta = (ClampMin = "0"))
    int32 MaxIndividuals = 0;

    // Отменять ли автоматические спавнеры в своём радиусе.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Spawner")
    bool bOverridesAuto = true;

    float GetRadiusMeters() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY()
    TWeakObjectPtr<AGridWorldManager> RegisteredManager;
};
