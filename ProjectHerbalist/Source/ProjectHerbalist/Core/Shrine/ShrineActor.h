// ShrineActor.h
//
// Капище как отдельное МЕСТО (02_GDD/15_Cycles_And_Shrines.md §15.5),
// 2026-09-02 — пересмотр решения v1 по прямому запросу пользователя
// ("хочу, чтобы капища были отдельными местами, которые не зависят от
// местоположения котла"). До этой правки капище не существовало само по
// себе: AAlchemyTableActor::BeginPlay регистрировал его на своей клетке,
// то есть капище было там и только там, где стоит котёл. Теперь наоборот —
// котёл капища не создаёт вовсе, капища расставляет левел-дизайнер этим
// актором, а место варки относительно них становится выбором игрока
// (надбавка к Coherence работает в ShrineInfluenceRadius, §15.5).
//
// Домовой и Роса Заряны остались на котле — они про очаг/жилище, не про
// капище, и от этой развязки не зависят.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Shrine/ShrineTypes.h"
#include "ShrineActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AShrineActor : public AActor
{
    GENERATED_BODY()

public:
    AShrineActor();

    // Тип капища по умолчанию определяется клеткой, как и раньше
    // (AGridWorldManager::ResolveShrineTypeForCell: сначала проверяется
    // граница биомов, затем биомная группа). Сними галку, чтобы задать тип
    // руками — например, поставить Родовое капище посреди леса.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine")
    bool bResolveTypeFromCell = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine", meta = (EditCondition = "!bResolveTypeFromCell"))
    EShrineType ShrineType = EShrineType::Ancestral;

    // Стартовое Restoration — капище, которое уже ухожено к началу партии
    // (или, если меньше нуля, уже осквернено). Применяется ТОЛЬКО при
    // создании нового капища на этой клетке: если на клетке уже есть
    // капище (например, второй актор поставлен туда же), накопленное
    // значение не перетирается. [-1, 1], тот же диапазон, что у FShrine.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float InitialRestoration = 0.0f;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
    FIntPoint GridCoords = FIntPoint(-1, -1);
};
