// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Map/RGMapSubsystem.h"
#include "RGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ARGCesiumMapManager;
struct FInputActionValue;

/**
 * ARGPlayerController
 *
 * Handles player input during a round:
 *  - Left mouse click  → raycast → URGMapSubsystem::PlaceGuess
 *  - Enter / Space     → ConfirmGuess → Server RPC → URGMatchSubsystem::SubmitGuess
 *
 * CachedMapManager is resolved once in BeginPlay to avoid per-click actor iteration.
 */
UCLASS()
class RADIOGUESSER_API ARGPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARGPlayerController();

    // ── Map interaction ───────────────────────────────────────────────────────

    /** Raycast under cursor → place guess marker */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Map")
    void TryPlaceGuessAtCursor();

    /** Confirm and submit the pending guess */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Match")
    void ConfirmGuess();

    /** Call at the start of each new round to allow a new guess */
    UFUNCTION(BlueprintCallable, Category = "Gameplay|Match")
    void ResetGuessLock();

    // ── Server RPC ────────────────────────────────────────────────────────────

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SubmitGuess(FRGGeoCoordinate Coordinate, const FString& RoundToken);

    // ── Input assets (assign in BP_RGPlayerController defaults) ───────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MapClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ConfirmGuessAction;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // Called when Escape is pressed — returns mouse cursor to UI/editor
    virtual void OnEscapePressed();

private:
    void OnMapClick    (const FInputActionValue& Value);
    void OnConfirmGuess(const FInputActionValue& Value);

    /** Cached at BeginPlay — avoids TActorIterator every click */
    UPROPERTY() TObjectPtr<ARGCesiumMapManager> CachedMapManager;

    bool bGuessSubmitted = false;
};
