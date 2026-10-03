// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/EngineTypes.h"
#include "CesiumGeoreference.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Globe camera — stable architecture
//
//  The key insight: when we call
//      SetOriginLongitudeLatitudeHeight(ViewLon, ViewLat, 0)
//  the UE world origin IS the surface point below the camera.
//  In that frame, the local East-South-Up axes satisfy:
//      +X  = East
//      +Y  = South
//      +Z  = Up   ← this is exactly UE world +Z after the rebasing!
//
//  So placing the camera at (0, 0, HeightUE) is always correct —
//  no extra orientation math needed for position.
//
//  For orientation we want the camera to look straight down (nadir)
//  with north at the top of the screen. We derive the world-space
//  "down" and "north" vectors directly from the ESU-to-Unreal matrix
//  returned by Cesium, then build the camera rotation from those vectors.
//  This avoids all Euler-angle ambiguity.
//
//  Rebasing is throttled (only when view drifts > 0.1° from last origin)
//  so the tileset does not reload every single tick.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    constexpr double MetersPerDegreeLat = 111320.0;

    // Cesium uses centimetres: 1 m = 100 cm
    constexpr double CmPerMeter = 100.0;

    double WrapLon(double Lon)
    {
        Lon = FMath::Fmod(Lon + 180.0, 360.0);
        if (Lon < 0.0) Lon += 360.0;
        return Lon - 180.0;
    }
}

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength         = 0.0f;
    SpringArm->bDoCollisionTest        = false;
    SpringArm->bEnableCameraLag        = false;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch           = true;
    SpringArm->bInheritYaw             = true;
    SpringArm->bInheritRoll            = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->SetFieldOfView(60.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    // Input
    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Globe"));

    IA_Drag    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    IA_MouseXY = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);
}

void ARGGlobePawn::BeginPlay()
{
    Super::BeginPlay();

    CachedGeoreference = FindGeoreference();
    CurrentArmLength   = FMath::Clamp(CurrentArmLength, MinArmLength, MaxArmLength);

    // Force first rebase immediately
    LastRebaseLatitude  = 1e30;
    LastRebaseLongitude = 1e30;

    if (CachedGeoreference)
    {
        // Rebase origin immediately so the first ApplyCameraToGlobe() call
        // already has a correct ESU frame. Without this the first frame
        // uses the stale level-saved origin and the camera points wrong.
        CachedGeoreference->SetOriginLongitudeLatitudeHeight(
            FVector(ViewLongitude, ViewLatitude, 0.0));
        LastRebaseLatitude  = ViewLatitude;
        LastRebaseLongitude = ViewLongitude;
    }

    ApplyCameraToGlobe();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
                Sys->AddMappingContext(MappingContext, 0);
        }
    }
}

void ARGGlobePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Drag,    ETriggerEvent::Started,   this, &ARGGlobePawn::OnDragStarted);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Completed, this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Canceled,  this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Zoom,    ETriggerEvent::Triggered, this, &ARGGlobePawn::OnZoom);
        EIC->BindAction(IA_MouseXY, ETriggerEvent::Triggered, this, &ARGGlobePawn::OnMouseXY);
    }

    PlayerInputComponent->BindAxis("MoveForward", this, &ARGGlobePawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ARGGlobePawn::MoveRight);
}

void ARGGlobePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedGeoreference)
        CachedGeoreference = FindGeoreference();

    // ── Camera fly-to animation ───────────────────────────────────────────────
    if (bCameraAnimating)
    {
        AnimElapsed += DeltaTime;
        // Smooth ease-in-out: f(t) = t*t*(3-2*t)  (smoothstep)
        const float RawT = FMath::Clamp(AnimElapsed / AnimDuration, 0.0f, 1.0f);
        const float T    = RawT * RawT * (3.0f - 2.0f * RawT);

        // Interpolate longitude with anti-meridian awareness
        double DLon = AnimTargetLongitude - ViewLongitude;
        if      (DLon >  180.0) DLon -= 360.0;
        else if (DLon < -180.0) DLon += 360.0;

        ViewLatitude   = FMath::Lerp(ViewLatitude,   AnimTargetLatitude,                    static_cast<double>(DeltaTime) * 4.0 * (1.0 - static_cast<double>(T) + 0.05));
        ViewLongitude  = WrapLon(ViewLongitude + DLon * static_cast<double>(DeltaTime) * 4.0 * (1.0 - static_cast<double>(T) + 0.05));
        CurrentArmLength = FMath::Lerp(CurrentArmLength, AnimTargetArmLength, DeltaTime * 4.0f * (1.0f - T + 0.05f));

        if (AnimElapsed >= AnimDuration)
        {
            // Snap to exact target when done
            ViewLatitude     = AnimTargetLatitude;
            ViewLongitude    = AnimTargetLongitude;
            CurrentArmLength = AnimTargetArmLength;
            bCameraAnimating = false;
        }
    }

    if (bIsDragging)
    {
        // Any drag input cancels the fly-to
        bCameraAnimating = false;
        ApplyDragPan();
    }

    ApplyCameraToGlobe();
}

ACesiumGeoreference* ARGGlobePawn::FindGeoreference() const
{
    if (UWorld* World = GetWorld())
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
            return *It;
    return nullptr;
}

void ARGGlobePawn::ApplyCameraToGlobe()
{
    if (!CachedGeoreference) return;

    // ── 1. Throttled origin rebase ────────────────────────────────────────────
    // Only call SetOriginLongitudeLatitudeHeight when the view has moved more
    // than 0.1° (~11 km at equator) from the last rebase point.
    // This keeps the UE origin close to the camera without causing the tileset
    // to rebuild its tile tree on every single frame.
    const double dLat = FMath::Abs(ViewLatitude  - LastRebaseLatitude);
    const double dLon = FMath::Abs(WrapLon(ViewLongitude - LastRebaseLongitude));
    if (dLat > 0.1 || dLon > 0.1 || LastRebaseLatitude > 1e20)
    {
        CachedGeoreference->SetOriginLongitudeLatitudeHeight(
            FVector(ViewLongitude, ViewLatitude, 0.0));
        LastRebaseLatitude  = ViewLatitude;
        LastRebaseLongitude = ViewLongitude;
    }

    // ── 2. Camera position ────────────────────────────────────────────────────
    // After rebasing, (ViewLon, ViewLat, 0) == UE world origin (0,0,0).
    // The camera sits HeightM metres above that point along the local Up axis.
    // In Cesium's ENU-at-origin frame, Up == UE +Z (Cesium sets this up).
    // So the camera world position is simply (0, 0, HeightCm).
    //
    // Using TransformLongitudeLatitudeHeightPositionToUnreal is equivalent and
    // more explicit — it accounts for any residual offset if the georeference
    // scale or parent transform is non-identity.
    const double HeightM  = static_cast<double>(CurrentArmLength) * 0.01; // cm→m
    const FVector CamPos  = CachedGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(
        FVector(ViewLongitude, ViewLatitude, HeightM));
    SetActorLocation(CamPos);

    // ── 3. Camera orientation — nadir, north up ───────────────────────────────
    // We extract the world-space direction vectors directly from the
    // ESU-to-Unreal transformation matrix evaluated at the camera position.
    // This avoids ALL Euler-angle ambiguity.
    //
    // ESU axes at the surface point below the camera:
    //   Column 0 of the matrix = East  direction in Unreal world space
    //   Column 1 of the matrix = South direction in Unreal world space
    //   Column 2 of the matrix = Up    direction in Unreal world space
    //
    // For a nadir view with north at the top:
    //   Camera forward  = -Up    (look straight down)
    //   Camera screen-up= -South = North  (north at top of screen)
    //   Camera right    = East
    //
    // FRotationMatrix::MakeFromXY(Forward, Right) builds the rotation so that
    // local X = Forward and local Y = Right (Z computed from cross product).
    const FMatrix EsuToUnreal =
        CachedGeoreference->ComputeEastSouthUpToUnrealTransformation(CamPos);

    // Column vectors: GetColumn(i) returns column i as a 3-vector
    const FVector WorldEast  = FVector(EsuToUnreal.M[0][0], EsuToUnreal.M[1][0], EsuToUnreal.M[2][0]).GetSafeNormal();
    const FVector WorldSouth = FVector(EsuToUnreal.M[0][1], EsuToUnreal.M[1][1], EsuToUnreal.M[2][1]).GetSafeNormal();
    const FVector WorldUp    = FVector(EsuToUnreal.M[0][2], EsuToUnreal.M[1][2], EsuToUnreal.M[2][2]).GetSafeNormal();

    const FVector CamForward = -WorldUp;      // nadir (straight down)
    const FVector CamUp      = -WorldSouth;   // north = up on screen (South is +Y in ESU, North is -Y)

    // MakeFromXZ: X = forward, Z = up, Y is computed as Z × X (right)
    // This guarantees north stays at screen top.
    const FRotator CamRot = FRotationMatrix::MakeFromXZ(CamForward, CamUp).Rotator();
    SetActorRotation(CamRot);
}

