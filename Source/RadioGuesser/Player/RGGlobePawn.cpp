// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Map/RGCesiumMapManager.h"
#include "Player/RGPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength  = 500000000.0f;  // ~5000 км
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed   = 8.0f;
    SpringArm->bInheritPitch    = false;  // Pitch всегда фиксированный
    SpringArm->bInheritYaw      = false;  // Yaw не наследуем от пауна
    SpringArm->bInheritRoll     = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    // Input assets created in constructor
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

    SetActorLocation(FVector::ZeroVector);
    SpringArm->TargetArmLength = CurrentArmLength;

    // Камера смотрит прямо вниз (pitch -90 = вертикально вниз)
    // Так карта выглядит как вид сверху без перспективного искажения
    SpringArm->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sys->AddMappingContext(MappingContext, 0);
            }
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

    // Smooth zoom
    if (SpringArm)
    {
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength, CurrentArmLength, DeltaTime, 8.0f);
    }

    // Mouse drag — перемещаем ПАУН горизонтально по карте.
    // Движение мыши вправо = карта едет влево = паун едет вправо.
    // Масштаб скорости пропорционален высоте камеры: чем выше — тем быстрее пан.
    if (bIsDragging && !LastMouseDelta.IsNearlyZero())
    {
        const float Scale = SpringArm->TargetArmLength * PanScale;
        const FVector Delta(
            -LastMouseDelta.Y * Scale,  // вперёд/назад
             LastMouseDelta.X * Scale,  // влево/вправо (инвертировано — тащим карту)
             0.0f
        );
        AddActorWorldOffset(Delta);
        LastMouseDelta = FVector2D::ZeroVector;
    }
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging    = true;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    CurrentArmLength = FMath::Clamp(
        CurrentArmLength - Axis * ZoomSpeed,
        MinArmLength,
        MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}

// ─── WASD pan ─────────────────────────────────────────────────────────────────

void ARGGlobePawn::MoveForward(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float Scale = SpringArm->TargetArmLength * PanScale;
    AddActorWorldOffset(FVector(Value * KeyboardPanSpeed * Scale, 0.0f, 0.0f));
}

void ARGGlobePawn::MoveRight(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;
    const float Scale = SpringArm->TargetArmLength * PanScale;
    AddActorWorldOffset(FVector(0.0f, Value * KeyboardPanSpeed * Scale, 0.0f));
}
