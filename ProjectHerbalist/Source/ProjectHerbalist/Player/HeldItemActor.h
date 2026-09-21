// HeldItemActor.h
//
// Видимая часть руки (UHeldItemComponent): предмет перед камерой. Пока это
// заглушка -- простая форма по классу предмета (корень и трава -- шар,
// камень -- куб, склянка и вода -- цилиндр), окрашенная по оси, которая в
// воспринятом состоянии преобладает. Настоящие модели и руки -- арт.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "HeldItemActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class PROJECTHERBALIST_API AHeldItemActor : public AActor
{
    GENERATED_BODY()

public:
    AHeldItemActor();

    // Показать этот предмет: форма по классу, цвет по воспринятому состоянию.
    void ShowItem(const FInventoryItem& Item, bool bIsMineral, bool bIsLiquid);

    // Цвет заглушки -- открыт для тестов: он и есть «вид» предмета до арта.
    static FLinearColor ColorForState(const FRealState& Perceived);

    // Ярче или тусклее последнего цвета: оберег на поясе светится, пока
    // действует (этап 4), и гаснет после.
    void SetBrightness(float Brightness);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> TintMaterial = nullptr;

    FLinearColor ShownColor = FLinearColor::White;

    // Заглушки держатся жёсткими ссылками с CDO (ревью 2026-09-21): LoadObject
    // по пути в упакованной игре вернул бы nullptr, если кукер не взял
    // /Engine/BasicShapes, -- и предмет в руке молча остался бы без меша.
    UPROPERTY()
    TObjectPtr<UStaticMesh> SphereMesh = nullptr;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh = nullptr;
    UPROPERTY()
    TObjectPtr<UStaticMesh> CylinderMesh = nullptr;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial = nullptr;
};
