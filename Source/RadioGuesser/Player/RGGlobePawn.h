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
 * ARGGlobePawn — Google Earth-style globe camera.
 *
 * Controls:
 *   LMB drag   — pan the map (rotate spring arm)
 *   Mouse wheel — zoom
 *   WASD        — pan (W=north, S=south, A=west, D=east)
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

    // ── Enhanced Input handlers ────────────────────────────────────────────────
    void OnDragStarted (const FInputActionValue& Value);
    void OnDragStopped (const FInputActionValue& Value);
    void OnZoom        (const FInputActionValue& Value);
    void OnMouseXY     (const FInputActionValue& Value);

    // ── Legacy axis handlers (WASD) ───────────────────────────────────────────
    void MoveForward(float Value);
    void MoveRight  (float Value);

    // ── Components ─────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> GlobeRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCameraComponent> Camera;

    // ── Tuning ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float RotationSpeed    = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ZoomSpeed        = 500000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength     = 700000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength     = 3500000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float KeyboardPanSpeed = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 8.0f;

private:
    // Created in constructor so they exist when SetupPlayerInputComponent runs
    UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Drag;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Zoom;
    UPROPERTY() TObjectPtr<UInputAction>         IA_MouseXY;

    bool      bIsDragging      = false;
    float     TotalDragPixels  = 0.0f;
    float     CurrentArmLength = 1800000.0f;

    FVector2D LastMouseDelta = FVector2D::ZeroVector;
};
