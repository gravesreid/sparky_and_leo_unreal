#include "N64Pad.h"
#include "SparkyLeo.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if PLATFORM_LINUX
#include <errno.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace
{
	// Names USB N64 pads and adapters report.
	const TCHAR* N64Names[] = {TEXT("N64"), TEXT("Nintendo 64"), TEXT("SWITCH CO.,LTD."), TEXT("Retrolink"),
	                           TEXT("Mayflash"), TEXT("Raphnet"), TEXT("Brawler64"), TEXT("ShanWan")};

	bool LooksLikeN64(const FString& Name)
	{
		for (const TCHAR* N : N64Names)
		{
			if (Name.Contains(N, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	// map spec keys, in EN64 order
	const TCHAR* Keys[] = {TEXT("A"), TEXT("B"), TEXT("Z"), TEXT("Start"), TEXT("L"), TEXT("R"), TEXT("CU"), TEXT("CD"),
	                       TEXT("CL"), TEXT("CR"), TEXT("DU"), TEXT("DD"), TEXT("DL"), TEXT("DR")};
	static_assert(UE_ARRAY_COUNT(Keys) == int32(EN64::Count), "one key per button");

	// the ShanWan / SWITCH CO.,LTD. adapter
	const TCHAR* DefaultMap = TEXT("A:1,B:2,Z:6,Start:12,L:4,R:5,CU:9,CD:0,CL:3,CR:8,DU:a5-,DD:a5+,DL:a4-,DR:a4+,SX:a0+,SY:a1-");

	const TCHAR* SaveSection = TEXT("Pads");
	FString SaveFile() { return FPaths::ProjectSavedDir() / TEXT("SparkyLeo_Pads.ini"); }
	FString SaveKey(const FString& DeviceName)
	{
		FString K;
		for (TCHAR C : DeviceName)
		{
			K.AppendChar(FChar::IsAlnum(C) ? C : TEXT('_'));
		}
		return K.IsEmpty() ? FString(TEXT("Unnamed")) : K;
	}

	/** "12" or "b12" = button 12, "a5-" / "a5+" = axis 5 that way, "-" = nothing. */
	bool ParseBind(const FString& V, FN64Pad::FBind& Out)
	{
		const FString S = V.TrimStartAndEnd();
		if (S == TEXT("-") || S.Equals(TEXT("none"), ESearchCase::IgnoreCase))
		{
			Out = FN64Pad::FBind();
			return true;
		}
		if (S.StartsWith(TEXT("a"), ESearchCase::IgnoreCase) && S.Len() >= 3)
		{
			Out.Index = uint8(FCString::Atoi(*S.Mid(1, S.Len() - 2)));
			Out.Kind = S.EndsWith(TEXT("-")) ? FN64Pad::FBind::AxisNeg : FN64Pad::FBind::AxisPos;
			return Out.Index < FN64Pad::MaxAxes;
		}
		const FString Num = S.StartsWith(TEXT("b"), ESearchCase::IgnoreCase) ? S.Mid(1) : S;
		if (Num.IsNumeric())
		{
			Out.Kind = FN64Pad::FBind::Button;
			Out.Index = uint8(FCString::Atoi(*Num));
			return Out.Index < 32;
		}
		return false;
	}

	FString BindString(const FN64Pad::FBind& B)
	{
		switch (B.Kind)
		{
		case FN64Pad::FBind::Button: return FString::Printf(TEXT("b%d"), B.Index);
		case FN64Pad::FBind::AxisPos: return FString::Printf(TEXT("a%d+"), B.Index);
		case FN64Pad::FBind::AxisNeg: return FString::Printf(TEXT("a%d-"), B.Index);
		default: return TEXT("-");
		}
	}

	constexpr int32 AxisPress = 16000;   // how far an axis has to move to count as a press
}

// ---------------------------------------------------------------- FN64Pad

FN64Pad::FN64Pad()
{
	ResetMap();
}

void FN64Pad::ResetMap()
{
	for (FBind& B : Bind)
	{
		B = FBind();
	}
	FMemory::Memzero(Rest);
	StickAxis[0] = 0;
	StickAxis[1] = 1;
	StickSign[0] = 1.f;
	StickSign[1] = -1.f;
	StickCenter[0] = StickCenter[1] = 0.f;
	StickPos[0] = StickPos[1] = StickNeg[0] = StickNeg[1] = 32767.f;
	SetMap(DefaultMap);
}

void FN64Pad::SetMap(const FString& Spec)
{
	TArray<FString> Parts;
	Spec.ParseIntoArray(Parts, TEXT(","));
	for (const FString& P : Parts)
	{
		FString K, V;
		if (!P.Split(TEXT(":"), &K, &V))
		{
			continue;
		}
		K.TrimStartAndEndInline();
		bool bDone = false;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Keys) && !bDone; ++i)
		{
			if (K.Equals(Keys[i], ESearchCase::IgnoreCase))
			{
				FBind B;
				if (ParseBind(V, B))
				{
					Bind[i] = B;
				}
				bDone = true;
			}
		}
		if (bDone)
		{
			continue;
		}
		const float F = FCString::Atof(*V);
		for (int32 k = 0; k < 2; ++k)
		{
			const TCHAR Ax = k == 0 ? 'X' : 'Y';
			FBind B;
			if (K.Equals(FString::Printf(TEXT("S%c"), Ax), ESearchCase::IgnoreCase) && ParseBind(V, B) && B.Kind != FBind::Button && B.Kind != FBind::None)
			{
				StickAxis[k] = B.Index;
				StickSign[k] = B.Kind == FBind::AxisPos ? 1.f : -1.f;
			}
			else if (K.Equals(FString::Printf(TEXT("%cC"), Ax), ESearchCase::IgnoreCase)) StickCenter[k] = F;
			else if (K.Equals(FString::Printf(TEXT("%cP"), Ax), ESearchCase::IgnoreCase)) StickPos[k] = FMath::Max(F, 4000.f);
			else if (K.Equals(FString::Printf(TEXT("%cN"), Ax), ESearchCase::IgnoreCase)) StickNeg[k] = FMath::Max(F, 4000.f);
		}
		if (K.Len() >= 2 && (K[0] == 'R' || K[0] == 'r') && K.Mid(1).IsNumeric())
		{
			const int32 I = FCString::Atoi(*K.Mid(1));
			if (I >= 0 && I < MaxAxes)
			{
				Rest[I] = int16(FMath::Clamp(FCString::Atoi(*V), -32767, 32767));
			}
		}
	}
	UpdateAxisBinds();
	Pressed = 0;
}

FString FN64Pad::GetMap() const
{
	TArray<FString> Parts;
	for (int32 i = 0; i < int32(EN64::Count); ++i)
	{
		Parts.Add(FString::Printf(TEXT("%s:%s"), Keys[i], *BindString(Bind[i])));
	}
	for (int32 k = 0; k < 2; ++k)
	{
		const TCHAR Ax = k == 0 ? 'X' : 'Y';
		Parts.Add(FString::Printf(TEXT("S%c:a%d%c"), Ax, StickAxis[k], StickSign[k] > 0.f ? '+' : '-'));
		Parts.Add(FString::Printf(TEXT("%cC:%d"), Ax, FMath::RoundToInt(StickCenter[k])));
		Parts.Add(FString::Printf(TEXT("%cP:%d"), Ax, FMath::RoundToInt(StickPos[k])));
		Parts.Add(FString::Printf(TEXT("%cN:%d"), Ax, FMath::RoundToInt(StickNeg[k])));
	}
	for (int32 i = 0; i < MaxAxes; ++i)
	{
		if (Rest[i] != 0)
		{
			Parts.Add(FString::Printf(TEXT("R%d:%d"), i, Rest[i]));
		}
	}
	return FString::Join(Parts, TEXT(","));
}

FString FN64Pad::LoadSaved(const FString& DeviceName)
{
	FConfigFile F;
	F.Read(SaveFile());
	FString V;
	F.GetString(SaveSection, *SaveKey(DeviceName), V);
	return V;
}

void FN64Pad::Save(const FString& DeviceName, const FString& Spec)
{
	FConfigFile F;
	F.Read(SaveFile());
	F.SetString(SaveSection, *SaveKey(DeviceName), *Spec);
	F.Write(SaveFile());
	UE_LOG(LogSparky, Log, TEXT("Saved controller calibration for %s: %s"), *DeviceName, *Spec);
}

bool FN64Pad::Open(const FString& Spec)
{
	Close();
	if (Spec.Equals(TEXT("off"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (Spec.IsEmpty() || Spec.Equals(TEXT("auto"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 0; i < 16; ++i)
		{
			if (OpenDevice(FString::Printf(TEXT("/dev/input/js%d"), i), true))
			{
				return true;
			}
		}
		return false;
	}
	return OpenDevice(Spec, false);
}

bool FN64Pad::OpenDevice(const FString& DevPath, bool bRequireN64Name)
{
#if PLATFORM_LINUX
	const int F = ::open(TCHAR_TO_UTF8(*DevPath), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (F < 0)
	{
		return false;
	}
	char NameBuf[128] = {};
	ioctl(F, JSIOCGNAME(sizeof(NameBuf) - 1), NameBuf);
	const FString DevName = UTF8_TO_TCHAR(NameBuf);
	const FString Saved = LoadSaved(DevName);
	// an N64-looking name, or anything that's been calibrated
	if (bRequireN64Name && !LooksLikeN64(DevName) && Saved.IsEmpty())
	{
		::close(F);
		return false;
	}
	uint8 Axes = 0;
	ioctl(F, JSIOCGAXES, &Axes);
	Fd = F;
	Name = DevName;
	Path = DevPath;
	NumAxes = Axes ? FMath::Min(int32(Axes), MaxAxes) : 8;  // (8 if the device won't say, like a test pipe)
	FMemory::Memzero(Axis);
	ResetMap();
	if (!Saved.IsEmpty())
	{
		SetMap(Saved);
	}
	Poll();  // the driver sends the current state as JS_EVENT_INIT events
	Pressed = 0;
	UE_LOG(LogSparky, Log, TEXT("Controller: %s (%s)%s"), *Name, *DevPath, Saved.IsEmpty() ? TEXT("") : TEXT(", calibrated"));
	return true;
#else
	return false;
#endif
}

void FN64Pad::Close()
{
#if PLATFORM_LINUX
	if (Fd >= 0)
	{
		::close(Fd);
	}
#endif
	Fd = -1;
	ButtonsDown = 0;
	Pressed = 0;
	AxisDown = 0;
	FMemory::Memzero(Axis);
}

bool FN64Pad::Poll()
{
#if PLATFORM_LINUX
	if (Fd < 0)
	{
		return false;
	}
	js_event E;
	for (;;)
	{
		const ssize_t N = ::read(Fd, &E, sizeof(E));
		if (N == sizeof(E))
		{
			const bool bInit = (E.type & JS_EVENT_INIT) != 0;
			if ((E.type & JS_EVENT_AXIS) && E.number < MaxAxes)
			{
				Axis[E.number] = E.value;
				const uint32 Before = AxisDown;
				UpdateAxisBinds();
				if (bInit)
				{
					Pressed &= ~(AxisDown & ~Before);
				}
			}
			else if ((E.type & JS_EVENT_BUTTON) && E.number < 32)
			{
				if (E.value)
				{
					ButtonsDown |= 1u << E.number;
					if (!bInit)
					{
						for (int32 i = 0; i < int32(EN64::Count); ++i)
						{
							if (Bind[i].Kind == FBind::Button && Bind[i].Index == E.number)
							{
								Pressed |= 1u << i;
							}
						}
					}
				}
				else
				{
					ButtonsDown &= ~(1u << E.number);
				}
			}
			continue;
		}
		if (N < 0 && errno == EAGAIN)
		{
			return true;
		}
		UE_LOG(LogSparky, Warning, TEXT("Controller %s disconnected"), *Name);
		Close();
		return false;
	}
#else
	return false;
#endif
}

bool FN64Pad::AxisBindDown(const FBind& B) const
{
	if (B.Kind != FBind::AxisPos && B.Kind != FBind::AxisNeg)
	{
		return false;
	}
	const int32 D = int32(Axis[B.Index]) - Rest[B.Index];
	return B.Kind == FBind::AxisPos ? D > AxisPress : D < -AxisPress;
}

void FN64Pad::UpdateAxisBinds()
{
	uint32 Now = 0;
	for (int32 i = 0; i < int32(EN64::Count); ++i)
	{
		if (AxisBindDown(Bind[i]))
		{
			Now |= 1u << i;
		}
	}
	Pressed |= Now & ~AxisDown;
	AxisDown = Now;
}

FVector2D FN64Pad::GetStick() const
{
	FVector2D V;
	for (int32 k = 0; k < 2; ++k)
	{
		const float D = float(Axis[FMath::Clamp(StickAxis[k], 0, MaxAxes - 1)]) - StickCenter[k];
		V[k] = FMath::Clamp(D / (D >= 0.f ? StickPos[k] : StickNeg[k]), -1.f, 1.f) * StickSign[k];
	}
	const float Len = V.Size();
	if (Len < DeadZone)
	{
		return FVector2D::ZeroVector;
	}
	// rescale so the edge of the dead zone is zero and full tilt stays full
	return V / Len * FMath::Min(1.f, (Len - DeadZone) / (1.f - DeadZone));
}

bool FN64Pad::IsDown(EN64 B) const
{
	const FBind& Bd = Bind[int32(B)];
	if (Bd.Kind == FBind::Button)
	{
		return Bd.Index < 32 && (ButtonsDown & (1u << Bd.Index)) != 0;
	}
	return (AxisDown & (1u << int32(B))) != 0;
}

bool FN64Pad::WasPressed(EN64 B)
{
	const uint32 Bit = 1u << int32(B);
	const bool bWas = (Pressed & Bit) != 0;
	Pressed &= ~Bit;
	return bWas;
}

// ---------------------------------------------------------------- FPadCalibrator

namespace
{
	constexpr int32 DetectThreshold = 20000;   // an axis this far from rest is a deliberate press
	constexpr int32 ReleaseThreshold = 8000;
	constexpr float RollTime = 3.f;

	const TCHAR* ButtonLabel(EN64 B)
	{
		static const TCHAR* L[] = {TEXT("A"), TEXT("B"), TEXT("Z"), TEXT("START"), TEXT("L"), TEXT("R"), TEXT("C-UP"),
		                           TEXT("C-DOWN"), TEXT("C-LEFT"), TEXT("C-RIGHT"), TEXT("D-PAD UP"), TEXT("D-PAD DOWN"),
		                           TEXT("D-PAD LEFT"), TEXT("D-PAD RIGHT")};
		return L[int32(B)];
	}
}

void FPadCalibrator::Start()
{
	Stop();
	bActive = true;
	Step = 0;
	StepTime = 0.f;
	Progress = 0.f;
	bWaitRelease = false;
	for (FN64Pad::FBind& B : Found)
	{
		B = FN64Pad::FBind();
	}
	StickAxis[0] = StickAxis[1] = -1;
	// listen to every joystick: whichever one A gets pressed on is the one being set up
	for (int32 i = 0; i < 16; ++i)
	{
		TUniquePtr<FN64Pad> P = MakeUnique<FN64Pad>();
		if (P->OpenAny(FString::Printf(TEXT("/dev/input/js%d"), i)))
		{
			CandidatePrev.Add(P->GetRawButtons());
			Candidates.Add(MoveTemp(P));
		}
	}
	FString Extra;  // dev: -PadTest=<path> listens to one more device (a pipe of fake events, say)
	if (FParse::Value(FCommandLine::Get(), TEXT("PadTest="), Extra))
	{
		TUniquePtr<FN64Pad> P = MakeUnique<FN64Pad>();
		if (P->OpenAny(Extra))
		{
			CandidatePrev.Add(P->GetRawButtons());
			Candidates.Add(MoveTemp(P));
		}
	}
}

void FPadCalibrator::Stop()
{
	bActive = false;
	Dev = nullptr;
	Candidates.Reset();
	CandidatePrev.Reset();
}

FPadCalibrator::EStepKind FPadCalibrator::Kind() const
{
	if (Step == 0) return Choose;
	if (Step < FirstButtonStep + 13) return Buttons;
	switch (Step - FirstButtonStep - 13)
	{
	case 0: return StickRest;
	case 1: return StickRight;
	case 2: return StickUp;
	case 3: return StickRoll;
	default: return Confirm;
	}
}

void FPadCalibrator::Next()
{
	++Step;
	StepTime = 0.f;
	Progress = 0.f;
}

void FPadCalibrator::CaptureRest()
{
	for (int32 i = 0; i < FN64Pad::MaxAxes; ++i)
	{
		Rest[i] = int16(Dev ? Dev->GetRawAxis(i) : 0);
	}
}

bool FPadCalibrator::IsUsed(const FN64Pad::FBind& B) const
{
	for (const FN64Pad::FBind& F : Found)
	{
		if (F.Kind != FN64Pad::FBind::None && F == B)
		{
			return true;
		}
	}
	return false;
}

bool FPadCalibrator::DetectInput(FN64Pad::FBind& Out)
{
	const uint32 Buttons = Dev->GetRawButtons();
	const uint32 New = Buttons & ~PrevButtons;
	PrevButtons = Buttons;
	if (New)
	{
		Out.Kind = FN64Pad::FBind::Button;
		Out.Index = uint8(FMath::CountTrailingZeros(New));
		return true;
	}
	for (int32 i = 0; i < Dev->GetNumAxes(); ++i)
	{
		const int32 D = Dev->GetRawAxis(i) - Rest[i];
		if (FMath::Abs(D) > DetectThreshold)
		{
			Out.Kind = D > 0 ? FN64Pad::FBind::AxisPos : FN64Pad::FBind::AxisNeg;
			Out.Index = uint8(i);
			return true;
		}
	}
	return false;
}

bool FPadCalibrator::AllReleased() const
{
	if (Dev->GetRawButtons() != 0)
	{
		return false;
	}
	for (int32 i = 0; i < Dev->GetNumAxes(); ++i)
	{
		if (FMath::Abs(Dev->GetRawAxis(i) - Rest[i]) > ReleaseThreshold)
		{
			return false;
		}
	}
	return true;
}

FString FPadCalibrator::BuildSpec() const
{
	TArray<FString> Parts;
	for (int32 i = 0; i < int32(EN64::Count); ++i)
	{
		Parts.Add(FString::Printf(TEXT("%s:%s"), Keys[i], *BindString(Found[i])));
	}
	for (int32 i = 0; i < FN64Pad::MaxAxes; ++i)
	{
		if (Rest[i] != 0)
		{
			Parts.Add(FString::Printf(TEXT("R%d:%d"), i, Rest[i]));  // axes that don't rest at zero
		}
	}
	for (int32 k = 0; k < 2; ++k)
	{
		const TCHAR Ax = k == 0 ? 'X' : 'Y';
		const int32 A = StickAxis[k];
		if (A < 0)
		{
			continue;
		}
		const float C = Rest[A];
		const float Pos = StickMax[k] - C, Neg = C - StickMin[k];
		Parts.Add(FString::Printf(TEXT("S%c:a%d%c"), Ax, A, StickSign[k] > 0.f ? '+' : '-'));
		Parts.Add(FString::Printf(TEXT("%cC:%d"), Ax, FMath::RoundToInt(C)));
		// a little short of the measured edge, so full tilt is reachable without straining
		Parts.Add(FString::Printf(TEXT("%cP:%d"), Ax, FMath::RoundToInt(Pos > 6000.f ? Pos * 0.92f : 32767.f)));
		Parts.Add(FString::Printf(TEXT("%cN:%d"), Ax, FMath::RoundToInt(Neg > 6000.f ? Neg * 0.92f : 32767.f)));
	}
	return FString::Join(Parts, TEXT(","));
}

bool FPadCalibrator::Tick(float Dt, FN64Pad& Target)
{
	if (!bActive)
	{
		return false;
	}
	StepTime += Dt;
	if (Kind() == Choose)
	{
		for (int32 c = 0; c < Candidates.Num(); ++c)
		{
			FN64Pad& P = *Candidates[c];
			P.Poll();
			const uint32 New = P.GetRawButtons() & ~CandidatePrev[c];
			CandidatePrev[c] = P.GetRawButtons();
			if (New && !Dev)
			{
				Dev = &P;
				PrevButtons = P.GetRawButtons();
				Found[int32(EN64::A)] = {FN64Pad::FBind::Button, uint8(FMath::CountTrailingZeros(New))};
				CaptureRest();
				bWaitRelease = true;
				Next();
			}
		}
		if (Dev)
		{
			// keep only the chosen one
			Candidates.RemoveAll([this](const TUniquePtr<FN64Pad>& P) { return P.Get() != Dev; });
		}
		else if (StepTime > 2.f && Candidates.Num() == 0)
		{
			StepTime = 0.f;
			Start();  // nothing plugged in yet: keep looking
		}
		return false;
	}
	if (!Dev || !Dev->Poll())
	{
		Start();  // unplugged: begin again
		return false;
	}
	if (bWaitRelease)
	{
		if (!AllReleased())
		{
			PrevButtons = Dev->GetRawButtons();
			return false;
		}
		bWaitRelease = false;
		PrevButtons = Dev->GetRawButtons();
	}
	FN64Pad::FBind B;
	switch (Kind())
	{
	case Buttons:
		if (DetectInput(B))
		{
			const EN64 Which = StepButton();
			const bool bDPad = Which >= EN64::DUp;
			if (bDPad && B == Found[int32(EN64::Start)])
			{
				Found[int32(Which)] = FN64Pad::FBind();  // no D-pad on this one: skipped
			}
			else if (IsUsed(B))
			{
				bWaitRelease = true;  // already taken: wait and try again
				return false;
			}
			else
			{
				Found[int32(Which)] = B;
			}
			bWaitRelease = true;
			Next();
		}
		break;
	case StickRest:
		// let go of everything for a moment, and take that as the middle
		PrevButtons = Dev->GetRawButtons();
		Progress = FMath::Clamp(StepTime / 1.2f, 0.f, 1.f);
		if (Dev->GetRawButtons() != 0)
		{
			StepTime = 0.f;
		}
		if (StepTime > 1.2f)
		{
			CaptureRest();
			Next();
		}
		break;
	case StickRight:
	case StickUp:
	{
		const int32 K = Kind() == StickRight ? 0 : 1;
		int32 Best = -1;
		int32 BestD = 0;
		for (int32 i = 0; i < Dev->GetNumAxes(); ++i)
		{
			const bool bButtonAxis = IsUsed({FN64Pad::FBind::AxisPos, uint8(i)}) || IsUsed({FN64Pad::FBind::AxisNeg, uint8(i)});
			if (bButtonAxis || (K == 1 && i == StickAxis[0]))
			{
				continue;
			}
			const int32 D = Dev->GetRawAxis(i) - Rest[i];
			if (FMath::Abs(D) > FMath::Abs(BestD))
			{
				BestD = D;
				Best = i;
			}
		}
		if (Best >= 0 && FMath::Abs(BestD) > 14000)
		{
			Progress = FMath::Clamp(Progress + Dt / 0.3f, 0.f, 1.f);
			if (Progress >= 1.f)
			{
				StickAxis[K] = Best;
				StickSign[K] = BestD > 0 ? 1.f : -1.f;
				bWaitRelease = true;
				Next();
			}
		}
		else
		{
			Progress = 0.f;
		}
		break;
	}
	case StickRoll:
	{
		for (int32 k = 0; k < 2; ++k)
		{
			const float V = Dev->GetRawAxis(StickAxis[k]);
			if (!bRollStarted)
			{
				StickMax[k] = StickMin[k] = Rest[StickAxis[k]];
			}
			StickMax[k] = FMath::Max(StickMax[k], V);
			StickMin[k] = FMath::Min(StickMin[k], V);
		}
		bRollStarted = true;
		const bool bMoving = FMath::Abs(Dev->GetRawAxis(StickAxis[0]) - Rest[StickAxis[0]]) > 6000
			|| FMath::Abs(Dev->GetRawAxis(StickAxis[1]) - Rest[StickAxis[1]]) > 6000;
		if (bMoving)
		{
			Progress = FMath::Clamp(Progress + Dt / RollTime, 0.f, 1.f);
		}
		if (Progress >= 1.f)
		{
			bRollStarted = false;
			bWaitRelease = true;
			Next();
		}
		break;
	}
	case Confirm:
		if (DetectInput(B))
		{
			if (B == Found[int32(EN64::A)])
			{
				const FString Spec = BuildSpec();
				const FString DevName = Dev->GetName(), DevPath = Dev->GetPath();
				FN64Pad::Save(DevName, Spec);
				Stop();
				Target.Open(DevPath);  // loads the saved map
				return true;
			}
			if (B == Found[int32(EN64::B)])
			{
				Start();
				return false;
			}
			bWaitRelease = true;
		}
		break;
	default:
		break;
	}
	return false;
}

FString FPadCalibrator::Prompt() const
{
	switch (Kind())
	{
	case Choose: return TEXT("PRESS A ON YOUR CONTROLLER");
	case Buttons: return FString::Printf(TEXT("PRESS %s"), ButtonLabel(StepButton()));
	case StickRest: return TEXT("LET GO OF THE STICK");
	case StickRight: return TEXT("PUSH THE STICK ALL THE WAY RIGHT");
	case StickUp: return TEXT("PUSH THE STICK ALL THE WAY UP");
	case StickRoll: return TEXT("ROLL THE STICK ROUND THE EDGE");
	default: return TEXT("ALL DONE!");
	}
}

FString FPadCalibrator::Hint() const
{
	switch (Kind())
	{
	case Choose: return Candidates.Num() ? FString::Printf(TEXT("%d CONTROLLER%s FOUND"), Candidates.Num(), Candidates.Num() == 1 ? TEXT("") : TEXT("S"))
	                                     : FString(TEXT("NO CONTROLLERS FOUND - PLUG ONE IN"));
	case Buttons:
		if (bWaitRelease) return TEXT("(LET GO)");
		return StepButton() >= EN64::DUp ? TEXT("NO D-PAD? PRESS START TO SKIP") : TEXT("");
	case StickRest: return TEXT("HANDS OFF FOR A SECOND...");
	case StickRight: case StickUp: return bWaitRelease ? TEXT("(LET GO)") : TEXT("AND HOLD IT THERE");
	case StickRoll: return TEXT("A FEW TIMES, AS FAR AS IT GOES");
	default: return TEXT("PRESS A TO SAVE, B TO START AGAIN");
	}
}

void FPadCalibrator::GetLive(TArray<float>& OutAxes, TArray<int32>& OutButtons) const
{
	OutAxes.Reset();
	OutButtons.Reset();
	if (!Dev)
	{
		return;
	}
	for (int32 i = 0; i < Dev->GetNumAxes(); ++i)
	{
		OutAxes.Add(Dev->GetRawAxis(i) / 32767.f);
	}
	for (int32 j = 0; j < 32; ++j)
	{
		if (Dev->GetRawButtons() & (1u << j))
		{
			OutButtons.Add(j);
		}
	}
}
