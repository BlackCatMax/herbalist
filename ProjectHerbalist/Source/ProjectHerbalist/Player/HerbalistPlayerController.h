// HerbalistPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "UI/InventoryWidget.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/JournalWidget.h"
#include "HerbalistPlayerController.generated.h"

class AStorageContainer;
class UInventoryTransferWidget;
class AAlchemyTableActor;
class AGridWorldManager;
class AHerbalistResourceActor;

UCLASS()
class PROJECTHERBALIST_API AHerbalistPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHerbalistPlayerController();
	
	virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UHerbalistInventoryComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Journal")
    UHerbalistJournalComponent* JournalComponent;

    UPROPERTY(BlueprintReadOnly, Category = "UI")
    bool bIsAnyWidgetOpen = false;

    UPROPERTY()
    UInventoryTransferWidget* CurrentTransferWidget = nullptr;

    UPROPERTY()
    UAlchemyTransferWidget* CurrentAlchemyWidget = nullptr;

    UPROPERTY()
    AAlchemyTableActor* CurrentAlchemyTable = nullptr;
	
    UPROPERTY(BlueprintReadOnly, Category = "Alchemy")
    float CurrentGlobalDistortion = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Herbalist|Harvesting")
    float MaxHarvestDistance = 200.0f;

    // Текущий инструмент сбора (DESIGN_Community_And_Homestead.md §2.3,
    // 2026-08-31) — читается AGridWorldManager::OnResourceCollected в
    // Cmd.Harvest.Tool. v1: переключается Exec-командой (SetGatheringTool),
    // не физическим предметом в инвентаре — экономика источников
    // инструментов (курганы/дары хозяев/торговля) отдельный, ещё не
    // реализованный проход; сам множитель качества работает уже сейчас.
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Harvesting")
    EGatheringTool CurrentGatheringTool = EGatheringTool::BareHands;

    // Два предмета-спутника (21_Journey_And_Artifacts.md §21.2, ревизия
    // "Update docs" 2026-09-01) — НЕ предметы инвентаря (FInventoryItem
    // заточен под распад/стек, плохо подходит постоянному дару), два bool,
    // тот же принцип, что уже CurrentGatheringTool не инвентарный предмет.
    // Выставляются в OfferForArtifact при честной добыче через общий путь
    // §21.3, как любой другой артефакт — рамка "неполный дар Аграфены
    // второпях" снята этой ревизией: "Аграфена их не даёт... оба — такие
    // же артефакты Легендарных, как остальные шесть". Прогретое состояние
    // — не отдельный bool здесь, а AGridWorldManager::IsArtifactWarmed
    // ("Зеркальце"/"Клубочек") — тот же Warmth-механизм §21.4, что и у
    // остальных шести (вариант C, прогрев через зелье в родном регионе).
    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bHasMirror = false;

    UPROPERTY(BlueprintReadOnly, Category = "Herbalist|Zaryana")
    bool bHasYarnBall = false;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CloseAnyWidget();

    // HarvestTest/MassHarvestTest удалены 2026-09-02 (чистка мёртвого кода) --
    // обёртки над одноимёнными заглушками AGridWorldManager, которые только
    // логировали "deprecated" и ничего не делали.
    UFUNCTION(Exec)
    void ApplyTest(int32 X, int32 Y);
    UFUNCTION(Exec)
    void ShowInventory();

    // Сбор без участия ввода (2026-09-03). Делает ровно то же, что Harvest()
    // по клавише: тот же трейс от камеры, та же проверка дистанции, тот же
    // путь в пайплайн. Нужен, чтобы отделить «сломан ввод» от «сломан сбор»
    // одной командой в консоли -- при незаданном UInputAction клавиша молчит
    // так же, как молчал бы неработающий сбор.
    UFUNCTION(Exec)
    void HarvestHere();
	
	UFUNCTION(Exec)
	void TestNewHarvest(int32 X, int32 Y, FName IngredientID);

    UFUNCTION(Exec, BlueprintCallable, Category = "Alchemy")
    void UsePotion();
    
    UFUNCTION(BlueprintCallable, Category = "Herbalist|Harvesting")
    bool CanHarvestActor(AActor* TargetActor) const;
	
    AGridWorldManager* FindWorldManager() const;
    void GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const;
    void UpdateDistortionFromCell(int32 X, int32 Y);

    // Свой фиксированный сид, не WorldRNG — тот же приём, что уже
    // AlchemySlotWidget.cpp::PerceptionRng: наблюдение через Зеркальце не
    // должно потреблять/возмущать детерминированный поток симуляции.
    FRandomStream MirrorPerceptionRng = FRandomStream(20260901);
	
    UFUNCTION(Exec)
    void TestNewTransfer(FName IngredientID, int32 Amount);

    UFUNCTION(Exec)
    void TestNewApply(int32 X, int32 Y, FString IngredientList);

    // Сохранения v1 (Core/Save/HerbalistSaveSubsystem.h) — тонкие обёртки над
    // подсистемой, тем же паттерном, что HarvestTest/ApplyTest над пайплайном.
    UFUNCTION(Exec)
    void SaveGame();

    UFUNCTION(Exec)
    void LoadGame();

    // Экран Травника (UI/JournalLogWidget.h, 07_UX §7.2.4) — открывается тем
    // же способом, что и инвентарь (Inventory()). Exec-обёртка отдельно от
    // JournalAction — тестируемо консолью и до того, как в редакторе назначен
    // Input Action asset (тот же паттерн, что InventoryAction). Сам виджет
    // (2026-08-29, "лог, чтобы можно было открыть и почитать") больше не
    // ждёт WBP — JournalLogWidget строит дерево в C++, готов без единого
    // .uasset; JournalWidgetClass/UJournalWidget ниже остаются на будущее,
    // если понадобится более богатый экран через редактор.
    UFUNCTION(Exec)
    void ToggleJournalUI();

    // Переключить текущий инструмент сбора (§ комментарий у CurrentGatheringTool
    // выше). ToolName: "hands"/"iron"/"copper"/"bone" — строка, не enum,
    // тем же паттерном, что SetGatheringTool видится проще с консоли, чем
    // числовой индекс EGatheringTool.
    UFUNCTION(Exec)
    void SetGatheringTool(FString ToolName);

    // Диалоги (DESIGN_Community_And_Homestead.md §1.1, 2026-08-31) — v1
    // консольный, тем же принципом, что SetGatheringTool/SetGardenPlot:
    // TalkTo открывает разговор с хозяином места на клетке (X,Y) — печатает
    // реплику узла и доступные по Respect ветки в лог; ChooseDialogueBranch
    // выбирает ветку по индексу из ПОСЛЕДНЕГО напечатанного списка. Текущий
    // узел разговора — состояние контроллера, не мира (см. комментарий у
    // FTalkCommand, CommandTypes.h): разговор не переживает выход игрока
    // из зоны взаимодействия, не сохраняется.
    UFUNCTION(Exec)
    void TalkTo(int32 X, int32 Y);

    UFUNCTION(Exec)
    void ChooseDialogueBranch(int32 BranchIndex);

    // Подношение общине (DESIGN_Community_And_Homestead.md §1.3, 2026-08-31)
    // — тот же приём, что TestNewApply: ингредиенты по именам через запятую,
    // ищутся в инвентаре, списываются, ΔMolva логируется в
    // AGridWorldManager::OfferToCommunity.
    UFUNCTION(Exec)
    void OfferToCommunity(FString IngredientList);

    // Торговля с общиной (§1.2) — WantedIngredientID должен существовать в
    // реестре (DT_IngredientClass), полученный предмет добавляется в
    // инвентарь тем же способом, что и любой другой (AddItem).
    UFUNCTION(Exec)
    void TradeWithCommunity(FString OfferedIngredientID, FString WantedIngredientID);

    // Зарегистрировать клетку как грядку сада с пристройкой NicheName
    // (DESIGN_Community_And_Homestead.md §2.4, 2026-08-31) — "mycelium"/
    // "cellar"/"pond"/"sunny"/"shade", "none" снимает регистрацию. v1:
    // тот же приём, что SetGatheringTool — консоль вместо физической
    // постройки-актора, сам механизм (AGridWorldManager::GardenPlots →
    // GetRandomResourceForNiche) работает уже сейчас.
    UFUNCTION(Exec)
    void SetGardenPlot(int32 X, int32 Y, FString NicheName);

    // Основать базу (21_Journey_And_Artifacts.md §21.2, 2026-09-01) — v1
    // тот же приём, что SetGardenPlot: консоль вместо физической
    // постройки-актора, сам механизм (AGridWorldManager::Bases →
    // IsValidBrewingLocation) работает уже сейчас. Валидация (не вода, не
    // дубликат) — на стороне AGridWorldManager::RegisterBase.
    UFUNCTION(Exec)
    void FoundBase(int32 X, int32 Y);

    // Отладочный ярлык — устанавливает bHasMirror/bHasYarnBall напрямую,
    // в обход честной добычи через OfferForArtifact (§21.3). НЕ канонiчный
    // путь получения с ревизии "Update docs" (2026-09-01) — тот же класс
    // v1-упрощения, что и остальные тестовые Exec-команды этого файла,
    // удобен для быстрой настройки сценария/теста без полного гейта.
    UFUNCTION(Exec)
    void GiveZaryanaGifts();

    // Зеркальце — наблюдение Заряны из любой базы, не перемещение
    // (§21.2). Переиспользует AGridWorldManager::GetZaryanaPerceivedState
    // (Слои 1+3 + честный шум PerceiveRealState, шаг 2) — не привилегированное
    // окно истины, тот же шум, что и роса вживую.
    UFUNCTION(Exec, BlueprintCallable, Category = "Zaryana")
    void UseMirror();

    // Клубочек — перемещение между уже основанными базами (индекс в
    // AGridWorldManager::GetBases()), НЕ мгновенное: тратит игровое время,
    // соразмерное дистанции (§21.2), двигая WorldManager->GameClockSeconds
    // вперёд тем же способом, что и обычный ход времени — протаскивает
    // погоду/сутки/регенерацию, не спецэффект.
    UFUNCTION(Exec, BlueprintCallable, Category = "Zaryana")
    void UseYarnBall(int32 BaseIndex);

    // Подношение за артефакт Легендарной сущности (21_Journey_And_Artifacts.md
    // §21.3-21.4) — тот же приём поиска по имени в инвентаре, что уже
    // OfferToCommunity. ArtifactID — точное имя из ArtifactTypes.h ("Рог",
    // "Гребень", "Молодильное яблоко", "Шапка-невидимка", "Камень-оберег",
    // "Фонарь", "Зеркальце", "Клубочек"). Для двух последних — успех переключает
    // bMirrorWarmed/bYarnBallWarmed вместо новой записи в
    // AGridWorldManager::AcquiredArtifacts (см. AGridWorldManager::
    // TryAcquireArtifact).
    UFUNCTION(Exec)
    void OfferForArtifact(FString ArtifactID, FString IngredientList);

    // Рог (Индрик-зверь, §21.3) — "слушает воду": честная диагностика
    // клетки-родника (X,Y), не требует запаса, просто предмет должен быть
    // добыт. Показ — тем же ShowMemoryRevealText, что и Зеркальце.
    UFUNCTION(Exec)
    void UseHorn(int32 X, int32 Y);

    // Гребень (Берегиня, §21.3) — расходуемый побег: снимает проявленную
    // сущность с клетки (X,Y), списывается после одного применения.
    UFUNCTION(Exec)
    void UseComb(int32 X, int32 Y);

    // Молодильное яблоко (Дуб-старец, §21.3) — расходуемое: временное окно
    // сниженного шума росы Заряны.
    UFUNCTION(Exec)
    void UseYouthApple();

    // Шапка-невидимка (Баба-Яга, §21.3) — не расходуется: временно
    // подавляет новые проявления в НАСТОЯЩЕЙ зоне (2026-09-02) вокруг
    // клетки, где стоит игрок в момент применения (радиус
    // InvisibilityCapRadius), можно активировать снова после истечения
    // (переносит зону на новую клетку).
    UFUNCTION(Exec)
    void UseInvisibilityCap();

    // Фонарь, прогретая версия (Болотный царь, §21.3, ревизия "Update
    // docs") — честно показывает реальное состояние клетки (X,Y),
    // требует и добытого Фонаря, и IsArtifactWarmed("Фонарь"); базовая
    // версия остаётся просто светом, без этой команды.
    UFUNCTION(Exec)
    void UseLanternDisclosure(int32 X, int32 Y);

    // ---- Перья вещих птиц (16_Entity_Manifestation.md §16.4, эндгейм-
    // трофеи, 2026-09-02) — тот же приём общего гейта получения по имени,
    // что уже OfferForArtifact, плюс отдельные именованные команды на
    // использование, тот же приём, что уже UseHorn/UseComb/... ----

    // FeatherID — "Перо Гамаюна"/"Перо Алконоста"/"Перо Сирина"/"Перо
    // Жар-птицы". Условия получения — AGridWorldManager::
    // TryAcquireProphetFeather.
    UFUNCTION(Exec)
    void AcquireFeather(FString FeatherID);

    // Перо Гамаюна — съедено, навсегда закрепляет пророческое Зеркальце.
    UFUNCTION(Exec)
    void EatGamayunFeather();

    // Перо Алконоста — биом клетки (X,Y), где стоит игрок, подавлен на весь
    // таймер Шапки.
    UFUNCTION(Exec)
    void UseAlkonostFeather(int32 X, int32 Y);

    // Перо Сирина — честное чтение клетки (X,Y) при активном Malign-спайке
    // в её биоме, без вреда самого спайка.
    UFUNCTION(Exec)
    void UseSirinFeather(int32 X, int32 Y);

    // Перо Жар-птицы — постоянная метка клетки (X,Y) как никогда не
    // деградирующей.
    UFUNCTION(Exec)
    void UseZharPtitsaFeather(int32 X, int32 Y);

    // Сцена обмана Болотного царя (21_Journey_And_Artifacts.md §21.3,
    // подраздел "Сцена обмана Болотного царя", 2026-09-02) — НЕ
    // OfferForArtifact: обманное зелье-приманка из инвентаря (найдена по
    // IngredientID, тот же приём поиска, что уже OfferForArtifact),
    // вылитая на клетку (X,Y) рядом с проявленным Царём. Расходуется в
    // любом случае (попытка/приманка потрачена), исход — вероятностный,
    // см. AGridWorldManager::TryLureSwampTsarWithPotion.
    UFUNCTION(Exec)
    void LureSwampTsar(int32 X, int32 Y, FString PotionIngredientID);

    // Три исхода у Буяна (18_Ending.md §18.1-18.2, 2026-09-01, ревизия
    // "Ending and artifacts") — выбор через действие (три разные команды),
    // не диалоговое меню (§18.1). Каждая требует AGridWorldManager::
    // TryChooseBuyanPath == true (не переигрывается, путь 1 гейтится
    // Clarity+Молвой) — при успехе он сам показывает гарантированный
    // финальный фрагмент памяти (BUYAN_GUARDIAN/BUYAN_TRADE_PLACES/
    // BUYAN_ACCEPT_REALITY, MemoryFragmentDefinitions.h) через
    // CollectMemoryFragment, эти три команды его не дублируют.
    UFUNCTION(Exec)
    void BecomeBuyanGuardian();

    UFUNCTION(Exec)
    void TradePlacesWithZaryana();

    UFUNCTION(Exec)
    void AcceptBuyanReality();

    // Экранный попап текста воспоминания Заряны/объявления Буяна
    // (UI/MemoryRevealWidget.h, "Прогрессия/Заряна" 2026-08-29) — вызывается
    // из AGridWorldManager (CollectMemoryFragment/CheckBuyanCondition), не
    // ждёт открытия Травника: фрагмент должен быть прочитан в момент сбора,
    // Травник ниже — для перечитать позже, не единственный способ увидеть.
    void ShowMemoryRevealText(const FText& Text);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* HarvestAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InfoAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InventoryAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ApplyAlchemyAction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JournalAction;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UJournalWidget> JournalWidgetClass;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Harvest();
    void Info();
    void Inventory();
    void Journal();
    void ApplyAlchemy();
    void Interact();

    bool GetHitResultFromCamera(FHitResult& OutHit, ECollisionChannel Channel = ECC_Visibility);
    // OnLeftClick() удалён 2026-09-02 (чистка мёртвого кода) — объявление без
    // реализации и без единого вызова; левый клик обрабатывается Harvest()/
    // Interact() через привязки ввода выше.
    void OnRightClick();
    void OnApplyAlchemyKey();

    void OnUsePotion();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* UsePotionAction;

