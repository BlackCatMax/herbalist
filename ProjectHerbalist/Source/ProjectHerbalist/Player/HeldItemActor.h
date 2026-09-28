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
    // ItemMesh -- меш растения из строки DT_IngredientClass (ResourceMesh):
    // с ним предмет узнаётся на вид (2026-09-28, по PIE: «шарики, не понятно,
    // что за предметы»), своими материалами, без окраски; нет -- заглушка.
    void ShowItem(const FInventoryItem& Item, bool bIsMineral, bool bIsLiquid, UStaticMesh* ItemMesh = nullptr);

    // Размер в долях метровой заглушки: 0.05 -- горсть в 5 см. Меш растения
    // приводится к той же величине по наибольшей стороне (в полтора раза
    // крупнее шара -- у травинки нет объёма).
    void SetShownSize(float Size);

    // Меш растения для предмета: ResourceMesh его строки в реестре; зелье,
    // вода и всё без строки или меша -- nullptr (заглушка).
    static UStaticMesh* FindItemMesh(const UObject* WorldContext, const FInventoryItem& Item);

    // Что показано -- для строки имени при взгляде.
    const FInventoryItem& GetShownItem() const { return ShownItem; }
    bool IsShowingItemMesh() const { return bShowingItemMesh; }

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
    FInventoryItem ShownItem;
    float ShownSize = 0.08f;
    float MeshNormalize = 1.0f;
    bool bShowingItemMesh = false;

    void ApplySize();

    static constexpr float PlantSizeOverShape = 1.5f;

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
