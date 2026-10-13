/*
* 后坐力数据。默认曲线是一把突击步枪：前几发直着往上，后面左右摆。
*/

#include "CustomFPS/Weapon/CustomFPSRecoilData.h"

#include "Curves/RichCurve.h"

UCustomFPSRecoilData::UCustomFPSRecoilData()
{
	FRichCurve* Vertical = VerticalPattern.GetRichCurve();
	Vertical->AddKey(1.f, 1.3f);
	Vertical->AddKey(4.f, 1.1f);
	Vertical->AddKey(10.f, 0.85f);
	Vertical->AddKey(30.f, 0.7f);

	FRichCurve* Horizontal = HorizontalPattern.GetRichCurve();
	Horizontal->AddKey(1.f, 0.f);
	Horizontal->AddKey(5.f, 0.2f);
	Horizontal->AddKey(9.f, -0.7f);
	Horizontal->AddKey(14.f, 0.8f);
	Horizontal->AddKey(20.f, -0.5f);
	Horizontal->AddKey(30.f, 0.4f);
}

FVector2D UCustomFPSRecoilData::EvaluatePattern(int32 ShotNumber) const
{
	const float Time = static_cast<float>(FMath::Max(ShotNumber, 1));

	const FRichCurve* Vertical = VerticalPattern.GetRichCurveConst();
	const float VerticalScale = Vertical && Vertical->GetNumKeys() > 0 ? Vertical->Eval(Time) : 1.f;

	const FRichCurve* Horizontal = HorizontalPattern.GetRichCurveConst();
	const float HorizontalScale = Horizontal && Horizontal->GetNumKeys() > 0 ? Horizontal->Eval(Time) : 0.f;

	return FVector2D(VerticalRecoil * VerticalScale, HorizontalRecoil * HorizontalScale);
}
