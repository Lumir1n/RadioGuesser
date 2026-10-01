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
 * ARGGlobePawn — north-up globe camera (Google Earth / Google Maps).
 *
 * The camera sits above a geographic look-at (lat/lon) and always looks
 * straight down with north at the top of the screen. It never orbits a
 * world-space point.
 *
 * Far out (whole planet): left/right drag spins Earth around its polar axis.
 *                         Up/down drag is locked — you cannot flip the globe.
 * Zoomed in: drag pans north/south/east/west over the surface.
 */
UCLASS()
class RADIOGUESSER_API ARGGlobePawn : public APawn
{
    GENERATED_BODY()

public:
    ARGGlobePawn();

    /** True if the current LMB press moved further than a click. */
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> GlobeRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCameraComponent> Camera;

    /** Unused leftover — pan speed is derived from camera height. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PanScale = 0.003f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ZoomSpeed = 500000.0f;

    /** Minimum camera height above ellipsoid, in centimetres. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MinArmLength = 10000000.0f;   // 100 km

    /** Maximum camera height above ellipsoid, in centimetres. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float MaxArmLength = 3000000000.0f; // 30 000 km

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float KeyboardPanSpeed = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float ClickDragThreshold = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float RotationSpeed = 0.15f;

    /** Height (cm) where north/south pan starts fading out. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewStartHeight = 800000000.0f;  // 8 000 km

    /** Height (cm) where only polar-axis spin remains. */
    UPROPERTY(EditDefaultsOnly, Category = "Globe|Camera")
    float PlanetViewFullHeight = 1800000000.0f; // 18 000 km

    /** Clamp so ESU frame does not degenerate at the poles. */
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

    bool  bIsDragging              = false;
    bool  bDragExceededThreshold   = false;
    bool  bHasGrabPoint            = false;
    float TotalDragPixels          = 0.0f;
    float CurrentArmLength         = 500000000.0f; // 5 000 km

    double ViewLatitude  = 30.0;
    double ViewLongitude = 20.0;
    double AppliedOriginLatitude  = TNumericLimits<double>::Max();
    double AppliedOriginLongitude = TNumericLimits<double>::Max();
    double GrabLatitude  = 0.0;
    double GrabLongitude = 0.0;

    FVector2D LastMouseDelta  = FVector2D::ZeroVector;
    FVector2D LastCursorPos   = FVector2D::ZeroVector;
    bool      bHaveCursorPos  = false;
};
