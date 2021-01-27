﻿/*!	@file
	@brief ズーム倍率算出
*/
/*
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

#include "StdAfx.h"
#include "zoom.h"
#include <algorithm>
#include <cmath>

/*!
	@brief 設定値の正当性判定
*/
bool ZoomSetting::IsValid() const
{
	return (m_nValueMin <= m_nValueMax)
		&& (0.0 <= m_nValueUnit)
		&& std::is_sorted( m_vZoomFactors.begin(), m_vZoomFactors.end() );
}

/*!
	@brief 値テーブル上における指定値の位置を取得
	@param[in] vTable 値テーブル
	@param[in] nValue 指定値
	@return 位置
	@note 「位置」は基本的にはテーブルのインデックスに相当しますが、
	      テーブル範囲外およびテーブル上の隣り合う二値間については 0.5 で表現します。
	@note 例として vTable:{10, 20, 30} の場合の nValue と戻り値との関係を示します。
	nValue |    0 |    5 |   10 |   15 |   25 |   30 |   35 |   40 |
	-------|------|------|------|------|------|------|------|------|
	戻り値 | -0.5 | -0.5 |  0.0 |  0.5 |  1.5 |  2.0 |  2.5 |  2.5 |
*/
static double GetPositionInTable( const std::vector<double>& vTable, double nValue )
{
	double nPosition = (double)vTable.size() - 0.5;
	for( size_t i = 0; i < vTable.size(); ++i ){
		if( nValue <= vTable[i] ){
			if( nValue == vTable[i] ){
				nPosition = (double)i;
			}else{
				nPosition = (double)i - 0.5;
			}
			break;
		}
	}
	return nPosition;
}

/*!
	@brief 最小単位で丸めた値を取得
	@param[in] nValue	対象値
	@param[in] nUnit	最小単位(0.0 の場合は丸めなし)
	@return 丸められた値
*/
static double GetQuantizedValue( double nValue, double nUnit )
{
	return (0.0 < nUnit) ? std::floor( nValue / nUnit ) * nUnit : nValue;
}

/*!
	@brief 基準値に対してズーム倍率を適用した値を取得
	@param[in] zoomSetting	ズーム設定
	@param[in] nBaseValue	基準値
	@param[in] nCurrentZoom	現在のズーム倍率
	@param[in] nSteps		ズーム段階の変更量
	@param[in] pnValueOut	変更後のズーム倍率を適用した値
	@param[in] pnZoomOut	変更後のズーム倍率
	@return 丸められた値
*/
bool GetZoomedValue( const ZoomSetting& zoomSetting, double nBaseValue, double nCurrentZoom, int nSteps, double* pnValueOut, double* pnZoomOut )
{
	if( nSteps == 0 ){
		return false;
	}

	const bool bZoomUp = (0 < nSteps);
	const int nTableIndexMin = 0;
	const int nTableIndexMax = (int)zoomSetting.m_vZoomFactors.size() - 1;

	const double nPosition = GetPositionInTable( zoomSetting.m_vZoomFactors, nCurrentZoom );
	int nTableIndex = (int)(bZoomUp ? std::floor( nPosition ) : std::ceil( nPosition ));
	if( (!bZoomUp && nTableIndex <= nTableIndexMin) || (bZoomUp && nTableIndexMax <= nTableIndex) ){
		// 現在の倍率がすでに倍率テーブルの範囲外でかつ
		// さらに外側へ移動しようとした場合は今の位置を維持
		return false;
	}

	const double nCurrentValue = GetQuantizedValue( nBaseValue * nCurrentZoom, zoomSetting.m_nValueUnit );
	const double nValueMin = std::min( {zoomSetting.m_nValueMin, nBaseValue, nCurrentValue} );
	const double nValueMax = std::max( {zoomSetting.m_nValueMax, nBaseValue, nCurrentValue} );
	double nNextValue = nCurrentValue;
	double nNextZoom = nCurrentZoom;
	double nLastValue = nCurrentValue;
	double nLastZoom = nCurrentZoom;
	bool bFindingOneMoreChange = false;

	// 最小単位で丸めた後の値が変更前の値から変わらなければ
	// 変わる位置までインデックスを動かしていく
	nTableIndex += nSteps;
	// 本当は while( true ) で良いが万一の暴走回避のため有限回
	for( size_t i = 0; i < zoomSetting.m_vZoomFactors.size(); ++i ){
		int clampedIndex = std::clamp( nTableIndex, nTableIndexMin, nTableIndexMax );
		nNextZoom = zoomSetting.m_vZoomFactors[clampedIndex];
		nNextValue = GetQuantizedValue( nBaseValue * nNextZoom, zoomSetting.m_nValueUnit );

		if( bFindingOneMoreChange ){
			if( nNextValue != nLastValue || clampedIndex != nTableIndex ){
				nNextValue = nLastValue;
				nNextZoom = nLastZoom;
				break;
			}
		}else{
			// 値の上下限を超過したら上下限に丸めて終わる
			// 倍率は丸めた後のサイズで再計算
			double clampedValue = std::clamp( nNextValue, nValueMin, nValueMax );
			if( nNextValue != clampedValue ){
				if( nBaseValue != 0.0 ){
					nNextZoom = clampedValue / nBaseValue;
				}
				nNextValue = clampedValue;
				break;
			}

			// 倍率テーブルの端まで到達していたら終わり
			if( clampedIndex != nTableIndex ){
				break;
			}

			bool bSizeChanged = (nNextValue != nCurrentValue);
			if( bZoomUp ){
				// 拡大側は値が変わったらすぐ終わる
				if( bSizeChanged ){
					break;
				}
			}else{
				// 縮小側は一度値が変わった後もう一度値が変わる位置までインデックスを進めてから終わる
				if( bSizeChanged ){
					bFindingOneMoreChange = true;
				}
			}
		}

		nLastValue = nNextValue;
		nLastZoom = nNextZoom;
		nTableIndex += bZoomUp ? 1 : -1;
	};

	if( nCurrentValue == nNextValue ){
		return false;
	}

	if( pnValueOut != NULL ){
		*pnValueOut = nNextValue;
	}
	if( pnZoomOut != NULL ){
		*pnZoomOut = nNextZoom;
	}

	return true;
}
