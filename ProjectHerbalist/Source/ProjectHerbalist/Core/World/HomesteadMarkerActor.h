// HomesteadMarkerActor.h
//
// "Обставление" (ROADMAP.md, DESIGN_Community_And_Homestead.md §2.1/§2.4,
// 2026-09-06) — постройки для прокачки, у которых механизм/экономика уже
// работают через Exec-команды/TMap-состояние, но не было ни одного
// физического актора. Домашнее хранилище уже своё (AStorageContainer,
// AGridWorldManager::SpawnHomeStorageContainer) — этот класс закрывает
// оставшиеся две: пристройку сада (грядку с нишей) и базу/лагерь.
// Тот же паттерн, что POI-акторы (Core/World/POIActors.h): голый маркер,
// меш не хардкодится, TODO на финальный арт.
//
// Честный пробел этого захода: спавнится только при ЖИВОЙ регистрации
// (RegisterGardenPlot/RegisterBase) в текущей сессии — НЕ пересоздаётся
// при восстановлении из сейва (HerbalistSaveSubsystem::LoadGame
// присваивает GardenPlots/Bases напрямую, без прохода по акторам). После
// загрузки сейва, где эти постройки уже существовали, но не были
// зарегистрированы заново в ЭТОЙ сессии, маркеры не появятся — механика
// (ниша/база) при этом работает корректно, страдает только видимость.
// Зафиксировано в ROADMAP.md, не скрыто.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HomesteadMarkerActor.generated.h"

class UStaticMeshComponent;

UENUM()
enum class EHomesteadMarkerKind : uint8
{
    GardenPristroyka,   // грядка с назначенной нишей (RegisterGardenPlot)
    Base                // база/лагерь (RegisterBase)
};

UCLASS(Blueprintable, BlueprintType)
class PROJECTHERBALIST_API AHomesteadMarkerActor : public AActor
{
    GENERATED_BODY()

public:
    AHomesteadMarkerActor();

    void Init(const FIntPoint& InGridCell, EHomesteadMarkerKind InKind, EGardenNiche InNiche = EGardenNiche::None);

    // Пристройка меняет нишу без повторного сева (RegisterGardenPlot
    // перезаписывает уже существующую запись) -- контент читает это поле,
    // чтобы подобрать меш/партикл под конкретную нишу, не пересоздавая актор.
    void SetNiche(EGardenNiche InNiche) { Niche = InNiche; }

    FIntPoint GetGridCell() const { return GridCell; }
    EHomesteadMarkerKind GetKind() const { return Kind; }

    UPROPERTY(BlueprintReadOnly, Category = "Homestead")
    EGardenNiche Niche = EGardenNiche::None;

protected:
    // TODO: заменить на финальный арт -- меш/партикл выбирается контентом
    // по Kind/Niche, код не хардкодит ассеты (тот же принцип, что уже
    // AShrineActor/POIActors).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MarkerMesh;

private:
    FIntPoint GridCell = FIntPoint(-1, -1);
    EHomesteadMarkerKind Kind = EHomesteadMarkerKind::GardenPristroyka;
};
