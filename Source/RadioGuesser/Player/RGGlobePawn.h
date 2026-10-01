// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Map/RGMapSubsystem.h"
#include "RGGlobePawn.generated.h"

class ACesiumGeoreference;
class UCesiumOriginShiftComponent;
class UCesiumGlobeAnchorComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * ARGGlobePawn — north-up globe camera, Google Earth style.
 *
 * Uses UCesiumOriginShiftComponent (ChangeCesiumGeoreference mode) to keep
 * the UE world origin under the camera at all times.  This is the same
 * mechanism Cesium's own GlobeAwareDefaultPawn uses, so tile LODs stay
 * stable and there are no duplicate-continent artefacts.
 *
 * The camera always looks straight down with north at the top of screen.
 * Drag pans the view in geographic space (lat/lon).
 * Scroll wheel zooms (height above ellipsoid).
 */
UCLASS()
class RADIOGUESSER_API ARGGlobePawn : public APawn
{
    GENERATED_BODY()

public:
    ARGGlobePawn();

    /** True if the current LMB press moved further than a click threshold. */
    UFUNCTION(BlueprintPure, Category = "Globe|Input")
    bool DidDragExceedClickThreshold() const { return bDragExceededThreshold; }

    /** Frame the camera so both guess and actual station are visible. */
    UFUNCTION(BlueprintCallable, Category = "Globe|Camera")
    void FocusOnGuessResult(FRGGeoCoordinate Guess, FRGGeoCoordinate Actual, float DistanceKm);

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaTime) override;

    void OnDragStarted (const FInputActionValue& Value);
    void OnDragStopped (const FInputActionValue& Value);
    void OnZoom        (const FInputActionValue& Value);
    void OnMouseXY     (const FInputActionValue& Value);

    void MoveForward(float Value);
    void MoveRight  (float Value);

    // ── Components ─────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> GlobeRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCameraComponent> Camera;

    /** Drives automatic Cesium origin rebasing as the camera moves. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCesiumOriginShiftComponent> OriginShift;

    /** Required by OriginShift to track the pawn's ECEF position. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    // ── Tuning ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength = 10000000.0f;   // 100 km

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength = 3000000000.0f; // 30 000 km

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float KeyboardPanSpeed = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 8.0f;

    /** Height (cm) where N/S pan starts fading out at planet view. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewStartHeight = 800000000.0f;

    /** Height (cm) where only E/W spin remains. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewFullHeight = 1800000000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxAbsLatitude = 85.0f;

private:
    ACesiumGeoreference* FindGeoreference() const;
    void ApplyCameraToGlobe();
    void ApplyDragPan();
    void PanByMeters(double EastMeters, double NorthMeters);
    float GetNorthSouthPanScale() const;
    double GetViewHeightMeters() const { return static_cast<double>(CurrentArmLength) * 0.01; }

    UPROPERTY() TObjectPtr<ACesiumGeoreference> CachedGeoreference;

    UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Drag;
    UPROPERTY() TObjectPtr<UInputAction>         IA_Zoom;
    UPROPERTY() TObjectPtr<UInputAction>         IA_MouseXY;

    bool  bIsDragging            = false;
    bool  bDragExceededThreshold = false;
    bool  bHasGrabPoint          = false;
    float TotalDragPixels        = 0.0f;
    float CurrentArmLength       = 500000000.0f; // 5 000 km start

    double ViewLatitude  = 30.0;
    double ViewLongitude = 20.0;
    double GrabLatitude  = 0.0;
    double GrabLongitude = 0.0;

    FVector2D LastMouseDelta = FVector2D::ZeroVector;
    FVector2D LastCursorPos  = FVector2D::ZeroVector;
    bool      bHaveCursorPos = false;
};
