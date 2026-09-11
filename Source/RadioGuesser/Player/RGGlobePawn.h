// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RGGlobePawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * ARGGlobePawn
 *
 * Google Earth-style globe camera:
 *  - Left Mouse Drag      → rotate/orbit the globe
 *  - Mouse Wheel          → zoom in/out
 *  - Left Mouse Click     → place guess marker (short click, no drag)
 */
UCLASS()
class RADIOGUESSER_API ARGGlobePawn : public APawn
{
    GENERATED_BODY()

public:
    ARGGlobePawn();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaTime) override;

    // ── Input handlers ─────────────────────────────────────────────────────────

    void OnDragStarted   (const FInputActionValue& Value);
    void OnDragOngoing   (const FInputActionValue& Value);
    void OnDragStopped   (const FInputActionValue& Value);
    void OnZoom          (const FInputActionValue& Value);
    void OnMouseXY       (const FInputActionValue& Value);

    // ── Components ─────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> GlobeRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCameraComponent> Camera;

    // ── Tuning ─────────────────────────────────────────────────────────────────

    /** How fast the globe rotates when dragging (degrees per pixel) */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float RotationSpeed = 0.3f;

    /** Zoom speed multiplier */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ZoomSpeed = 500000.0f;

    /** Minimum arm length (closest zoom) in cm */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength = 700000.0f;   // ~7000 km

    /** Maximum arm length (farthest zoom) in cm */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength = 3500000.0f;  // ~35000 km

    /** Drag must exceed this many pixels to NOT count as a click */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 8.0f;

private:
    // Input assets (auto-loaded in BeginPlay)
    UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Drag;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Zoom;
    UPROPERTY() TObjectPtr<UInputAction>         IA_MouseXY;

    bool   bIsDragging      = false;
    float  TotalDragPixels  = 0.0f;
    float  CurrentArmLength = 1800000.0f;

    FVector2D LastMouseDelta = FVector2D::ZeroVector;
};