private:
    // Артефакты/перья как настоящие предметы инвентаря (2026-09-02) —
    // раньше существовали только как записи в AGridWorldManager::
    // AcquiredArtifacts/AcquiredFeathers (внутренняя бухгалтерия, игрок не
    // видел их в собственном инвентаре вовсе). AGridWorldManager остаётся
    // единственным источником истины для Warmth/деталей эффекта — эти два
    // хелпера добавляют/убирают только "квитанцию" в InventoryComponent
    // для видимости в UI, не дублируют и не заменяют мировую бухгалтерию.
    // ВНИМАНИЕ: если игрок вручную выбросит предмет из инвентаря через UI,
    // сам эффект/владение НЕ исчезнет (гейты по-прежнему читают
    // AcquiredArtifacts/AcquiredFeathers) — известный, принятый разрыв
    // косметики и механики, не заводим отдельную систему синхронизации
    // ради редкого ручного действия игрока.
    void AddArtifactToInventory(FName ArtifactOrFeatherID);

    // Возвращает true, если предмет был найден и убран (Count=1 -- все
    // артефакты/перья не стекаются, каждый уникален).
    bool RemoveArtifactFromInventory(FName ArtifactOrFeatherID);

public:
    // Инъекция для тестов (2026-09-02, тот же паттерн, что уже
    // AGridWorldManager::SetAcquiredArtifacts/SetClarityAnchor и т.д.) —
    // FindWorldManager() ищет первый AGridWorldManager в мире через
    // TActorIterator; в автотестах, где несколько таких акторов
    // спавнятся/уничтожаются подряд в одном и том же персистентном
    // редакторском мире (Destroy() не гарантирует немедленное удаление из
    // итератора в том же кадре), это может найти чужой, устаревший
    // экземпляр вместо только что заспавненного тестом. Явная инъекция
    // обходит эту гонку в тестовом окружении — в реальной игре
    // FindWorldManager() по-прежнему единственный путь.
    void SetWorldManagerForTests(AGridWorldManager* InManager) { CachedWorldManager = InManager; }

private:

    UPROPERTY()
    UInventoryWidget* InventoryWidgetInstance = nullptr;

    // UUserWidget, не UJournalWidget -- Journal() сейчас строит UJournalLogWidget
    // (см. комментарий у ToggleJournalUI ниже), JournalWidgetInstance хранит
    // и то, и другое одинаково: обе точки использования (IsInViewport/
    // RemoveFromParent) базовые, конкретный подкласс не нужен.
    UPROPERTY()
    UUserWidget* JournalWidgetInstance = nullptr;

    UPROPERTY()
    class UMemoryRevealWidget* MemoryRevealWidgetInstance = nullptr;

    // Кэш для мира
    UPROPERTY()
    AGridWorldManager* CachedWorldManager = nullptr;

    bool TryHarvestResource(AHerbalistResourceActor* Resource);

    // Текущий разговор (TalkTo/ChooseDialogueBranch, § комментарий у их
    // объявления выше) — состояние UI-сессии, не мира, не UPROPERTY(SaveGame)
    // намеренно.
    FName CurrentDialogueID = NAME_None;
    FName CurrentDialogueNodeID = NAME_None;
    FIntPoint CurrentDialogueCell = FIntPoint(-1, -1);
};