// ─────────────────────────────────────────────────────────────────────────────

void ARGGlobePawn::ApplyDragPan()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference)
    {
        LastMouseDelta = FVector2D::ZeroVector;
        return;
    }

    FVector2D CursorPos = LastCursorPos;
    const bool bGotCursor = PC->GetMousePosition(CursorPos.X, CursorPos.Y);

    FVector2D Delta = LastMouseDelta;
    LastMouseDelta = FVector2D::ZeroVector;

    if (bGotCursor)
    {
        if (bHaveCursorPos)
            Delta = CursorPos - LastCursorPos;
        LastCursorPos  = CursorPos;
        bHaveCursorPos = true;
    }

    TotalDragPixels += Delta.Size();
    if (TotalDragPixels >= ClickDragThreshold)
        bDragExceededThreshold = true;

    const float NsScale = GetNorthSouthPanScale();

    // ── Grab-to-cursor: keep the geographic point under the cursor ────────────
    if (bHasGrabPoint)
    {
        FHitResult Hit;
        if (PC->GetHitResultUnderCursorByChannel(
                UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)
            && Hit.bBlockingHit)
        {
            const FVector HitLLH =
                CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);

            ViewLongitude = WrapLon(ViewLongitude + (GrabLongitude - HitLLH.X));
            ViewLatitude  = FMath::Clamp(
                ViewLatitude + (HitLLH.Y - GrabLatitude) * static_cast<double>(NsScale),
                -static_cast<double>(MaxAbsLatitude),
                static_cast<double>(MaxAbsLatitude));
            return;
        }
    }

    if (Delta.IsNearlyZero()) return;

    // ── Pixel-delta fallback ──────────────────────────────────────────────────
    int32 SizeX = 1920, SizeY = 1080;
    PC->GetViewportSize(SizeX, SizeY);
    SizeY = FMath::Max(SizeY, 1);

    const double HalfFovRad     = FMath::DegreesToRadians((Camera ? Camera->FieldOfView : 60.0f) * 0.5);
    const double MetersPerPixel = (2.0 * GetViewHeightMeters() * FMath::Tan(HalfFovRad))
                                  / static_cast<double>(SizeY);

    PanByMeters(-static_cast<double>(Delta.X) * MetersPerPixel,
                 static_cast<double>(Delta.Y) * MetersPerPixel * NsScale);
}

