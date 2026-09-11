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
 * ARGGlobePawn — вид сверху, как в GeoGuessr.
 *
 * Паун перемещается горизонтально по карте.
 * SpringArm смотрит вертикально вниз (pitch -70).
 * Zoom меняет высоту (длину SpringArm).
 *
 * Управление:
 *   ЛКМ + тащи  — двигаем карту под камерой
 *   Колёсико    — zoom (приближение/отдаление)
 *   WASD        — движение по карте
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

    // ── Enhanced Input ─────────────────────────────────────────────────────────
    void OnDragStarted (const FInputActionValue& Value);
    void OnDragStopped (const FInputActionValue& Value);
    void OnZoom        (const FInputActionValue& Value);
    void OnMouseXY     (const FInputActionValue& Value);

    // ── Legacy axis (WASD) ────────────────────────────────────────────────────
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

    /** Скорость пана мышью: смещение = delta_mouse * arm_length * PanScale */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PanScale = 0.0005f;

    /** Скорость zoom колёсиком (см за тик колёсика) */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ZoomSpeed = 500000.0f;

    /** Минимальная высота камеры (ближайший zoom) */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength = 10000000.0f;   // 100 км — детали города

    /** Максимальная высота камеры (дальний zoom) — весь глобус */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength = 3000000000.0f;  // 30 000 км — весь мир

    /** Скорость WASD (доля arm_length за тик) */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float KeyboardPanSpeed = 0.3f;

    /** Устаревшее, оставлено для совместимости */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float RotationSpeed = 0.3f;

private:
    UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Drag;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Zoom;
    UPROPERTY() TObjectPtr<UInputAction>         IA_MouseXY;

    bool      bIsDragging      = false;
    float     TotalDragPixels  = 0.0f;
    float     CurrentArmLength = 500000000.0f;  // старт ~5000 км — видны континенты

    FVector2D LastMouseDelta = FVector2D::ZeroVector;
};
