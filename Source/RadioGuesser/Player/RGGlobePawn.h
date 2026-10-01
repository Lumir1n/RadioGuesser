// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Map/RGMapSubsystem.h"
#include "RGGlobePawn.generated.h"

class ACesiumGeoreference;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * ARGGlobePawn — north-up globe camera, Google Earth / Google Maps style.
 *
 * Architecture (stable, no OriginShift conflicts):
 *   • Every tick: SetOriginLongitudeLatitudeHeight(ViewLon, ViewLat, 0)
 *     → UE origin is always directly below the camera on the surface.
 *   • Camera placed at (0, 0, HeightCm * CmToUE) in UE space.
 *     When the origin is right below us, +Z in UE IS the local "Up" direction.
 *   • Orientation computed from the ESU matrix at the origin:
 *     East→+X, North→-Y (ESU has South=+Y so North=-Y), Up→+Z.
 *     Camera looks down (-Z) with screen-up = North (-Y).
 *
 * Drag pans ViewLat/ViewLon (geographic coordinates).
 * Scroll wheel adjusts CurrentArmLength (height above ellipsoid, cm).
 */
UCLASS()
class RADIOGUESSER_API ARGGlobePawn : public APawn
{
    GENERATED_BODY()

public:
    ARGGlobePawn();

    /** True if LMB drag exceeded the click threshold (used by controller). */
    UFUNCTION(BlueprintPure, Category = "Globe|Input")
    bool DidDragExceedClickThreshold() const { return bDragExceededThreshold; }

    /** Move the view to frame both guess and actual locations. */
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

    // ── Camera tuning ──────────────────────────────────────────────────────────
    /** Minimum altitude above ellipsoid in centimetres (1 km — allows close zoom). */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength = 100000.0f;   // 1 km

    /** Maximum altitude above ellipsoid in centimetres (30 000 km). */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength = 3000000000.0f;

    /** WASD speed multiplier (fraction of view height per second). */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float KeyboardPanSpeed = 1.5f;

    /** Minimum pixel movement before a click is treated as a drag. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 15.0f;

    /** Latitude above which N/S panning starts to fade (planet-scale view). */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewStartHeight = 800000000.0f;   // 8 000 km

    /** Altitude at which N/S panning is fully suppressed. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewFullHeight = 1800000000.0f;   // 18 000 km

    /** Latitude clamped to ±this value to avoid pole singularities. */
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
    float CurrentArmLength       = 500000000.0f; // 5 000 km default

    double ViewLatitude  = 48.0;   // start over central Europe
    double ViewLongitude = 15.0;

    double GrabLatitude  = 0.0;
    double GrabLongitude = 0.0;

    // Throttle origin rebasing: only rebase when view moved more than this
    double LastRebaseLatitude  = 1e30;
    double LastRebaseLongitude = 1e30;

    FVector2D LastMouseDelta = FVector2D::ZeroVector;
    FVector2D LastCursorPos  = FVector2D::ZeroVector;
    bool      bHaveCursorPos = false;
};