void ARGGlobePawn::PanByMeters(double EastMeters, double NorthMeters)
{
    const double CosLat = FMath::Max(0.05,
        FMath::Abs(FMath::Cos(FMath::DegreesToRadians(ViewLatitude))));

    ViewLatitude = FMath::Clamp(
        ViewLatitude + NorthMeters / MetersPerDegreeLat,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    ViewLongitude = WrapLon(
        ViewLongitude + EastMeters / (MetersPerDegreeLat * CosLat));
}

float ARGGlobePawn::GetNorthSouthPanScale() const
{
    const float Span  = FMath::Max(1.0f, PlanetViewFullHeight - PlanetViewStartHeight);
    const float Alpha = FMath::Clamp((CurrentArmLength - PlanetViewStartHeight) / Span, 0.0f, 1.0f);
    return 1.0f - Alpha;
}

// ─── Input ────────────────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging            = true;
    bDragExceededThreshold = false;
    bHasGrabPoint          = false;
    TotalDragPixels        = 0.0f;
    LastMouseDelta         = FVector2D::ZeroVector;
    bHaveCursorPos         = false;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference) return;

    if (PC->GetMousePosition(LastCursorPos.X, LastCursorPos.Y))
        bHaveCursorPos = true;

    FHitResult Hit;
    if (PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit)
        && Hit.bBlockingHit)
    {
        const FVector LLH =
            CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.ImpactPoint);
        GrabLongitude = LLH.X;
        GrabLatitude  = LLH.Y;
        bHasGrabPoint = true;
    }
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    bHasGrabPoint  = false;
    bHaveCursorPos = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis      = Value.Get<float>();
    const float ZoomDelta = CurrentArmLength * 0.15f * Axis;
    CurrentArmLength = FMath::Clamp(CurrentArmLength - ZoomDelta, MinArmLength, MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}

// ─── WASD ─────────────────────────────────────────────────────────────────────

void ARGGlobePawn::MoveForward(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float NsScale = GetNorthSouthPanScale();
    if (NsScale <= KINDA_SMALL_NUMBER) return;
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    PanByMeters(0.0, static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt * NsScale);
}

void ARGGlobePawn::MoveRight(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    PanByMeters(static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt, 0.0);
}

// ─── Focus ────────────────────────────────────────────────────────────────────

void ARGGlobePawn::FocusOnGuessResult(FRGGeoCoordinate Guess, FRGGeoCoordinate Actual, float DistanceKm)
{
    // Compute midpoint between the two locations (with anti-meridian handling)
    const double TargetLat = FMath::Clamp(
        (Guess.Latitude + Actual.Latitude) * 0.5,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    double LonA = Guess.Longitude, LonB = Actual.Longitude;
    const double DLon = LonB - LonA;
    if      (DLon >  180.0) LonB -= 360.0;
    else if (DLon < -180.0) LonB += 360.0;
    const double TargetLon = WrapLon((LonA + LonB) * 0.5);

    // Height: zoom out enough to show both markers, with a nice margin
    const float HeightKm     = FMath::Clamp(FMath::Max(DistanceKm * 2.5f, 500.0f), 500.0f, 18000.0f);
    const float TargetArm    = FMath::Clamp(HeightKm * 100000.0f, MinArmLength, MaxArmLength);

    // Start smooth animated fly-to
    AnimTargetLatitude  = TargetLat;
    AnimTargetLongitude = TargetLon;
    AnimTargetArmLength = TargetArm;
    AnimElapsed         = 0.0f;

    // Duration scales with angular distance — longer for antipodal, shorter for nearby
    const double AngularDist = static_cast<double>(DistanceKm) / 111.0; // rough degrees
    AnimDuration = FMath::Clamp(static_cast<float>(AngularDist * 0.08), 1.5f, 4.0f);

    bCameraAnimating = true;

    UE_LOG(LogTemp, Log, TEXT("FocusOnGuessResult: fly to lat=%.2f lon=%.2f alt=%.0fkm dur=%.1fs"),
        TargetLat, TargetLon, HeightKm, AnimDuration);
}
