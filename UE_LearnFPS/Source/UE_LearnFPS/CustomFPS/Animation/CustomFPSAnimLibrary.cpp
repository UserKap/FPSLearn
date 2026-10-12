/*
* 动画蓝图用的查询。序列评估器的显式时间用秒，这里直接给出播放长度。
*/

#include "CustomFPSAnimLibrary.h"

#include "Animation/AnimSequenceBase.h"

float UCustomFPSAnimLibrary::GetAnimPlayLength(const UAnimSequenceBase* Sequence)
{
	return Sequence ? Sequence->GetPlayLength() : 0.f;
}
